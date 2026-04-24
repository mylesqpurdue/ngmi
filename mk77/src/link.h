#ifndef LINK_H
#define LINK_H

// TX-only UART link to the Celine/Alexia master board.
// We fire events at the master and don't listen for replies - the master
// owns all bomb-wide state (strike total, defused/exploded, timer).

#define LINK_UART_ID  uart1
#define LINK_UART_TX_PIN 8
#define LINK_BAUD_RATE 115200

void link_init(void);

// Blocking sends (short lines, a few ms worst case at 115200 baud).
void link_send_strike(const char *who); // "$STRIKE:<who>\n"
void link_send_solved(const char *module); // "$SOLVED:<module>\n"
void link_send_raw(const char *line); // ad-hoc debug line

#endif // LINK_H