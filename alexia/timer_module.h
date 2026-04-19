#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "../common/common.h"
#include "seg7.h"

// Pin assignments are defined in seg7.h (SEG7_SCK_PIN, SEG7_TX_PIN, SEG7_CSN_PIN).

// ─── Timer button pins ───────────────────────────────────────────────────────
#define TIMER_BTN_START  21   // press to start the countdown
#define TIMER_BTN_RESET  26   // press to reset countdown back to default

// ─── Default countdown duration (seconds) ───────────────────────────────────
#define TIMER_DEFAULT_SECONDS  300   // 5:00

// ─── Timer Module API ────────────────────────────────────────────────────────

// Call once at startup.
void timer_module_init(void);

// Start the countdown from the configured duration.
void timer_start(void);

// Reset to initial duration and go back to IDLE.
void timer_reset(void);

// Called by the master via UART; updates the local game state.
void timer_set_game_state(game_state_t state);

// Call from main loop — handles UART RX, refreshes display.
void timer_update(void);

// Returns remaining seconds (0 when expired).
uint32_t timer_get_remaining(void);

// True if the countdown has hit zero.
bool timer_is_expired(void);
