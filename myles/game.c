#include "game.h"
#include "led.h"
#include <math.h>
#include "pico/rand.h"
#include "pico/stdlib.h"

// Track initial pot values for dead-zone detection in IDLE state
static float initial_freq = -1.0f;
static float initial_amp = -1.0f;

void game_init(game_ctx_t *ctx) {
    ctx->state = WAVE_IDLE;
    ctx->lock_start_us = 0;
    ctx->win_start_ms = 0;
    ctx->play_start_us = 0;
    ctx->freq_matched = false;
    ctx->amp_matched = false;

    // Pick random signal color (0-3) which determines base rounds (2-5)
    ctx->signal_color = get_rand_32() % NUM_SIGNAL_COLORS;
    ctx->base_rounds = BASE_ROUNDS_MIN + ctx->signal_color;
    ctx->strikes = 0;
    ctx->rounds_won = 0;
    ctx->module_complete = false;

    game_new_target(&ctx->target);
}

void game_new_target(wave_params_t *target) {
    target->frequency = 1.0f + (float)(get_rand_32() % 901) / 100.0f;
    target->amplitude = (float)(15 + (get_rand_32() % 41));
}

bool game_check_freq_match(float player_freq, float target_freq, float tolerance) {
    return fabsf(player_freq - target_freq) <= tolerance;
}

bool game_check_amp_match(float player_amp, float target_amp, float tolerance) {
    return fabsf(player_amp - target_amp) <= tolerance;
}

int game_rounds_needed(const game_ctx_t *ctx) {
    return ctx->base_rounds + ctx->strikes;
}

int game_update(game_ctx_t *ctx, float player_freq, float player_amp,
                uint64_t now_us) {
    int event = 0;  // 0=nothing, 1=strike, 2=module complete

    // If module is already complete, do nothing
    if (ctx->module_complete) {
        return 0;
    }

    switch (ctx->state) {
        case WAVE_IDLE:
            if (initial_freq < 0.0f) {
                initial_freq = player_freq;
                initial_amp = player_amp;
            }
            if (fabsf(player_freq - initial_freq) > DEAD_ZONE ||
                fabsf(player_amp - initial_amp) > DEAD_ZONE) {
                ctx->state = WAVE_PLAYING;
                ctx->play_start_us = now_us;
                led_set_playing();
            }
            break;

        case WAVE_PLAYING: {
            // Check for strike (too slow)
            uint64_t play_elapsed = now_us - ctx->play_start_us;
            if (play_elapsed >= STRIKE_TIME_US) {
                ctx->strikes++;
                ctx->play_start_us = now_us;  // Reset strike timer
                event = 1;
                // Flash red briefly to indicate strike
                led_set_color(255, 0, 0);
                sleep_ms(200);
                led_set_playing();
            }

            // Evaluate match conditions
            ctx->freq_matched = game_check_freq_match(player_freq,
                                    ctx->target.frequency, FREQ_TOLERANCE);
            ctx->amp_matched = game_check_amp_match(player_amp,
                                    ctx->target.amplitude, AMP_TOLERANCE);

            if (ctx->freq_matched && ctx->amp_matched) {
                if (ctx->lock_start_us == 0) {
                    ctx->lock_start_us = now_us;
                }
                uint64_t elapsed = now_us - ctx->lock_start_us;
                float progress = (float)elapsed / (float)LOCK_DURATION_US;
                if (progress > 1.0f) progress = 1.0f;
                led_set_locking(progress);

                if (elapsed >= LOCK_DURATION_US) {
                    ctx->rounds_won++;
                    if (ctx->rounds_won >= game_rounds_needed(ctx)) {
                        // Module complete!
                        ctx->state = WAVE_COMPLETE;
                        ctx->module_complete = true;
                        led_set_win();
                        event = 2;
                    } else {
                        ctx->state = WAVE_WIN;
                        ctx->win_start_ms = now_us / 1000;
                        led_set_win();
                    }
                }
            } else {
                ctx->lock_start_us = 0;
                led_set_playing();
            }
            break;
        }

        case WAVE_WIN: {
            uint64_t elapsed_ms = (now_us / 1000) - ctx->win_start_ms;
            if (elapsed_ms >= WIN_HOLD_MS) {
                ctx->state = WAVE_RESET;
            }
            break;
        }

        case WAVE_RESET:
            led_flash_white();
            game_new_target(&ctx->target);
            ctx->lock_start_us = 0;
            ctx->play_start_us = 0;
            ctx->freq_matched = false;
            ctx->amp_matched = false;
            initial_freq = -1.0f;
            initial_amp = -1.0f;
            ctx->state = WAVE_IDLE;
            // LED handled by main loop (blinks signal color in IDLE)
            break;

        case WAVE_COMPLETE:
            // Stay here forever — module is done
            break;

        default:
            ctx->state = WAVE_IDLE;
            break;
    }

    return event;
}

void game_reset_idle(game_ctx_t *ctx) {
    ctx->state = WAVE_IDLE;
    ctx->lock_start_us = 0;
    ctx->play_start_us = 0;
    ctx->freq_matched = false;
    ctx->amp_matched = false;
    // Reset dead-zone tracking so IDLE re-captures current pot position
    initial_freq = -1.0f;
    initial_amp = -1.0f;
}
