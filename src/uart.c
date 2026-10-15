#include "board.h"
#include "uart.h"
#include "uart_internal.h"

#define UART_TX_PIN LATDbits.LATD1
#define UART_RX_PIN PORTDbits.RD2
#define UART_OVERSAMPLE 8u
#define UART_RX_GUARD_TICKS UART_OVERSAMPLE
#define UART_TIMER_POSTSCALER 3u
#define UART_TIMER_PERIOD \
   (((_XTAL_FREQ / 4UL) + \
     ((uint32_t)UART_BAUD * UART_OVERSAMPLE * UART_TIMER_POSTSCALER / 2UL)) / \
    ((uint32_t)UART_BAUD * UART_OVERSAMPLE * UART_TIMER_POSTSCALER))
#define UART_QUEUE_SIZE 64u
#define UART_QUEUE_MASK (UART_QUEUE_SIZE - 1u)
#define UART_RX_ERROR_OVERFLOW 0x01u
#define UART_RX_ERROR_FRAMING  0x02u

// The connector pins are GPIO-only: Timer2 supplies bit timing while this
// state machine samples RX and changes TX.
enum { UART_LISTEN, UART_RECEIVE, UART_TRANSMIT };

// Shared with the Timer2 ISR. One unused queue slot distinguishes full from
// empty, so each 64-byte ring can hold at most 63 bytes.
static volatile uint8_t uart_state;
static volatile uint8_t rx_armed, rx_start_check, rx_phase, rx_bit, rx_data;
static volatile uint8_t rx_head, rx_tail, rx_queue[UART_QUEUE_SIZE], rx_errors;
static uint8_t rx_scan;
static volatile uint8_t tx_head, tx_tail, tx_queue[UART_QUEUE_SIZE];
static volatile uint8_t tx_phase, tx_bit, tx_data, turnaround;

void uart_init(void) {
   uint8_t gie = INTCONbits.GIE;

   // Reset the queues and state before enabling timer-driven operation.
   INTCONbits.GIE = 0;
   T2CONbits.T2ON = 0;
   PIE4bits.TMR2IE = 0;
   rx_head = rx_tail = rx_errors = 0;
   rx_scan = 0;
   tx_head = tx_tail = 0;
   uart_state = UART_LISTEN;
   rx_start_check = rx_phase = rx_bit = rx_data = 0;
   tx_phase = tx_bit = tx_data = turnaround = 0;

   // RD2 must be digital for PORT reads. RD1 is open-drain at idle: its high
   // latch value releases the line for the board's external pull-up.
   ANSELD &= (uint8_t)~0b00000100;
   UART_TX_PIN = 1;
   ODCONDbits.ODCD1 = 1;
   TRISDbits.TRISD1 = 0;
   TRISDbits.TRISD2 = 1;

   // CS=1 selects Fosc/4 (8 MHz). Timer2 supplies 8 ticks per bit with a
   // 1:1 prescaler and 1:3 postscaler (T2CON=0x82).
   T2CLKCON = 0x01;
   T2HLT = 0;
   T2RST = 0;
   T2TMR = 0;
   T2PR = (uint8_t)(UART_TIMER_PERIOD - 1UL);
   PIR4bits.TMR2IF = 0;
   T2CON = 0x82;
   PIE4bits.TMR2IE = 1;
   rx_armed = UART_RX_PIN;
   turnaround = 0;
   INTCONbits.GIE = gie;
}

void uart_suspend(void) {
   uint8_t gie = INTCONbits.GIE;
   INTCONbits.GIE = 0;
   PIE4bits.TMR2IE = 0;
   T2CONbits.T2ON = 0;
   PIR4bits.TMR2IF = 0;
   ODCONDbits.ODCD1 = 1;
   UART_TX_PIN = 1;
   INTCONbits.GIE = gie;
}

