#pragma once
#include <stdint.h>
#include <stdbool.h>

// ─── Game-wide State ────────────────────────────────────────────────────────

typedef enum {
    GAME_IDLE,
    GAME_ARMED,
    GAME_ACTIVE,
    GAME_DEFUSED,
    GAME_EXPLODED
} game_state_t;

// ─── Module IDs ─────────────────────────────────────────────────────────────

typedef enum {
    MODULE_MASTER = 0x00,
    MODULE_ALEXIA = 0x01,   // Simon Says + Timer
    MODULE_MYLES  = 0x02,   // FFT Wave + Sound
    MODULE_CELINE = 0x03,   // Button + Complicated Wires
    MODULE_KRISH  = 0x04,   // Morse Code + Ring of Fire
} module_id_t;

// ─── UART Protocol ──────────────────────────────────────────────────────────
// Text-based, newline-terminated for easy debugging over serial monitor.
//
// Outgoing (module → master):
//   "$SOLVED:<MODULE_NAME>\n"   module puzzle solved
//   "$STRIKE\n"                 wrong input, request a strike
//
// Incoming (master → module):
//   "$STATE:<IDLE|ARMED|ACTIVE|DEFUSED|EXPLODED>\n"
//   "$TIME:<seconds_remaining>\n"
//   "$RESET\n"

#define UART_BAUD_RATE  115200
#define MAX_STRIKES     3

// UART0 pins (shared bus between all modules and master)
#define UART_TX_PIN     12
#define UART_RX_PIN     13
