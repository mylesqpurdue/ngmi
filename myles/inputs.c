#include "inputs.h"
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"

// Smoothed state — initialized to minimum values
static float smoothed_freq = FREQ_MIN;
static float smoothed_amp = AMP_MIN;

// Last raw ADC readings (for debug display)
static uint16_t last_raw_freq = 0;
static uint16_t last_raw_amp = 0;

void inputs_init(void) {
    adc_init();
    adc_gpio_init(POT_FREQ_PIN);  // GP41 = ADC1
    adc_gpio_init(POT_AMP_PIN);   // GP42 = ADC2
}

float map_freq(uint16_t raw) {
    return FREQ_MIN + (raw / ADC_MAX) * (FREQ_MAX - FREQ_MIN);
}

float map_amp(uint16_t raw) {
    return AMP_MIN + (raw / ADC_MAX) * (AMP_MAX - AMP_MIN);
}

float apply_smoothing(float previous, float new_sample, float alpha) {
    return (1.0f - alpha) * previous + alpha * new_sample;
}

void inputs_update(void) {
    // Read frequency pot (GP29 = ADC channel 3)
    // Multiple throwaway reads + delay to fully settle the S&H capacitor
    adc_select_input(POT_FREQ_CH);
    sleep_us(50);
    (void)adc_read();
    (void)adc_read();
    (void)adc_read();
    last_raw_freq = adc_read();

    // Read amplitude pot (GP42 = ADC channel 2)
    adc_select_input(POT_AMP_CH);
    sleep_us(50);
    (void)adc_read();
    (void)adc_read();
    (void)adc_read();
    last_raw_amp = adc_read();

    float mapped_freq = map_freq(last_raw_freq);
    float mapped_amp  = map_amp(last_raw_amp);

    smoothed_freq = apply_smoothing(smoothed_freq, mapped_freq, SMOOTH_ALPHA);
    smoothed_amp  = apply_smoothing(smoothed_amp,  mapped_amp,  SMOOTH_ALPHA);
}

float inputs_get_freq(void) {
    return smoothed_freq;
}

float inputs_get_amp(void) {
    return smoothed_amp;
}

uint16_t inputs_get_raw_freq(void) {
    return last_raw_freq;
}

uint16_t inputs_get_raw_amp(void) {
    return last_raw_amp;
}
