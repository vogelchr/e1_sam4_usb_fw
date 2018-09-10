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

/* this file contains code to control the pinmuxing and GPIOs */

#include "sam4s_pinmux.h"
#include "sam4s_clock.h"

#include <sam4s8b.h>

#include <stddef.h>

extern void
sam4s_pinmux_init() {
	/* three banks, each 32 io lines, C only usable on 100pin device */
	sam4s_clock_peripheral_onoff(ID_PIOA, 1 /* on */);
	sam4s_clock_peripheral_onoff(ID_PIOB, 1 /* on */);
#if 0 /* will block on "B" devices with only port A, B */
	sam4s_clock_peripheral_onoff(ID_PIOC, 1 /* on */);
#endif

}

static Pio * const
pio_addrs[] = {
	PIOA, PIOB,
#if 0
	PIOC
#endif
};

#define SAM4S_PINMUX_PORT(pin) ((pin) >> 5)
#define SAM4S_PINMUX_BIT(pin)  ((pin) & 0x1f)
#define SAM4S_PINMUX_MASK(pin) ((1UL << SAM4S_PINMUX_BIT(pin)))
#define SAM4S_PINMUX_CTRLR(pin) (pio_addrs[SAM4S_PINMUX_PORT(pin)])

void
sam4s_pinmux_function(int pin, enum sam4s_pinmux_function func)
{
	Pio *pio = SAM4S_PINMUX_CTRLR(pin);
	uint32_t sr0=0, sr1=0;

	if (func == SAM4S_PINMUX_GPIO)
		pio->PIO_PER = SAM4S_PINMUX_MASK(pin);
	else
		pio->PIO_PDR = SAM4S_PINMUX_MASK(pin);

	if (func == SAM4S_PINMUX_B || func == SAM4S_PINMUX_D)
		pio->PIO_ABCDSR[0] |= SAM4S_PINMUX_MASK(pin);
	else
		pio->PIO_ABCDSR[0] &= ~(SAM4S_PINMUX_MASK(pin));

#if 0
	if (func == SAM4S_PINMUX_C || func == SAM4S_PINMUX_D)
		pio->PIO_ABCDSR[1] |= SAM4S_PINMUX_MASK(pin);
	else
		pio->PIO_ABCDSR[1] &= ~(SAM4S_PINMUX_MASK(pin));
#endif

}

void sam4s_pinmux_pull(int pin, enum sam4s_pinmux_pull what)
{
	Pio *pio = SAM4S_PINMUX_CTRLR(pin);

	switch (what) {
	case SAM4S_PINMUX_PULLNONE:
		pio->PIO_PPDDR |= SAM4S_PINMUX_MASK(pin);
		pio->PIO_PUDR  |= SAM4S_PINMUX_MASK(pin);
		break;
	case SAM4S_PINMUX_PULLUP:
		pio->PIO_PPDDR |= SAM4S_PINMUX_MASK(pin);
		pio->PIO_PUER  |= SAM4S_PINMUX_MASK(pin);
		break;
	case SAM4S_PINMUX_PULLDOWN:
		pio->PIO_PPDER |= SAM4S_PINMUX_MASK(pin);
		pio->PIO_PUDR  |= SAM4S_PINMUX_MASK(pin);
		break;
	}
}

void
sam4s_pinmux_open_drain(int pin, int onoff)
{
	Pio *pio = SAM4S_PINMUX_CTRLR(pin);

	if (onoff)
		pio->PIO_MDER = SAM4S_PINMUX_MASK(pin);
	else
		pio->PIO_MDDR = SAM4S_PINMUX_MASK(pin);	
}

void
sam4s_pinmux_gpio_set(int pin, int val) {
	Pio *pio = SAM4S_PINMUX_CTRLR(pin);

	if (val)
		pio->PIO_SODR = SAM4S_PINMUX_MASK(pin);
	else
		pio->PIO_CODR = SAM4S_PINMUX_MASK(pin);
}

void
sam4s_pinmux_gpio_oe(int pin, int oe) {
	Pio *pio = SAM4S_PINMUX_CTRLR(pin);

	if (oe)
		pio->PIO_OER = SAM4S_PINMUX_MASK(pin);
	else
		pio->PIO_ODR = SAM4S_PINMUX_MASK(pin);
}

int
sam4s_pinmux_gpio_get(int pin) {
	Pio *pio = SAM4S_PINMUX_CTRLR(pin);
	return !! (pio->PIO_PDSR & (SAM4S_PINMUX_MASK(pin)));
}
