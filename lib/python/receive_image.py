#!/usr/bin/env python3
"""
receive_image.py — Receive images from nRF54L15 over UART (1 Mbaud).

Pixel formats (bpp field):
  1  greyscale  (1 byte per pixel)
  2  RGB565     (2 bytes per pixel, endian flag applies)
  3  RGB888     (3 bytes per pixel)

Protocol:
  [0xDE 0xAD 0xBE 0xEF]    start-of-frame marker
  [W_lo W_hi H_lo H_hi]    dimensions as two little-endian uint16
  [bpp]                     bytes per pixel (1=grey, 2=RGB565, 3=RGB888)
  [endian]                  0 = little-endian pixels, 1 = big-endian (RGB565 only)
  [meta_len_lo meta_len_hi] metadata length (little-endian uint16), 0 = none
  [meta_len bytes]          UTF-8 metadata string: "label=X,folder=Y,filename=Z"
  [W * H * bpp bytes]       raw pixel data
  [0xCA 0xFE 0xBA 0xBE]    end-of-frame marker

Requirements:
  pip install pyserial pillow

Usage:
  python receive_image.py PORT [--folder FOLDER] [--filename NAME]

  PORT        Serial port, e.g. COM4 or /dev/ttyACM0
  --folder    Fallback base output folder if the frame has no folder= metadata
  --filename  Fallback filename if the frame has no filename= metadata

Output path: [folder/][label/][filename]
  Values from the frame metadata take precedence over CLI fallbacks.
  If no label is present no subfolder is created.
  If no filename is available, auto-generates frame_NNNN_TIMESTAMP.png.
"""

import sys
import os
import struct
import time
import argparse
from collections import deque

try:
    import serial
except ImportError:
    sys.exit("pyserial not found — run:  pip install pyserial")

try:
    from PIL import Image, PngImagePlugin
except ImportError:
    sys.exit("Pillow not found — run:  pip install pillow")

try:
    import colorama
    colorama.init()
except ImportError:
    pass

BASE_DIR = os.path.dirname(os.path.abspath(__file__))

BAUD_RATE = 1_000_000
FRAME_SOF = bytes([0xDE, 0xAD, 0xBE, 0xEF])
FRAME_EOF = bytes([0xCA, 0xFE, 0xBA, 0xBE])
ACK = bytes([0x06])
NAK = bytes([0x15])
PACKET_SIZE = 512  # Must match UART_IMG_PACKET_SIZE in uart_img_send.h
MAX_CONSECUTIVE_ERRORS = 10

_FMT_NAMES = {1: "greyscale", 2: "RGB565", 3: "RGB888"}

RED = "\033[31m"
RESET = "\033[0m"

previous_file_prefix = ""

def error_line(message: str) -> None:
    print(f"{RED}{message}{RESET}", file=sys.stderr)


def extract_meta_field(metadata: str, key: str) -> str:
    """Return the value for key in a comma-separated 'key=value' metadata string."""
    for part in metadata.split(","):
        kv = part.strip().split("=", 1)
        if len(kv) == 2 and kv[0].strip() == key:
            return kv[1].strip()
    return ""


def rgb565_to_image(data: bytes, width: int, height: int,
                    big_endian: bool) -> Image.Image:
    try:
        import numpy as np
        dtype = ">u2" if big_endian else "<u2"
        pixels = np.frombuffer(data, dtype=dtype).reshape(height, width)
        r = ((pixels >> 11) & 0x1F) * 255 // 31
        g = ((pixels >>  5) & 0x3F) * 255 // 63
        b = ( pixels        & 0x1F) * 255 // 31
        rgb = np.stack([r, g, b], axis=-1).astype("uint8")
        return Image.fromarray(rgb, "RGB")
    except ImportError:
        rgb = bytearray()
        for i in range(0, len(data), 2):
            if big_endian:
                px = (data[i] << 8) | data[i + 1]
            else:
                px = data[i] | (data[i + 1] << 8)
            rgb += bytes([
                ((px >> 11) & 0x1F) * 255 // 31,
                ((px >>  5) & 0x3F) * 255 // 63,
                ( px        & 0x1F) * 255 // 31,
            ])
        return Image.frombytes("RGB", (width, height), bytes(rgb))


def pixels_to_image(data: bytes, width: int, height: int,
                    big_endian: bool, bpp: int) -> Image.Image:
    if bpp == 1:
        return Image.frombytes("L", (width, height), data)
    elif bpp == 2:
        return rgb565_to_image(data, width, height, big_endian)
    elif bpp == 3:
        return Image.frombytes("RGB", (width, height), data)
    else:
        raise ValueError(f"Unsupported bpp: {bpp}")


def find_marker(ser: "serial.Serial", marker: bytes) -> None:
    """Block until the exact byte sequence appears in the stream."""
    window = deque(maxlen=len(marker))
    while bytes(window) != marker:
        b = ser.read(1)
        if b:
            window.append(b[0])


