#include "tft_display.h"

#include <string.h>
#include <zephyr/drivers/display.h>
#include <errno.h>
#include "pixel_conversion.h"

/* ── 5×7 bitmap font, ASCII 32–126 ─────────────────────────────────────── */
/* Column-major encoding: each byte = one column, bit 0 = top row.         */
#define FONT_FIRST 32

static const uint8_t font5x7[][TFT_FONT_W] = {
	{0x00,0x00,0x00,0x00,0x00}, /* ' ' 32 */
	{0x00,0x00,0x5F,0x00,0x00}, /* '!' 33 */
	{0x00,0x07,0x00,0x07,0x00}, /* '"' 34 */
	{0x14,0x7F,0x14,0x7F,0x14}, /* '#' 35 */
	{0x24,0x2A,0x7F,0x2A,0x12}, /* '$' 36 */
	{0x23,0x13,0x08,0x64,0x62}, /* '%' 37 */
	{0x36,0x49,0x55,0x22,0x50}, /* '&' 38 */
	{0x00,0x05,0x03,0x00,0x00}, /* '\'' 39 */
	{0x00,0x1C,0x22,0x41,0x00}, /* '(' 40 */
	{0x00,0x41,0x22,0x1C,0x00}, /* ')' 41 */
	{0x14,0x08,0x3E,0x08,0x14}, /* '*' 42 */
	{0x08,0x08,0x3E,0x08,0x08}, /* '+' 43 */
	{0x00,0x50,0x30,0x00,0x00}, /* ',' 44 */
	{0x08,0x08,0x08,0x08,0x08}, /* '-' 45 */
	{0x00,0x60,0x60,0x00,0x00}, /* '.' 46 */
	{0x20,0x10,0x08,0x04,0x02}, /* '/' 47 */
	{0x3E,0x51,0x49,0x45,0x3E}, /* '0' 48 */
	{0x00,0x42,0x7F,0x40,0x00}, /* '1' 49 */
	{0x42,0x61,0x51,0x49,0x46}, /* '2' 50 */
	{0x21,0x41,0x45,0x4B,0x31}, /* '3' 51 */
	{0x18,0x14,0x12,0x7F,0x10}, /* '4' 52 */
	{0x27,0x45,0x45,0x45,0x39}, /* '5' 53 */
	{0x3C,0x4A,0x49,0x49,0x30}, /* '6' 54 */
	{0x01,0x71,0x09,0x05,0x03}, /* '7' 55 */
	{0x36,0x49,0x49,0x49,0x36}, /* '8' 56 */
	{0x06,0x49,0x49,0x29,0x1E}, /* '9' 57 */
	{0x00,0x36,0x36,0x00,0x00}, /* ':' 58 */
	{0x00,0x56,0x36,0x00,0x00}, /* ';' 59 */
	{0x08,0x14,0x22,0x41,0x00}, /* '<' 60 */
	{0x14,0x14,0x14,0x14,0x14}, /* '=' 61 */
	{0x00,0x41,0x22,0x14,0x08}, /* '>' 62 */
	{0x02,0x01,0x51,0x09,0x06}, /* '?' 63 */
	{0x32,0x49,0x79,0x41,0x3E}, /* '@' 64 */
	{0x7E,0x11,0x11,0x11,0x7E}, /* 'A' 65 */
	{0x7F,0x49,0x49,0x49,0x36}, /* 'B' 66 */
	{0x3E,0x41,0x41,0x41,0x22}, /* 'C' 67 */
	{0x7F,0x41,0x41,0x22,0x1C}, /* 'D' 68 */
	{0x7F,0x49,0x49,0x49,0x41}, /* 'E' 69 */
	{0x7F,0x09,0x09,0x09,0x01}, /* 'F' 70 */
	{0x3E,0x41,0x49,0x49,0x7A}, /* 'G' 71 */
	{0x7F,0x08,0x08,0x08,0x7F}, /* 'H' 72 */
	{0x00,0x41,0x7F,0x41,0x00}, /* 'I' 73 */
	{0x20,0x40,0x41,0x3F,0x01}, /* 'J' 74 */
	{0x7F,0x08,0x14,0x22,0x41}, /* 'K' 75 */
	{0x7F,0x40,0x40,0x40,0x40}, /* 'L' 76 */
	{0x7F,0x02,0x0C,0x02,0x7F}, /* 'M' 77 */
	{0x7F,0x04,0x08,0x10,0x7F}, /* 'N' 78 */
	{0x3E,0x41,0x41,0x41,0x3E}, /* 'O' 79 */
	{0x7F,0x09,0x09,0x09,0x06}, /* 'P' 80 */
	{0x3E,0x41,0x51,0x21,0x5E}, /* 'Q' 81 */
	{0x7F,0x09,0x19,0x29,0x46}, /* 'R' 82 */
	{0x46,0x49,0x49,0x49,0x31}, /* 'S' 83 */
	{0x01,0x01,0x7F,0x01,0x01}, /* 'T' 84 */
	{0x3F,0x40,0x40,0x40,0x3F}, /* 'U' 85 */
	{0x1F,0x20,0x40,0x20,0x1F}, /* 'V' 86 */
	{0x3F,0x40,0x38,0x40,0x3F}, /* 'W' 87 */
	{0x63,0x14,0x08,0x14,0x63}, /* 'X' 88 */
	{0x07,0x08,0x70,0x08,0x07}, /* 'Y' 89 */
	{0x61,0x51,0x49,0x45,0x43}, /* 'Z' 90 */
	{0x00,0x7F,0x41,0x41,0x00}, /* '[' 91 */
	{0x02,0x04,0x08,0x10,0x20}, /* '\\' 92 */
	{0x00,0x41,0x41,0x7F,0x00}, /* ']' 93 */
	{0x04,0x02,0x01,0x02,0x04}, /* '^' 94 */
	{0x40,0x40,0x40,0x40,0x40}, /* '_' 95 */
	{0x00,0x01,0x02,0x04,0x00}, /* '`' 96 */
	{0x20,0x54,0x54,0x54,0x78}, /* 'a' 97 */
	{0x7F,0x48,0x44,0x44,0x38}, /* 'b' 98 */
	{0x38,0x44,0x44,0x44,0x20}, /* 'c' 99 */
	{0x38,0x44,0x44,0x48,0x7F}, /* 'd' 100 */
	{0x38,0x54,0x54,0x54,0x18}, /* 'e' 101 */
	{0x08,0x7E,0x09,0x01,0x02}, /* 'f' 102 */
	{0x0C,0x52,0x52,0x52,0x3E}, /* 'g' 103 */
	{0x7F,0x08,0x04,0x04,0x78}, /* 'h' 104 */
	{0x00,0x44,0x7D,0x40,0x00}, /* 'i' 105 */
	{0x20,0x40,0x44,0x3D,0x00}, /* 'j' 106 */
	{0x7F,0x10,0x28,0x44,0x00}, /* 'k' 107 */
	{0x00,0x41,0x7F,0x40,0x00}, /* 'l' 108 */
	{0x7C,0x04,0x18,0x04,0x78}, /* 'm' 109 */
	{0x7C,0x08,0x04,0x04,0x78}, /* 'n' 110 */
	{0x38,0x44,0x44,0x44,0x38}, /* 'o' 111 */
	{0x7C,0x14,0x14,0x14,0x08}, /* 'p' 112 */
	{0x08,0x14,0x14,0x18,0x7C}, /* 'q' 113 */
	{0x7C,0x08,0x04,0x04,0x08}, /* 'r' 114 */
	{0x48,0x54,0x54,0x54,0x20}, /* 's' 115 */
	{0x04,0x3F,0x44,0x40,0x20}, /* 't' 116 */
	{0x3C,0x40,0x40,0x20,0x7C}, /* 'u' 117 */
	{0x1C,0x20,0x40,0x20,0x1C}, /* 'v' 118 */
	{0x3C,0x40,0x30,0x40,0x3C}, /* 'w' 119 */
	{0x44,0x28,0x10,0x28,0x44}, /* 'x' 120 */
	{0x0C,0x50,0x50,0x50,0x3C}, /* 'y' 121 */
	{0x44,0x64,0x54,0x4C,0x44}, /* 'z' 122 */
	{0x00,0x08,0x36,0x41,0x00}, /* '{' 123 */
	{0x00,0x00,0x7F,0x00,0x00}, /* '|' 124 */
	{0x00,0x41,0x36,0x08,0x00}, /* '}' 125 */
	{0x10,0x08,0x08,0x10,0x08}, /* '~' 126 */
};

