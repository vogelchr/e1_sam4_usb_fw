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

/* this file contains code to setup the system clock, enable clock
   to peripherals and the SysTick interrupt (counting at 100 Hz) */

#include "sam4s_clock.h"
#include "sam4s_pinmux.h"
#include <sam4s8b.h>
#include <string.h>

#define CKGR_MOR_KEYVAL  CKGR_MOR_KEY(0x37)
#define PMC_XTAL_STARTUP_TIME   (0x3F)
#define PLL_COUNT                0x3fU

/* millisecond ticks */
volatile unsigned long sam4s_clock_tick;

static uint32_t sam4s_clock_blink_ctr;

void
SysTick_Handler() {
	sam4s_clock_tick++;
	sam4s_clock_blink_ctr++;

	if (sam4s_clock_blink_ctr == 50) {
		sam4s_pinmux_gpio_set(SAM4S_PINMUX_PA(24), 0);
	} else if (sam4s_clock_blink_ctr == 100) {
		sam4s_pinmux_gpio_set(SAM4S_PINMUX_PA(24), 1);
		sam4s_clock_blink_ctr=0;
	}
}

/*
 * Configure the PLL. Output frequency
 * will be fMAINCK / div * mul.
 *
 * pll_b is 1 for PLLB, 0 for PLLA
 */

static void
sam4s_clock_config_pll(int div, int mul, int pll_b)
{
	/* first disable, then re-enable again */
	if (pll_b)
		PMC->CKGR_PLLBR = CKGR_PLLBR_MULB(0);
	else
		PMC->CKGR_PLLAR = CKGR_PLLAR_MULA(0) | CKGR_PLLAR_ONE;

	if (div == 0 || mul == 0)
		return;

	/* pll can divide by 1..255 and multiply by 8..63 */
	if (div < 1 || div >255 || mul < 8 || mul > 63)
		return;
	/*
		MULx: PLLx Multiplier, 7 up to 62 = The PLLx Clock frequency
			is the PLLx input frequency multiplied by MULx + 1.
		DIVx: 1: divider is bypassed, 2–255: Clock is divided by DIVx
	*/
	mul--;

	if (pll_b) {
		PMC->CKGR_PLLBR = CKGR_PLLBR_DIVB(div) | /* pll freq. division */
		                  CKGR_PLLBR_PLLBCOUNT(PLL_COUNT) |
		                  CKGR_PLLBR_MULB(mul); /* pll freq. multiplicaiton-1 */
		/* pll_wait_for_lock(0); */
		while ((PMC->PMC_SR & PMC_SR_LOCKB) == 0);
	} else {
		PMC->CKGR_PLLAR = CKGR_PLLAR_DIVA(div) |  /* pll freq. division */
		                  CKGR_PLLAR_PLLACOUNT(PLL_COUNT) |
		                  CKGR_PLLAR_MULA(mul) |  /* pll freq multiplication-1 */
				  CKGR_PLLAR_ONE ;
		while ((PMC->PMC_SR & PMC_SR_LOCKA) == 0);
	}
}

/*
 * Switch MAINCK to internal RC oscillator, rc_freq are
 * flags for CKGR_MOR. Will turn off crystall oscillator.
 */
static void
sam4s_clock_mainck_to_rc(uint32_t rc_freq)
{
	/* enable RC oscillator, set frequency */
	PMC->CKGR_MOR = (PMC->CKGR_MOR & ~CKGR_MOR_MOSCRCF_Msk) |
		CKGR_MOR_KEYVAL | CKGR_MOR_MOSCRCEN | 
		(rc_freq & CKGR_MOR_MOSCRCF_Msk);
 	while (!(PMC->PMC_SR & PMC_SR_MOSCRCS)); /* RC osc ready */

 	/* select RC oscillator (clear MOSCSEL) */
	PMC->CKGR_MOR = (PMC->CKGR_MOR | CKGR_MOR_KEYVAL) & ~CKGR_MOR_MOSCSEL;
	while(!(PMC->PMC_SR & PMC_SR_MOSCSELS)); /* switchover done */

	/* turn off crystal oscillator */
	PMC->CKGR_MOR = (PMC->CKGR_MOR | CKGR_MOR_KEYVAL) &
		~(CKGR_MOR_MOSCXTEN|CKGR_MOR_MOSCXTBY);
}

/* 
 * Switch MAINCK to crystal oscillator (bypass=0)
 * or external clock input (byass=1). Will switch off
 * RC oscillator.
 */
static void
sam4s_clock_mainck_to_xtal(int bypass)
{
	PMC->CKGR_MOR = PMC->CKGR_MOR | CKGR_MOR_KEYVAL |
		CKGR_MOR_MOSCXTEN | (bypass ? CKGR_MOR_MOSCXTBY : 0);
 	while (!(PMC->PMC_SR & PMC_SR_MOSCXTS)); /* XTAL osc ready */

	PMC->CKGR_MOR = PMC->CKGR_MOR | CKGR_MOR_KEYVAL | CKGR_MOR_MOSCSEL;
	while(!(PMC->PMC_SR & PMC_SR_MOSCSELS)); /* switchover done */

	/* turn off RC oscillator */
	PMC->CKGR_MOR = (PMC->CKGR_MOR | CKGR_MOR_KEYVAL) &
		~(CKGR_MOR_MOSCRCF_Msk|CKGR_MOR_MOSCRCEN);
}

/*
 * Source master clock from main, slow, plla or pllb clock.
 */
