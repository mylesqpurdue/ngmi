#include "simon_says.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include <string.h>
#include <stdlib.h>

// ─── Internal state ──────────────────────────────────────────────────────────

typedef enum {
    SS_STATE_IDLE,          // waiting for game to go ACTIVE
    SS_STATE_SHOW_SEQUENCE, // playing back the sequence to the player
    SS_STATE_WAIT_INPUT,    // waiting for button presses from player
    SS_STATE_ROUND_WIN,     // brief celebration flash before next round
    SS_STATE_SOLVED,        // all rounds complete
    SS_STATE_FAILED,        // wrong press — strike
} ss_state_t;

static ss_state_t s_state     = SS_STATE_IDLE;
static uint8_t    s_sequence[SS_MAX_ROUNDS];
static uint8_t    s_round     = 0;   // current round (0-indexed)
static uint8_t    s_input_idx = 0;   // how many presses entered this round
static bool       s_active    = false;

// Debounce: track last press time per button.
static uint32_t s_last_press_ms[4] = {0, 0, 0, 0};
#define DEBOUNCE_MS 50

// Playback / timing state
static uint32_t s_show_step_start = 0;
static uint8_t  s_show_idx        = 0;
static bool     s_show_led_on     = false;

// Input timeout
static uint32_t s_input_start_ms = 0;

// One-shot solved flag for caller
static bool s_just_solved = false;

// ─── Helpers ─────────────────────────────────────────────────────────────────

static const uint LED_PINS[4] = {SS_LED_RED, SS_LED_GREEN, SS_LED_BLUE, SS_LED_YELLOW};
static const uint BTN_PINS[4] = {SS_BTN_RED, SS_BTN_GREEN, SS_BTN_BLUE, SS_BTN_YELLOW};

static void leds_all_off(void) {
    for (int i = 0; i < 4; i++) gpio_put(LED_PINS[i], 0);
}

// Brief flash of all four LEDs used as a "success" indicator.
static void flash_all_leds(int times_ms, int count) {
    for (int n = 0; n < count; n++) {
        for (int i = 0; i < 4; i++) gpio_put(LED_PINS[i], 1);
        sleep_ms(times_ms);
        leds_all_off();
        sleep_ms(times_ms);
    }
}

// Extend the sequence by one random color.
static void append_random_color(void) {
    // Use microsecond timer entropy for a simple random pick.
    uint8_t color = (uint8_t)(time_us_32() % 4);
    s_sequence[s_round] = color;
}

// ─── Non-blocking sequence playback state machine ────────────────────────────
// Called every loop iteration while s_state == SS_STATE_SHOW_SEQUENCE.
// Steps through: LED on for SS_SHOW_MS → LED off for SS_SHOW_GAP_MS → next.

static void run_show_sequence(void) {
    uint32_t now = to_ms_since_boot(get_absolute_time());

    if (!s_show_led_on) {
        // Turn on the next LED in the sequence
        leds_all_off();
        gpio_put(LED_PINS[s_sequence[s_show_idx]], 1);
        s_show_led_on    = true;
        s_show_step_start = now;
    } else if (now - s_show_step_start >= SS_SHOW_MS) {
        // Time to turn the LED off
        leds_all_off();
        s_show_led_on    = false;
        s_show_step_start = now;

        // Wait for gap, then decide next step
        // We handle gap in the next call by checking elapsed > SHOW_MS + GAP_MS
        // Simpler: just use a two-phase elapsed check.
    }

    // Gap phase: after LED off, wait GAP_MS before proceeding
    if (!s_show_led_on && (now - s_show_step_start >= SS_SHOW_GAP_MS)) {
        s_show_idx++;
        if (s_show_idx > s_round) {
            // Finished showing current sequence → move to input phase
            s_show_idx       = 0;
            s_show_led_on    = false;
            s_input_idx      = 0;
            s_input_start_ms = to_ms_since_boot(get_absolute_time());
            s_state          = SS_STATE_WAIT_INPUT;
        } else {
            // Show next LED
            gpio_put(LED_PINS[s_sequence[s_show_idx]], 1);
            s_show_led_on    = true;
            s_show_step_start = now;
        }
    }
}

