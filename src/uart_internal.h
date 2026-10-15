#ifndef UART_INTERNAL_H
#define UART_INTERNAL_H

// Called by the shared interrupt handler on each Timer2 period match.
void uart_timer_isr(void);

#endif
