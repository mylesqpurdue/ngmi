#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "../common/common.h"

// ─── Pin Assignments ─────────────────────────────────────────────────────────
// LEDs (output)
#define SS_LED_BLUE   9
#define SS_LED_RED    8
#define SS_LED_GREEN  5
#define SS_LED_YELLOW 2

// Buttons (input, active-low with internal pull-up)
#define SS_BTN_BLUE   36
#define SS_BTN_RED    38
#define SS_BTN_GREEN  40
#define SS_BTN_YELLOW 42

// ─── Game parameters ─────────────────────────────────────────────────────────
#define SS_MAX_ROUNDS    8    // rounds needed to solve the module
#define SS_SHOW_MS       500  // how long each LED stays lit during playback
#define SS_SHOW_GAP_MS   200  // gap between lit LEDs during playback
#define SS_INPUT_TIMEOUT_MS 10000  // ms the player has to finish inputting

// ─── Simon Says API ──────────────────────────────────────────────────────────

// Call once at startup.
void simon_says_init(void);

// Wait for any button press (for entropy), then play a random sequence of
// `length` lights (blocking). Guarantees 4 distinct colors in first 4 steps.
void simon_says_demo(int length);

// Record one button press by color index (0=RED,1=GREEN,2=BLUE,3=YELLOW).
void simon_says_record_input(uint8_t color);

// Block until `length` button presses are recorded with LED feedback.
// Returns true if every press matched the demo sequence, false on first mismatch.
bool simon_says_collect_input(int length);

// Generate sequence + pick code without playing LEDs (use before color round).
void simon_says_generate(int length);

// Color round: lights one LED at a time and waits for the correct mapped press.
bool simon_says_color_round(int length);

// Returns the current code letter index (0=A, 1=B, 2=C, 3=D).
uint8_t simon_says_get_code(void);

// Returns true if the recorded input matches the demo sequence exactly.
bool simon_says_check_input(void);

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
