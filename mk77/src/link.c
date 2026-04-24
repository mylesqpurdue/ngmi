// link.c -> TX-only UART link to the master (Celine/Alexia) board.
// Master/slave architecture: this board is a dumb event emitter. It never
// reads from UART1.

#include "link.h"

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"

void link_init(void) {
    uart_init(LINK_UART_ID, LINK_BAUD_RATE);
    gpio_set_function(LINK_UART_TX_PIN, GPIO_FUNC_UART);
    uart_set_format(LINK_UART_ID, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(LINK_UART_ID, true);
    printf("[LINK] UART1 TX-only on GP%d @ %d baud\n", LINK_UART_TX_PIN, LINK_BAUD_RATE);
}

void link_send_raw(const char *line) {
    uart_puts(LINK_UART_ID, line);
}

void link_send_strike(const char *who) {
    char buf[32];
    if (who && who[0]) snprintf(buf, sizeof(buf), "$STRIKE:%s\n", who);
    else               snprintf(buf, sizeof(buf), "$STRIKE\n");
    uart_puts(LINK_UART_ID, buf);
}

void link_send_solved(const char *module) {
    char buf[32];
    snprintf(buf, sizeof(buf), "$SOLVED:%s\n", module ? module : "UNKNOWN");
    uart_puts(LINK_UART_ID, buf);
}