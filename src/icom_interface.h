#ifndef ICOM_INTERFACE_H
#define ICOM_INTERFACE_H

#include <stdint.h>

void icom_interface_init(void);
void icom_interface_suspend(void);
void icom_interface_clear(void);
void icom_interface_timer_tick(void);
uint8_t icom_interface_event(void);
void icom_interface_tuning(uint8_t active);

#endif
