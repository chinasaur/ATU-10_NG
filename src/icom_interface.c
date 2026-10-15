#include "board.h"
#include "buttons.h"
#include "icom_interface.h"

#define ICOM_START    (!PORTDbits.RD1)
#define ICOM_KEY_IN   PORTDbits.RD2
#define ICOM_KEY_OUT  LATDbits.LATD2

#define EXT_SHORT_MIN 2
#define EXT_SHORT_MAX 10
#define EXT_LONG_T    20

static volatile uint8_t active, ext_held, ext_released;
static uint8_t ext_long_sent, pending;

void icom_interface_init(void) {
   uint8_t gie = INTCONbits.GIE;
   INTCONbits.GIE = 0;
   active = 1;
   ext_held = ext_released = 0;
   ext_long_sent = 1;
   pending = EV_NONE;
   ODCONDbits.ODCD2 = 1;
   TRISDbits.TRISD1 = 1;
   TRISDbits.TRISD2 = 0;
   ICOM_KEY_OUT = 1;
   INTCONbits.GIE = gie;
}

void icom_interface_suspend(void) {
   uint8_t gie = INTCONbits.GIE;
   INTCONbits.GIE = 0;
   active = 0;
   ext_held = ext_released = 0;
   ext_long_sent = 1;
   pending = EV_NONE;
   ICOM_KEY_OUT = 1;
   INTCONbits.GIE = gie;
}

void icom_interface_clear(void) {
   uint8_t gie = INTCONbits.GIE;
   INTCONbits.GIE = 0;
   ext_held = ext_released = 0;
   ext_long_sent = 1;
   pending = EV_NONE;
   INTCONbits.GIE = gie;
}

void icom_interface_timer_tick(void) {
   if(!active) return;
   if(ICOM_START) {
      if(ext_held != 255) ext_held++;
   }
   else if(ext_held) {
      ext_released = ext_held;
      ext_held = 0;
   }
}

uint8_t icom_interface_event(void) {
   uint8_t eh, erel, ev;
   if(pending) {
      ev = pending;
      pending = EV_NONE;
      return ev;
   }

   INTCONbits.GIE = 0;
   eh = ext_held;
   erel = ext_released;
   ext_released = 0;
   INTCONbits.GIE = 1;

   ev = EV_NONE;
   if(erel) {
      if(erel >= EXT_SHORT_MIN && erel < EXT_SHORT_MAX) ev = EV_EXT_SHORT;
      ext_long_sent = 0;
   }
   if(eh >= EXT_LONG_T && !ext_long_sent && ICOM_KEY_IN) {
      ext_long_sent = 1;
      if(ev) pending = EV_EXT_LONG;
      else ev = EV_EXT_LONG;
   }
   if(eh == 0 && !erel) ext_long_sent = 0;
   return ev;
}

void icom_interface_tuning(uint8_t tuning) {
   if(active) ICOM_KEY_OUT = tuning ? 0 : 1;
}
