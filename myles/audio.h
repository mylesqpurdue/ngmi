#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>
#include "game.h"   // for wave_state_t

// Audio output pin — GP36 (Proton board speaker jack)
#define AUDIO_PIN 36

// One-time hardware setup — call in main() before the game loop
void audio_init(void);

// Call every frame from the main game loop.
// Reads the wave state and drives sound accordingly.
// dt_ms = milliseconds since the last call (e.g. 33 for ~30 FPS).
void audio_tick(wave_state_t state, uint32_t dt_ms);

#endif // AUDIO_H
