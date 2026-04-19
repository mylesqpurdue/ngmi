#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include <stdbool.h>

// Game states
typedef enum {
    WAVE_IDLE,
    WAVE_PLAYING,
    WAVE_WIN,       // Brief win flash between rounds
    WAVE_RESET,     // Generate new target, back to IDLE
    WAVE_WAIT,      // Solved enough — wait without oversolving to complete
    WAVE_COMPLETE   // Module fully solved
} wave_state_t;

// Match tolerance constants
#define FREQ_TOLERANCE   0.3f
#define AMP_TOLERANCE    5.0f
#define LOCK_DURATION_US 1500000  // 1.5 seconds to lock a match
#define WIN_HOLD_MS      2000     // 2 seconds visible win state
#define DEAD_ZONE        0.5f     // Minimum pot change to exit IDLE
#define STRIKE_TIME_US   15000000 // 15 seconds to get a strike
#define WAIT_TIME_US     15000000 // 15 seconds to wait after solving enough

// LED signal colors for manual decoding
#define NUM_SIGNAL_COLORS 4
#define BASE_ROUNDS_MIN   2
#define BASE_ROUNDS_MAX   5

// Target wave parameters
typedef struct {
    float frequency;
    float amplitude;
} wave_params_t;

// Game state
typedef struct {
    wave_state_t state;
    wave_params_t target;
    uint64_t lock_start_us;
    uint64_t win_start_ms;
    uint64_t play_start_us;
    uint64_t wait_start_us;   // When WAIT state began
    bool freq_matched;
    bool amp_matched;

    int signal_color;
    int base_rounds;
    int strikes;
    int rounds_won;
    bool module_complete;
} game_ctx_t;

void game_init(game_ctx_t *ctx);
void game_new_target(wave_params_t *target);
bool game_check_freq_match(float player_freq, float target_freq, float tolerance);
bool game_check_amp_match(float player_amp, float target_amp, float tolerance);
int game_rounds_needed(const game_ctx_t *ctx);

// Returns: 1=strike, 2=module complete, 0=nothing
int game_update(game_ctx_t *ctx, float player_freq, float player_amp,
                uint64_t now_us);

void game_reset_idle(game_ctx_t *ctx);

#endif // GAME_H
