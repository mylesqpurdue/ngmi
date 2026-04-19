#include "timer_module.h"
#include "seg7.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include <string.h>
#include <stdio.h>

// Make sure to set these in main.c
extern const int SPI_7SEG_SCK; extern const int SPI_7SEG_CSn; extern const int SPI_7SEG_TX;
extern const int SEG7_DMA_CHANNEL;

// ─── Internal state ──────────────────────────────────────────────────────────

static volatile uint32_t s_remaining  = TIMER_DEFAULT_SECONDS;
static volatile bool     s_running    = false;
static volatile bool     s_expired    = false;
static game_state_t      s_game_state = GAME_IDLE;
static bool              s_colon_blink = false;

// Repeating hardware alarm fires every 1 000 ms.
static struct repeating_timer s_hw_timer;

// ─── Hardware timer callback (ISR context) ───────────────────────────────────

static bool timer_irq_callback(struct repeating_timer *t) {
    (void)t;
    if (!s_running || s_expired) return true;

    if (s_remaining > 0) {
        s_remaining--;
    }
    if (s_remaining == 0) {
        s_running = false;
        s_expired = true;
    }
    s_colon_blink = !s_colon_blink;
    return true;
}

// ─── Button polling (debounced, active-low) ──────────────────────────────────

#define BTN_DEBOUNCE_MS 50

static uint32_t s_start_last_ms = 0;
static uint32_t s_reset_last_ms = 0;

static void poll_buttons(void) {
    uint32_t now = to_ms_since_boot(get_absolute_time());

    if (!gpio_get(TIMER_BTN_START) && (now - s_start_last_ms > BTN_DEBOUNCE_MS)) {
        s_start_last_ms = now;
        if (s_game_state == GAME_IDLE || s_game_state == GAME_ARMED) {
            s_game_state = GAME_ACTIVE;
            timer_start();
        }
    }

    if (!gpio_get(TIMER_BTN_RESET) && (now - s_reset_last_ms > BTN_DEBOUNCE_MS)) {
        s_reset_last_ms = now;
        timer_reset();
    }
}

// ─── Public API ─────────────────────────────────────────────────────────────

void timer_module_init(void) {
    seg7_init();

    gpio_init(TIMER_BTN_START);
    gpio_set_dir(TIMER_BTN_START, GPIO_IN);
    gpio_pull_up(TIMER_BTN_START);

    gpio_init(TIMER_BTN_RESET);
    gpio_set_dir(TIMER_BTN_RESET, GPIO_IN);
    gpio_pull_up(TIMER_BTN_RESET);

    add_repeating_timer_ms(-1000, timer_irq_callback, NULL, &s_hw_timer);

    s_remaining   = TIMER_DEFAULT_SECONDS;
    s_running     = false;
    s_expired     = false;
    s_game_state  = GAME_IDLE;
}

void timer_start(void) {
    s_expired = false;
    s_running = true;
}

void timer_reset(void) {
    s_running     = false;
    s_expired     = false;
    s_remaining   = TIMER_DEFAULT_SECONDS;
    s_game_state  = GAME_ARMED;
    s_colon_blink = false;
}

void timer_set_game_state(game_state_t state) {
    s_game_state = state;
    switch (state) {
        case GAME_ACTIVE:
            timer_start();
            break;
        case GAME_DEFUSED:
        case GAME_EXPLODED:
            s_running = false;
            break;
        case GAME_IDLE:
        case GAME_ARMED:
            break;
    }
}

void timer_update(void) {
    poll_buttons();

    uint32_t rem  = s_remaining;
    uint8_t  mins = rem / 60;
    uint8_t  secs = rem % 60;

    switch (s_game_state) {
        case GAME_IDLE:
            seg7_show_dashes();
            seg7_refresh();
            break;

        case GAME_ARMED:
            seg7_display_time(mins, secs, true);
            seg7_refresh();
            break;

        case GAME_ACTIVE:
            seg7_display_time(mins, secs, s_colon_blink);
            seg7_refresh();
            if (s_expired) {
                // Flash 00.00 to indicate explosion
                for (int i = 0; i < 6; i++) {
                    seg7_display_time(0, 0, true);  seg7_refresh(); sleep_ms(150);
                    seg7_clear();                   seg7_refresh(); sleep_ms(150);
                }
                seg7_display_time(0, 0, true);
                seg7_refresh();
            }
            break;

        case GAME_DEFUSED:
            seg7_display_time(mins, secs, true);
            seg7_refresh();
            break;

        case GAME_EXPLODED:
            seg7_display_time(0, 0, true);
            seg7_refresh();
            break;
    }
}

uint32_t timer_get_remaining(void) { return s_remaining; }
bool     timer_is_expired(void)    { return s_expired;   }
