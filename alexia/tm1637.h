#pragma once
#include <stdint.h>
#include <stdbool.h>

// ─── TM1637 4-Digit 7-Segment Display Driver ────────────────────────────────
// 2-wire interface: CLK and DIO (bit-banged).

void tm1637_init(uint clk_pin, uint dio_pin);

// Display MM:SS.  colon=true lights the center colon.
void tm1637_display_time(uint8_t minutes, uint8_t seconds, bool colon);

// Display an arbitrary 4-digit number (0-9999).
void tm1637_display_number(uint16_t number);

// Turn all segments off.
void tm1637_clear(void);

// Set brightness 0 (dimmest) – 7 (brightest).
void tm1637_set_brightness(uint8_t level);