static void
sam4s_clock_master_clock_select(uint32_t flags)
{
	uint32_t css = flags & PMC_MCKR_CSS_Msk;
	uint32_t pres = flags & PMC_MCKR_PRES_Msk;
	const uint32_t PRES_DIV_MASK = PMC_MCKR_PRES_Msk|PMC_MCKR_PLLADIV2|PMC_MCKR_PLLBDIV2;

	/* -------------------------------------------------------------
	   29.14 Programming Sequence
	   PMC_MCKR must not be programmed in a single write operation,
	   and order depends on whether PLL is used, or not.
	   ------------------------------------------------------------- */
	if (css == PMC_MCKR_CSS_PLLA_CLK || css == PMC_MCKR_CSS_PLLB_CLK) {
		PMC->PMC_MCKR = (PMC->PMC_MCKR & ~PRES_DIV_MASK) |
			(flags & PRES_DIV_MASK);
		while(!(PMC->PMC_SR & PMC_SR_MCKRDY));
		PMC->PMC_MCKR = (PMC->PMC_MCKR & ~PMC_MCKR_CSS_Msk) |
			(flags & PMC_MCKR_CSS_Msk);
		while(!(PMC->PMC_SR & PMC_SR_MCKRDY));
	} else {
		PMC->PMC_MCKR = (PMC->PMC_MCKR & ~PMC_MCKR_CSS_Msk) |
			(flags & PMC_MCKR_CSS_Msk);
		while(!(PMC->PMC_SR & PMC_SR_MCKRDY));
		PMC->PMC_MCKR = (PMC->PMC_MCKR & ~PRES_DIV_MASK) |
			(flags & PRES_DIV_MASK);
		while(!(PMC->PMC_SR & PMC_SR_MCKRDY));
	}
}

/*
 * Initialize the SAM4S clocks.
 */
void
sam4s_clock_init()
{
	/*
	 * when we run from SRAM via GDB we might be in an unknown state,
	 * e.g. SAM-BA might have run, or code from flash, so we try to
	 * first go into a sane state.
	 * Most useful diagram to understand this is in §29.3, Page 516.
	 */
	PMC->PMC_SCDR = PMC_SCDR_UDP | PMC_SCDR_PCK0 | PMC_SCDR_PCK1 | PMC_SCDR_PCK2;
	PMC->PMC_PCK[0] = 0;
	PMC->PMC_PCK[1] = 0;
	PMC->PMC_PCK[2] = 0;

	/* select non-PLL mainck, XTAL or RC. Turn off PLLs. */
	sam4s_clock_master_clock_select(PMC_MCKR_CSS_MAIN_CLK);
	sam4s_clock_config_pll(0, 0, 0/*PLLA*/);
	sam4s_clock_config_pll(0, 0, 1/*PLLB*/);

	/* We might want to enable input for external oscillator, but
	 * clock controller may be set to XTAL oscillator (requiring a real XTAL).
	 * So we temporarily switch to RC. */
	if ( (PMC->CKGR_MOR & CKGR_MOR_MOSCSEL)  /* <debug> &&
	    !(PMC->CKGR_MOR & CKGR_MOR_MOSCXTBY) </debug> */ ) {
		sam4s_clock_mainck_to_rc(CKGR_MOR_MOSCRCF_12_MHz);
	}

	/* switch to external input (xtal input, but with bypass) */
	if (!(PMC->CKGR_MOR & CKGR_MOR_MOSCSEL)) {
		sam4s_clock_mainck_to_xtal(CKGR_MOR_MOSCXTBY);
	}

	/* Oscillator is 30.72 MHz */

	/* PLLA: CPU Clock + Peripherals 30.72 MHz / 5 * 18 = 110.592 MHz */
	/* will be divided by SSC by 54 to get 2.048 MHz */
	sam4s_clock_config_pll(5, 18, 0/*PLLA*/);

	/* PLLB: USB clock, 30.72 MHz / 8 * 25 = 96 MHz */
	sam4s_clock_config_pll(8, 25, 1/*PLLB*/);

	/* siwtch master clock to PLLA 110.592 MHz, fmclk <= 120MHz! */
	sam4s_clock_master_clock_select(PMC_MCKR_CSS_PLLA_CLK);

	/* enable USB clock: 96 MHz (PLLB) / USBDIV+1 (=2) = 48 MHz  */
	PMC->PMC_USB = PMC_USB_USBDIV(1) | PMC_USB_USBS; /* USBS=1(PLLB) */
	PMC->PMC_SCER |= PMC_SCDR_UDP;

	/* enable SysTick timer */
	SysTick->CTRL= 0; /* disable */
	SysTick->LOAD = (F_MCK_HZ / SAM4S_CLOCK_HZ)-1;
	SysTick->VAL = 0;

	/* set highest priority */
	NVIC_SetPriority (SysTick_IRQn, (1UL << __NVIC_PRIO_BITS) - 1UL);
	NVIC_EnableIRQ(SysTick_IRQn);

	SysTick->CTRL = SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk;
}

void
sam4s_clock_peripheral_onoff(int periph_id, int on_off) {
	/* Page 50, Table 11-1: Only peripherals 8..34 have clock control */

	if (periph_id < 8 || periph_id > 34)
		return;

	if (periph_id > 31) {
		periph_id -= 32;
		if (on_off) {
			PMC->PMC_PCER1 = (1UL << periph_id);
			while (!(PMC->PMC_PCSR1 & (1UL << periph_id)));
		} else {
			PMC->PMC_PCDR1 = (1UL << periph_id);
			while (PMC->PMC_PCSR1 & (1UL << periph_id));
		}
	} else {
		if (on_off) {
			PMC->PMC_PCER0 = (1UL << periph_id);
			while (!(PMC->PMC_PCSR0 & (1UL << periph_id)));
		} else {
			PMC->PMC_PCDR0 = (1UL << periph_id);
			while (PMC->PMC_PCSR0 & (1UL << periph_id));
		}
	}
}
