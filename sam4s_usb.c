#include "sam4s_usb.h"
#include "sam4s_clock.h"
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
#define UDP_IxR_EPxINT 0xff /* UDP_ISR_EP0INT, EP1INT... EP7INT */
#define UDP_IxR_EPnINT(n) (1U << ((n) & 0x7))

#define CSR_NOEFFECT_BITS   (UDP_CSR_RX_DATA_BK0 | UDP_CSR_RX_DATA_BK1 | \
                             UDP_CSR_STALLSENT | UDP_CSR_RXSETUP | \
                             UDP_CSR_TXCOMP)

#define USB_REQ_DIR_IN (1<<7)  /* highest bit: 1: in, 0: out */

struct usb_ctrlreq {
	uint8_t type;
	uint8_t req;
	uint16_t value;
	uint16_t index;
	uint16_t len;
} __attribute__((packed));

enum sam4s_usb_ep_state {
	SAM4S_USB_EP_DISABLED,
	SAM4S_USB_EP_STALLED,
	SAM4S_USB_EP_IDLE,
	SAM4S_USB_EP_SENDING,
	SAM4S_USB_EP_RXSTOPPED,
	SAM4S_USB_EP_EP0_DATA_OUT,
	SAM4S_USB_EP_EP0_STATUS_IN,
	SAM4S_USB_EP_EP0_ADDRESS
};

enum sam4s_usb_dev_state {
	SAM4S_USB_DEV_SUSPENDED,
	SAM4S_USB_DEV_POWERED,
	SAM4S_USB_DEV_DEFAULT,
	SAM4S_USB_DEV_ADDRESSED,
	SAM4S_USB_DEV_CONFIGURED
};

/* state of our endpoints */
enum sam4s_usb_ep_state sam4s_usb_ep_state[SAM4S_USB_NENDP];
enum sam4s_usb_dev_state sam4s_usb_dev_state;

/* last bank read from this endpoint? */
unsigned char sam4s_usb_lastbank[SAM4S_USB_NENDP];
unsigned char sam4s_usb_devaddr;

struct usb_ctrlreq sam4s_usb_ctrl; /* global buffer for control requests */

/* some operations on the CSR register need a few NOPs */
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


/* go either in the addressed (addr==0) or default (addr!=0)
   state, see state diagram §40.6.3, Fig 40-14 USB Device State Diagram */
static void
sam4s_usb_setaddr(unsigned int addr)
{
	if (addr) {
		/* toto addressed state */
		UDP->UDP_FADDR = addr | UDP_GLB_STAT_FADDEN;
		UDP->UDP_GLB_STAT = (UDP->UDP_GLB_STAT | UDP_GLB_STAT_FADDEN) &
			~UDP_GLB_STAT_CONFG;
	} else {
		UDP->UDP_FADDR = UDP_FADDR_FEN;
		UDP->UDP_GLB_STAT = UDP->UDP_GLB_STAT &
			~(UDP_GLB_STAT_CONFG | UDP_GLB_STAT_FADDEN);
	}
}

static void
sam4s_usb_ep0setup() {

}

/* data is received via two banks (0, 1) per endpoint */
static void
sam4s_usb_handle_bankint(unsigned int ep, int bank)
{
	uint32_t pktsize = UDP_CSR_RXBYTECNT(UDP->UDP_CSR[ep]);
	unsigned char c, *p;

	/* normal payload */
	if (sam4s_usb_ep_state[ep] == SAM4S_USB_EP_IDLE) {
		while (pktsize--)
			c = UDP->UDP_FDR[ep];
	/* control endpoint data */
	} else if (sam4s_usb_ep_state[ep] == SAM4S_USB_EP_EP0_DATA_OUT) {
		/* should always be ep==0 and bank == 0! */

		p = sam4s_usb_ctrlbuf;
		while (pktsize--)
			*p++ = UDP->UDP_FDR[ep];
		sam4s_usb_ep0setup();
	}
	sam4s_usb_lastbank[ep] = bank;

	/* clear bank status bit */
	sam4s_usb_csr_clr(ep, bank ? UDP_CSR_RX_DATA_BK1 : UDP_CSR_RX_DATA_BK0);
}

