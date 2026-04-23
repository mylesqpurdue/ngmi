#ifndef TIMER_H
#define TIMER_H

#include "pico/stdlib.h"

extern volatile int countdown_secs;
extern volatile bool timer_active;
extern volatile bool update_display;

void init_hardware_timer();
void alarm_irq_handler();

#endif
