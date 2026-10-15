#ifndef UART_H
#define UART_H

#include <stdint.h>

#ifndef UART_BAUD
#define UART_BAUD 2400u
#endif

#define UART_COMMAND_CODE(a, b, c, d) \
   (((uint32_t)(uint8_t)(a) << 24) | ((uint32_t)(uint8_t)(b) << 16) | \
    ((uint32_t)(uint8_t)(c) << 8) | (uint32_t)(uint8_t)(d))

typedef uint32_t uart_command_t;

#define UART_COMMAND_NONE                 0UL
#define UART_COMMAND_BYPASS_OFF           UART_COMMAND_CODE('B', 'Y', '-', 0)
#define UART_COMMAND_BYPASS_ON            UART_COMMAND_CODE('B', 'Y', '+', 0)
#define UART_COMMAND_AUTO_OFF             UART_COMMAND_CODE('A', 'U', '-', 0)
#define UART_COMMAND_AUTO_ON              UART_COMMAND_CODE('A', 'U', '+', 0)
#define UART_COMMAND_L_SET                UART_COMMAND_CODE('L', 0, 0, 0)
#define UART_COMMAND_C_SET                UART_COMMAND_CODE('C', 0, 0, 0)
#define UART_COMMAND_L_QUERY              UART_COMMAND_CODE('L', '?', 0, 0)
#define UART_COMMAND_C_QUERY              UART_COMMAND_CODE('C', '?', 0, 0)
#define UART_COMMAND_BYPASS_QUERY         UART_COMMAND_CODE('B', 'Y', '?', 0)
#define UART_COMMAND_AUTO_QUERY           UART_COMMAND_CODE('A', 'U', '?', 0)
#define UART_COMMAND_CAP_LOAD             UART_COMMAND_CODE('L', 'C', 0, 0)
#define UART_COMMAND_CAP_SOURCE           UART_COMMAND_CODE('C', 'L', 0, 0)
#define UART_COMMAND_CAP_POSITION_QUERY   UART_COMMAND_CODE('L', 'C', '?', 0)
#define UART_COMMAND_SWR_QUERY             UART_COMMAND_CODE('S', 'W', 'R', '?')
#define UART_COMMAND_LAST_SWR_QUERY        UART_COMMAND_CODE('L', 'S', 'W', '?')
#define UART_COMMAND_PEAK_POWER_QUERY      UART_COMMAND_CODE('P', 'K', '?', 0)
#define UART_COMMAND_POWER_QUERY           UART_COMMAND_CODE('P', 'W', 'R', '?')

typedef struct {
   uart_command_t command;
   uint8_t argument;
} uart_command_result_t;

// Software UART on the former EXT port (RD1=TX, RD2=RX).
// Timer2 services bytes asynchronously; none of the queue functions waits
// for a character or frame to finish. Both queues hold up to 63 bytes.
void uart_init(void);                   // fixed at UART_BAUD
void uart_suspend(void);                // stop sampling while the device sleeps
uint8_t uart_tx_string(const char *s);  // atomically enqueue a string; 0 if it won't fit

// Parse one ';'-terminated command; an unsigned 8-bit decimal may follow '='.
// The argument is zero when omitted; malformed arguments and values over 127
// are rejected. L and C set a 7-bit relay mask (zero when omitted); L? and C?
// query the current mask. Other commands ignore a valid argument. Names are
// case-insensitive; BY? queries bypass, AU? queries auto tune. AU+ and AU-
// enable and disable auto tune.
// LC selects the load side for the shunt capacitor; CL selects the source side;
// LC? queries the side. SWR? queries live SWR in hundredths (0 with no RF);
// LSW? returns the last displayed SWR in hundredths (0 until measured).
// PK? returns peak-held power, and PWR? live power, both in tenths of a watt;
// PWR? returns 0 below the live measurement threshold.
uart_command_result_t uart_command_poll(void);

#endif
