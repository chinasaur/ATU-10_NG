#include "board.h"
#include "timer.h"
#include "icom_interface.h"
#include "uart_internal.h"

static volatile uint32_t tick;
static volatile uint8_t div10;
volatile uint8_t btn_held, btn_released;

// Keep the interrupt call depth low for the PIC's 16-level hardware stack.
void __interrupt() isr(void) {
   // 1 ms app tick.
   if(PIR0bits.TMR0IF) {
      PIR0bits.TMR0IF = 0;
      tick++;
      if(++div10 >= 10) {
         div10 = 0;
         icom_interface_timer_tick();
         if(BUTTON_DOWN) {
            if(btn_held != 255) btn_held++;
         }
         else if(btn_held) {
            btn_released = btn_held;
            btn_held = 0;
         }
      }
   }

   // UART's oversampling clock.
   if(PIR4bits.TMR2IF) {
      PIR4bits.TMR2IF = 0;
      uart_timer_isr();
   }
}

uint32_t tick_ms(void) {
   uint32_t t;
   INTCONbits.GIE = 0;
   t = tick;
   INTCONbits.GIE = 1;
   return t;
}
