#include "board.h"

void board_init(void) {
   // Ports: RB0..RB2 analog (detectors, battery), RB5 button, rest digital out
   ANSELA = 0;
   ANSELB = 0b00000111;
   ANSELC = 0;
   ANSELD = 0;
   ANSELE = 0;
   CM1CON0bits.C1ON = 0;
   CM2CON0bits.C2ON = 0;
   LATA = 0b00000000;
   LATB = 0b00011000;   // LEDs off
   LATC = 0b00010000;   // relay driver to plus off
   LATD = 0b00000110;   // External interface pins released
   LATE = 0b00000000;
   TRISA = 0b00000000;
   TRISB = 0b00100111;
   TRISC = 0b00000000;
   ODCONDbits.ODCD1 = 1;   // Open-drain external interface pins
   ODCONDbits.ODCD2 = 1;
   TRISD = 0b00000110;   // Interface-specific directions set after Cells load
   TRISE = 0b00000000;
   ODCONAbits.ODCA2 = 1;   // I2C
   ODCONAbits.ODCA3 = 1;
   WPUD = 0;               // Disable internal pull-ups (hardware has 10k on RD1/RD2)

   // Unused modules off. Kept on: Timer0, Timer2 (software UART), NVM, FVR,
   // IOC (wake-up by the button), and ADC.
   PMD0 = 0b00011010;   // CLKR, SCAN, CRC off
   PMD1 = 0b11111010;   // Timer0 and Timer2 on; Timer1, Timer3-6 off
   PMD2 = 0b01000111;   // ZCD, comparators, DAC off
   PMD3 = 0b01111111;   // CCP, PWM off
   PMD4 = 0b01110111;   // CWG, MSSP, hardware UART off (software UART uses GPIO)
   PMD5 = 0b11011111;   // DSM, CLC, SMT off

   // Timer0: 8 bit with period register, Fosc/4 / 32 = 250 kHz, 250 counts:
   // an interrupt every 1 ms exactly, no reloading
   T0CON1 = 0b01000101;
   T0CON0 = 0b00000000;
   TMR0H = 249;
   TMR0L = 0;
   PIR0bits.TMR0IF = 0;
   PIE0bits.TMR0IE = 1;
   INTCONbits.PEIE = 1;
   T0CON0bits.T0EN = 1;
   INTCONbits.GIE = 1;
}

void delay_ms(uint16_t ms) {
   while(ms--) {
      CLRWDT();
      __delay_ms(1);
   }
}
