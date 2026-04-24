// peer_link.c -> RX-only binary UART link from the mk77 slave board.
// Each incoming byte is a complete event. 
// See peer_link.h for the table

#include "peer_link.h"

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"

void peer_link_init(void) {
    uart_init(PEER_UART_ID, PEER_BAUD_RATE);
    gpio_set_function(PEER_UART_RX_PIN, GPIO_FUNC_UART);
    uart_set_format(PEER_UART_ID, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(PEER_UART_ID, true);
    printf("[PEER] UART1 RX on GP%d @ %d baud (binary)\n",
           PEER_UART_RX_PIN, PEER_BAUD_RATE);
}

static const char *module_name(uint8_t mod_id) {
    switch (mod_id) {
        case MOD_KRISH: return "KRISH";
        case MOD_MYLES: return "MYLES";
        case MOD_MORSE: return "MORSE";
        default: return "UNKNOWN";
    }
}

bool peer_link_poll(peer_event_t *out) {
    if (!uart_is_readable(PEER_UART_ID)) return false;

    uint8_t op = (uint8_t)uart_getc(PEER_UART_ID);
    uint8_t evt = op & EVT_MASK;
    uint8_t mod = op & MOD_MASK;

    switch (evt) {
        case EVT_STRIKE: out->type = PEER_EVT_STRIKE; break;
        case EVT_SOLVED: out->type = PEER_EVT_SOLVED; break;
        default:
            printf("[PEER] unknown opcode 0x%02X\n", op);
            return false;
    }

    strncpy(out->payload, module_name(mod), sizeof(out->payload) - 1);
    out->payload[sizeof(out->payload) - 1] = '\0';
    return true;
}