static void
sam4s_usb_handle_epint(unsigned int ep)  /* nuttx: sam_ep_interrupt */
{
	uint32_t csr = UDP->UDP_CSR[ep];
	enum sam4s_usb_ep_state *state = & sam4s_usb_ep_state[ep];
	unsigned int bank0, bank1;
	char *p;

	/* §40.6.2.2 Data IN transactions are used ..
	   conduct the transfer of data from the device to the host */

	/* Data IN transaction is achieved, acknowledged by the Host */
	if (csr & UDP_CSR_TXCOMP) {
		sam4s_usb_csr_clr(ep, UDP_CSR_TXCOMP);

		/* completion of a normal write request */
		if (*state == SAM4S_USB_EP_SENDING ||
		    *state == SAM4S_USB_EP_EP0_STATUS_IN
		) {
			/* sam req_write() */
		} else if (*state == SAM4S_USB_EP_EP0_ADDRESS) {
			sam4s_usb_setaddr(sam4s_usb_devaddr);
		} else {
			/* error, unexpected state! */
		}
		*state = SAM4S_USB_EP_IDLE;

	}

	/* in case both banks have data, we have to choose the order
	   according to the last time the interrupt had fired... */
	if (csr & (UDP_CSR_RX_DATA_BK0 | UDP_CSR_RX_DATA_BK1)) {
		/* last bank was 1 -> read bank 0 first, and vice versa */
		if (sam4s_usb_lastbank[ep] == 1) {
			if (csr & UDP_CSR_RX_DATA_BK0)
				sam4s_usb_handle_bankint(ep, 0);
			if (csr & UDP_CSR_RX_DATA_BK1)
				sam4s_usb_handle_bankint(ep, 1);
		} else {
			if (csr & UDP_CSR_RX_DATA_BK1)
				sam4s_usb_handle_bankint(ep, 1);
			if (csr & UDP_CSR_RX_DATA_BK0)
				sam4s_usb_handle_bankint(ep, 0);
		}
	}

	if (csr & UDP_CSR_STALLSENT) {
		sam4s_usb_csr_clr(ep, UDP_CSR_STALLSENT);
		if (UDP_CSR_EPTYPE(csr) == UDP_CSR_EPTYPE_ISO_IN ||
		    UDP_CSR_EPTYPE(csr) == UDP_CSR_EPTYPE_ISO_OUT
		) {
			/* isochronous error! */
			*state = SAM4S_USB_EP_IDLE;
		} else if (*state != SAM4S_USB_EP_STALLED) {
			sam4s_usb_csr_clr(ep, UDP_CSR_FORCESTALL);			
		}
	}

	/* setup packet received see line 2157 of nuttx sam_udp.c */
	if (csr & UDP_CSR_RXSETUP) {
		unsigned int len;
		unsigned char *dst;

		dst = (unsigned char *)&sam4s_usb_ctrl;
		len = UDP_CSR_RXBYTECNT(csr);
		while (len--)
			*dst = UDP->UDP_FDR[0];

		/* request is OUT */
		if (!(sam4s_usb_ctrl.req & USB_REQ_DIR_IN)) {
			sam4s_usb_ep_state[ep] = SAM4S_USB_EP_EP0_DATA_OUT;
			sam4s_usb_csr_clr(ep, UDP_CSR_DIR);
			sam4s_usb_csr_clr(ep, UDP_CSR_RXSETUP);		
		} else {
			sam4s_usb_ep_state[ep] = SAM4S_USB_EP_IDLE;
			sam4s_usb_csr_clr(ep, UDP_CSR_RXSETUP);		
			sam4s_usb_ctrl_setup();
		}
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
			UDP->UDP_IER = UDP_IER_WAKEUP | UDP_IER_RXRSM;
			UDP->UDP_ICR = UDP_ICR_RXSUSP;
			/* XXX suspend board, not implemented! */

			sam4s_usb_dev_state = SAM4S_USB_DEV_SUSPENDED;
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

			/* fix me, should be state saved during suspend time */
			sam4s_usb_dev_state = SAM4S_USB_DEV_DEFAULT;

			break;
		}

		if (irq_pending & UDP_ISR_ENDBUSRES) {
			UDP->UDP_ICR = UDP_ICR_ENDBUSRES;
			break;
		}

		if (irq_pending & UDP_IxR_EPxINT) {
			for (i=0; i<SAM4S_USB_NENDP; i++) {
				if (irq_pending & UDP_IxR_EPnINT(i))
					sam4s_usb_handle_epint(i);
			}
			break;
		}
	}
}

/* setup is used to begin control transfers */
static void
sam4s_usb_ep0_setup()
{

}

static void
sam4s_usb_ep_reset(unsigned int ep)
{
	if (ep >= SAM4S_USB_NENDP)
		return;

	UDP->UDP_IDR =  1<<ep;   /* disable interrupt */
	UDP->UDP_RST_EP = 1<<ep; /* reset endpoint */
	UDP->UDP_RST_EP = 0;

	sam4s_usb_ep_state[ep] = SAM4S_USB_EP_DISABLED;
}

static void
sam4s_usb_full_reset()
{
	int i;

	sam4s_usb_devaddr = 0;
	sam4s_usb_setaddr(0);
	sam4s_usb_dev_state = SAM4S_USB_DEV_DEFAULT;
	for (i=0; i<SAM4S_USB_NENDP; i++) {
		sam4s_usb_ep_reset(i);
		UDP->UDP_CSR[i] = 0;
	}

	UDP->UDP_ICR = UDP_ICR_WAKEUP | UDP_ICR_ENDBUSRES | UDP_ICR_SOFINT | UDP_ICR_RXSUSP;
	UDP->UDP_IDR = UDP_ICR_SOFINT | UDP_ICR_RXSUSP | UDP_IxR_EPxINT;
	UDP->UDP_IER = UDP_IER_WAKEUP | UDP_IER_RXSUSP | UDP_IER_EP0INT;
}

void
sam4s_usb_init()
{
	int i;

	NVIC_DisableIRQ(UDP_IRQn);
	UDP->UDP_IDR = UDP->UDP_ISR; /* disable all interrupts */

	sam4s_clock_peripheral_onoff(ID_UDP, 1 /* on */);
	sam4s_usb_full_reset();

	UDP->UDP_GLB_STAT = UDP_GLB_STAT_RMWUPE;

	/* Enable DDM(PB10), DDP(PB11), should be the default, but I got
	   confused along the way and better leave it in here explicitly. */
	MATRIX->CCFG_SYSIO &= ~(CCFG_SYSIO_SYSIO11|CCFG_SYSIO_SYSIO10);

	/* enable USB peripheral, enable pull up resistor */
	UDP->UDP_TXVC = UDP_TXVC_PUON;

	/* configure endpoint 0 as control endpoint */
	UDP->UDP_CSR[0] = 0;
	UDP->UDP_CSR[0] = UDP_CSR_EPTYPE_CTRL;
	UDP->UDP_IER = UDP_IER_EP0INT; /* enable endpoint 0 interrupt */
	sam4s_usb_ep_state[0] =  SAM4S_USB_EP_IDLE;

	NVIC_SetPriority (UDP_IRQn, 1);
	NVIC_EnableIRQ(UDP_IRQn);
}