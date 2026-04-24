# send-image-to-computer

Displays the USN logo on a 1.8" TFT screen and sends it to a PC over UART when a button is pressed. The PC-side Python script receives the binary image data and saves it as a PNG file.

---

## Hardware

| Component | Details |
|-----------|---------|
| Board | nRF54L15 DK (`nrf54l15dk/nrf54l15/cpuapp`) |
| Display | 1.8" TFT, ST7735R driver, 160×128 px |
| Button | Button 1 (`sw0`, P1.13, active-low) |
| UART | uart20 — USB-UART bridge on the DK (1 Mbaud) |

### TFT wiring

| TFT pin | nRF54L15 DK pin | Function |
|---------|-----------------|----------|
| SCK | P0.00 | SPI clock (spi30) |
| SDA / MOSI | P0.01 | SPI data |
| CS | P0.02 | Chip select (active-low) |
| DC / RS | P0.03 | Data/Command select |
| RESET | 3.3 V via 10 kΩ | No MCU pin needed |
| VCC | 3.3 V | |
| GND | GND | |
| LED / BL | 3.3 V | Backlight |

> `uart30` is disabled in the overlay because it shares the same peripheral address
> block (`0x104000`) as `spi30`. Both cannot be active simultaneously.

---

## How it works

1. On boot the firmware initialises the TFT and draws the USN logo (160×128, RGB565).
2. Press **Button 1** on the DK.
3. The firmware sends the logo pixel buffer over UART as a binary frame.
4. The Python script on the PC receives the frame and saves it as a PNG.

### UART frame protocol

```
[0xDE 0xAD 0xBE 0xEF]   4 bytes — start-of-frame marker
[W_lo  W_hi]             2 bytes — image width  (little-endian uint16)
[H_lo  H_hi]             2 bytes — image height (little-endian uint16)
[bpp]                    1 byte  — bytes per pixel (2 = RGB565)
[endian]                 1 byte  — 0 = little-endian pixels, 1 = big-endian
[W × H × bpp bytes]      raw pixel data
[0xCA 0xFE 0xBA 0xBE]   4 bytes — end-of-frame marker
```

At 1 Mbaud the 40 960-byte USN logo payload transfers in about 0.41 s.
The Python script tolerates arbitrary log text between frames — it searches the
stream for the start marker and ignores everything else.

---

## Build & flash

```bash
west build -b nrf54l15dk/nrf54l15/cpuapp send-image-to-computer
west flash
```

Open a serial terminal at **1 000 000 baud** (8N1) to see log output. The baud
rate has been raised from the board default of 115 200, so make sure your
terminal emulator is set to match.

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
| Windows | `COM3` |
| Linux | `/dev/ttyACM0` |
| macOS | `/dev/cu.usbmodem…` |

The script scans the serial stream continuously for the start-of-frame marker.
Each valid frame is saved to the current directory as:

```
frame_0001_<unix_timestamp>.png
```

Press **Ctrl+C** to stop.

### Example session

```
$ python scripts/receive_image.py COM3
Opening COM3 at 1000000 baud ...
Connected. Waiting for frames — press Button 1 on the board.

Frame incoming: 160×128, 2 bpp, big-endian, 40960 bytes
  Saved: frame_0001_1745493210.png

Frame incoming: 160×128, 2 bpp, big-endian, 40960 bytes
  Saved: frame_0002_1745493225.png
```

---

## Project structure

```
send-image-to-computer/
├── boards/
│   └── nrf54l15dk_nrf54l15_cpuapp.overlay   # SPI30 TFT pins, 1 Mbaud uart20
├── scripts/
│   ├── convert_logo.py                       # PNG → usnlogo.h converter
│   └── receive_image.py                      # PC-side UART image receiver
├── src/
│   ├── main.c                                # Button, UART send, TFT display
│   ├── tft_display.c / .h                    # ST7735R driver + drawing API
│   └── usnlogo.h                             # Logo pixel data (auto-generated)
├── CMakeLists.txt
└── prj.conf
```

To regenerate `usnlogo.h` from a new source image:

```bash
python scripts/convert_logo.py usnlogo.png
```

---

## TFT display API

The `tft_display` module is reusable in other projects. Copy `tft_display.h`
and `tft_display.c` into your project and add `tft_display.c` to your
`CMakeLists.txt`.

### Lifecycle

| Function | Description |
|---|---|
| `TFT_DEVICE()` | Get the `st7735r` device from devicetree |
| `tft_init(dev)` | Check device ready, turn blanking off. Returns `0` or `-ENODEV`. |

### Constants

| Macro | Description |
|---|---|
| `TFT_WIDTH` / `TFT_HEIGHT` | 160 / 128 px |
| `TFT_BPP` | 2 (RGB565) |
| `TFT_COLOR_BLACK/WHITE/RED/…` | Common RGB565 colour constants |

### Drawing functions

| Function | Description |
|---|---|
| `tft_fill_screen(dev, color)` | Fill entire display with one colour |
| `tft_fill_rect(dev, x, y, w, h, color)` | Fill a rectangle |
| `tft_draw_hline(dev, x, y, w, color)` | Horizontal line |
| `tft_draw_vline(dev, x, y, h, color)` | Vertical line |
| `tft_draw_char(dev, x, y, c, fg, bg)` | Single 5×7 character |
| `tft_draw_string(dev, x, y, str, fg, bg)` | Null-terminated string |
| `tft_draw_image(dev, x, y, w, h, rgb565)` | Blit a raw RGB565 frame buffer |
| `tft_draw_bounding_box(dev, x, y, w, h, label)` | CV-style labelled bounding box |
