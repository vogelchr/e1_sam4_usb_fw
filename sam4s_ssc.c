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
#include <string.h> // memcpy

#include "sam4s_clock.h"
#include "sam4s_pinmux.h"
#include "e1_mgmt.h"

/* externally visible buffer for received realigned data */
uint32_t sam4s_ssc_rx_buf[SAM4S_SSC_DBLFRM_LONGWORDS*SAM4S_SSC_BUF_DBLFRAMES];
volatile int sam4s_ssc_rx_last_dblfrm;
static int sam4s_ssc_rx_curr_dblfrm;

/* externally visible buffer for data that needs to be transmitted */
uint32_t sam4s_ssc_tx_buf[SAM4S_SSC_DBLFRM_LONGWORDS*SAM4S_SSC_BUF_DBLFRAMES];
volatile int sam4s_ssc_tx_last_dblfrm;
static int sam4s_ssc_tx_curr_dblfrm;

struct sam4s_ssc_irqstats {
	unsigned int tx_ctr;
	unsigned int tx_underflow;
	unsigned int rx_ctr;
	unsigned int rx_overflow;
};

static struct sam4s_ssc_irqstats sam4s_ssc_irqstats;

void
sam4s_ssc_get_irqstats(struct sam4s_ssc_irqstats *p)
{
	__disable_irq();
	memcpy(p, &sam4s_ssc_irqstats, sizeof(sam4s_ssc_irqstats));
	__enable_irq();
}

/* start or re-start the DMA, also happens on over/underflow which
   basically only happens during debugging (when CPU is stopped) */
static void
sam4s_ssc_init_rx_dma() {
	/* enable DMA for SSC, receive and transmit...
	  first next pointer register, then pointer register and counters */
	sam4s_ssc_rx_last_dblfrm = -1;
	sam4s_ssc_rx_curr_dblfrm = 0;
	PDC_SSC->PERIPH_RPR = (uint32_t)&sam4s_ssc_rx_buf;
	PDC_SSC->PERIPH_RCR = SAM4S_SSC_DBLFRM_LONGWORDS;
	PDC_SSC->PERIPH_RNPR = (uint32_t)&sam4s_ssc_rx_buf[SAM4S_SSC_DBLFRM_LONGWORDS];
	PDC_SSC->PERIPH_RNCR = SAM4S_SSC_DBLFRM_LONGWORDS;
}

static void
sam4s_ssc_init_tx_dma() {
	sam4s_ssc_tx_last_dblfrm = -1;
	sam4s_ssc_tx_curr_dblfrm = 0;
	PDC_SSC->PERIPH_TPR = (uint32_t)&sam4s_ssc_tx_buf;
	PDC_SSC->PERIPH_TCR = SAM4S_SSC_DBLFRM_LONGWORDS;
	PDC_SSC->PERIPH_TNPR = (uint32_t)&sam4s_ssc_tx_buf[SAM4S_SSC_DBLFRM_LONGWORDS];
	PDC_SSC->PERIPH_TNCR = SAM4S_SSC_DBLFRM_LONGWORDS;
}