// ─── Button polling with debounce ────────────────────────────────────────────
// Returns 0–3 if a new press detected, -1 otherwise.

static int poll_buttons(void) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    for (int i = 0; i < 4; i++) {
        // Active-low: button pressed = GPIO reads 0
        if (!gpio_get(BTN_PINS[i]) && (now - s_last_press_ms[i] > DEBOUNCE_MS)) {
            s_last_press_ms[i] = now;
            // Brief LED feedback on the pressed button's LED
            gpio_put(LED_PINS[i], 1);
            sleep_ms(100);
            gpio_put(LED_PINS[i], 0);
            return i;
        }
    }
    return -1;
}

// ─── Public API ─────────────────────────────────────────────────────────────

void simon_says_init(void) {
    for (int i = 0; i < 4; i++) {
        gpio_init(LED_PINS[i]);
        gpio_set_dir(LED_PINS[i], GPIO_OUT);
        gpio_put(LED_PINS[i], 0);

        gpio_init(BTN_PINS[i]);
        gpio_set_dir(BTN_PINS[i], GPIO_IN);
        gpio_pull_up(BTN_PINS[i]);
    }
    simon_says_reset();
}

void simon_says_reset(void) {
    leds_all_off();
    s_state        = SS_STATE_IDLE;
    s_round        = 0;
    s_input_idx    = 0;
    s_show_idx     = 0;
    s_show_led_on  = false;
    s_just_solved  = false;
    memset(s_sequence, 0, sizeof(s_sequence));
}

void simon_says_set_active(bool active) {
    s_active = active;
    if (active && s_state == SS_STATE_IDLE) {
        // Begin round 0: add first color and start showing
        append_random_color();
        s_show_idx    = 0;
        s_show_led_on = false;
        s_state       = SS_STATE_SHOW_SEQUENCE;
    }
}

bool simon_says_update(void) {
    if (!s_active) return false;

    s_just_solved = false;

    switch (s_state) {

        case SS_STATE_IDLE:
            // Waiting for simon_says_set_active(true)
            break;

        case SS_STATE_SHOW_SEQUENCE:
            run_show_sequence();
            break;

        case SS_STATE_WAIT_INPUT: {
            // Check input timeout
            uint32_t elapsed = to_ms_since_boot(get_absolute_time()) - s_input_start_ms;
            if (elapsed > SS_INPUT_TIMEOUT_MS) {
                s_state = SS_STATE_FAILED;
                break;
            }

            int pressed = poll_buttons();
            if (pressed < 0) break;

            if ((uint8_t)pressed == s_sequence[s_input_idx]) {
                s_input_idx++;
                if (s_input_idx > s_round) {
                    // Correct round complete
                    s_round++;
                    if (s_round >= SS_MAX_ROUNDS) {
                        flash_all_leds(200, 3);
                        s_state       = SS_STATE_SOLVED;
                        s_just_solved = true;
                    } else {
                        s_state = SS_STATE_ROUND_WIN;
                    }
                }
            } else {
                // Wrong button
                s_state = SS_STATE_FAILED;
            }
            break;
        }

        case SS_STATE_ROUND_WIN:
            // Brief pause then start next round
            sleep_ms(600);
            append_random_color();
            s_show_idx    = 0;
            s_show_led_on = false;
            s_state       = SS_STATE_SHOW_SEQUENCE;
            break;

        case SS_STATE_SOLVED:
            // Stay solved; caller reads s_just_solved once.
            break;

        case SS_STATE_FAILED:
            // Flash all LEDs to indicate failure, then restart sequence
            for (int i = 0; i < 3; i++) {
                leds_all_off(); sleep_ms(150);
                for (int j = 0; j < 4; j++) gpio_put(LED_PINS[j], 1);
                sleep_ms(150);
            }
            leds_all_off();

            // Reset and regenerate — player must retry from round 1
            simon_says_reset();
            s_active = true;
            append_random_color();
            s_show_idx    = 0;
            s_show_led_on = false;
            s_state       = SS_STATE_SHOW_SEQUENCE;
            break;
    }

    return s_just_solved;
}