/* ── Shared draw buffer (one full row: 160px × 2 bytes = 320 bytes) ─────── */
static uint8_t draw_buf[TFT_WIDTH * TFT_BPP];

/* ── Lifecycle ──────────────────────────────────────────────────────────── */

int tft_init(const struct device *dev)
{
	if (!device_is_ready(dev)) {
		return -ENODEV;
	}
	return display_blanking_off(dev);
}

/* ── Fill primitives ────────────────────────────────────────────────────── */

void tft_fill_rect(const struct device *dev, int x, int y, int w, int h,
		   uint16_t color)
{
	if (w <= 0 || h <= 0) {
		return;
	}
	for (int i = 0; i < w; i++) {
		draw_buf[i * 2]     = (color >> 8) & 0xFF;
		draw_buf[i * 2 + 1] = color & 0xFF;
	}
	struct display_buffer_descriptor d = {
		.buf_size = (uint32_t)(w * TFT_BPP),
		.width    = (uint16_t)w,
		.height   = 1,
		.pitch    = (uint16_t)w,
	};
	for (int row = y; row < y + h; row++) {
		display_write(dev, x, row, &d, draw_buf);
	}
}

void tft_fill_screen(const struct device *dev, uint16_t color)
{
	tft_fill_rect(dev, 0, 0, TFT_WIDTH, TFT_HEIGHT, color);
}

