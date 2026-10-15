#include "check.h"
#include <stdint.h>
#include <xc.h>
#include "../src/uart.h"
#include "../src/uart_internal.h"

struct INTCONbits_t INTCONbits;
struct LATDbits_t LATDbits;
struct PORTDbits_t PORTDbits;
struct TRISDbits_t TRISDbits;
struct ODCONDbits_t ODCONDbits;
struct PIE4bits_t PIE4bits;
struct PIR4bits_t PIR4bits;
struct T2CONbits_t T2CONbits;
uint8_t ANSELD, TRISD, T2CON, T2CLKCON, T2HLT, T2RST, T2TMR, T2PR;

static void ticks(uint8_t n) {
   while(n--) uart_timer_isr();
}

static void receive(uint8_t c, uint8_t stop) {
   uint8_t i;

   PORTDbits.RD2 = 0;
   ticks(1);                         // detect start
   ticks(4);                         // confirm at its midpoint
   for(i = 0; i < 8; i++) {
      PORTDbits.RD2 = (c >> i) & 1u;
      ticks(8);
   }
   PORTDbits.RD2 = stop;
   ticks(8);                         // sample stop bit
}

int main(void) {
   uint8_t i;
   char tx_buffer[64];
   uart_command_result_t command;

   for(i = 0; i < 63; i++) tx_buffer[i] = (char)('a' + i % 26);
   tx_buffer[63] = 0;

   PORTDbits.RD2 = 1;
   INTCONbits.GIE = 1;
   uart_init();
   CHECK_EQ(T2CLKCON, 1);           // Fosc/4 is Timer2 clock source CS=1
   CHECK_EQ(T2PR, 138);             // 139 Timer2 periods at 2400 baud
   CHECK_EQ(T2CON, 0x82);           // ON, prescale 1:1, postscale 1:3
   CHECK(T2CON & 0x80);              // Timer2 started
   CHECK(PIE4bits.TMR2IE);           // period-match interrupt enabled

   receive('B', 1);
   CHECK_EQ(uart_command_poll().command, UART_COMMAND_NONE);
   receive('Y', 1);
   CHECK_EQ(uart_command_poll().command, UART_COMMAND_NONE);
   receive('+', 1);
   CHECK_EQ(uart_command_poll().command, UART_COMMAND_NONE);
   receive(';', 1);
   command = uart_command_poll();
   CHECK_EQ(command.command, UART_COMMAND_BYPASS_ON);
   CHECK_EQ(command.argument, 0);

   receive('B', 1);
   receive('Y', 1);
   receive('+', 1);
   receive('X', 1);
   receive(';', 1);
   CHECK_EQ(uart_command_poll().command, UART_COMMAND_CODE('B', 'Y', '+', 'X'));

   receive(0, 1);
   receive('B', 1);
   receive('Y', 1);
   receive('+', 1);
   receive(';', 1);
   CHECK_EQ(uart_command_poll().command, UART_COMMAND_CODE(0, 'B', 'Y', '+'));

   receive('x', 1);
   receive(';', 1);
   receive('B', 1);
   receive('Y', 1);
   receive('-', 1);
   receive(';', 1);
   CHECK_EQ(uart_command_poll().command, UART_COMMAND_CODE('X', 0, 0, 0));
   CHECK_EQ(uart_command_poll().command, UART_COMMAND_BYPASS_OFF);
   CHECK_EQ(uart_command_poll().command, UART_COMMAND_NONE);

   receive('B', 1);
   receive('Y', 1);
   receive('-', 1);
   CHECK_EQ(uart_command_poll().command, UART_COMMAND_NONE);
   receive(';', 1);
   CHECK_EQ(uart_command_poll().command, UART_COMMAND_BYPASS_OFF);

   receive('L', 1);
   receive('+', 1);
   receive(';', 1);
   CHECK_EQ(uart_command_poll().command, UART_COMMAND_CODE('L', '+', 0, 0));
   receive('L', 1);
   receive('-', 1);
   receive(';', 1);
   CHECK_EQ(uart_command_poll().command, UART_COMMAND_CODE('L', '-', 0, 0));
   receive('C', 1);
   receive('+', 1);
   receive(';', 1);
   CHECK_EQ(uart_command_poll().command, UART_COMMAND_CODE('C', '+', 0, 0));
   receive('C', 1);
   receive('-', 1);
   receive(';', 1);
   CHECK_EQ(uart_command_poll().command, UART_COMMAND_CODE('C', '-', 0, 0));

   receive('L', 1);
   receive('=', 1);
   receive('1', 1);
   receive('2', 1);
   receive('7', 1);
   receive(';', 1);
   command = uart_command_poll();
   CHECK_EQ(command.command, UART_COMMAND_L_SET);
   CHECK_EQ(command.argument, 127);
   receive('c', 1);
   receive('=', 1);
   receive('0', 1);
   receive(';', 1);
   command = uart_command_poll();
   CHECK_EQ(command.command, UART_COMMAND_C_SET);
   CHECK_EQ(command.argument, 0);
   receive('l', 1);
   receive('?', 1);
   receive(';', 1);
   CHECK_EQ(uart_command_poll().command, UART_COMMAND_L_QUERY);
   receive('C', 1);
   receive('?', 1);
   receive(';', 1);
   CHECK_EQ(uart_command_poll().command, UART_COMMAND_C_QUERY);
   receive('b', 1);
   receive('y', 1);
   receive('?', 1);
   receive(';', 1);
   CHECK_EQ(uart_command_poll().command, UART_COMMAND_BYPASS_QUERY);
   receive('a', 1);
   receive('u', 1);
   receive('+', 1);
   receive(';', 1);
   CHECK_EQ(uart_command_poll().command, UART_COMMAND_AUTO_ON);
   receive('A', 1);
   receive('U', 1);
   receive('-', 1);
   receive(';', 1);
   CHECK_EQ(uart_command_poll().command, UART_COMMAND_AUTO_OFF);
   receive('a', 1);
   receive('u', 1);
   receive('?', 1);
   receive(';', 1);
   CHECK_EQ(uart_command_poll().command, UART_COMMAND_AUTO_QUERY);
   receive('l', 1);
   receive('c', 1);
   receive(';', 1);
   CHECK_EQ(uart_command_poll().command, UART_COMMAND_CAP_LOAD);
   receive('C', 1);
   receive('L', 1);
   receive(';', 1);
   CHECK_EQ(uart_command_poll().command, UART_COMMAND_CAP_SOURCE);
   receive('l', 1);
   receive('c', 1);
   receive('?', 1);
   receive(';', 1);
   CHECK_EQ(uart_command_poll().command, UART_COMMAND_CAP_POSITION_QUERY);
   receive('s', 1);
   receive('w', 1);
   receive('r', 1);
   receive('?', 1);
   receive(';', 1);
   CHECK_EQ(uart_command_poll().command, UART_COMMAND_SWR_QUERY);
   receive('L', 1);
   receive('S', 1);
   receive('W', 1);
   receive('?', 1);
   receive(';', 1);
   CHECK_EQ(uart_command_poll().command, UART_COMMAND_LAST_SWR_QUERY);
   receive('p', 1);
   receive('k', 1);
   receive('?', 1);
   receive(';', 1);
   CHECK_EQ(uart_command_poll().command, UART_COMMAND_PEAK_POWER_QUERY);
   receive('P', 1);
   receive('W', 1);
   receive('R', 1);
   receive('?', 1);
   receive(';', 1);
   CHECK_EQ(uart_command_poll().command, UART_COMMAND_POWER_QUERY);
   receive('L', 1);
   receive('?', 1);
   receive('=', 1);
   receive('1', 1);
   receive(';', 1);
   CHECK_EQ(uart_command_poll().command, UART_COMMAND_L_QUERY); // trailing values are ignored on queries
   receive('l', 1);
   receive(':', 1);
   receive('1', 1);
   receive(';', 1);
   CHECK_EQ(uart_command_poll().command, UART_COMMAND_CODE('L', ':', '1', 0));
   receive('l', 1);
   receive('=', 1);
   receive('1', 1);
   receive(';', 1);
   command = uart_command_poll();
   CHECK_EQ(command.command, UART_COMMAND_L_SET); // command names are case-insensitive
   CHECK_EQ(command.argument, 1);
   receive('L', 1);
   receive(';', 1);
   command = uart_command_poll();
   CHECK_EQ(command.command, UART_COMMAND_L_SET); // omitted set value defaults to zero
   CHECK_EQ(command.argument, 0);
   receive('C', 1);
   receive('=', 1);
   receive('1', 1);
   receive('2', 1);
   receive('8', 1);
   receive(';', 1);
   CHECK_EQ(uart_command_poll().command, UART_COMMAND_NONE); // relay mask is limited to 127

   receive('B', 1);
   receive('Y', 1);
   receive('x', 0);
   CHECK_EQ(uart_command_poll().command, UART_COMMAND_NONE); // framing error discards partial command
   PORTDbits.RD2 = 1;
   ticks(8);                         // let the receiver re-arm after the framing error
   receive('B', 1);
   receive('Y', 1);
   receive('+', 1);
   receive(';', 1);
   command = uart_command_poll();
   CHECK_EQ(command.command, UART_COMMAND_BYPASS_ON);
   CHECK_EQ(command.argument, 0);

   receive('B', 1);
   receive('Y', 1);
   receive('+', 1);
   receive('=', 1);
   receive('1', 1);
   receive('2', 1);
   receive('3', 1);
   receive(';', 1);
   command = uart_command_poll();
   CHECK_EQ(command.command, UART_COMMAND_BYPASS_ON);
   CHECK_EQ(command.argument, 123);

   receive('B', 1);
   receive('Y', 1);
   receive('T', 1);
   receive('=', 1);
   receive('-', 1);
   receive('4', 1);
   receive('2', 1);
   receive(';', 1);
   command = uart_command_poll();
   CHECK_EQ(command.command, UART_COMMAND_NONE);
   CHECK_EQ(command.argument, 0); // negative values are rejected

   receive('B', 1);
   receive('Y', 1);
   receive('-', 1);
   receive('=', 1);
   receive('2', 1);
   receive('5', 1);
   receive('5', 1);
   receive(';', 1);
   command = uart_command_poll();
   CHECK_EQ(command.command, UART_COMMAND_NONE);
   CHECK_EQ(command.argument, 0);

   receive('B', 1);
   receive('Y', 1);
   receive('-', 1);
   receive('=', 1);
   receive('2', 1);
   receive('5', 1);
   receive('6', 1);
   receive(';', 1);
   command = uart_command_poll();
   CHECK_EQ(command.command, UART_COMMAND_NONE);
   CHECK_EQ(command.argument, 0); // out-of-range arguments are rejected

   receive('B', 1);
   receive('Y', 1);
   receive('+', 1);
   receive('=', 1);
   receive('1', 1);
   receive('x', 1);
   receive(';', 1);
   CHECK_EQ(uart_command_poll().command, UART_COMMAND_NONE); // non-decimal argument is rejected

   PORTDbits.RD2 = 1;
   ticks(1);
   for(i = 0; i < 64; i++) {
      if(i) ticks(4);               // complete the prior stop bit
      receive(i, 1);
   }
   CHECK_EQ(uart_command_poll().command, UART_COMMAND_NONE); // overflow discards queued input

   ticks(8);                         // allow the receive-to-transmit guard
   CHECK(uart_tx_string("A"));
   uart_timer_isr();
   CHECK_EQ(LATDbits.LATD1, 0);      // start bit
   CHECK_EQ(ODCONDbits.ODCD1, 0);    // actively driven during frame
   ticks(8);
   CHECK_EQ(LATDbits.LATD1, 1);      // bit 0 of 'A'
   for(i = 1; i < 8; i++) {
      ticks(8);
      CHECK_EQ(LATDbits.LATD1, ('A' >> i) & 1u);
   }
   ticks(8);
   CHECK_EQ(LATDbits.LATD1, 1);      // stop bit
   ticks(8);
   CHECK_EQ(ODCONDbits.ODCD1, 1);    // released between frames

   CHECK(uart_tx_string(tx_buffer));
   uart_suspend();
   CHECK_EQ(PIE4bits.TMR2IE, 0);
   CHECK_EQ(T2CONbits.T2ON, 0);
   CHECK_EQ(ODCONDbits.ODCD1, 1);
   uart_init();                      // resume with a clean software UART
   CHECK(uart_tx_string(""));
   CHECK(!uart_tx_string(0));
   tx_buffer[62] = 0;
   CHECK(uart_tx_string(tx_buffer));
   CHECK(!uart_tx_string("XY"));     // string enqueue must be atomic
   CHECK(uart_tx_string("Z"));
   CHECK(!uart_tx_string("!"));
   return check_done("uart");
}
