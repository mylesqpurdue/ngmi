#ifndef WAVES_H
#define WAVES_H

#include <stdint.h>

#define WAVE_WIDTH    320
#define PLOT_HEIGHT   180
#define PLOT_CENTER_Y 90

// Compute sine wave Y-coordinates into dst array (320 elements)
void wave_compute(int16_t *dst, float frequency, float amplitude, int center_y, int width);

// Clamp a Y-coordinate to plotting area bounds [0, PLOT_HEIGHT-1]
int16_t wave_clamp_y(int16_t y);

// Render target wave (static, drawn once per round)
void wave_draw_target(const int16_t *y_coords);

// Render player wave (erase old, draw new)
void wave_draw_player(const int16_t *old_y, const int16_t *new_y);

#endif // WAVES_H