#include "sam4s_usb.h"
#include <sam4s4c.h>

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

/* handle SAM4S USB communications */

/*
   this file is heavily inspired by, and replicating in large parts
   nuttx/blob/master/arch/arm/src/sam34/sam_udp.c
*/

#define SAM4S_USB_NENDP ((int)(sizeof(UDP->UDP_CSR) / sizeof(UDP->UDP_CSR[0])))
#define UDP_ISR_EPxINT 0xff /* UDP_ISR_EP0INT, EP1INT... EP7INT */
#define UDP_ISR_EPnINT(n) (1U << ((n) & 0x7))

#define CSR_NOEFFECT_BITS   (UDP_CSR_RX_DATA_BK0 | UDP_CSR_RX_DATA_BK1 | \
                             UDP_CSR_STALLSENT | UDP_CSR_RXSETUP | \
                             UDP_CSR_TXCOMP)



static inline void
sam4s_usb_nops(unsigned int n) {
	while (n)
		asm volatile ("nop"::);
}

static inline void /* nuttx: sam_csr_clrbits */
sam4s_usb_csr_clr(unsigned int ep, uint32_t mask) {
	uint32_t csr = UDP->UDP_CSR[ep];
	csr |= CSR_NOEFFECT_BITS; /* must be set 1 for no change! */
	csr &= ~mask;
	UDP->UDP_CSR[ep] = csr;
	sam4s_usb_nops(16);
}

static inline void /* nuttx: sam_csr_setbits */
sam4s_usb_csr_set(unsigned int ep, uint32_t mask) {
	uint32_t csr = UDP->UDP_CSR[ep];
	csr |= mask;
	csr |= CSR_NOEFFECT_BITS; /* must be set to 1 for no change! */
	UDP->UDP_CSR[ep] = csr;
	sam4s_usb_nops(16);
}

static void
sam4s_usb_handle_epint(unsigned int ep)  /* nuttx: sam_ep_interrupt */
{
	uint32_t csr = UDP->UDP_CSR[ep];

	if (csr & UDP_CSR_TXCOMP) {
		sam4s_usb_csr_clr(ep, UDP_CSR_TXCOMP);
	}
}

void
UDP_Handler()
{
	uint32_t irq_pending;
	unsigned int i;

	while (1) {
		irq_pending = UDP->UDP_ISR & UDP->UDP_IMR;
		if (!irq_pending)
			break;

		/* handled last, therefore we compare ==, not & */
		if (irq_pending == UDP_ISR_RXSUSP) {
			UDP->UDP_IDR = UDP_IDR_RXSUSP;
			UDP->UDP_IER = UDP_IER_WAKEUP;
			UDP->UDP_ICR = UDP_ICR_RXSUSP;
			/* XXX suspend board, not implemented! */
			break;
		}

		/* start of frame */
		if (irq_pending & UDP_ISR_SOFINT) {
			UDP->UDP_ICR = UDP_ICR_SOFINT;
			break;
		}

		if (irq_pending & (UDP_ISR_RXRSM|UDP_ISR_WAKEUP)) {
			/* XXX wakeup board, not implemented! */
			UDP->UDP_ICR = (UDP_ICR_WAKEUP|UDP_ICR_RXRSM|UDP_ICR_WAKEUP);
			/* disable wakeup interrupt */
			UDP->UDP_IDR = UDP_IDR_RXRSM | UDP_IDR_WAKEUP;
			/* enable suspend interrupt */
			UDP->UDP_IER = UDP_IER_RXSUSP;
			break;
		}

		if (irq_pending & UDP_ISR_ENDBUSRES) {
			UDP->UDP_ICR = UDP_ICR_ENDBUSRES;
			break;
		}

		if (irq_pending & UDP_ISR_EPxINT) {
			for (i=0; i<SAM4S_USB_NENDP; i++) {
				if (irq_pending & UDP_ISR_EPnINT(i))
					sam4s_usb_handle_epint(i);
			}
			break;
		}
	}
}

static void
sam4s_usb_ep_reset(unsigned int ep)
{
	if (ep >= SAM4S_USB_NENDP)
		return;

	UDP->UDP_IDR =  1<<ep;   /* disable interrupt */
	UDP->UDP_RST_EP = 1<<ep; /* reset endpoint */
}

void
sam4s_usb_init()
{
	int i;

	for (i=0; i<SAM4S_USB_NENDP; i++) {
		sam4s_usb_ep_reset(i);
		UDP->UDP_CSR[i] = 0;
	}

	UDP->UDP_GLB_STAT = UDP_GLB_STAT_RMWUPE;
	UDP->UDP_IDR = UDP->UDP_ISR; /* disable all interrupts */

	UDP->UDP_TXVC = UDP_TXVC_PUON;

	NVIC_SetPriority (UDP_IRQn, 1);
	NVIC_EnableIRQ(UDP_IRQn);
}