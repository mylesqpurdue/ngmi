#include "timer_module.h"
#include "tm1637.h"
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include <string.h>
#include <stdio.h>

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
    return true;  // keep repeating
}

// ─── UART helpers ────────────────────────────────────────────────────────────

static void uart_send(const char *msg) {
    uart_puts(uart0, msg);
}

static char s_rx_buf[64];
static uint  s_rx_idx = 0;

// Parse one complete line received from master.
static void process_rx_line(const char *line) {
    if (strncmp(line, "$STATE:", 7) == 0) {
        const char *state_str = line + 7;
        if      (strcmp(state_str, "IDLE")     == 0) timer_set_game_state(GAME_IDLE);
        else if (strcmp(state_str, "ARMED")    == 0) timer_set_game_state(GAME_ARMED);
        else if (strcmp(state_str, "ACTIVE")   == 0) timer_set_game_state(GAME_ACTIVE);
        else if (strcmp(state_str, "DEFUSED")  == 0) timer_set_game_state(GAME_DEFUSED);
        else if (strcmp(state_str, "EXPLODED") == 0) timer_set_game_state(GAME_EXPLODED);
    } else if (strcmp(line, "$RESET") == 0) {
        timer_reset();
    }
}

// Non-blocking character-by-character UART RX, line-assembled.
static void poll_uart_rx(void) {
    while (uart_is_readable(uart0)) {
        char c = (char)uart_getc(uart0);
        if (c == '\n' || c == '\r') {
            if (s_rx_idx > 0) {
                s_rx_buf[s_rx_idx] = '\0';
                process_rx_line(s_rx_buf);
                s_rx_idx = 0;
            }
        } else if (s_rx_idx < sizeof(s_rx_buf) - 1) {
            s_rx_buf[s_rx_idx++] = c;
        }
    }
}

// Broadcast current time to the master every second.
static uint32_t s_last_broadcast = 0;

static void broadcast_time_if_needed(void) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - s_last_broadcast >= 1000) {
        s_last_broadcast = now;
        char buf[32];
        snprintf(buf, sizeof(buf), "$TIME:%lu\n", (unsigned long)s_remaining);
        uart_send(buf);
    }
}

// ─── Public API ─────────────────────────────────────────────────────────────

void timer_module_init(void) {
    // TM1637 display
    tm1637_init(TM1637_CLK_PIN, TM1637_DIO_PIN);
    tm1637_set_brightness(5);
    tm1637_clear();

    // UART0 for master communication
    uart_init(uart0, UART_BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    uart_set_format(uart0, 8, 1, UART_PARITY_NONE);

    // Hardware repeating timer — 1 Hz
    add_repeating_timer_ms(-1000, timer_irq_callback, NULL, &s_hw_timer);

    s_remaining  = TIMER_DEFAULT_SECONDS;
    s_running    = false;
    s_expired    = false;
    s_game_state = GAME_IDLE;
}

void timer_start(void) {
    s_expired = false;
    s_running = true;
}

void timer_reset(void) {
    s_running    = false;
    s_expired    = false;
    s_remaining  = TIMER_DEFAULT_SECONDS;
    s_game_state = GAME_IDLE;
    s_colon_blink = false;
    tm1637_clear();
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
            // nothing — wait for ACTIVE
            break;
    }
}

void timer_update(void) {
    poll_uart_rx();

    uint32_t rem = s_remaining;
    uint8_t  mins = rem / 60;
    uint8_t  secs = rem % 60;

    switch (s_game_state) {
        case GAME_IDLE:
            // Show dashes: -- : --
            // Reuse clear so the display is blank until the game arms.
            tm1637_clear();
            break;

        case GAME_ARMED:
            // Show full start time, solid colon
            tm1637_display_time(mins, secs, true);
            break;

        case GAME_ACTIVE:
            // Count down; blink colon each second
            tm1637_display_time(mins, secs, s_colon_blink);
            if (s_running) broadcast_time_if_needed();
            if (s_expired) {
                // Notify master
                uart_send("$SOLVED:TIMER_EXPIRED\n");
                // Flash 00:00 rapidly to indicate explosion
                for (int i = 0; i < 6; i++) {
                    tm1637_display_time(0, 0, true);  sleep_ms(150);
                    tm1637_clear();                    sleep_ms(150);
                }
                tm1637_display_time(0, 0, true);
            }
            break;

        case GAME_DEFUSED:
            // Freeze current display, colon on
            tm1637_display_time(mins, secs, true);
            break;

        case GAME_EXPLODED:
            // Show 00:00
            tm1637_display_time(0, 0, true);
            break;
    }
}

uint32_t timer_get_remaining(void) { return s_remaining; }
bool     timer_is_expired(void)    { return s_expired;   }
