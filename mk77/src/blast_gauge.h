#ifndef BLAST_GAUGE_H
#define BLAST_GAUGE_H

#include <stdint.h>
#include <stdbool.h>

#define BG_WS2812_PIN 16 // PIO output -> WS2812 data in
#define BG_SERVO_PIN  20 // PWM -> servo signal
#define BG_BUTTON_PIN 5 // Active-low, internal pull-up (unchanged)

// Dedicated RGB LED for Blast Gauge target colors
#define BG_LED_R_PIN 6
#define BG_LED_G_PIN 3
#define BG_LED_B_PIN 2

// Module states 
// Servo sweeps continuously; button press = stop-and-check.
typedef enum {
    BG_IDLE,
    BG_ROUND1, // Sweeping -> zone stop
    BG_ROUND2, // Sweeping -> find the green
    BG_ROUND3, // Sweeping -> shifting zones
    BG_FLASH_PASS, // Brief green feedback flash after correct stop
    BG_FLASH_STRIKE, // Brief red feedback flash after wrong stop
    BG_INTER_ROUND,  // Short pause before next round starts
    BG_DEFUSED, // All 3 rounds cleared
    BG_EXPLODED, // (unused in integrated build — master decides)
} bg_state_t;

// Module context (owned by main; passed into every update) 
typedef struct {
    bg_state_t state;
    uint8_t strikes; // Local strike count (this module only)

    // Round data
    uint8_t target_zone;
    uint8_t r2_green_start; // Round 2: which LED cluster is green
    uint8_t r3_offset; // Round 3: current zone-rotation offset
    uint64_t r3_last_shift_us;

    // Servo sweep state
    int16_t angle; // Current servo angle in degrees
    int8_t direction; // +1 or -1
    uint16_t sweep_delay_ms; // ms per degree step
    uint64_t next_step_us; // When we're allowed to take the next step
    bool sweeping;

    // Feedback / transition state
    uint64_t hold_until_us; // When the current hold state ends
    uint8_t flash_remaining; // Remaining flash toggles (on+off counts as 2)
    bool flash_on; 
    uint64_t flash_next_toggle_us;

    // After a flash completes, where do we go?
    bg_state_t pending_next;
} bg_ctx_t;


// One-time setup (WS2812, servo PWM, button GPIO, RNG)
void bg_init(bg_ctx_t *ctx);

// Tick the module once per main-loop frame.
// button_edge: true on the frame where the button transitioned pressed.
// now_us: current monotonic microsecond timestamp.
// Returns: 0 = no event, 1 = strike, 2 = module solved (defused)
int bg_update(bg_ctx_t *ctx, bool button_edge, uint64_t now_us);

// Non-blocking button edge detector (debounced). Call once per frame.
// Returns true on the frame of a press (falling edge).
bool bg_button_poll(uint64_t now_us);

// Force the module back to IDLE (called on reset - e.g. power cycle handoff).
void bg_reset(bg_ctx_t *ctx);

// Accessors (for main's display / debug)
const char *bg_state_name(bg_state_t s);
uint16_t bg_servo_angle(const bg_ctx_t *ctx);
uint8_t bg_current_round(const bg_ctx_t *ctx); // 1, 2, 3, or 0

#endif // BLAST_GAUGE_H