#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "display.h"
#include "inputs.h"
#include "waves.h"
#include "led.h"
#include "game.h"

// Quick LED pin test — comment out to return to game
// LED test done — back to game mode
// #define LED_TEST

#ifdef LED_TEST

int main(void) {
    stdio_init_all();
    display_init();
    led_init();

    display_draw_string(10, 10, "LED PIN TEST", COLOR_WHITE, COLOR_BLACK);

    while (1) {
        // GP10 only
        display_draw_string(10, 40, "GP10 only - what color?", COLOR_YELLOW, COLOR_BLACK);
        led_set_color(255, 0, 0);  // R channel
        sleep_ms(3000);

        // GP11 only
        display_draw_string(10, 40, "GP11 only - what color?", COLOR_YELLOW, COLOR_BLACK);
        led_set_color(0, 255, 0);  // G channel
        sleep_ms(3000);

        // GP12 only
        display_draw_string(10, 40, "GP12 only - what color?", COLOR_YELLOW, COLOR_BLACK);
        led_set_color(0, 0, 255);  // B channel
        sleep_ms(3000);

        // All off
        display_draw_string(10, 40, "ALL OFF              ", COLOR_YELLOW, COLOR_BLACK);
        led_set_color(0, 0, 0);
        sleep_ms(2000);
    }

    return 0;
}

#else

static int16_t target_y[WAVE_WIDTH];
static int16_t player_y[WAVE_WIDTH];
static int16_t prev_player_y[WAVE_WIDTH];
static game_ctx_t game_ctx;
static char uart_rx_buf[32];
static int  uart_rx_idx = 0;

static void led_signal_on(int signal_color) {
    switch (signal_color) {
        case 0: led_set_color(0,   0,   255); break;
        case 1: led_set_color(255, 255, 0);   break;
        case 2: led_set_color(255, 0,   0);   break;
        case 3: led_set_color(128, 0,   255); break;
    }
}

static void prime_idle(void) {
    for (int i = 0; i < 20; i++) {
        inputs_update();
        sleep_ms(5);
    }
    game_reset_idle(&game_ctx);
}

int main(void) {
    stdio_init_all();
    display_init();
    inputs_init();
    led_init();
    game_init(&game_ctx);

    prime_idle();

    wave_compute(target_y, game_ctx.target.frequency,
                 game_ctx.target.amplitude, PLOT_CENTER_Y, WAVE_WIDTH);
    wave_draw_target(target_y);

    for (int i = 0; i < WAVE_WIDTH; i++) {
        prev_player_y[i] = PLOT_CENTER_Y;
    }

    uint64_t blink_toggle_ms = 0;
    bool blink_on = true;

    while (1) {
        if (game_ctx.module_complete) {
            led_set_win();
            display_draw_string(10, 190, "MODULE COMPLETE!    ", COLOR_GREEN, COLOR_BLACK);
            display_draw_string(10, 210, "                    ", COLOR_BLACK, COLOR_BLACK);
            sleep_ms(100);
            continue;
        }

        inputs_update();
        float freq = inputs_get_freq();
        float amp  = inputs_get_amp();

        uint64_t now = time_us_64();
        uint64_t now_ms = now / 1000;
        wave_state_t old_state = game_ctx.state;
        int event = game_update(&game_ctx, freq, amp, now);

        if (event == 1) printf("$STRIKE\n");
        if (event == 2) printf("$SOLVED:MYLES\n");

        if (game_ctx.state == WAVE_IDLE) {
            if (now_ms - blink_toggle_ms >= 500) {
                blink_on = !blink_on;
                blink_toggle_ms = now_ms;
            }
            if (blink_on) led_signal_on(game_ctx.signal_color);
            else led_set_color(0, 0, 0);
        }

        if (game_ctx.state == WAVE_IDLE &&
                (old_state == WAVE_RESET || old_state == WAVE_WIN)) {
            wave_compute(target_y, game_ctx.target.frequency,
                         game_ctx.target.amplitude, PLOT_CENTER_Y, WAVE_WIDTH);
            display_fill_rect(0, 0, TFT_WIDTH, PLOT_HEIGHT, COLOR_BLACK);
            wave_draw_target(target_y);
            for (int i = 0; i < WAVE_WIDTH; i++)
                prev_player_y[i] = PLOT_CENTER_Y;
        }

        // WAIT state: keep showing player wave so oversolve is possible
        if (game_ctx.state == WAVE_WAIT) {
            memcpy(prev_player_y, player_y, sizeof(player_y));
            wave_compute(player_y, freq, amp, PLOT_CENTER_Y, WAVE_WIDTH);
            wave_draw_player(prev_player_y, player_y);
        }

        if (game_ctx.state == WAVE_PLAYING) {
            memcpy(prev_player_y, player_y, sizeof(player_y));
            wave_compute(player_y, freq, amp, PLOT_CENTER_Y, WAVE_WIDTH);
            wave_draw_player(prev_player_y, player_y);
        }

        char readout[48];
        snprintf(readout, sizeof(readout),
                 "Freq:%4.1f/%4.1f Amp:%2.0f/%2.0f",
                 freq, game_ctx.target.frequency,
                 amp,  game_ctx.target.amplitude);
        display_draw_string(10, 190, readout, COLOR_WHITE, COLOR_BLACK);

        char status[48];
        snprintf(status, sizeof(status),
                 "Strikes: %d            ", game_ctx.strikes);
        display_draw_string(10, 210, status, COLOR_LGRAY, COLOR_BLACK);

        int ch = getchar_timeout_us(0);
        if (ch != PICO_ERROR_TIMEOUT) {
            if (ch == '\n' || uart_rx_idx >= (int)sizeof(uart_rx_buf) - 1) {
                uart_rx_buf[uart_rx_idx] = '\0';
                if (strcmp(uart_rx_buf, "$RESET") == 0) {
                    game_init(&game_ctx);
                    prime_idle();
                    wave_compute(target_y, game_ctx.target.frequency,
                                 game_ctx.target.amplitude, PLOT_CENTER_Y, WAVE_WIDTH);
                    display_fill_rect(0, 0, TFT_WIDTH, PLOT_HEIGHT, COLOR_BLACK);
                    wave_draw_target(target_y);
                }
                uart_rx_idx = 0;
            } else {
                uart_rx_buf[uart_rx_idx++] = (char)ch;
            }
        }

        sleep_ms(33);
    }

    return 0;
}

#endif
