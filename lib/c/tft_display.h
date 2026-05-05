#ifndef TFT_DISPLAY_H_
#define TFT_DISPLAY_H_

#include <stdint.h>
#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Display geometry ───────────────────────────────────────────────────── */
#define TFT_WIDTH   160
#define TFT_HEIGHT  128
#define TFT_BPP     2   /* RGB565 = 2 bytes per pixel */

/* ── Common RGB565 colors (big-endian byte order) ───────────────────────── */
#define TFT_COLOR_BLACK       ((uint16_t)0x0000U)
#define TFT_COLOR_WHITE       ((uint16_t)0xFFFFU)
#define TFT_COLOR_RED         ((uint16_t)0xF800U)
#define TFT_COLOR_GREEN       ((uint16_t)0x07E0U)
#define TFT_COLOR_BLUE        ((uint16_t)0x001FU)

// Secondary Colors
#define TFT_COLOR_YELLOW      ((uint16_t)0xFFE0U)
#define TFT_COLOR_CYAN        ((uint16_t)0x07FFU)
#define TFT_COLOR_MAGENTA     ((uint16_t)0xF81FU)

// Common Variations
#define TFT_COLOR_ORANGE      ((uint16_t)0xFD20U)
#define TFT_COLOR_DARKGREEN   ((uint16_t)0x03E0U)
#define TFT_COLOR_DARKCYAN    ((uint16_t)0x03EFU)
#define TFT_COLOR_MAROON      ((uint16_t)0x7800U)
#define TFT_COLOR_PURPLE      ((uint16_t)0x780FU)
#define TFT_COLOR_OLIVE       ((uint16_t)0x7BE0U)
#define TFT_COLOR_LIGHTGREY   ((uint16_t)0xC618U)
#define TFT_COLOR_DARKGREY    ((uint16_t)0x7BEFU)
#define TFT_COLOR_NAVY        ((uint16_t)0x000FU)


/* ── Font metrics (built-in 5×7 bitmap font, ASCII 32–126) ─────────────── */
#define TFT_FONT_W      5
#define TFT_FONT_H      7
#define TFT_FONT_STRIDE 6  /* pixel advance per character (5px glyph + 1px gap) */

/**
 * TFT_DEVICE - Obtain the display device from the devicetree.
 *
 * Requires the st7735r node to be labelled "st7735r" in the board overlay.
 * Use result with tft_init() before any draw call.
 */
#define TFT_DEVICE() DEVICE_DT_GET(DT_NODELABEL(st7735r))

/* ── Lifecycle ──────────────────────────────────────────────────────────── */

/**
 * tft_init - Verify the display is ready and turn blanking off.
 *
 * Returns 0 on success, -ENODEV if the device is not ready.
 */
int tft_init(const struct device *dev);

/* ── Fill primitives ────────────────────────────────────────────────────── */

void tft_fill_screen(const struct device *dev, uint16_t color);
void tft_fill_rect(const struct device *dev, int x, int y, int w, int h,
		   uint16_t color);

/* ── Line primitives ────────────────────────────────────────────────────── */

void tft_draw_hline(const struct device *dev, int x, int y, int w,
		    uint16_t color);
void tft_draw_vline(const struct device *dev, int x, int y, int h,
		    uint16_t color);

/* ── Text ───────────────────────────────────────────────────────────────── */

void tft_draw_char(const struct device *dev, int x, int y, char c,
		   uint16_t fg, uint16_t bg);
void tft_draw_string(const struct device *dev, int x, int y, const char *str,
		     uint16_t fg, uint16_t bg);

/* ── Image ──────────────────────────────────────────────────────────────── */

/**
 * tft_draw_image - Blit a raw RGB565 frame buffer onto the display.
 *
 * @dev:    Display device
 * @x:      Top-left column
 * @y:      Top-left row
 * @w:      Image width in pixels
 * @h:      Image height in pixels
 * @rgb565: Row-major RGB565 buffer, big-endian, w*h*2 bytes
 *
 * Suitable for displaying camera frames directly:
 *   tft_draw_image(dev, 0, 0, 160, 128, frame_buf);
 */
void tft_draw_image(const struct device *dev, int x, int y, int w, int h,
		    const uint8_t *rgb565);

/**
 * tft_draw_grayscale_image - Blit a raw grayscale frame buffer onto the display using a buffer to render the grayscale to RGB565.
 *
 * @dev:    Display device
 * @x:      Top-left column
 * @y:      Top-left row
 * @w:      Image width in pixels
 * @h:      Image height in pixels
 * @grayscale: Row-major grayscale buffer, 1 byte per pixel
 * @padded_grayscale: Whether the grayscale buffer has padding
 *
 * Suitable for displaying grayscale camera frames:
 *   tft_draw_grayscale_image(dev, 0, 0, 160, 128, grayscale_buf, false);
 */
void tft_draw_grayscale_image(const struct device *dev, int x, int y, int w, int h,
			      const uint8_t *grayscale, bool padded_grayscale);


void tft_draw_scaled_grayscale_image(const struct device *dev, int x, int y, int w, int h,
			      const uint8_t *grayscale, int scale);

/* ── Computer-vision overlays ───────────────────────────────────────────── */

/**
 * tft_draw_bounding_box - Draw a CV-style bounding box with a label tag.
 *
 * @dev:   Display device
 * @x:     Top-left column
 * @y:     Top-left row
 * @w:     Box width in pixels
 * @h:     Box height in pixels
 * @label: Null-terminated ASCII label (may be NULL for unlabelled box)
 *
 * Draws a 2-pixel-thick yellow rectangle. A filled yellow tag with black
 * text is placed inside the top-left corner of the box.
 */
void tft_draw_bounding_box(const struct device *dev, int x, int y, int w,
			   int h, const char *label);

void tft_draw_bounding_box_color(const struct device *dev, int x, int y, int w,
			   int h, const char *label, const uint16_t color);
#ifdef __cplusplus
}
#endif

#endif /* TFT_DISPLAY_H_ */
