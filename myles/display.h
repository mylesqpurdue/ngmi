#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

// Pin assignments
#define TFT_CS_PIN    17
#define TFT_RESET_PIN 15
#define TFT_DC_PIN    14
#define TFT_MOSI_PIN  19
#define TFT_SCK_PIN   18

// Display dimensions (landscape)
#define TFT_WIDTH     320
#define TFT_HEIGHT    240

// RGB565 color helpers
#define RGB565(r, g, b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))
#define COLOR_BLACK     0x0000
#define COLOR_WHITE     0xFFFF
#define COLOR_GREEN     0x07E0
#define COLOR_YELLOW    0xFFE0
#define COLOR_LGRAY     0xC618
#define COLOR_RED       0xF800

// Initialization
void display_init(void);

// Drawing primitives
void display_draw_pixel(uint16_t x, uint16_t y, uint16_t color);
void display_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void display_draw_vline(uint16_t x, uint16_t y0, uint16_t y1, uint16_t color);
void display_draw_char(uint16_t x, uint16_t y, char c, uint16_t color, uint16_t bg);
void display_draw_string(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bg);

// Low-level SPI
void display_send_cmd(uint8_t cmd);
void display_send_data(uint8_t data);
void display_send_data16(uint16_t data);
void display_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

#endif // DISPLAY_H
