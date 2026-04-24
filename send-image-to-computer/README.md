# send-image-to-computer

Sends an RGB565 image to a PC over UART when a button is pressed. Each frame carries an optional UTF-8 metadata string (e.g. a class label or detection result). The PC-side Python script receives frames and saves them as PNG files with the metadata embedded.

---

## Hardware

| Component | Details |
|-----------|---------|
| Board | nRF54L15 DK (`nrf54l15dk/nrf54l15/cpuapp`) |
| Button | Button 1 (`sw0`, P1.13, active-low) |
| UART | uart20 — USB-UART bridge on the DK (1 Mbaud) |

No extra wiring needed beyond the USB cable.

---

## How it works

1. Press **Button 1** on the DK.
2. The firmware attaches a metadata string to the image and sends the frame over UART.
3. The Python script on the PC receives the frame, prints the metadata, and saves a PNG.

---

## UART frame protocol

```
[0xDE 0xAD 0xBE 0xEF]    4 bytes — start-of-frame marker
[W_lo  W_hi]              2 bytes — image width  (little-endian uint16)
[H_lo  H_hi]              2 bytes — image height (little-endian uint16)
[bpp]                     1 byte  — bytes per pixel (2 = RGB565)
[endian]                  1 byte  — 0 = little-endian pixels, 1 = big-endian
[meta_len_lo meta_len_hi] 2 bytes — metadata length (little-endian uint16)
[meta_len bytes]          UTF-8 metadata string (no null terminator)
[W × H × bpp bytes]       raw pixel data
[0xCA 0xFE 0xBA 0xBE]    4 bytes — end-of-frame marker
```

Set `meta_len = 0` to send a frame with no metadata.
At 1 Mbaud the 40 960-byte USN logo payload transfers in about 0.41 s.

### Metadata examples

```c
uart_img_send(uart, pixels, 160, 128, 2, 1, "drone=1");
uart_img_send(uart, pixels, 160, 128, 2, 1, "class=drone,conf=0.97");
uart_img_send(uart, pixels, 160, 128, 2, 1, "label=clear");
uart_img_send(uart, pixels, 160, 128, 2, 1, NULL);  /* no metadata */
```

---

## Build & flash

```bash
west build -b nrf54l15dk/nrf54l15/cpuapp send-image-to-computer
west flash
```

Open a serial terminal at **1 000 000 baud** (8N1) to see log output.

---

## PC receive script

### Requirements

```bash
pip install pyserial pillow
pip install numpy          # optional — speeds up pixel conversion
```

### Usage

```bash
python scripts/receive_image.py <port>
```

| Platform | Typical port |
|----------|-------------|
| Windows | `COM4` |
| Linux | `/dev/ttyACM0` |
| macOS | `/dev/cu.usbmodem…` |

Each valid frame is saved as a PNG with the metadata embedded in the file's
`tEXt` chunk (readable by any image viewer that shows PNG metadata):

```
frame_0001_<unix_timestamp>.png
```

Press **Ctrl+C** to stop.

### Example session

```
$ python scripts/receive_image.py COM4
Opening COM4 at 1000000 baud ...
Connected. Waiting for frames — press Button 1 on the board.

Frame incoming: 160×128, 2 bpp, big-endian, 40960 bytes
  Metadata: label=usnlogo
  Saved: frame_0001_1745493210.png
```

---

## Project structure

```
send-image-to-computer/
├── boards/
│   └── nrf54l15dk_nrf54l15_cpuapp.overlay   # Sets uart20 to 1 Mbaud
├── scripts/
│   ├── convert_logo.py                       # PNG → usnlogo.h converter
│   └── receive_image.py                      # PC-side UART image receiver
├── src/
│   ├── main.c                                # Button handler, sends usnlogo
│   ├── uart_img_send.c / .h                  # Reusable UART frame sender
│   └── usnlogo.h                             # Test image pixel data
├── CMakeLists.txt
└── prj.conf
```

### Reusing `uart_img_send` in another project

Copy `uart_img_send.c` and `uart_img_send.h` into the other project, add the `.c` to `CMakeLists.txt`, then include the header:

```c
#include "uart_img_send.h"

uart_img_send(uart, frame_buf, IMG_W, IMG_H, 2, 0, "drone=1");
```

The only Zephyr dependency is `CONFIG_SERIAL=y` (already needed for any UART use).
