#ifndef LED_H
#define LED_H

#include <stdint.h>

// Pin assignments (actual wiring: GP10=Blue, GP11=Green, GP12=Red)
#define LED_R_PIN  12
#define LED_G_PIN  11
#define LED_B_PIN  10

// Initialization
void led_init(void);

// Set RGB color directly (0-255 per channel)
void led_set_color(uint8_t r, uint8_t g, uint8_t b);

// Convenience state colors
void led_set_idle(void); // Solid blue
void led_set_playing(void); // Solid yellow
void led_set_locking(float progress); // Yellow->Green blend (0.0 to 1.0)
void led_set_win(void); // Solid green
void led_flash_white(void); // Brief white flash

#endif // LED_H