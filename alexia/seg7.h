#pragma once
#include <stdint.h>
#include <stdbool.h>

// ─── SPI1 Pin Assignments ────────────────────────────────────────────────────
// These reuse the same GPIO numbers that were previously TM1637 CLK/DIO.
// SPI1 supports SCK on GPIO14 and TX on GPIO15 natively.
#define SEG7_SCK_PIN  14   // SPI1 SCK
#define SEG7_TX_PIN   15   // SPI1 TX (MOSI)
#define SEG7_CSN_PIN   9   // chip-select (free GPIO, active-low)

#define SEG7_DMA_CHANNEL 0

// msg[] is the live display buffer — 8 digits, each entry is:
//   bits [10:8] = digit position (0–7)
//   bits  [7:0] = 7-segment pattern (a–g + dp in bit 7)
// Defined in seg7.c; extern here so the autotest can take its address.
extern uint16_t msg[8];

// font[] maps ASCII → 7-segment pattern.
extern char font[128];

// ─── API ─────────────────────────────────────────────────────────────────────

// Initialise SPI1 and drive all segments off.
void seg7_init(void);

// Show MM.SS on the 8-digit display (decimal point used as separator).
// separator_on controls whether the dot between minutes and seconds is lit.
void seg7_display_time(uint8_t minutes, uint8_t seconds, bool separator_on);

// Fill all 8 digits with dashes (used during IDLE).
void seg7_show_dashes(void);

// Blank the display.
void seg7_clear(void);

// Write msg[] to the display over SPI (call from main loop).
void seg7_refresh(void);

// Low-level: write an arbitrary string (up to 8 visible chars, '.' = decimal point).
void seg7_print(const char *str);
