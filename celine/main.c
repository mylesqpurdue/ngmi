#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "timer.h"
#include "button.h"

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


#define BRIGHTNESS 50 
#define OFF_LEVEL 255
#define BLINK_LEVEL (255 - BRIGHTNESS + 30)
#define ON_LEVEL (255 - BRIGHTNESS)
#define ALARM_NUM 0
#define CORRECT_LED 33

#define BLINK_INTERVAL_MS 200

void init_sevenseg_spi();
void init_sevenseg_dma();
void sevenseg_display(const char* str);

struct repeating_timer blink_timer;
volatile bool is_blinking = false;
volatile bool blink_on = true;
volatile bool current_r = false, current_g = false, current_b = false;
volatile uint64_t button_press_time = 0;

// Serial code display mode
volatile bool display_serial_code = false;
char serial_code[4] = "AAA"; // 3 characters + null terminator
char letters[9] = {'A', 'C', 'E', 'F', 'H', 'J', 'L', 'P', 'U'};
int parity = 0; // 0 for even, 1 for odd
int strike_count = 0; // Count of strikes
volatile bool module_complete = false; // Module complete flag

extern char font[];

void generate_serial_code() {
    uint32_t rand_val = get_rand_32();
    
    // First letter (A-Z)
    serial_code[0] = letters[rand_val % 9];
    rand_val = get_rand_32();
    
    // Second letter (A-Z)
    serial_code[1] = letters[rand_val % 9];
    rand_val = get_rand_32();
    
    // Number (0-9)
    serial_code[2] = '0' + (rand_val % 10);
    parity = (int)serial_code[2] % 2;
    serial_code[3] = '\0';
}

void irq_callback(uint gpio, uint32_t events) {
    if (gpio == RESET_PIN && (events & GPIO_IRQ_EDGE_FALL)){
        reset_isr();
    }
    if(gpio == BUTTON_PIN && (events & GPIO_IRQ_EDGE_FALL)){
        button_isr();
    }

    busy_wait_ms(500); // Debounce delay
}

void reset_isr() {

    countdown_secs = 300;
    timer_active = true;
    update_display = true;
    generate_serial_code();
    strike_count = 0; // Reset strikes on reset
    module_complete = false; // Reset completion flag
    gpio_put(CORRECT_LED, 0);
    // Stop any existing blink timer
    cancel_repeating_timer(&blink_timer);

    // 1. Pick a random color
    uint32_t button_type = get_rand_32() % 10;
    current_r = (button_type % 5 == 0 || button_type % 5 == 3 || button_type % 5 == 4); //red, green, blue, yellow, white
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

void reset_init(){
    gpio_init(RESET_PIN);
    gpio_set_dir(RESET_PIN, GPIO_IN);
    gpio_disable_pulls(RESET_PIN);
    gpio_set_irq_enabled_with_callback(RESET_PIN, GPIO_IRQ_EDGE_FALL, true, &irq_callback);
}

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

            snprintf(display_buffer, sizeof(display_buffer), "%s%2d-%02d", serial_code, mins, secs);
            sevenseg_display(display_buffer);
            update_display = false;
        }
        sleep_ms(10);
        
    }

    return 0;

}
