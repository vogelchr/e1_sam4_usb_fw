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

/* synchronous serial interface, the most important peripheral for our E1
   interface */

#include "sam4s_ssc.h"
#include <sam4s4c.h>

#include "sam4s_clock.h"
#include "sam4s_pinmux.h"

unsigned char sam4s_ssc_rxbuf[SAM4S_SSC_BUF_NFRAMES*SAM4S_SSC_FRAME_BYTES];
int sam4s_ssc_rx_last_frame;
static int sam4s_ssc_rx_curr_frame;

unsigned char sam4s_ssc_txbuf[SAM4S_SSC_BUF_NFRAMES*SAM4S_SSC_FRAME_BYTES];
int sam4s_ssc_tx_last_frame;
static int sam4s_ssc_tx_curr_frame;

/* statistics for debugging */
unsigned long sam4s_ssc_tx_irq_ctr;
unsigned long sam4s_ssc_rx_irq_ctr;

void
SSC_Handler()
{
	/* Receiver */
	/* "current" rx buffer has finished receiving, but we have to submit
	   the one after the next (rxbuf_submit) to keep the queue full */
	if (SSC->SSC_SR & SSC_SR_ENDRX) {
		int cp = sam4s_ssc_rx_curr_frame;

		/* this is the period that just finished being received */
		sam4s_ssc_rx_last_frame = cp; /* last frame is for consumer! */

		/* this is the period that is currently being received */
		cp = (cp + 1) % SAM4S_SSC_BUF_NFRAMES;
		sam4s_ssc_rx_curr_frame = cp;

		/* this is the next period that will be received */
		cp = ( cp + 1 ) % SAM4S_SSC_BUF_NFRAMES;
		PDC_SSC->PERIPH_RNPR = (uint32_t) &sam4s_ssc_rxbuf[cp * SAM4S_SSC_FRAME_BYTES];
		PDC_SSC->PERIPH_RNCR = SAM4S_SSC_FRAME_BYTES;

		/* debug */
		sam4s_ssc_rx_irq_ctr++;
		sam4s_pinmux_gpio_set(SAM4S_PINMUX_PA(25),sam4s_ssc_rx_irq_ctr & 1);
	}

	if (SSC->SSC_SR & SSC_SR_ENDTX) {
		int cp = sam4s_ssc_tx_curr_frame;
	
		sam4s_ssc_tx_last_frame = cp;

		cp = (cp + 1) % SAM4S_SSC_BUF_NFRAMES;
		sam4s_ssc_tx_curr_frame = cp;

		cp = ( cp + 1 ) % SAM4S_SSC_BUF_NFRAMES;
		PDC_SSC->PERIPH_TNPR = (uint32_t) &sam4s_ssc_txbuf[cp * SAM4S_SSC_FRAME_BYTES];
		PDC_SSC->PERIPH_TNCR = SAM4S_SSC_FRAME_BYTES;

		sam4s_ssc_tx_irq_ctr++;
		sam4s_pinmux_gpio_set(SAM4S_PINMUX_PA(26),sam4s_ssc_tx_irq_ctr & 1);

	}
}


void
sam4s_ssc_init()
{
	NVIC_DisableIRQ(SSC_IRQn);

	sam4s_clock_peripheral_onoff(ID_SSC, 1 /* on */);

	sam4s_pinmux_function(SAM4S_PINMUX_PA(16), SAM4S_PINMUX_A); /* TK */
	sam4s_pinmux_function(SAM4S_PINMUX_PA(17), SAM4S_PINMUX_A); /* TD */
	sam4s_pinmux_function(SAM4S_PINMUX_PA(18), SAM4S_PINMUX_A); /* RD */
	sam4s_pinmux_function(SAM4S_PINMUX_PA(19), SAM4S_PINMUX_A); /* RK */

	SSC->SSC_CR = SSC_CR_SWRST;

	SSC->SSC_IDR = SSC->SSC_IMR;     /* disable all: disable <== mask */
	PDC_SSC->PERIPH_PTCR = PERIPH_PTCR_RXTDIS|PERIPH_PTCR_TXTDIS;

	SSC->SSC_CMR = SSC_CMR_DIV(27); /* 110.592 MHz / (27*2) = 2.048 MHz */

	/* RK is an input, receive clock */
	SSC->SSC_RCMR = SSC_RCMR_CKS_RK;
	SSC->SSC_RFMR = SSC_RFMR_DATLEN(7);

	/* TK is an output, transmit clock */
	SSC->SSC_TCMR = SSC_TCMR_CKS_MCK | SSC_TCMR_CKO_CONTINUOUS | SSC_TCMR_CKI;
	SSC->SSC_TFMR = SSC_TFMR_DATLEN(7);

	/* enable receiver and transmitter */
	SSC->SSC_CR = SSC_CR_TXEN | SSC_CR_RXEN;

	sam4s_ssc_tx_last_frame = -1;
	sam4s_ssc_rx_last_frame = -1;

	sam4s_ssc_tx_curr_frame = 0;
	sam4s_ssc_rx_curr_frame = 0;

	/* enable DMA for SSC, receive and transmit...
	  first next pointer register, then pointer register and counters */
	PDC_SSC->PERIPH_RNPR = (uint32_t)&sam4s_ssc_rxbuf[SAM4S_SSC_FRAME_BYTES];
	PDC_SSC->PERIPH_RNCR = SAM4S_SSC_FRAME_BYTES;
	PDC_SSC->PERIPH_RPR = (uint32_t)&sam4s_ssc_rxbuf;
	PDC_SSC->PERIPH_RCR = SAM4S_SSC_FRAME_BYTES;

	PDC_SSC->PERIPH_TNPR = (uint32_t)&sam4s_ssc_txbuf[SAM4S_SSC_FRAME_BYTES];
	PDC_SSC->PERIPH_TNCR = SAM4S_SSC_FRAME_BYTES;
	PDC_SSC->PERIPH_TPR = (uint32_t)&sam4s_ssc_txbuf;
	PDC_SSC->PERIPH_TCR = SAM4S_SSC_FRAME_BYTES;

	/* enable DMA on Rx and Tx, then enable interrupts */
	PDC_SSC->PERIPH_PTCR = PERIPH_PTCR_RXTEN | PERIPH_PTCR_TXTEN;
	SSC->SSC_IER = SSC_IER_ENDTX| SSC_IER_ENDRX;
	NVIC_EnableIRQ(SSC_IRQn);
}
