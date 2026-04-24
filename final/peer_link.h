#ifndef PEER_LINK_H
#define PEER_LINK_H

#include <stdint.h>
#include <stdbool.h>

// RX-only UART link from the mk77 slave board.
// Binary protocol: 1 byte per event.
// Wire: mk77 GP8 (TX) -> this board GP25 (RX). Common GND.

#define PEER_UART_ID uart1
#define PEER_UART_RX_PIN 25
#define PEER_BAUD_RATE 115200

// PROTOCOL -> KEEP IN SYNC WITH mk77/src/link.h

// Event types (upper 3 bits)
#define EVT_STRIKE (0x00 << 5)
#define EVT_SOLVED (0x01 << 5)

// Module IDs (lower 5 bits)
#define MOD_KRISH (0x00)
#define MOD_MYLES (0x01)
#define MOD_MORSE (0x02)

#define EVT_MASK (0xE0)
#define MOD_MASK (0x1F)

typedef enum {
    PEER_EVT_NONE = 0,
    PEER_EVT_STRIKE,
    PEER_EVT_SOLVED,
} peer_event_type_t;

typedef struct {
    peer_event_type_t type;
    char payload[16]; // "KRISH" | "MYLES" | "MORSE"
} peer_event_t;

void peer_link_init(void);

// Non-blocking. Reads one opcode byte from the UART FIFO if available,
// decodes it, and fills *out. Returns true when an event is ready.
bool peer_link_poll(peer_event_t *out);

#endif // PEER_LINK_H