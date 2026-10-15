// Button events, from the raw press times the timer interrupt counts.
// Times in 10 ms:
//   button  short: released after 30..240 ms   long: held 250 ms
//           extra long: held 2.5 s

#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdint.h>

enum { EV_NONE, EV_SHORT, EV_LONG, EV_XLONG, EV_EXT_SHORT, EV_EXT_LONG };

uint8_t buttons_event(void);         // next event, EV_NONE if none
void buttons_unget(uint8_t ev);      // hand an event back for later
void buttons_clear(void);            // forget everything (after a power-up)

#endif