uint8_t uart_tx_string(const char *s) {
   uint8_t gie, head, tail, length = 0, used;
   if(!s) return 0;

   // Pause the ISR while checking capacity and copying. Publish the new head
   // last so the whole string is visible to the transmitter or none of it is.
   gie = INTCONbits.GIE;
   INTCONbits.GIE = 0;
   while(length < UART_QUEUE_MASK && s[length]) length++;
   if(s[length]) {
      INTCONbits.GIE = gie;
      return 0;
   }

   head = tx_head;
   tail = tx_tail;
   used = (head - tail) & UART_QUEUE_MASK;
   if(length > UART_QUEUE_MASK - used) {
      INTCONbits.GIE = gie;
      return 0;
   }

   while(*s) {
      tx_queue[head] = *s++;
      head = (head + 1u) & UART_QUEUE_MASK;
   }
   tx_head = head;
   INTCONbits.GIE = gie;
   return 1;
}

static uint8_t uart_parse_integer(uint8_t start, uint8_t end, uint8_t *value) {
   uint8_t length = (end - start) & UART_QUEUE_MASK;
   uint8_t pos = start, i = 0;
   uint8_t magnitude = 0;

   if(!length) return 0;

   while(i < length) {
      uint8_t c = rx_queue[pos];
      uint8_t digit;
      if(c < '0' || c > '9') return 0;
      digit = c - '0';
      if(magnitude > (255u - digit) / 10u) return 0;
      magnitude = magnitude * 10u + digit;
      pos = (pos + 1u) & UART_QUEUE_MASK;
      i++;
   }
   *value = (uint8_t)magnitude;
   return 1;
}

static uint32_t uart_encode_command(uint8_t start, uint8_t length) {
   uint32_t encoded = 0;
   uint8_t pos = start, i;

   for(i = 0; i < length; i++) {
      uint8_t c = rx_queue[pos];
      if(c >= 'a' && c <= 'z') c -= 'a' - 'A';
      encoded = (encoded << 8) | c;
      pos = (pos + 1u) & UART_QUEUE_MASK;
   }
   for(i = length; i < 4; i++) encoded <<= 8;
   return encoded;
}

static uart_command_result_t uart_parse_command(uint8_t start, uint8_t end) {
   uart_command_result_t result = {UART_COMMAND_NONE, 0};
   uint8_t name_end = end, argument_start = end;
   uint8_t length, pos = start;

   while(pos != end) {
      if(rx_queue[pos] == '=') {
         name_end = pos;
         argument_start = (pos + 1u) & UART_QUEUE_MASK;
         if(!uart_parse_integer(argument_start, end, &result.argument)) return result;
         break;
      }
      pos = (pos + 1u) & UART_QUEUE_MASK;
   }

   length = (name_end - start) & UART_QUEUE_MASK;
   if(length > 4) return result;
   result.command = uart_encode_command(start, length);

   if(result.argument > 127u) result.command = UART_COMMAND_NONE;
   if(result.command == UART_COMMAND_NONE) result.argument = 0;
   return result;
}

uart_command_result_t uart_command_poll(void) {
   uint8_t gie, head, start, pos;
   uart_command_result_t no_command = {UART_COMMAND_NONE, 0};

   // Take a stable queue snapshot and discard buffered input after a receive error.
   gie = INTCONbits.GIE;
   INTCONbits.GIE = 0;
   if(rx_errors) {
      rx_errors = 0;
      rx_tail = rx_head;
      rx_scan = rx_tail;
      INTCONbits.GIE = gie;
      return no_command;
   }

   head = rx_head;
   start = rx_tail;
   pos = rx_scan;
   INTCONbits.GIE = gie;

   // Leave incomplete input in the ring. Once a terminator is found, parse
   // only the bytes before it and consume that command, including ';'.
   while(pos != head) {
      if(rx_queue[pos] == ';') {
         uart_command_result_t command = uart_parse_command(start, pos);
         rx_tail = (pos + 1u) & UART_QUEUE_MASK;
         rx_scan = rx_tail;
         return command;
      }
      pos = (pos + 1u) & UART_QUEUE_MASK;
   }
   rx_scan = head;
   return no_command;
}

