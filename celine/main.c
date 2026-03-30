#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"

#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/timer.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"
#include "hardware/spi.h"
#include "hardware/uart.h"
#include "pico/rand.h"


#define BUTTON_PIN 17

#define ALARM_NUM 0
void init_sevenseg_spi();
void init_sevenseg_dma();
void sevenseg_display(const char* str);

extern char font[];

volatile int countdown_secs = 300; 
volatile bool timer_active = false;
volatile bool update_display = true;   

//button press
void button_isr(uint gpio, uint32_t events) {
    if (gpio == BUTTON_PIN) {
        countdown_secs = 300;  
        timer_active = true;    
        update_display = true;  
    }
}

void button_init(){
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_disable_pulls(BUTTON_PIN);
    gpio_set_irq_enabled_with_callback(BUTTON_PIN, GPIO_IRQ_EDGE_FALL, true, &button_isr);
}


//timer
void alarm_irq_handler(void) {
    hw_clear_bits(&timer_hw->intr, 1u << ALARM_NUM);

    if (timer_active && countdown_secs > 0) {
        countdown_secs--;
        update_display = true;
    } else if (countdown_secs <= 0) {
        timer_active = false;
        update_display = true;
    }

    // Reschedule for 1 second later
    uint64_t target = timer_hw->timerawl + 1000000;
    hardware_alarm_set_target(ALARM_NUM, target);
}

void init_hardware_timer() {
    hardware_alarm_set_callback(ALARM_NUM, &alarm_irq_handler);
    uint64_t target = timer_hw->timerawl + 1000000;
    hardware_alarm_set_target(ALARM_NUM, target);
}

//main
int main() {
    stdio_init_all();

    init_sevenseg_spi();
    init_sevenseg_dma();
    button_init();
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