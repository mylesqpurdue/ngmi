#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "../common/common.h"
#include "simon_says.h"
#include "timer_module.h"
#include <string.h>
#include <stdio.h>

// ─── Board: Alexia's RP2350 ───────────────────────────────────────────────────
// Responsibilities:
//   1. Simon Says / Memory Game  (GPIO 0–7, see simon_says.h)
//   2. Countdown Timer display   (TM1637 on GPIO 14–15, see timer_module.h)
//   3. UART0 to master           (GPIO 12–13, 115200 baud)
//
// Game loop:
//   - Listens for game-state broadcasts from master via UART0.
//   - When ACTIVE: runs Simon Says; timer counts down.
//   - When Simon Says is solved: notifies master via UART0.
//   - When timer expires: notifies master (EXPLODED).

// ─── Local UART RX for game-state parsing ────────────────────────────────────
// (timer_module already owns the UART instance; we share it here for state
// updates that affect Simon Says independently.)

static game_state_t s_current_state   = GAME_IDLE;
static bool         s_simon_complete  = false;

static char s_rx_buf[64];
static uint s_rx_idx = 0;

static void handle_master_message(const char *line) {
    if (strncmp(line, "$STATE:", 7) == 0) {
        const char *s = line + 7;
        game_state_t new_state = s_current_state;
        if      (strcmp(s, "ACTIVE")   == 0) new_state = GAME_ACTIVE;
        else if (strcmp(s, "DEFUSED")  == 0) new_state = GAME_DEFUSED;
        else if (strcmp(s, "EXPLODED") == 0) new_state = GAME_EXPLODED;
        else if (strcmp(s, "IDLE")     == 0) new_state = GAME_IDLE;
        else if (strcmp(s, "ARMED")    == 0) new_state = GAME_ARMED;

        if (new_state != s_current_state) {
            s_current_state = new_state;
            timer_set_game_state(new_state);
            simon_says_set_active(new_state == GAME_ACTIVE && !s_simon_complete);
        }
    } else if (strcmp(line, "$RESET") == 0) {
        s_current_state  = GAME_IDLE;
        s_simon_complete = false;
        simon_says_reset();
        timer_reset();
    }
}

static void poll_uart(void) {
    while (uart_is_readable(uart0)) {
        char c = (char)uart_getc(uart0);
        if (c == '\n' || c == '\r') {
            if (s_rx_idx > 0) {
                s_rx_buf[s_rx_idx] = '\0';
                handle_master_message(s_rx_buf);
                s_rx_idx = 0;
            }
        } else if (s_rx_idx < (uint)(sizeof(s_rx_buf) - 1)) {
            s_rx_buf[s_rx_idx++] = c;
        }
    }
}

// ─── Entry point ─────────────────────────────────────────────────────────────

int main(void) {
    stdio_init_all();

    simon_says_init();
    timer_module_init();   // also initialises UART0

    while (true) {
        // 1. Read any incoming UART messages from master
        poll_uart();

        // 2. Update timer display + internal countdown
        timer_update();

        // 3. Run Simon Says game tick
        if (!s_simon_complete && s_current_state == GAME_ACTIVE) {
            bool just_solved = simon_says_update();
            if (just_solved) {
                s_simon_complete = true;
                uart_puts(uart0, "$SOLVED:SIMON_SAYS\n");
            }
        }

        // A small yield keeps the loop from spinning too hot while still
        // being responsive to button presses.
        sleep_us(500);
    }

    return 0;
}
