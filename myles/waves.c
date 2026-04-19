#include "waves.h"
#include "display.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

int16_t wave_clamp_y(int16_t y) {
    if (y < 0) return 0;
    if (y > PLOT_HEIGHT - 1) return PLOT_HEIGHT - 1;
    return y;
}

void wave_compute(int16_t *dst, float frequency, float amplitude,
                  int center_y, int width) {
    for (int x = 0; x < width; x++) {
        float val = center_y + amplitude * sinf(2.0f * (float)M_PI * frequency * x / (float)width);
        int16_t y = (int16_t)roundf(val);
        dst[x] = wave_clamp_y(y);
    }
}

void wave_draw_target(const int16_t *y_coords) {
    for (int x = 0; x < WAVE_WIDTH; x++) {
        display_draw_pixel(x, y_coords[x], COLOR_LGRAY);
    }
}

void wave_draw_player(const int16_t *old_y, const int16_t *new_y) {
    for (int x = 0; x < WAVE_WIDTH; x++) {
        display_draw_pixel(x, old_y[x], COLOR_BLACK);
        display_draw_pixel(x, new_y[x], COLOR_GREEN);
    }
}
