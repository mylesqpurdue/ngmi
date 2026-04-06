#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"

#define ALARM_NUM 0

volatile int countdown_secs = 300; //300 = 5 mins
volatile bool timer_active = false;
volatile bool update_display = true;   

void alarm_irq_handler() {
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
