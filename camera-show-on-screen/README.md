# OV7670 + FIFO Camera Demo

Minimal Zephyr application that captures QQVGA (160x120) RGB565 frames from an
OV7670 camera module with AL422B FIFO, running on the **nRF54L15 DK**.

A snapshot is taken once per second and stored in a RAM buffer (`frame_buf`).

## Hardware

| Component | Details |
|---|---|
| MCU board | nRF54L15 DK (`nrf54l15dk/nrf54l15/cpuapp`) |
| Camera module | OV7670 + AL422B FIFO |
| Master clock | 12 MHz oscillator on the camera PCB (no MCU pin needed) |
| Resolution | QQVGA 160x120, RGB565 (38 400 bytes/frame) |

## Pin Wiring

### Port 0 — TFT Display (SPI) and Camera WEN

P0.00–P0.03 are available because `uart30` is disabled in this build and the
camera data bus is on Port 2 (QSPI flash disconnected via Board Configurator).
RESET is tied high in hardware — no MCU pin required.

> When the connection says "3.3 V" - connect to an available VDD:IO pin.
> When connecting the camera, all camera pins except HREF and STR should end up connected. Read *all* the tables below.

| Display Pin | nRF54L15 DK Pin | Function | Notes |
|---|---|---|---|
| SCK | P0.00 | SPI Clock | uart30 disabled — dedicated to TFT |
| SDA/MOSI | P0.01 | SPI Data | uart30 disabled — dedicated to TFT |
| CS | P0.02 | Chip Select | uart30 disabled — dedicated to TFT |
| DC/A0 | P0.03 | Data/Command | uart30 disabled — dedicated to TFT |
| RESET | 3.3 V via 10 kΩ | Hardware reset | no MCU pin needed — tie high. (The 10 kΩ resistor is not required - connect directly to a 3.3 V pin)  |
| VCC | 3.3 V | Power | |
| GND | GND | Ground | |
| LED/BL | 3.3 V | Backlight | |

| Camera Signal | nRF54L15 DK Pin | Direction | Notes |
|---|---|---|---|
| AL422B WEN (WR) | P0.04 | Output | FIFO write enable (active low); shared with Button3 |

> To use hardware SPI for the display, add `SPIM30` to the overlay (the SPIM30
> peripheral shares the uart30 address block, which is disabled). Bit-banged GPIO
> also works on these pins.

