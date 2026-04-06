#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "timer.h"

#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/timer.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"
#include "hardware/spi.h"
#include "hardware/uart.h"
#include "pico/rand.h"


#define RESET_PIN 17
#define BUTTON_PIN 34

#define RED_PIN   37
#define GREEN_PIN 38
#define BLUE_PIN  39
#define BRIGHTNESS 50 
#define OFF_LEVEL 255
#define BLINK_LEVEL (255 - BRIGHTNESS + 30)
#define ON_LEVEL (255 - BRIGHTNESS)
#define ALARM_NUM 0

#define BLINK_INTERVAL_MS 200

void init_sevenseg_spi();
void init_sevenseg_dma();
void sevenseg_display(const char* str);

struct repeating_timer blink_timer;
volatile bool is_blinking = false;
volatile bool blink_on = true;
volatile bool current_r = false, current_g = false, current_b = false;


extern char font[];


//LED
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

void button_isr(uint gpio, uint32_t events){

}
//button press
void reset_isr(uint gpio, uint32_t events) {

    if(gpio == BUTTON_PIN){
        int mins = countdown_secs / 60;
        int secs = countdown_secs % 60;
        if (gpio == BUTTON_PIN && ((mins == 5) || (secs % 10 == 5) || (secs / 10 == 5))) {
            gpio_put(33, 1);
        }
    }
    
    if (gpio == RESET_PIN) {
        countdown_secs = 300;
        timer_active = true;
        update_display = true;

        // Stop any existing blink timer
        cancel_repeating_timer(&blink_timer);

        // 1. Pick a random color
        uint32_t button_type = get_rand_32() % 10;
        current_r = (button_type % 5 == 0 || button_type % 5 == 3 || button_type % 5 == 4);
        current_g = (button_type % 5 == 1 || button_type % 5 == 3 || button_type % 5 == 4);
        current_b = (button_type % 5 == 2 || button_type % 5 == 4);

        // 2. Pick a random mode
        is_blinking = button_type / 5;

        if (is_blinking) {
            blink_on = true;
            apply_led_state(true);
            // Start the separate LED timer
            add_repeating_timer_ms(BLINK_INTERVAL_MS, led_blink_callback, NULL, &blink_timer);
        } else {
            // Solid mode: just turn it on and leave it
            apply_led_state(true);
        }
    }

}

void reset_init(){
    gpio_init(RESET_PIN);
    gpio_set_dir(RESET_PIN, GPIO_IN);
    gpio_disable_pulls(RESET_PIN);
    gpio_set_irq_enabled_with_callback(RESET_PIN, GPIO_IRQ_EDGE_FALL, true, &reset_isr);
}

void button_init(){
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_disable_pulls(BUTTON_PIN);
    gpio_set_irq_enabled_with_callback(BUTTON_PIN, GPIO_IRQ_EDGE_FALL, true, &reset_isr);

    gpio_init(33);
    gpio_set_dir(33, GPIO_OUT);
    gpio_put(33, 0);
}

//main
int main() {
    stdio_init_all();

    init_sevenseg_spi();
    init_sevenseg_dma();
    reset_init();
    button_init();
    rgb_init();

    init_hardware_timer(); 

    char display_buffer[9]; 

    for(;;) {
        if (update_display) {
            int mins = countdown_secs / 60;
            int secs = countdown_secs % 60;

            snprintf(display_buffer, sizeof(display_buffer), "  %02d-%02d ", mins, secs);
            sevenseg_display(display_buffer);
            update_display = false;
        }
        sleep_ms(10);
        
    }

    return 0;

}
