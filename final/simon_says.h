#pragma once
#include <stdint.h>
#include <stdbool.h>

// ─── Pin Assignments ─────────────────────────────────────────────────────────
// LEDs (output)
#define SS_LED_RED    8
#define SS_LED_YELLOW 7
#define SS_LED_GREEN  6
#define SS_LED_BLUE   5

// Buttons (input, active-low with internal pull-up)
#define SS_BTN_RED    12
#define SS_BTN_YELLOW 11
#define SS_BTN_GREEN  10
#define SS_BTN_BLUE   9

// ─── Game parameters ─────────────────────────────────────────────────────────
#define SS_MAX_ROUNDS    8
#define SS_SHOW_MS       500
#define SS_SHOW_GAP_MS   200
#define SS_INPUT_TIMEOUT_MS 10000

// ─── Simon Says API ──────────────────────────────────────────────────────────

void simon_says_init(void);
void simon_says_demo(int length);
void simon_says_record_input(uint8_t color);
bool simon_says_collect_input(int length);
void simon_says_generate(int length);
bool simon_says_color_round(int length);
uint8_t simon_says_get_code(void);
bool simon_says_check_input(void);
void simon_says_startup_animation(void);
bool simon_says_selftest(void);
bool simon_says_update(void);
void simon_says_reset(void);
void simon_says_set_active(bool active);
