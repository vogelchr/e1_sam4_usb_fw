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

/* ==== TRANSMITTING ==== */

/* externally visible buffer for data that needs to be transmitted */
unsigned char sam4s_ssc_txbuf[SAM4S_SSC_BUF_TX_NFRAMES*SAM4S_SSC_FRAME_BYTES];
static int sam4s_ssc_tx_curr_frame; /* internally used: currently active frame */
int sam4s_ssc_tx_last_frame;        /* externally used: last successfully sent frame */


/* ==== RECEIVING ==== */

#define SAM4S_SSC_BUF_RXRAW_NFRAMES 2   /* we only use 2 buffers for non-aligned frame data */

/* internal buffer to receive raw, unaligned frames */
static unsigned char sam4s_ssc_rxraw_buf[SAM4S_SSC_BUF_RXRAW_NFRAMES*SAM4S_SSC_FRAME_BYTES];
static int sam4s_ssc_rxraw_curr_frame;

/* externally visible buffer for received realigned data */
unsigned char sam4s_ssc_rx_buf[SAM4S_SSC_BUF_RX_NFRAMES * SAM4S_SSC_FRAME_BYTES];
int sam4s_ssc_rx_last_frame;        /* ext. used: last sucecssfully read & realigned */

/* internally used... */
static unsigned char *sam4s_ssc_rx_writep; /* next char to be written */
static unsigned char *sam4s_ssc_rx_buf_end = sam4s_ssc_rx_buf + sizeof(sam4s_ssc_rx_buf);
static uint32_t sam4s_ssc_rx_shiftreg;     /* carry fractional octets over... */
static int sam4s_ssc_rx_align;             /* number of bits aligned data trains input data */

/* ==== STATISTICS ==== */

unsigned long sam4s_ssc_rx_irq_ctr; /* statistics */
unsigned long sam4s_ssc_tx_irq_ctr; /* statistics */

void
sam4s_ssc_realign_reset(int align)
{
	int i;

	align = align % SAM4S_SSC_FRAME_BYTES;

	__disable_irq();

	sam4s_ssc_rx_align = align;
	sam4s_ssc_rx_last_frame = -1;
	sam4s_ssc_rx_writep = sam4s_ssc_rx_buf + (align / 8);
	sam4s_ssc_rx_shiftreg = 0;

	for (i=0; i<SAM4S_SSC_FRAME_BYTES; i++)
		sam4s_ssc_rx_buf[i] = 0;

	__enable_irq();
}

/* to be called by the ISR */
static void
sam4s_ssc_realign_frame(unsigned char *readp)
{
	int i;
	unsigned char c;
	int align_fract = sam4s_ssc_rx_align % 8;
	unsigned char *writep = sam4s_ssc_rx_writep;
	unsigned char shiftreg = sam4s_ssc_rx_shiftreg;

	for (i=0; i<SAM4S_SSC_FRAME_BYTES; i++) {
		c = *readp++;
		*writep++ = ( c >> align_fract ) | shiftreg;
		shiftreg = c << (8-align_fract);
		if (writep == sam4s_ssc_rx_buf_end) /* wraparound */
			writep = sam4s_ssc_rx_buf;
	}

	sam4s_ssc_rx_shiftreg = shiftreg;
	sam4s_ssc_rx_writep = writep;
	sam4s_ssc_rx_last_frame = (sam4s_ssc_rx_last_frame + 1) % SAM4S_SSC_BUF_RX_NFRAMES;
}

void
SSC_Handler()
{
	/* Receiver */
	/* "current" rx buffer has finished receiving, but we have to submit
	   the one after the next (rxbuf_submit) to keep the queue full */
	if (SSC->SSC_SR & SSC_SR_ENDRX) {
		int cp = sam4s_ssc_rxraw_curr_frame;

		sam4s_ssc_realign_frame(&sam4s_ssc_rxraw_buf[cp]);

		/* this is the period that is currently being received */
		cp = (cp + 1) % SAM4S_SSC_BUF_RXRAW_NFRAMES;
		sam4s_ssc_rxraw_curr_frame = cp;

		/* this is the next period that will be received */
		cp = ( cp + 1 ) % SAM4S_SSC_BUF_RXRAW_NFRAMES;
		PDC_SSC->PERIPH_RNPR = (uint32_t) &sam4s_ssc_rxraw_buf[cp * SAM4S_SSC_FRAME_BYTES];
		PDC_SSC->PERIPH_RNCR = SAM4S_SSC_FRAME_BYTES;

		/* debug */
		sam4s_ssc_rx_irq_ctr++;
		sam4s_pinmux_gpio_set(SAM4S_PINMUX_PA(25),sam4s_ssc_rx_irq_ctr & 1);
	}

	if (SSC->SSC_SR & SSC_SR_ENDTX) {
		int cp = sam4s_ssc_tx_curr_frame;
	
		sam4s_ssc_tx_last_frame = cp;

		cp = (cp + 1) % SAM4S_SSC_BUF_TX_NFRAMES;
		sam4s_ssc_tx_curr_frame = cp;

		cp = ( cp + 1 ) % SAM4S_SSC_BUF_TX_NFRAMES;
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
	sam4s_ssc_tx_curr_frame = 0;

	sam4s_ssc_rxraw_curr_frame = 0;
	sam4s_ssc_realign_reset(0);

	/* enable DMA for SSC, receive and transmit...
	  first next pointer register, then pointer register and counters */
	PDC_SSC->PERIPH_RNPR = (uint32_t)&sam4s_ssc_rxraw_buf[SAM4S_SSC_FRAME_BYTES];
	PDC_SSC->PERIPH_RNCR = SAM4S_SSC_FRAME_BYTES;
	PDC_SSC->PERIPH_RPR = (uint32_t)&sam4s_ssc_rxraw_buf;
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
