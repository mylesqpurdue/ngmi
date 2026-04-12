#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "../common/common.h"

// ─── Pin Assignments ─────────────────────────────────────────────────────────
// LEDs (output)
#define SS_LED_RED    0
#define SS_LED_GREEN  1
#define SS_LED_BLUE   2
#define SS_LED_YELLOW 3

// Buttons (input, active-low with internal pull-up)
#define SS_BTN_RED    4
#define SS_BTN_GREEN  5
#define SS_BTN_BLUE   6
#define SS_BTN_YELLOW 7

// ─── Game parameters ─────────────────────────────────────────────────────────
#define SS_MAX_ROUNDS    8    // rounds needed to solve the module
#define SS_SHOW_MS       500  // how long each LED stays lit during playback
#define SS_SHOW_GAP_MS   200  // gap between lit LEDs during playback
#define SS_INPUT_TIMEOUT_MS 10000  // ms the player has to finish inputting

// ─── Simon Says API ──────────────────────────────────────────────────────────

// Call once at startup.
void simon_says_init(void);

// LED chase animation played at boot before the game arms.
void simon_says_startup_animation(void);

// Hardware self-test: flashes each LED in order and waits for the matching
// button press. Prints PASS/FAIL for each pair over USB serial.
// Returns true if all 4 pairs pass.
bool simon_says_selftest(void);

// Call repeatedly from the main loop.
// Returns true when the module has just been solved (one-shot).
bool simon_says_update(void);

// Reset to initial state (e.g., when master sends RESET).
void simon_says_reset(void);

// Pause / resume accepting button input (called by main when game isn't ACTIVE).
void simon_says_set_active(bool active);
