#ifndef INPUTS_H
#define INPUTS_H

#include <stdint.h>

// Pin assignments (Proton board ADC mapping)
#define POT_FREQ_PIN 41 // ADC1
#define POT_AMP_PIN  42 // ADC2
#define POT_FREQ_CH  1
#define POT_AMP_CH   2

// Mapping ranges
#define FREQ_MIN  1.0f
#define FREQ_MAX  10.0f
#define AMP_MIN   10.0f
#define AMP_MAX   60.0f
#define ADC_MAX   4095.0f

// Smoothing coefficient
#define SMOOTH_ALPHA  0.2f

// Initialization
void inputs_init(void);

// Read current smoothed values (call each game loop iteration)
void inputs_update(void);

// Getters
float inputs_get_freq(void);
float inputs_get_amp(void);
uint16_t inputs_get_raw_freq(void);
uint16_t inputs_get_raw_amp(void);

// Pure mapping functions 
float map_freq(uint16_t raw);
float map_amp(uint16_t raw);
float apply_smoothing(float previous, float new_sample, float alpha);

#endif // INPUTS_H