/* ── Line primitives ────────────────────────────────────────────────────── */

void tft_draw_hline(const struct device *dev, int x, int y, int w,
		    uint16_t color)
{
	if (y < 0 || y >= TFT_HEIGHT || w <= 0) {
		return;
	}
	if (x < 0) { w += x; x = 0; }
	if (x + w > TFT_WIDTH) { w = TFT_WIDTH - x; }
	if (w <= 0) {
		return;
	}
	tft_fill_rect(dev, x, y, w, 1, color);
}

void tft_draw_vline(const struct device *dev, int x, int y, int h,
		    uint16_t color)
{
	if (x < 0 || x >= TFT_WIDTH || h <= 0) {
		return;
	}
	if (y < 0) { h += y; y = 0; }
	if (y + h > TFT_HEIGHT) { h = TFT_HEIGHT - y; }
	if (h <= 0) {
		return;
	}
	for (int i = 0; i < h; i++) {
		draw_buf[i * 2]     = (color >> 8) & 0xFF;
		draw_buf[i * 2 + 1] = color & 0xFF;
	}
	struct display_buffer_descriptor d = {
		.buf_size = (uint32_t)(h * TFT_BPP),
		.width    = 1,
		.height   = (uint16_t)h,
		.pitch    = 1,
	};
	display_write(dev, x, y, &d, draw_buf);
}

/* ── Text ───────────────────────────────────────────────────────────────── */

void tft_draw_char(const struct device *dev, int x, int y, char c,
		   uint16_t fg, uint16_t bg)
{
	if (c < FONT_FIRST || c > 126) {
		c = '?';
	}
	const uint8_t *glyph = font5x7[(uint8_t)c - FONT_FIRST];
	uint8_t row_buf[TFT_FONT_W * TFT_BPP];
	struct display_buffer_descriptor d = {
		.buf_size = TFT_FONT_W * TFT_BPP,
		.width    = TFT_FONT_W,
		.height   = 1,
		.pitch    = TFT_FONT_W,
	};

	for (int row = 0; row < TFT_FONT_H; row++) {
		for (int col = 0; col < TFT_FONT_W; col++) {
			uint16_t px = ((glyph[col] >> row) & 1) ? fg : bg;
			row_buf[col * 2]     = (px >> 8) & 0xFF;
			row_buf[col * 2 + 1] = px & 0xFF;
		}
		display_write(dev, x, y + row, &d, row_buf);
	}
}

void tft_draw_string(const struct device *dev, int x, int y, const char *str,
		     uint16_t fg, uint16_t bg)
{
	for (; *str; str++, x += TFT_FONT_STRIDE) {
		if (x + TFT_FONT_W > TFT_WIDTH) {
			break;
		}
		tft_draw_char(dev, x, y, *str, fg, bg);
	}
}

/* ── Image ──────────────────────────────────────────────────────────────── */