// Called once per Timer2 period match; processing should be kept minimal.
void uart_timer_isr(void) {
   if(uart_state == UART_LISTEN) {
      // Arm only after seeing a high level, then treat a falling edge as a
      // possible start bit. This avoids re-entering on a line stuck low.
      if(!UART_RX_PIN) {
         if(!rx_armed) return;
         uart_state = UART_RECEIVE;
         rx_start_check = 1;
         rx_armed = rx_phase = rx_bit = rx_data = 0;
         return;
      }
      rx_armed = 1;
      if(turnaround) {
         turnaround--;
         return;
      }

      if(tx_tail != tx_head) {
         // Only transmit while RX is idle/high.
         tx_data = tx_queue[tx_tail];
         tx_tail = (tx_tail + 1u) & UART_QUEUE_MASK;
         ODCONDbits.ODCD1 = 0;
         UART_TX_PIN = 0;
         tx_phase = tx_bit = 0;
         uart_state = UART_TRANSMIT;
      }
      return;
   }

   if(uart_state == UART_RECEIVE) {
      if(rx_start_check) {
         // Confirm the start bit halfway through it to reject short noise.
         if(++rx_phase < UART_OVERSAMPLE / 2u) return;
         if(UART_RX_PIN) {
            uart_state = UART_LISTEN;
            rx_armed = 1;
            rx_start_check = 0;
            return;
         }
         rx_start_check = 0;
         rx_phase = 0;
         return;
      }

      if(++rx_phase < UART_OVERSAMPLE) return;
      rx_phase = 0;
      if(rx_bit < 8) {
         // UART sends least-significant bit first; sample each bit at its
         // center, timing by oversample ticks.
         if(UART_RX_PIN) rx_data |= 1u << rx_bit;
         rx_bit++;
         return;
      }

      // The following sample is the stop bit. Report framing or queue overflow
      // rather than exposing a partial or overwritten byte to foreground code.
      if(UART_RX_PIN) {
         uint8_t next = (rx_head + 1u) & UART_QUEUE_MASK;
         if(next == rx_tail) {
            rx_errors |= UART_RX_ERROR_OVERFLOW;
         } else {
            rx_queue[rx_head] = rx_data;
            rx_head = next;
         }
         rx_armed = 1;
         turnaround = UART_RX_GUARD_TICKS;
      } else {
         rx_errors |= UART_RX_ERROR_FRAMING;
         rx_armed = 0;
         turnaround = 0;
      }
      rx_start_check = 0;
      uart_state = UART_LISTEN;
      return;
   }

   // Assume TRANSMIT, advance the output once per bit, after oversample interrupts.
   if(++tx_phase < UART_OVERSAMPLE) return;
   tx_phase = 0;

   // Send data least-significant bit first, then hold the stop bit high.
   // If another byte is queued, start it immediately after that stop bit.
   if(tx_bit < 8) {
      UART_TX_PIN = (tx_data >> tx_bit) & 1u;
      tx_bit++;
   } else if(tx_bit == 8) {
      UART_TX_PIN = 1;
      tx_bit++;
   } else if(tx_tail != tx_head) {
      tx_data = tx_queue[tx_tail];
      tx_tail = (tx_tail + 1u) & UART_QUEUE_MASK;
      UART_TX_PIN = 0;
      tx_bit = 0;
   } else {
      // Release TX for idle and return to listening after the final stop bit.
      ODCONDbits.ODCD1 = 1;
      uart_state = UART_LISTEN;
      // Arm RX only if the shared line is already idle-high; if it is low,
      // wait for the listener to observe high before accepting a start bit.
      rx_armed = UART_RX_PIN;
   }
}