void
SSC_Handler()
{
	/* Receiver */
	/* "current" rx buffer has finished receiving, but we have to submit
	   the one after the next (rxbuf_submit) to keep the queue full */
	if (SSC->SSC_SR & SSC_SR_ENDRX) {
		int cp = sam4s_ssc_rx_curr_dblfrm;

		if (SSC->SSC_SR & SSC_SR_RXBUFF) {
			/* should never happen! */
			sam4s_ssc_irqstats.rx_overflow++;
			sam4s_ssc_init_rx_dma();
		}

		sam4s_ssc_rx_last_dblfrm = cp;

		/* this is the period that is currently being received */
		cp = (cp + 1) % SAM4S_SSC_BUF_DBLFRAMES;
		sam4s_ssc_rx_curr_dblfrm = cp;

		/* this is the next period that will be received */
		cp = ( cp + 1 ) % SAM4S_SSC_BUF_DBLFRAMES;
		PDC_SSC->PERIPH_RNPR = (uint32_t) &sam4s_ssc_rx_buf[cp*SAM4S_SSC_DBLFRM_LONGWORDS];
		PDC_SSC->PERIPH_RNCR = SAM4S_SSC_DBLFRM_LONGWORDS;

		e1_mgmt_rx_dblfrm_irq(&sam4s_ssc_rx_buf[sam4s_ssc_rx_last_dblfrm*SAM4S_SSC_DBLFRM_LONGWORDS]);

		/* debug */
		sam4s_ssc_irqstats.rx_ctr++;
		sam4s_pinmux_gpio_set(SAM4S_PINMUX_PA(25),sam4s_ssc_irqstats.rx_ctr & 1);
	}

	if (SSC->SSC_SR & SSC_SR_ENDTX) {
		int cp = sam4s_ssc_tx_curr_dblfrm;

		if (SSC->SSC_SR & SSC_SR_TXBUFE) {
			sam4s_ssc_irqstats.tx_underflow++;
			sam4s_ssc_init_tx_dma();
		}

		sam4s_ssc_tx_last_dblfrm = cp;

		cp = (cp + 1) % SAM4S_SSC_BUF_DBLFRAMES;
		sam4s_ssc_tx_curr_dblfrm = cp;

		cp = ( cp + 1 ) % SAM4S_SSC_BUF_DBLFRAMES;
		PDC_SSC->PERIPH_TNPR = (uint32_t) &sam4s_ssc_tx_buf[cp * SAM4S_SSC_DBLFRM_LONGWORDS];
		PDC_SSC->PERIPH_TNCR = SAM4S_SSC_DBLFRM_LONGWORDS;

		sam4s_ssc_irqstats.tx_ctr++;
		sam4s_pinmux_gpio_set(SAM4S_PINMUX_PA(26),sam4s_ssc_irqstats.tx_ctr & 1);
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

	/* ======== RECEIVE ======== */

	/* RK is an input, receive clock */
	SSC->SSC_RCMR = SSC_RCMR_CKS_RK | /* RK receive clock pin */
		SSC_RCMR_CKO_NONE | /* RK is an input */
		/* SSC_RCMR_CKI | */ /* sample on falling(0)/rising/(CKI) edge */
		SSC_RCMR_CKG_CONTINUOUS | /* no clock gating */
		SSC_RCMR_START_RF_RISING; /* on RF(frame) rising edge */


	/* 32 bit per word, MSB first, SAM4S_SSC_DBLFRM_LONGWORDS words */
	SSC->SSC_RFMR = SSC_RFMR_DATLEN(SAM4S_SSC_BITS_PER_LONGWORD-1) |
		SSC_RFMR_MSBF |
		SSC_RFMR_DATNB(SAM4S_SSC_DBLFRM_LONGWORDS-1);

	/* ========= TRANSMIT ======== */

	/* TK is an output, transmit clock, shifted on rising edge (CKI) */
	SSC->SSC_TCMR = SSC_TCMR_CKS_MCK | SSC_TCMR_CKO_CONTINUOUS | SSC_TCMR_CKI;

	/* 32 bit data, MSB first */
	SSC->SSC_TFMR = SSC_TFMR_DATLEN(SAM4S_SSC_BITS_PER_LONGWORD-1) | SSC_TFMR_MSBF;

	/* enable receiver and transmitter */
	SSC->SSC_CR = SSC_CR_TXEN | SSC_CR_RXEN;

	sam4s_ssc_init_rx_dma();
	sam4s_ssc_init_tx_dma();

	/* enable DMA on Rx and Tx, then enable interrupts */
	PDC_SSC->PERIPH_PTCR = PERIPH_PTCR_RXTEN | PERIPH_PTCR_TXTEN;
	SSC->SSC_IER = SSC_IER_ENDTX| SSC_IER_ENDRX;

	/* set highest priority */
	NVIC_SetPriority (SSC_IRQn, (1UL << __NVIC_PRIO_BITS) - 1UL);
	NVIC_EnableIRQ(SSC_IRQn);
}
