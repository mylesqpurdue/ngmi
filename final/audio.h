#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>

// Audio output pin — GP36 (Proton board speaker jack)
#define AUDIO_PIN 36

typedef enum {
    AUDIO_STATE_IDLE,
    AUDIO_STATE_TICKING,
    AUDIO_STATE_BOOM,
    AUDIO_STATE_DEFUSED
} audio_state_t;

// One-time hardware setup — call in main() before the game loop
void audio_init(void);

// Call every frame from the main game loop.
// Reads the global state and drives sound accordingly.
// dt_ms = milliseconds since the last call (e.g. 10 for 100 FPS).
void audio_tick(audio_state_t state, uint32_t dt_ms);

#endif // AUDIO_H