void tft_draw_image(const struct device *dev, int x, int y, int w, int h,
		    const uint8_t *rgb565)
{
	struct display_buffer_descriptor d = {
		.buf_size = (uint32_t)(w * TFT_BPP),
		.width    = (uint16_t)w,
		.height   = 1,
		.pitch    = (uint16_t)w,
	};
	for (int row = 0; row < h; row++) {
		display_write(dev, x, y + row, &d,
			      rgb565 + row * w * TFT_BPP);
	}
}

static uint8_t grayscale_row_buf[TFT_WIDTH * TFT_BPP];

void tft_draw_grayscale_image(const struct device *dev, int x, int y, int w, int h,
			      const uint8_t *grayscale, bool padded_grayscale)
{
	struct display_buffer_descriptor d = {
		.buf_size = (uint32_t)(w * TFT_BPP),
		.width    = (uint16_t)w,
		.height   = 1,
		.pitch    = (uint16_t)w,
	};
	for (int row = 0; row < h; row++) {
		for (int col = 0; col < w; ++col)
		{
			int grayidx = padded_grayscale ? (row + 1) * (w + 2) + (col + 1) : row * w + col;
			uint16_t rgb565_pixel = calculate_rgb565(grayscale[grayidx]);
			grayscale_row_buf[col * 2]     = (rgb565_pixel >> 8) & 0xFF;
			grayscale_row_buf[col * 2 + 1] = rgb565_pixel & 0xFF;
		}

		display_write(dev, x, y + row, &d, grayscale_row_buf);
	}
}

void tft_draw_scaled_grayscale_image(const struct device *dev, int x, int y, int w, int h,
			      const uint8_t *grayscale, int scale)
{
	struct display_buffer_descriptor d = {
		.buf_size = (uint32_t)(w * TFT_BPP * scale),
		.width    = (uint16_t)(w * scale),
		.height   = 1,
		.pitch    = (uint16_t)(w * scale),
	};
	for (int row = 0; row < h; row++) {
		for (int col = 0; col < w; ++col)
		{
			int grayidx = row * w + col;
			uint16_t rgb565_pixel = calculate_rgb565(grayscale[grayidx]);
			for (int scaleCtr = 0; scaleCtr < scale; ++scaleCtr) {
				int pix1idx = (col * scale) * 2 + scaleCtr * 2;
				int pix2idx = (col * scale) * 2 + scaleCtr * 2 + 1;
				grayscale_row_buf[pix1idx]     = (rgb565_pixel >> 8) & 0xFF;
				grayscale_row_buf[pix2idx] = rgb565_pixel & 0xFF;
			}
		}

		for (int ctr = 0; ctr < scale; ++ctr) {
			display_write(dev, x, y + (row * scale) + ctr, &d, grayscale_row_buf);
		}
	}
}


/* ── Computer-vision overlays ───────────────────────────────────────────── */
void tft_draw_bounding_box(const struct device *dev, int x, int y, int w,
			   int h, const char *label)
{
	tft_draw_bounding_box_color(dev, x, y, w, h, label, TFT_COLOR_YELLOW);
}

void tft_draw_bounding_box_color(const struct device *dev, int x, int y, int w,
			   int h, const char *label, const uint16_t color)
{
	/* 2-pixel-thick border */
	for (int t = 0; t < 2; t++) {
		tft_draw_hline(dev, x + t,         y + t,         w - 2 * t,
			       color);
		tft_draw_hline(dev, x + t,         y + h - 1 - t, w - 2 * t,
			       color);
		tft_draw_vline(dev, x + t,         y + t,         h - 2 * t,
			       color);
		tft_draw_vline(dev, x + w - 1 - t, y + t,         h - 2 * t,
			       color);
	}

	if (!label || !*label) {
		return;
	}

	/* Filled yellow tag with black text, inset inside the top-left corner */
	int label_len = (int)strlen(label);
	int tag_w     = label_len * TFT_FONT_STRIDE + 4; /* 2px padding each side */
	int tag_h     = TFT_FONT_H + 4;

	if (tag_w > w) {
		tag_w = w;
	}

	tft_fill_rect(dev, x + 2, y + 2, tag_w, tag_h, color);
	tft_draw_string(dev, x + 4, y + 4, label, color == TFT_COLOR_BLACK ? TFT_COLOR_WHITE : TFT_COLOR_BLACK, color);
}
