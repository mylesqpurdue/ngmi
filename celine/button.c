#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"
#include "hardware/timer.h"
#include "timer.h"
#include "button.h"

void rgb_init() {
    // 1. Tell the pins to use the PWM hardware
    gpio_set_function(RED_PIN, GPIO_FUNC_PWM);
    gpio_set_function(GREEN_PIN, GPIO_FUNC_PWM);
    gpio_set_function(BLUE_PIN, GPIO_FUNC_PWM);

    // 2. Fetch slices for all pins individually (safest for RP2350 mapping)
    uint slice_r = pwm_gpio_to_slice_num(RED_PIN); 
    uint slice_g = pwm_gpio_to_slice_num(GREEN_PIN);
    uint slice_b = pwm_gpio_to_slice_num(BLUE_PIN);

    // 3. Set the "Wrap" value to 255
    pwm_set_wrap(slice_r, 255);
    pwm_set_wrap(slice_g, 255);
    pwm_set_wrap(slice_b, 255);

    // 4. Enable the PWM slices
    pwm_set_enabled(slice_g, true);
    pwm_set_enabled(slice_b, true);
    pwm_set_enabled(slice_r, true);

    // 5. Start with everything OFF (255 for Common Anode)
    pwm_set_gpio_level(RED_PIN, OFF_LEVEL);
    pwm_set_gpio_level(GREEN_PIN, OFF_LEVEL);
    pwm_set_gpio_level(BLUE_PIN, OFF_LEVEL);
}

void update_led_hw() {

    // If we are in blinking mode and the blink cycle is "OFF", kill the light
    if (is_blinking && !blink_on) {
        pwm_set_gpio_level(RED_PIN, OFF_LEVEL);
        pwm_set_gpio_level(GREEN_PIN, OFF_LEVEL);
        pwm_set_gpio_level(BLUE_PIN, OFF_LEVEL);
    } else {
        // Otherwise, set the pins to the chosen color at our dim level
        pwm_set_gpio_level(RED_PIN,   current_r ? ON_LEVEL : OFF_LEVEL);
        pwm_set_gpio_level(GREEN_PIN, current_g ? ON_LEVEL : OFF_LEVEL);
        pwm_set_gpio_level(BLUE_PIN,  current_b ? ON_LEVEL : OFF_LEVEL);
    }
}

void apply_led_state(bool force_on) {
    if (force_on) {
        pwm_set_gpio_level(RED_PIN,   current_r ? ON_LEVEL : OFF_LEVEL);
        pwm_set_gpio_level(GREEN_PIN, current_g ? ON_LEVEL : OFF_LEVEL);
        pwm_set_gpio_level(BLUE_PIN,  current_b ? ON_LEVEL : OFF_LEVEL);
    } else {
        pwm_set_gpio_level(RED_PIN, current_r ? BLINK_LEVEL : OFF_LEVEL);
        pwm_set_gpio_level(GREEN_PIN, current_g ? BLINK_LEVEL : OFF_LEVEL);
        pwm_set_gpio_level(BLUE_PIN, current_b ? BLINK_LEVEL : OFF_LEVEL);
    }
}

bool led_blink_callback(struct repeating_timer *t) {
    if (is_blinking) {
        blink_on = !blink_on;
        apply_led_state(blink_on);
    }
    return true; // Keep the timer running
}

int get_required_digit() {
    // Check light colors (accounting for common anode RGB)
    bool is_red = current_r && !current_g && !current_b;
    bool is_green = !current_r && current_g && !current_b;
    bool is_blue = !current_r && !current_g && current_b;
    bool is_yellow = current_r && current_g && !current_b;
    bool is_white = current_r && current_g && current_b;
    
    // Green and even parity → digit 5
    if (is_green && parity == 0) {
        return 5;
    }
    
    // Blue and blinking → digit 2
    if (is_blue && is_blinking) {
        return 2;
    }
    
    // Red and one strike → digit 3
    if (is_red && strike_count == 1) {
        return 3;
    }
    
    // Yellow and odd parity → digit 8
    if (is_yellow && parity == 1) {
        return 8;
    }
    
    // Default → digit 4
    return 4;
}

bool time_contains_digit(int digit) {
    int mins = countdown_secs / 60;
    int secs = countdown_secs % 60;
    return (mins == digit) || (secs % 10 == digit) || (secs / 10 == digit);
}

void button_isr(){
    // If module is already complete, don't process button presses
    if (module_complete) {
        return;
    }
    
    int required_digit = get_required_digit();
    int mins = countdown_secs / 60;
    int secs = countdown_secs % 60;
    
    // Check if the countdown contains the required digit
    if (time_contains_digit(required_digit)) {
        // Correct press!
        gpio_put(CORRECT_LED, 1);
        // Turn off all RGB LEDs
        current_r = 0;
        current_g = 0;
        current_b = 0;
        apply_led_state(true);
        module_complete = true;
        printf("Module complete!\n");
    }
    else {
        // Incorrect press - give strike
        strike_count++;
        printf("Strike %d! Required digit: %d, Time was %d:%02d\n", strike_count, required_digit, mins, secs);
    }
}

void button_init(){
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_disable_pulls(BUTTON_PIN);
    gpio_set_irq_enabled_with_callback(BUTTON_PIN, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &irq_callback);

    gpio_init(CORRECT_LED);
    gpio_set_dir(CORRECT_LED, GPIO_OUT);
    gpio_put(CORRECT_LED, 0);
}
