// link.c -> TX-only binary UART link to the master board.
// Sends a single opcode byte per event. See link.h for the protocol table.

#include "link.h"

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"

void link_init(void) {
    uart_init(LINK_UART_ID, LINK_BAUD_RATE);
    gpio_set_function(LINK_UART_TX_PIN, GPIO_FUNC_UART);
    uart_set_format(LINK_UART_ID, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(LINK_UART_ID, true);
    printf("[LINK] UART1 TX-only on GP%d @ %d baud (binary)\n",
           LINK_UART_TX_PIN, LINK_BAUD_RATE);
}

void link_send_op(uint8_t opcode) {
    uart_putc_raw(LINK_UART_ID, (char)opcode);
}

// Map module name -> module ID. Returns 0xFF if unknown.
static uint8_t module_id_from_name(const char *name) {
    if (!name) return 0xFF;
    if (strcmp(name, "KRISH") == 0) return MOD_KRISH;
    if (strcmp(name, "MYLES") == 0) return MOD_MYLES;
    if (strcmp(name, "MORSE") == 0) return MOD_MORSE;
    return 0xFF;
}

void link_send_strike(const char *who) {
    uint8_t mod = module_id_from_name(who);
    if (mod == 0xFF) {
        printf("[LINK] ERROR: unknown module '%s' for strike\n", who ? who : "?");
        return;
    }
    link_send_op(EVT_STRIKE | mod);
}

void link_send_solved(const char *module) {
    uint8_t mod = module_id_from_name(module);
    if (mod == 0xFF) {
        printf("[LINK] ERROR: unknown module '%s' for solved\n", module ? module : "?");
        return;
    }
    link_send_op(EVT_SOLVED | mod);
}