#ifndef BUTTON_H
#define BUTTON_H

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"

// GPIO Pins
#define RED_PIN    37
#define GREEN_PIN  38
#define BLUE_PIN   39
#define BUTTON_PIN 34
#define RESET_PIN  17
#define CORRECT_LED 33

// LED Control Constants
#define BRIGHTNESS 20
#define OFF_LEVEL 255
#define BLINK_LEVEL (255 - BRIGHTNESS + 30)
#define ON_LEVEL (255 - BRIGHTNESS)
#define BLINK_INTERVAL_MS 200

// External variables (defined in main.c)
extern struct repeating_timer blink_timer;
extern volatile bool is_blinking;
extern volatile bool blink_on;
extern volatile bool current_r;
extern volatile bool current_g;
extern volatile bool current_b;
extern volatile bool module_complete;
extern volatile int countdown_secs;
extern int parity;
extern volatile int strike_count;

// Function declarations
void rgb_init(void);
void update_led_hw(void);
void apply_led_state(bool force_on);
bool led_blink_callback(struct repeating_timer *t);
int get_required_digit(void);
bool time_contains_digit(int digit);
void button_init(void);
void irq_callback(uint gpio, uint32_t events);
void reset_isr(void);

#endif // BUTTON_H
