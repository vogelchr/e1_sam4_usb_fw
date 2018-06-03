/* 
 * This file is part of the osmocom sam4s usb interface firmware.
 * Copyright (c) 2018 Christian Vogel <vogelchr@vogel.cx>.
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/* timer peripheral to capture GPS PPS pulses */

#include "sam4s_timer.h"
#include "sam4s_pinmux.h"
#include "sam4s_clock.h"

#include <sam4s4c.h>

/* The timer unfortunately only is 16bit, so we count the number
   of overflows as the 16 MSB of capture timestamps.*/
static uint16_t sam4s_timer_capt_msb;
static uint32_t sam4s_timer_capt_rising;         /* msb << 16 | timstamp */
static uint32_t sam4s_timer_capt_falling;        /* for rising & falling edge */
static volatile uint32_t sam4s_timer_capt_flags;

void TC0_Handler()
{
	uint32_t sr = TC0->TC_CHANNEL[0].TC_SR; /* status register */
	uint16_t tv = TC0->TC_CHANNEL[0].TC_CV; /* timer value */

	if (!(sr & TC_SR_COVFS)) /* only interrupt source is overflow, so... */
		return;          /* this should should never happen */

	/* we read, in order: status register, timer value and then
	   the capture registers. If status register read in the first
	   step indicates a timer capture event and the captured value
	   is yet smaller than tv, this means that the capture has happened
	   in between the overflow of TV from MAX to 0 and the moment we
	   handle the interrupt. Therefore the msb (number of overflows)
	   for this capture event is one more than the (at this point in
	   the IRQ handler not yet incremented) msb counter. */

	if (sr & TC_SR_LDRAS) {
		uint16_t ra_msb = sam4s_timer_capt_msb;
		uint16_t ra = TC0->TC_CHANNEL[0].TC_RA;

		if (ra <= tv) /* read remark above */
			ra_msb++;
		sam4s_timer_capt_rising = (ra_msb << 16) | ra;
		sam4s_timer_capt_flags |= SAM4S_TIMER_CAPT_RISING;
	}

	if (sr & TC_SR_LDRBS) {
		uint16_t rb_msb = sam4s_timer_capt_msb;
		uint16_t rb = TC0->TC_CHANNEL[0].TC_RB;

		if (rb <= tv) /* read remark above */
			rb_msb++;
		sam4s_timer_capt_falling = (rb_msb << 16) | rb;
		sam4s_timer_capt_flags |= SAM4S_TIMER_CAPT_FALLING;
	}

	sam4s_timer_capt_msb++; /* number of timer overflows */
}

void
sam4s_timer_init() {
	sam4s_clock_peripheral_onoff(ID_TC0, 1/*on*/);

	/* channel 0 capture, overflow counts the most significant bits */
	TC0->TC_CHANNEL[0].TC_CCR = TC_CCR_CLKDIS;
	TC0->TC_CHANNEL[0].TC_CMR = TC_CMR_TCCLKS_TIMER_CLOCK1 |
		TC_CMR_LDRA_RISING | TC_CMR_LDRB_FALLING;
	TC0->TC_CHANNEL[0].TC_RA = 0;
	TC0->TC_CHANNEL[0].TC_RB = 0;
	TC0->TC_CHANNEL[0].TC_RC = 0;
	TC0->TC_CHANNEL[0].TC_IDR = TC0->TC_CHANNEL[0].TC_IMR; /* disable all */
	TC0->TC_CHANNEL[0].TC_IER = TC_IER_COVFS; /* fire ISR on overflow */

	/* channel 1 is unused  */
	TC0->TC_CHANNEL[1].TC_CCR = TC_CCR_CLKDIS;
	TC0->TC_CHANNEL[1].TC_CMR = 0;
	TC0->TC_CHANNEL[1].TC_RA = 0;
	TC0->TC_CHANNEL[1].TC_RB = 0;
	TC0->TC_CHANNEL[1].TC_RC = 0;
	TC0->TC_CHANNEL[1].TC_IDR = TC0->TC_CHANNEL[1].TC_IMR; /* disable all */

	/* channel 2 is unused */
	TC0->TC_CHANNEL[2].TC_CCR = TC_CCR_CLKDIS;
	TC0->TC_CHANNEL[2].TC_CMR = 0;
	TC0->TC_CHANNEL[2].TC_RA = 0;
	TC0->TC_CHANNEL[2].TC_RB = 0;
	TC0->TC_CHANNEL[2].TC_RC = 0;
	TC0->TC_CHANNEL[2].TC_IDR = TC0->TC_CHANNEL[2].TC_IMR; /* disable all */

	TC0->TC_BMR = 0;

	NVIC_EnableIRQ(TC0_IRQn);
	TC0->TC_CHANNEL[0].TC_CCR = TC_CCR_CLKEN|TC_CCR_SWTRG; /* start channel 0 */
}

extern unsigned int
sam4s_timer_capt_poll(
	uint32_t *rising,
	uint32_t *falling
) {
	unsigned int ret = 0;

	__disable_irq();
	if ((sam4s_timer_capt_flags & SAM4S_TIMER_CAPT_RISING) && rising)
		*rising = sam4s_timer_capt_rising;
	if ((sam4s_timer_capt_flags & SAM4S_TIMER_CAPT_FALLING) && falling)
		*falling = sam4s_timer_capt_falling;
	ret = sam4s_timer_capt_flags;
	sam4s_timer_capt_flags = 0;
	__enable_irq();


	return ret;
}
