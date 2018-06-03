#ifndef SAM4S_CLOCK_H
#define SAM4S_CLOCK_H

#define SAM4S_CLOCK_HZ 100  /* our SysTick handler runs at 100 Hz */

/* system clock, in increments of 10ms */
extern volatile unsigned long sam4s_clock_tick;

/* turn on/off clock to the given peripheral */
extern void
sam4s_clock_peripheral_onoff(int peripheral, int on_off);

/* initialize the clock system to what our board needs */
extern void
sam4s_clock_init();

#endif
