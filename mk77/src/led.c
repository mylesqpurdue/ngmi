#include "led.h"
#include "pico/stdlib.h"
#include "hardware/pwm.h"

// Common-anode RGB LED: PWM level is INVERTED
// 255 = off (full voltage, no current through LED)
// 0 = full brightness (ground path, max current)
#define LED_OFF 255

// Brightness cap (0-255). Lower = dimmer. 60 is visible without being harsh.
#define BRIGHTNESS 30

void led_init(void) {
    gpio_set_function(LED_R_PIN, GPIO_FUNC_PWM);
    gpio_set_function(LED_G_PIN, GPIO_FUNC_PWM);
    gpio_set_function(LED_B_PIN, GPIO_FUNC_PWM);

    uint slice_r = pwm_gpio_to_slice_num(LED_R_PIN);
    uint slice_g = pwm_gpio_to_slice_num(LED_G_PIN);
    uint slice_b = pwm_gpio_to_slice_num(LED_B_PIN);

    pwm_set_wrap(slice_r, 255);
    pwm_set_wrap(slice_g, 255);
    pwm_set_wrap(slice_b, 255);

    pwm_set_enabled(slice_r, true);
    pwm_set_enabled(slice_g, true);
    pwm_set_enabled(slice_b, true);

    // Start with LED off (common-anode: 255 = off)
    pwm_set_gpio_level(LED_R_PIN, LED_OFF);
    pwm_set_gpio_level(LED_G_PIN, LED_OFF);
    pwm_set_gpio_level(LED_B_PIN, LED_OFF);
}

void led_set_color(uint8_t r, uint8_t g, uint8_t b) {
    // Scale down to BRIGHTNESS cap, then invert for common-anode
    uint8_t sr = (uint8_t)((r * BRIGHTNESS) / 255);
    uint8_t sg = (uint8_t)((g * BRIGHTNESS) / 255);
    uint8_t sb = (uint8_t)((b * BRIGHTNESS) / 255);
    pwm_set_gpio_level(LED_R_PIN, 255 - sr);
    pwm_set_gpio_level(LED_G_PIN, 255 - sg);
    pwm_set_gpio_level(LED_B_PIN, 255 - sb);
}

void led_set_idle(void) {
    led_set_color(0, 0, 255);
}

void led_set_playing(void) {
    led_set_color(0, 0, 0);
}

void led_set_locking(float progress) {
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;

    uint8_t r = (uint8_t)(255.0f * (1.0f - progress) + 0.5f);
    led_set_color(r, 255, 0);
}

void led_set_win(void) {
    led_set_color(0, 255, 0);
}

void led_flash_white(void) {
    led_set_color(255, 255, 255);
    sleep_ms(100);
    led_set_color(0, 0, 0);
}