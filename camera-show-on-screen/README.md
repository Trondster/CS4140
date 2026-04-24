# Camera Show on Screen

Captures QQVGA (160×120) RGB565 frames from an OV7670 FIFO camera module and
displays them live on a 1.8" TFT screen (ST7735R, 160×128 px) on the
**nRF54L15 DK**.

---

## Hardware Required

| Component | Part |
|-----------|------|
| Development board | Nordic nRF54L15 DK (PCA10156) |
| Camera module | OV7670 with AL422B FIFO (the 18-pin module with onboard 12 MHz crystal) |
| Display | 1.8" TFT SPI (ST7735R, 160×128 px) |

---

## Pin Connections

### OV7670 FIFO Camera Module → nRF54L15 DK

| Camera pin | nRF54L15 DK pin | Notes |
|------------|-----------------|-------|
| 3V3 | 3.3V | Power |
| GND | GND | Ground |
| SIOD | P1.11 | SCCB data (Arduino I2C SDA header) |
| SIOC | P1.12 | SCCB clock (Arduino I2C SCL header) |
| D0 | P2.00 | Data bit 0 |
| D1 | P2.01 | Data bit 1 |
| D2 | P2.02 | Data bit 2 |
| D3 | P2.03 | Data bit 3 |
| D4 | P2.04 | Data bit 4 |
| D5 | P2.05 | Data bit 5 |
| D6 | P2.06 | Data bit 6 |
| D7 | P2.07 | Data bit 7 (also LED2 — flickers with data) |
| VS / VSYNC | P2.08 | Frame sync input |
| RCK | P2.09 | FIFO read clock (also LED0 — blinks during readout) |
| WRST | P2.10 | FIFO write-pointer reset (active-low) |
| WEN | P0.04 | FIFO write enable (active-low, shared with Button 3) |
| RRST | P1.14 | FIFO read-pointer reset (active-low, shared with LED3) |
| HREF | — | No MCU pin — see note below |
| XCLK | — | 12 MHz crystal onboard — no MCU pin needed |
| OE | GND | FIFO output always enabled |
| RST | 3.3V via 10 kΩ | Camera held out of reset — no MCU pin needed |
| PWDN | GND | Camera always powered |

> **HREF note:** HREF (horizontal reference) signals valid pixel data on the
> data bus during each line. On the OV7670 FIFO module PCB, the camera's HREF
> output is wired directly to the AL422B FIFO's internal write-timing circuit,
> so the FIFO automatically captures only valid pixels without any MCU
> involvement. The driver configures the HREF window via the `OV7670_REG_HREF`
> register (0x32 = 0x24) but never reads the pin. No wire to the DK is needed.

> **SCCB wiring note:** Keep the SIOD/SIOC wires short. Long or shared wires
> add capacitance that corrupts SCCB writes and produces stuck or tinted images.
> External 4.7 kΩ pull-ups to 3.3V are required on both lines.

---

### 1.8" TFT Display (ST7735R) → nRF54L15 DK

| Display pin | nRF54L15 DK pin | Notes |
|-------------|-----------------|-------|
| VCC | 3.3V | Power |
| GND | GND | Ground |
| SCK / CLK | P0.00 | SPI clock (SPI30) |
| SDA / MOSI | P0.01 | SPI data (SPI30) |
| CS | P0.02 | Chip select (active-low) |
| DC / RS / A0 | P0.03 | Data/command select |
| RESET | 3.3V via 10 kΩ | Held out of reset — no MCU pin needed |

---

## Important Board Notes

- **On-board SPI flash (MX25R64) is disabled** — its pins (P2.00-P2.05)
  overlap with the camera data bus D0-D5. `spi00` is disabled in the overlay.
- **uart30 is disabled** — it shares the same address block as spi30, which
  drives the TFT display on P0.00-P0.03.
- **UART20 (P1.04-P1.07) is the console** — do not connect anything to those
  pins. Serial output appears at 115200 baud on the DK's USB-UART bridge.

---

## Build and Flash

```bash
west build -b nrf54l15dk/nrf54l15/cpuapp <path-to>/camera-show-on-screen
west flash
```

Monitor serial output (115200 baud):

```bash
west espressif monitor   # or any terminal at 115200 8N1
```

---

## How It Works

1. `ov7670_init()` configures the OV7670 over SCCB (I2C) for QQVGA 160×120
   RGB565 output using the onboard 12 MHz crystal as the pixel clock source.
2. `fifo_init()` sets up the GPIO pins for the AL422B FIFO interface.
3. In the main loop, `fifo_capture()` waits for a VSYNC frame boundary, arms
   the FIFO write cycle, then clocks out 38 400 bytes (160×120×2) into a
   static frame buffer.
4. `tft_draw_image()` blits the RGB565 frame buffer directly to the display
   row by row using the Zephyr display driver.
5. A yellow bounding box overlay is drawn with `tft_draw_bounding_box()` as a
   placeholder for future CV inference output.
