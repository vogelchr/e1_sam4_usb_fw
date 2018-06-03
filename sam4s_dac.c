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

#include "sam4s_dac.h"
#include <sam4s4c.h>
#include "sam4s_clock.h"
#include "sam4s_pinmux.h"

void
sam4s_dac_init()
{
	sam4s_clock_peripheral_onoff(ID_DACC, 1 /* on */);
	/* DACC_MR_TAG_EN: data register [13:12] is channel number */
	DACC->DACC_MR = DACC_MR_ONE | DACC_MR_TAG_EN;
	/* enable both channels */
	DACC->DACC_CHER = DACC_CHER_CH1 | DACC_CHER_CH0;

	sam4s_dac_update(0, 0);
	sam4s_dac_update(1, 0);
}

/* See 43.6.5 Channel Selection */
#define DACC_CDR_TAG_MASK      0x00003000
#define DACC_CDR_TAG_CH(ch)    ((ch & 1) << 12)  /* [13:12] channel, 00 or 01 */
#define DACC_CDR_VALUE(v)      ((v) & 0xfff)     /* [11:0]  value */
#define DACC_CDR_CH_VAL(ch, val)  (DACC_CDR_TAG_CH(ch) | DACC_CDR_VALUE(val))

void
sam4s_dac_update(int ch, unsigned int value)
{
	uint32_t reg;

	while(!(DACC->DACC_ISR & DACC_ISR_TXRDY));
	DACC->DACC_CDR = DACC_CDR_CH_VAL(ch, value);
}