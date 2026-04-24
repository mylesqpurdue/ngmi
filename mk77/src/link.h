#ifndef LINK_H
#define LINK_H

#include <stdint.h>

// TX-only UART link to the Celine/Alexia master board.
// Binary protocol: 1 byte per event. No framing, no checksum.
// Upper 3 bits = event type. Lower 5 bits = module ID.

#define LINK_UART_ID uart1
#define LINK_UART_TX_PIN 24
#define LINK_BAUD_RATE 115200

// PROTOCOL -> KEEP IN SYNC WITH final/peer_link.h

// Event types (upper 3 bits)
#define EVT_STRIKE (0x00 << 5)
#define EVT_SOLVED (0x01 << 5)

// Module IDs (lower 5 bits)
#define MOD_KRISH 0x00
#define MOD_MYLES 0x01
#define MOD_MORSE 0x02 // reserved for when morse board is wired in

// Combined opcodes
#define OP_STRIKE_KRISH (EVT_STRIKE | MOD_KRISH) // 0x00
#define OP_STRIKE_MYLES (EVT_STRIKE | MOD_MYLES) // 0x01
#define OP_STRIKE_MORSE (EVT_STRIKE | MOD_MORSE) // 0x02
#define OP_SOLVED_KRISH (EVT_SOLVED | MOD_KRISH) // 0x20
#define OP_SOLVED_MYLES (EVT_SOLVED | MOD_MYLES) // 0x21
#define OP_SOLVED_MORSE (EVT_SOLVED | MOD_MORSE) // 0x22

// API

void link_init(void);

// Sends 1 byte. Takes ~87us at 115200 baud.
void link_send_strike(const char *who); // who = "KRISH" | "MYLES" | "MORSE"
void link_send_solved(const char *module); // module = "KRISH" | "MYLES" | "MORSE"

// Raw 1-byte send (skip the string lookup)
void link_send_op(uint8_t opcode);

#endif // LINK_H