def receive_packet(ser: "serial.Serial") -> tuple:
    """Read one packet, validate XOR checksum, and return (seq, payload).

    Raises IOError on timeout and ValueError on checksum mismatch.
    Packet format: [seq_lo seq_hi len_lo len_hi] [payload...] [xor_checksum]
    """
    hdr = ser.read(4)
    if len(hdr) < 4:
        raise IOError("Timeout reading packet header")
    seq = struct.unpack_from("<H", hdr, 0)[0]
    length = struct.unpack_from("<H", hdr, 2)[0]
    payload = ser.read(length)
    if len(payload) < length:
        raise IOError(f"Truncated payload ({len(payload)}/{length} bytes)")
    chk_byte = ser.read(1)
    if not chk_byte:
        raise IOError("Timeout reading checksum")
    expected = 0
    for b in hdr:
        expected ^= b
    for b in payload:
        expected ^= b
    if chk_byte[0] != expected:
        raise ValueError(
            f"Checksum mismatch (got 0x{chk_byte[0]:02x}, expected 0x{expected:02x})"
        )
    return seq, bytes(payload)


def receive_frames(port: str, default_folder: str, default_filename: str) -> None:
    print(f"Opening {port} at {BAUD_RATE} baud ...")
    with serial.Serial(port, BAUD_RATE, timeout=2) as ser:
        print("Connected. Waiting for frames — press Button 1 on the board.\n")
        frame_count = 0

        while True:
            find_marker(ser, FRAME_SOF)

            # --- Packet 0: image header + metadata ---
            # Retry in case of corruption; sender retransmits on NAK.
            header_ok = False
            for _ in range(MAX_CONSECUTIVE_ERRORS):
                try:
                    seq, hdr_payload = receive_packet(ser)
                    if seq != 0:
                        error_line(f"Expected header packet seq=0, got {seq} — sent NAK")
                        ser.write(NAK)
                        continue
                    if len(hdr_payload) < 8:
                        error_line("Header packet payload too short — sent NAK")
                        ser.write(NAK)
                        continue
                    ser.write(ACK)
                    header_ok = True
                    break
                except (IOError, ValueError) as exc:
                    error_line(f"Header packet error: {exc} — sent NAK")
                    ser.write(NAK)

            if not header_ok:
                error_line("Could not receive header packet, re-syncing ...")
                continue

            width, height = struct.unpack_from("<HH", hdr_payload, 0)
            bpp        = hdr_payload[4]
            big_endian = bool(hdr_payload[5])
            meta_len   = struct.unpack_from("<H", hdr_payload, 6)[0]
            metadata   = ""
            if meta_len > 0:
                metadata = hdr_payload[8:8 + meta_len].decode("utf-8", errors="replace")

            meta_folder = extract_meta_field(metadata, "folder")
            label       = extract_meta_field(metadata, "label")
            filename    = extract_meta_field(metadata, "filename") or default_filename

            filename_prefix = filename.split("_")[0] if filename else ""
            global previous_file_prefix
            if filename_prefix != previous_file_prefix:
                print()
                previous_file_prefix = filename_prefix

            # --- Packets 1..N: pixel data ---
            total_bytes = width * height * bpp
            data = bytearray()
            expected_seq = 1
            frame_ok = True
            consecutive_errors = 0

            while len(data) < total_bytes:
                try:
                    seq, payload = receive_packet(ser)
                    consecutive_errors = 0
                    if seq == expected_seq:
                        data.extend(payload)
                        ser.write(ACK)
                        expected_seq += 1
                    elif seq == expected_seq - 1:
                        # Duplicate: our previous ACK was lost; re-ACK without appending.
                        ser.write(ACK)
                    else:
                        error_line(
                            f"Sequence error: expected {expected_seq}, got {seq} — sent NAK"
                        )
                        ser.write(NAK)
                        frame_ok = False
                        break
                except (IOError, ValueError) as exc:
                    error_line(f"Packet error (seq {expected_seq}): {exc} — sent NAK")
                    ser.write(NAK)
                    consecutive_errors += 1
                    if consecutive_errors >= MAX_CONSECUTIVE_ERRORS:
                        error_line("Too many consecutive errors, re-syncing ...")
                        frame_ok = False
                        break

            if not frame_ok:
                continue

            # --- EOF marker + final ACK ---
            eof = ser.read(len(FRAME_EOF))
            if eof != FRAME_EOF:
                error_line(f"Bad EOF marker: {eof.hex()} — sent NAK")
                ser.write(NAK)
                continue

            ser.write(ACK)

            # --- Save image ---
            subparts = [p for p in [meta_folder, label] if p]
            outdir = os.path.join(default_folder, *subparts)
            os.makedirs(outdir, exist_ok=True)

            if not filename:
                frame_count += 1
                filename = f"frame_{frame_count:04d}_{int(time.time())}.png"

            filepath = os.path.join(outdir, filename)
            if os.path.exists(filepath):
                print(f"  Skipping save: file already exists: {filepath}")
                continue

            img = pixels_to_image(bytes(data), width, height, big_endian, bpp)

            png_meta = PngImagePlugin.PngInfo()
            if metadata:
                png_meta.add_text("metadata", metadata)
            img.save(filepath, pnginfo=png_meta)
            print(f"  Saved: {filepath}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Receive images from nRF54L15 over UART",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__)
    parser.add_argument("port",
                        help="Serial port (e.g. COM4 or /dev/ttyACM0)")
    parser.add_argument("--folder", default=os.path.abspath(os.path.join(BASE_DIR, "../../train/dataset")),
                        help="Fallback base output folder (default: dataset/ next to this script)")
    parser.add_argument("--filename", default="",
                        help="Fallback output filename (default: frame_NNNN_TIMESTAMP.png)")
    args = parser.parse_args()

    try:
        receive_frames(args.port, args.folder, args.filename)
    except KeyboardInterrupt:
        print("\nStopped.")
    except serial.SerialException as exc:
        sys.exit(f"Serial error: {exc}")
