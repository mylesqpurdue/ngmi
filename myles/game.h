#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include <stdbool.h>

// Game states
typedef enum {
    WAVE_IDLE,
    WAVE_PLAYING,
    WAVE_WIN,
    WAVE_RESET,
    WAVE_COMPLETE    // Module fully solved — no more rounds
} wave_state_t;

// Match tolerance constants
#define FREQ_TOLERANCE   0.3f
#define AMP_TOLERANCE    5.0f
#define LOCK_DURATION_US 1500000  // 1.5 seconds in microseconds
#define WIN_HOLD_MS      2000     // 2 seconds visible win state
#define DEAD_ZONE        0.5f     // Minimum pot change to exit IDLE
#define STRIKE_TIME_US   15000000 // 15 seconds to get a strike

// LED signal colors for manual decoding
// 0=blue(2 rounds), 1=yellow(3), 2=red(4), 3=purple(5)
#define NUM_SIGNAL_COLORS 4
#define BASE_ROUNDS_MIN   2
#define BASE_ROUNDS_MAX   5

// Target wave parameters
typedef struct {
    float frequency;    // [1.0, 10.0]
    float amplitude;    // [15.0, 55.0] (integer range, stored as float)
} wave_params_t;

// Game state
typedef struct {
    wave_state_t state;
    wave_params_t target;
    uint64_t lock_start_us;   // Timestamp when lock began (0 = not locked)
    uint64_t win_start_ms;    // Timestamp when WIN state entered
    uint64_t play_start_us;   // Timestamp when PLAYING started (for strike timer)
    bool freq_matched;
    bool amp_matched;

    // Round/strike system
    int signal_color;         // 0-3, randomly chosen at init
    int base_rounds;          // 2-5, derived from signal_color
    int strikes;              // Number of strikes accumulated
    int rounds_won;           // Rounds successfully completed
    bool module_complete;     // True when all rounds done
} game_ctx_t;

// Initialization
void game_init(game_ctx_t *ctx);

// Generate new random target parameters
void game_new_target(wave_params_t *target);

// Pure match detection (testable)
bool game_check_freq_match(float player_freq, float target_freq, float tolerance);
bool game_check_amp_match(float player_amp, float target_amp, float tolerance);

// How many total rounds needed
int game_rounds_needed(const game_ctx_t *ctx);

// State machine update (call each game loop iteration)
// Returns: 1 if strike just happened, 2 if module just completed, 0 otherwise
int game_update(game_ctx_t *ctx, float player_freq, float player_amp,
                uint64_t now_us);

// Reset dead-zone tracking so IDLE re-captures current pot position
void game_reset_idle(game_ctx_t *ctx);

#endif // GAME_H
