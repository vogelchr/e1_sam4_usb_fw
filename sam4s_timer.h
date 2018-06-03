#ifndef SAM4S_TIMER_H
#define SAM4S_TIMER_H

#define SAM4S_TIMER_CAPT_RISING  0x01
#define SAM4S_TIMER_CAPT_FALLING 0x02

#include <stdint.h>

extern void sam4s_timer_init();

/* get last captured timestamp info */
extern unsigned int
sam4s_timer_capt_poll(uint32_t *rising, uint32_t *falling );

#endif