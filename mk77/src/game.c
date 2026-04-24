#include "game.h"
#include "led.h"
#include <math.h>
#include "pico/rand.h"
#include "pico/stdlib.h"

static float initial_freq = -1.0f;
static float initial_amp = -1.0f;

// Stability tracking -> pots must be still before lock counts
static float prev_freq = -1.0f;
static float prev_amp = -1.0f;
static uint64_t stable_since_us = 0;
#define STABLE_THRESHOLD 0.15f // Max change per frame to count as "still"
#define STABLE_TIME_US 500000 // 0.5 seconds of stillness required

void game_init(game_ctx_t *ctx) {
    ctx->state = WAVE_IDLE;
    ctx->lock_start_us = 0;
    ctx->win_start_ms = 0;
    ctx->play_start_us = 0;
    ctx->wait_start_us = 0;
    ctx->flash_start_us = 0;
    ctx->flash_duration_us = 0;
    ctx->pending_state = WAVE_IDLE;
    ctx->freq_matched = false;
    ctx->amp_matched = false;

    ctx->signal_color = get_rand_32() % NUM_SIGNAL_COLORS;
    ctx->base_rounds = BASE_ROUNDS_MIN + ctx->signal_color;
    ctx->strikes = 0;
    ctx->rounds_won = 0;
    ctx->module_complete = false;

    initial_freq = -1.0f;
    initial_amp = -1.0f;

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
    int event = 0;

    if (ctx->module_complete) return 0;

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
            // Strike timer > too slow
            uint64_t play_elapsed = now_us - ctx->play_start_us;
            if (play_elapsed >= STRIKE_TIME_US) {
                ctx->strikes++;
                ctx->play_start_us = now_us;
                event = 1;
                led_set_color(255, 0, 0);
                ctx->flash_start_us = now_us;
                ctx->flash_duration_us = 200000;
                ctx->pending_state = WAVE_PLAYING;
                ctx->state = WAVE_FLASH_STRIKE;
            }

            // Track pot stability
            bool pots_stable = false;
            if (prev_freq >= 0.0f) {
                bool freq_still = fabsf(player_freq - prev_freq) < STABLE_THRESHOLD;
                bool amp_still = fabsf(player_amp - prev_amp) < STABLE_THRESHOLD;
                if (freq_still && amp_still) {
                    if (stable_since_us == 0) stable_since_us = now_us;
                    pots_stable = (now_us - stable_since_us) >= STABLE_TIME_US;
                } else {
                    stable_since_us = 0;
                }
            }
            prev_freq = player_freq;
            prev_amp = player_amp;

            // Match detection -> only lock when pots are stable
            ctx->freq_matched = game_check_freq_match(player_freq, ctx->target.frequency, FREQ_TOLERANCE);
            ctx->amp_matched = game_check_amp_match(player_amp, ctx->target.amplitude, AMP_TOLERANCE);

            if (ctx->freq_matched && ctx->amp_matched && pots_stable) {
                if (ctx->lock_start_us == 0) {
                    ctx->lock_start_us = now_us;
                }
                uint64_t elapsed = now_us - ctx->lock_start_us;
                float progress = (float)elapsed / (float)LOCK_DURATION_US;
                if (progress > 1.0f) progress = 1.0f;
                led_set_locking(progress);

                if (elapsed >= LOCK_DURATION_US) {
                    ctx->rounds_won++;
                    ctx->state = WAVE_WIN;
                    ctx->win_start_ms = now_us / 1000;
                    led_set_win();
                    stable_since_us = 0;
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
                // Check if player has solved enough
                if (ctx->rounds_won >= game_rounds_needed(ctx)) {
                    // Enter WAIT -> generate new target so current pot position
                    // doesn't accidentally match, and player must stop
                    ctx->state = WAVE_WAIT;
                    ctx->wait_start_us = now_us;
                    ctx->lock_start_us = 0;
                    game_new_target(&ctx->target);
                } else {
                    // More rounds needed
                    ctx->state = WAVE_RESET;
                }
            }
            break;
        }

        case WAVE_RESET:
            led_set_color(255, 255, 255);
            game_new_target(&ctx->target);
            ctx->lock_start_us = 0;
            ctx->play_start_us = 0;
            ctx->freq_matched = false;
            ctx->amp_matched = false;
            initial_freq = -1.0f;
            initial_amp = -1.0f;
            ctx->flash_start_us = now_us;
            ctx->flash_duration_us = 100000;
            ctx->pending_state = WAVE_IDLE;
            ctx->state = WAVE_FLASH_WHITE;
            break;

        case WAVE_FLASH_STRIKE:
        case WAVE_FLASH_WHITE:
            if (now_us - ctx->flash_start_us >= ctx->flash_duration_us) {
                if (ctx->state == WAVE_FLASH_WHITE) {
                    led_set_color(0, 0, 0); // End of white flash
                } else if (ctx->pending_state == WAVE_PLAYING) {
                    led_set_playing(); // Restore playing color
                }
                ctx->state = ctx->pending_state;
            }
            break;

        case WAVE_WAIT: {
            uint64_t wait_elapsed = now_us - ctx->wait_start_us;

            // Hold green while waiting
            led_set_win();

            // Module complete after waiting long enough
            if (wait_elapsed >= WAIT_TIME_US) {
                ctx->state = WAVE_COMPLETE;
                ctx->module_complete = true;
                led_set_win();
                event = 2;
                break;
            }

            // Oversolve check — only after 3s grace period
            if (wait_elapsed > 3000000) {
                ctx->freq_matched = game_check_freq_match(player_freq,
                                        ctx->target.frequency, FREQ_TOLERANCE);
                ctx->amp_matched = game_check_amp_match(player_amp,
                                        ctx->target.amplitude, AMP_TOLERANCE);

                if (ctx->freq_matched && ctx->amp_matched) {
                    if (ctx->lock_start_us == 0) {
                        ctx->lock_start_us = now_us;
                    }
                    if (now_us - ctx->lock_start_us >= LOCK_DURATION_US) {
                        ctx->strikes++;
                        ctx->rounds_won++;
                        event = 1;
                        led_set_color(255, 0, 0);
                        ctx->flash_start_us = now_us;
                        ctx->flash_duration_us = 300000;
                        ctx->pending_state = WAVE_RESET;
                        ctx->state = WAVE_FLASH_STRIKE;
                        ctx->lock_start_us = 0;
                    }
                } else {
                    ctx->lock_start_us = 0;
                }
            }
            break;
        }

        case WAVE_COMPLETE:
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
    ctx->wait_start_us = 0;
    ctx->flash_start_us = 0;
    ctx->flash_duration_us = 0;
    ctx->pending_state = WAVE_IDLE;
    ctx->freq_matched = false;
    ctx->amp_matched = false;
    initial_freq = -1.0f;
    initial_amp = -1.0f;
    prev_freq = -1.0f;
    prev_amp = -1.0f;
    stable_since_us = 0;
}