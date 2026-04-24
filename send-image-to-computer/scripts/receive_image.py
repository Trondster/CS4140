#!/usr/bin/env python3
"""
receive_image.py — Receive RGB565 images from nRF54L15 over UART (1 Mbaud).

Protocol:
  [0xDE 0xAD 0xBE 0xEF]  start-of-frame marker
  [W_lo W_hi H_lo H_hi]  dimensions as two little-endian uint16
  [bpp]                   bytes per pixel (2 = RGB565)
  [endian]                0 = little-endian pixels, 1 = big-endian pixels
  [W * H * bpp bytes]     raw pixel data
  [0xCA 0xFE 0xBA 0xBE]  end-of-frame marker

Requirements:
  pip install pyserial pillow

Usage:
  python receive_image.py COM3          # Windows
  python receive_image.py /dev/ttyACM0  # Linux / macOS
"""

import sys
import struct
import time
from collections import deque

try:
    import serial
except ImportError:
    sys.exit("pyserial not found — run:  pip install pyserial")

try:
    from PIL import Image
except ImportError:
    sys.exit("Pillow not found — run:  pip install pillow")

BAUD_RATE  = 1_000_000
FRAME_SOF  = bytes([0xDE, 0xAD, 0xBE, 0xEF])
FRAME_EOF  = bytes([0xCA, 0xFE, 0xBA, 0xBE])


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
        # Pure-Python fallback (slower)
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


def find_marker(ser: "serial.Serial", marker: bytes) -> None:
    """Block until the exact byte sequence appears in the stream."""
    window = deque(maxlen=len(marker))
    while bytes(window) != marker:
        b = ser.read(1)
        if b:
            window.append(b[0])


def receive_frames(port: str) -> None:
    print(f"Opening {port} at {BAUD_RATE} baud ...")
    with serial.Serial(port, BAUD_RATE, timeout=2) as ser:
        print("Connected. Waiting for frames — press Button 1 on the board.\n")
        frame_count = 0

        while True:
            find_marker(ser, FRAME_SOF)

            hdr = ser.read(6)  # W_lo W_hi H_lo H_hi bpp endian
            if len(hdr) < 6:
                print("Timeout reading header, retrying ...")
                continue

            width, height = struct.unpack_from("<HH", hdr, 0)
            bpp        = hdr[4]
            big_endian = bool(hdr[5])
            size       = width * height * bpp

            print(f"Frame incoming: {width}×{height}, {bpp} bpp, "
                  f"{'big' if big_endian else 'little'}-endian, {size} bytes")

            data = ser.read(size)
            if len(data) < size:
                print(f"  Truncated ({len(data)}/{size} bytes) — skipping")
                continue

            eof = ser.read(len(FRAME_EOF))
            if eof != FRAME_EOF:
                print(f"  Bad end marker: {eof.hex()} — skipping")
                continue

            frame_count += 1
            filename = f"frame_{frame_count:04d}_{int(time.time())}.png"
            img = rgb565_to_image(data, width, height, big_endian)
            img.save(filename)
            print(f"  Saved: {filename}\n")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    try:
        receive_frames(sys.argv[1])
    except KeyboardInterrupt:
        print("\nStopped.")
    except serial.SerialException as exc:
        sys.exit(f"Serial error: {exc}")
