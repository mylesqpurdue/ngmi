#include "simon_says.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include <string.h>

static uint8_t s_demo_sequence[8];
static int     s_demo_length    = 0;
static uint8_t s_user_input[8];
static int     s_user_input_len = 0;

// ─── Init ────────────────────────────────────────────────────────────────────



// ─── Startup animation ───────────────────────────────────────────────────────

void simon_says_startup_animation(void) {
    for (int cycle = 0; cycle < 2; cycle++) {
        gpio_put(SS_LED_RED, 1);    gpio_put(SS_LED_GREEN, 0); gpio_put(SS_LED_BLUE, 0); gpio_put(SS_LED_YELLOW, 0); busy_wait_ms(120);
        gpio_put(SS_LED_RED, 0);    gpio_put(SS_LED_GREEN, 1); gpio_put(SS_LED_BLUE, 0); gpio_put(SS_LED_YELLOW, 0); busy_wait_ms(120);
        gpio_put(SS_LED_RED, 0);    gpio_put(SS_LED_GREEN, 0); gpio_put(SS_LED_BLUE, 1); gpio_put(SS_LED_YELLOW, 0); busy_wait_ms(120);
        gpio_put(SS_LED_RED, 0);    gpio_put(SS_LED_GREEN, 0); gpio_put(SS_LED_BLUE, 0); gpio_put(SS_LED_YELLOW, 1); busy_wait_ms(120);
    }
    gpio_put(SS_LED_RED, 0); gpio_put(SS_LED_GREEN, 0); gpio_put(SS_LED_BLUE, 0); gpio_put(SS_LED_YELLOW, 0);
    busy_wait_ms(80);
    for (int i = 0; i < 3; i++) {
        gpio_put(SS_LED_RED, 1); gpio_put(SS_LED_GREEN, 1); gpio_put(SS_LED_BLUE, 1); gpio_put(SS_LED_YELLOW, 1); busy_wait_ms(150);
        gpio_put(SS_LED_RED, 0); gpio_put(SS_LED_GREEN, 0); gpio_put(SS_LED_BLUE, 0); gpio_put(SS_LED_YELLOW, 0); busy_wait_ms(150);
    }
    busy_wait_ms(200);
}

// ─── Demo ────────────────────────────────────────────────────────────────────
// Waits for any button press (entropy), generates a sequence, then plays it.
// Colors are stored as 1=RED 2=GREEN 3=BLUE 4=YELLOW.

void simon_says_demo(int length) {
    // Sequence generation — time_us_32() varies based on when the caller
    // triggered this (human press timing in main.c provides the entropy).
    uint8_t pool[4] = {1, 2, 3, 4};
    for (int i = 3; i > 0; i--) {
        int j = (int)(time_us_32() % (uint32_t)(i + 1));
        uint8_t tmp = pool[i]; pool[i] = pool[j]; pool[j] = tmp;
    }
    for (int i = 0; i < length; i++)
        s_demo_sequence[i] = (i < 4) ? pool[i] : (uint8_t)(time_us_32() % 4 + 1);

    s_demo_length    = length;
    s_user_input_len = 0;

    // play sequence
    for (int i = 0; i < length; i++) {
        gpio_put(SS_LED_RED, 0); gpio_put(SS_LED_GREEN, 0);
        gpio_put(SS_LED_BLUE, 0); gpio_put(SS_LED_YELLOW, 0);

        if (s_demo_sequence[i] == 1) gpio_put(SS_LED_RED,    1);
        if (s_demo_sequence[i] == 2) gpio_put(SS_LED_GREEN,  1);
        if (s_demo_sequence[i] == 3) gpio_put(SS_LED_BLUE,   1);
        if (s_demo_sequence[i] == 4) gpio_put(SS_LED_YELLOW, 1);

        busy_wait_ms(SS_SHOW_MS);
        gpio_put(SS_LED_RED, 0); gpio_put(SS_LED_GREEN, 0);
        gpio_put(SS_LED_BLUE, 0); gpio_put(SS_LED_YELLOW, 0);
        busy_wait_ms(SS_SHOW_GAP_MS);
    }
}

// ─── Collect input ───────────────────────────────────────────────────────────

bool simon_says_collect_input(int length) {
    int step = 0;

    // wait for all buttons released before starting
    while (!gpio_get(SS_BTN_RED) || !gpio_get(SS_BTN_GREEN) ||
           !gpio_get(SS_BTN_BLUE) || !gpio_get(SS_BTN_YELLOW)) busy_wait_ms(10);
    busy_wait_ms(100);

    while (step < length) {

        if (!gpio_get(SS_BTN_RED)) {
            gpio_put(SS_LED_RED, 1);
            while (!gpio_get(SS_BTN_RED)) busy_wait_ms(5);
            gpio_put(SS_LED_RED, 0);
            if (s_demo_sequence[step] != 1) return false;
            step++;
            busy_wait_ms(80);

        } else if (!gpio_get(SS_BTN_GREEN)) {
            gpio_put(SS_LED_GREEN, 1);
            while (!gpio_get(SS_BTN_GREEN)) busy_wait_ms(5);
            gpio_put(SS_LED_GREEN, 0);
            if (s_demo_sequence[step] != 2) return false;
            step++;
            busy_wait_ms(80);

        } else if (!gpio_get(SS_BTN_BLUE)) {
            gpio_put(SS_LED_BLUE, 1);
            while (!gpio_get(SS_BTN_BLUE)) busy_wait_ms(5);
            gpio_put(SS_LED_BLUE, 0);
            if (s_demo_sequence[step] != 3) return false;
            step++;
            busy_wait_ms(80);

        } else if (!gpio_get(SS_BTN_YELLOW)) {
            gpio_put(SS_LED_YELLOW, 1);
            while (!gpio_get(SS_BTN_YELLOW)) busy_wait_ms(5);
            gpio_put(SS_LED_YELLOW, 0);
            if (s_demo_sequence[step] != 4) return false;
            step++;
            busy_wait_ms(80);
        }
    }
    return true;
}

// ─── Remaining API stubs ─────────────────────────────────────────────────────

bool simon_says_check_input(void) {
    if (s_user_input_len != s_demo_length) return false;
    for (int i = 0; i < s_demo_length; i++)
        if (s_user_input[i] != s_demo_sequence[i]) return false;
    return true;
}

void simon_says_record_input(uint8_t color) {
    if (s_user_input_len < s_demo_length)
        s_user_input[s_user_input_len++] = color;
}

void simon_says_reset(void) {
    gpio_put(SS_LED_RED, 0); gpio_put(SS_LED_GREEN, 0);
    gpio_put(SS_LED_BLUE, 0); gpio_put(SS_LED_YELLOW, 0);
    s_demo_length    = 0;
    s_user_input_len = 0;
    memset(s_demo_sequence, 0, sizeof(s_demo_sequence));
    memset(s_user_input,    0, sizeof(s_user_input));
}

void simon_says_set_active(bool active) { (void)active; }
bool simon_says_update(void)            { return false; }