> For the pull-up - use for example a bread board to connect the two pins together with a connection to 3.3 V through a 4.7k resistor.
> See for example [the two top connections in this circuit](https://hades.mech.northwestern.edu/index.php/I2C_communication_between_PICs#Circuit).

### Port 1 — Camera I2C, FIFO control and sensors

| Module | Signal | nRF54L15 DK Pin | Direction | Notes |
|---|---|---|---|---|
| OV7670 (Camera) | SIOD | P1.11 | I2C | SCCB data — 4.7 kΩ pull-up to 3.3 V *(Arduino SDA, i2c21)* |
| OV7670 (Camera) | SIOC | P1.12 | I2C | SCCB clock — 4.7 kΩ pull-up to 3.3 V *(Arduino SCL, i2c21)* |
| AL422B (Camera) | RRST | P1.14 | Output | FIFO read-pointer reset (active low); shared with LED3 |
| LM393 (Microphone) | OUT | P1.13 | Input | Digital comparator output (sampled using ADC so could be analog mic); shared with Button0 |

> **P1.04–P1.07** reserved for console UART (uart20) — do not use.
> **P1.00/P1.01** are not wired to any expansion header on PCA10156.
> **P1.02/P1.03** are NFC antenna pins — require 0Ω resistor rework, do not use.

### LM393 Microphone — Power

| LM393 Pin | Connection |
|---|---|
| VCC | VDD pin on P1 header |
| GND | GND pin on P1 header |
| OUT | P1.13 (see Port 1 table above) *(P1.13 also drives Button0 — sounds may result in flickering button presses)* |

### Port 2 — Camera Data Bus and Timing

> **Board Configurator required:** P2.00–P2.05 are routed to the on-board QSPI flash
> by default. Use the [Nordic Board Configurator](https://docs.nordicsemi.com/bundle/swtools_docs/page/app/pc-nrfconnect-board-configurator/index.html)
> to disconnect the flash and expose those pins on the expansion header. The `spi00`
> peripheral is also disabled in `boards/nrf54l15dk_nrf54l15_cpuapp.overlay`.

| Signal | nRF54L15 DK Pin | Direction | Notes |
|---|---|---|---|
| D0 | P2.00 | Input | Data bit 0 |
| D1 | P2.01 | Input | Data bit 1 |
| D2 | P2.02 | Input | Data bit 2 |
| D3 | P2.03 | Input | Data bit 3 |
| D4 | P2.04 | Input | Data bit 4 |
| D5 | P2.05 | Input | Data bit 5 |
| D6 | P2.06 | Input | Data bit 6 *(P2.06 also drives LED2 — camera data dominates)* |
| D7 | P2.07 | Input | Data bit 7 |
| VSYNC | P2.08 | Input | Frame synchronisation |
| RCK | P2.09 | Output | FIFO read clock *(P2.09 is LED0 — blinks during readout)* |
| WRST | P2.10 | Output | FIFO write-pointer reset (active low) |

### Camera — Directly Wired (no MCU pin)

| Module Label | Connection | Notes |
|---|---|---|
| XCLK | 12 MHz oscillator on PCB | No MCU connection required (There is no such pin)|
| OE | GND | FIFO output always enabled (active low) |
| RST | 3.3 V via 10 kΩ resistor | Keep camera out of reset (The 10 kΩ resistor is not required)|
| PWDN | GND | Camera always powered on |
| HREF | — | Not connected (unused in FIFO capture mode) |
| STR | — | Not connected |
| 3V3 | 3.3 V | |
| GND | GND | |

### Free Resources (available for application code)

Moving the data bus from P1.08/09/10/13 → P2.00–P2.07 freed all buttons and LED1.

| Resource | Pin | Notes |
|---|---|---|
| Button1 (SW1) | P1.09 | Active low, internal pull-up |
| Button2 (SW2) | P1.08 | Active low, internal pull-up |
| LED1 | P1.10 | Active high; also PWM20-capable |
| *(Button0/LM393)* | *P1.13* | *Shared with LM393 microphone* |
| *(Button3/WEN)* | *P0.04* | *Shared with camera WEN — active-low output during capture* |
| *(LED0/RCK)* | *P2.09* | *Shared with FIFO read clock — blinks during readout* |
| *(LED2/D6)* | *P2.06* | *Shared with camera D6 — unavailable* |
| *(LED3/RRST)* | *P1.14* | *Shared with FIFO read-pointer reset — active-low output* |

## Build & Flash

```bash
west build -b nrf54l15dk/nrf54l15/cpuapp
west flash
```

## Console Output

Connect a serial terminal at 115200 baud. You should see:

```raw
[00:00:00.000,000] <inf> ov7670: === OV7670 FIFO Camera Demo ===
[00:00:00.000,000] <inf> ov7670: Frame: 160x120 RGB565, 38400 bytes
[00:00:00.010,000] <inf> ov7670: OV7670 found — PID 0x76  VER 0x73
[00:00:00.120,000] <inf> ov7670: OV7670 configured — QQVGA 160x120 RGB565
[00:00:01.500,000] <inf> ov7670: Frame #1 captured (38400 bytes @ 0x...)
```

## Dumping Frames over UART

Set `DUMP_FRAME_UART` to `1` in `src/main.c` and rebuild. Each frame is printed
as hex between `---FRAME_START---` and `---FRAME_END---` markers. Pipe the
serial output to a file on the host and convert with a script, for example:

```python
# convert_frame.py — hex dump to raw RGB565 PNG
from PIL import Image
import sys

W, H = 160, 120
raw = bytes.fromhex(open(sys.argv[1]).read().strip())
img = Image.frombytes("RGB", (W, H), b"".join(
    bytes([(p >> 11) << 3, ((p >> 5) & 0x3F) << 2, (p & 0x1F) << 3])
    for p in (int.from_bytes(raw[i:i+2], "big") for i in range(0, len(raw), 2))
))
img.save("frame.png")
```

## Project Structure

```raw
ov7670-fifo-camera/
├── CMakeLists.txt
├── prj.conf
├── README.md
├── boards/
│   └── nrf54l15dk_nrf54l15_cpuapp.overlay   # i2c21 config; spi00 disabled
└── src/
    ├── ov7670.h     # OV7670 public API: ov7670_init()
    ├── ov7670.c     # SCCB read/write, register table, init, readback verify
    ├── fifo.h       # FIFO public API: fifo_init(), fifo_capture(); IMG_* dims
    ├── fifo.c       # GPIO setup, FIFO control, frame capture, pin-map notes
    └── main.c       # Boot, init calls, 1 Hz capture loop, optional hex dump
```

## Notes

- The OV7670 does not have a native 128x160 mode. QQVGA (160x120) is the
  closest standard resolution. Custom windowing via HSTART/HSTOP/VSTART/VSTOP
  registers can crop to 128x160 but requires per-module tuning.
- D0-D7 are mapped to P2.00-P2.07 so the firmware can read all 8 bits in a
  single `gpio_port_get_raw()` call (masked with `& 0xFF`).
- The I2C pull-up resistors (4.7 kΩ to 3.3 V) on SDA/SCL (P1.11/P1.12) are essential.
- Keep SIOD and SIOC wires **short and unextended**. Chaining jumper wires raises line
  capacitance enough to corrupt SCCB register writes at 100 kHz. The OV7670 silently
  accepts the garbled writes and runs unconfigured, producing repetitive output that
  looks identical to stuck FIFO data.
- Use `nrf54l15dk/nrf54l15/cpuapp` as build configuration - the samples may not
  necessarily compile if you use `nrf54l15dk/nrf54l15/cpuapp/ns`.
- If you run out of 3.3 V pins - be creative using a bread board or similar.
