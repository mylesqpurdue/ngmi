// main.c — Integrated Krish + Myles slave board
//
// This board runs two game modules concurrently on a single RP2350:
//   • Blast Gauge (Krish)  — WS2812 ring + servo, armed by button press
//   • Wave Match (Myles)   — pots + TFT, armed by moving either pot
//
// Both modules run in parallel every frame — player can switch attention
// back and forth, no "active module" arbitration needed.
//
// Architecture: this board is a DUMB SLAVE. It fires events up to the
// Celine/Alexia master via UART1 TX and doesn't listen for anything back:
//
// On local strike -> link_send_strike("KRISH" | "MYLES")
// On local solve -> link_send_solved("KRISH" | "MYLES")
//
// The master owns:
// - Total strike count (from all 4 modules across both boards)
// - Countdown timer
// - DEFUSED / EXPLODED terminal state
// - Visible bomb status display
//
// This board just keeps playing until power is cycled. Blast gauge still
// tracks its own local strike count for round-flash feedback, but that's
// decorative -- the authoritative count lives on the master.

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"

#include "blast_gauge.h"
#include "link.h"

// Myles's module (copied from ../myles/)
#include "display.h"
#include "inputs.h"
#include "waves.h"
#include "led.h"
#include "game.h"

// Module contexts
static bg_ctx_t bg;
static game_ctx_t waves;

static int16_t target_y[WAVE_WIDTH];
static int16_t player_y[WAVE_WIDTH];
static int16_t prev_player_y[WAVE_WIDTH];

// Myles idle-LED signaling helper
static void led_signal_on(int signal_color) {
    switch (signal_color) {
        case 0: led_set_color(0, 0, 255); break;
        case 1: led_set_color(255, 255, 0); break;
        case 2: led_set_color(255, 0, 0); break;
        case 3: led_set_color(128, 0, 255); break;
    }
}

static void prime_waves_idle(void) {
    // Myles's init: settle pot baseline before sampling the first "real" value
    for (int i = 0; i < 20; i++) {
        inputs_update();
        sleep_ms(5);
    }
    game_reset_idle(&waves);
}

// Status lines on TFT (bottom ~50 px below Myles's wave plot)
static void draw_status_line(void) {
    char line[48];

    // Top: blast gauge status
    snprintf(line, sizeof(line), "Blast: %-6s R%u Str:%u   ", bg_state_name(bg.state), bg_current_round(&bg), bg.strikes);
    display_draw_string(10, 190, line, COLOR_WHITE, COLOR_BLACK);

    // Bottom: wave game pot readouts (what Myles's original had)
    snprintf(line, sizeof(line), "F:%4.1f/%4.1f A:%2.0f/%2.0f   ", inputs_get_freq(), waves.target.frequency, inputs_get_amp(), waves.target.amplitude);
    display_draw_string(10, 210, line, COLOR_LGRAY, COLOR_BLACK);
}

// Main
int main(void) {
    stdio_init_all();
    sleep_ms(500);

    printf("\n========================================\n");
    printf("  NGMI — Krish + Myles Slave Board\n");
    printf("  TX-only link → master on UART1\n");
    printf("========================================\n");

    // Myles's init
    display_init();
    inputs_init();
    led_init();
    game_init(&waves);
    prime_waves_idle();

    // Krish's init
    bg_init(&bg);

    // Peer link (TX-only)
    link_init();

    // Paint initial target wave
    wave_compute(target_y, waves.target.frequency,
                 waves.target.amplitude, PLOT_CENTER_Y, WAVE_WIDTH);
    wave_draw_target(target_y);
    for (int i = 0; i < WAVE_WIDTH; i++) prev_player_y[i] = PLOT_CENTER_Y;

    uint64_t blink_toggle_ms = 0;
    bool blink_on = true;

    // Main loop -> target ~30 Hz
    while (1) {
        uint64_t now_us = time_us_64();
        uint64_t now_ms = now_us / 1000;

        // 1. Poll the button (non-blocking edge detector)
        bool button_edge = bg_button_poll(now_us);

        // Button inert once blast gauge has locked its result in
        if (bg.state == BG_DEFUSED || bg.state == BG_EXPLODED) {
            button_edge = false;
        }

        // 2. Tick blast gauge
        int bg_evt = bg_update(&bg, button_edge, now_us);

        if (bg_evt == 1) {
            link_send_strike("KRISH");
            printf("[MAIN] → master: $STRIKE:KRISH\n");
        } else if (bg_evt == 2) {
            link_send_solved("KRISH");
            printf("[MAIN] → master: $SOLVED:KRISH\n");
        }

        // 3. Tick Myles's wave game
        if (!waves.module_complete) {
            inputs_update();
            float freq = inputs_get_freq();
            float amp = inputs_get_amp();

            wave_state_t old_state = waves.state;
            int waves_evt = game_update(&waves, freq, amp, now_us);

            if (waves_evt == 1) {
                link_send_strike("MYLES");
                printf("[MAIN] → master: $STRIKE:MYLES\n");
            } else if (waves_evt == 2) {
                link_send_solved("MYLES");
                printf("[MAIN] → master: $SOLVED:MYLES\n");
            }

            // Idle-state LED blink (Myles's manual-decoding signal color).
            // Only drive the RGB LED when blast gauge is also idle so we're
            // not fighting for the player's attention.
            if (waves.state == WAVE_IDLE) {
                if (now_ms - blink_toggle_ms >= 500) {
                    blink_on = !blink_on;
                    blink_toggle_ms = now_ms;
                }
                if (bg.state == BG_IDLE) {
                    if (blink_on) led_signal_on(waves.signal_color);
                    else led_set_color(0, 0, 0);
                }
            }

            // Repaint target on new-round transitions
            if (waves.state == WAVE_IDLE &&
                (old_state == WAVE_RESET || old_state == WAVE_WIN)) {
                wave_compute(target_y, waves.target.frequency,
                             waves.target.amplitude, PLOT_CENTER_Y, WAVE_WIDTH);
                display_fill_rect(0, 0, TFT_WIDTH, PLOT_HEIGHT, COLOR_BLACK);
                wave_draw_target(target_y);
                for (int i = 0; i < WAVE_WIDTH; i++) prev_player_y[i] = PLOT_CENTER_Y;
            }

            // Render player wave in WAIT or PLAYING
            if (waves.state == WAVE_WAIT || waves.state == WAVE_PLAYING) {
                memcpy(prev_player_y, player_y, sizeof(player_y));
                wave_compute(player_y, freq, amp, PLOT_CENTER_Y, WAVE_WIDTH);
                wave_draw_player(prev_player_y, player_y);
            }
        }

        // 4. Render status lines
        draw_status_line();
        sleep_ms(33);
    }

    return 0;
}