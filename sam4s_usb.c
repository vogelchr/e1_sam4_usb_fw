#include "sam4s_usb.h"
#include "sam4s_clock.h"
#include "sam4s_usb_descriptors.h"
#include "trace_util.h"
#include <sam4s4c.h>
#include <unistd.h>
#include <string.h>

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

#define TRACE_TAG_USB(l) ('u' | ('s' << 8) | ((l) << 16))

#define TRACE(v)      trace_util_user(TRACE_TAG_USB(__LINE__), (v))
#define TRACE_IRQ(v)  trace_util_in_irq(TRACE_TAG_USB(__LINE__), (v))

#define SAM4S_USB_NENDP ((int)(sizeof(UDP->UDP_CSR) / sizeof(UDP->UDP_CSR[0])))

/* convenience macros for registers */

#define UDP_IxR_EPxINT 0xff /* UDP_ISR_EP0INT, EP1INT... EP7INT */
#define UDP_IxR_EPnINT(n) (1U << ((n) & 0x7))
#define RXBYTECNT(ep) ((UDP->UDP_CSR[(ep)]>>UDP_CSR_RXBYTECNT_Pos)&UDP_CSR_RXBYTECNT_Msk)
#define CSR_NOEFFECT_BITS   (UDP_CSR_RX_DATA_BK0 | UDP_CSR_RX_DATA_BK1 | \
                             UDP_CSR_STALLSENT | UDP_CSR_RXSETUP | \
                             UDP_CSR_TXCOMP)

#define BMREQUESTTYPE_DIR_MASK        (0x80)
#define BMREQUESTTYPE_DIR_DEV_TO_HOST (0x80) /* in */
#define BMREQUESTTYPE_DIR_HOST_TO_DEV (0x00) /* out */
#define BMREQUESTTYPE_DIR(v)          ((v) & 0x80)

#define BMREQUESTTYPE_TYPE_MASK   (0x60)
#define BMREQUESTTYPE_TYPE_STD    (0x00)  /* D6..5=0, Standard */
#define BMREQUESTTYPE_TYPE_CLASS  (0x20)  /* D6..5=1, Class */
#define BMREQUESTTYPE_TYPE_VENDOR (0x40)  /* D6..5=2, Vendor */
#define BMREQUESTTYPE_TYPE_RES    (0x60)  /* D6..5=3, Reserved */
#define BMREQUESTTYPE_TYPE(v)     ((v) & BMREQUESTTYPE_TYPE_MASK)

#define BMREQUESTTYPE_RECP_MASK   (0x1f)
#define BMREQUESTTYPE_RECP_DEV    (0x00)
#define BMREQUESTTYPE_RECP_INTF   (0x01)
#define BMREQUESTTYPE_RECP_ENDP   (0x02)
#define BMREQUESTTYPE_RECP_OTH    (0x03)
#define BMREQUESTTYPE_RECP(v)     ((v) & BMREQUESTTYPE_RECP_MASK)

#define BREQUEST_STD_GETSTATUS         0x00
#define BREQUEST_STD_CLEAR_FEATURE     0x01
#define BREQUEST_STD_SET_FEATURE       0x03
#define BREQUEST_STD_SET_ADDRESS       0x05
#define BREQUEST_STD_GET_DESCRIPTOR    0x06
#define BREQUEST_STD_SET_DESCRIPTOR    0x07
#define BREQUEST_STD_GET_CONFIGURATIOn 0x08
#define BREQUEST_STD_SET_CONFIGURATIOn 0x09

struct usb_ctrlreq {
	uint8_t bmRequestType;
	uint8_t bRequest;
	uint16_t wValue;   /* little endian! */
	uint16_t wIndex;
	uint16_t wLength;
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

/* §40.6.3, Fig 40-14 USB Device Status Diagram */

enum sam4s_usb_dev_state {
	SAM4S_USB_DEV_POWERED = 0,
	SAM4S_USB_DEV_ATTACHED = 1,
	SAM4S_USB_DEV_DEFAULT = 2,
	SAM4S_USB_DEV_ADDRESSED = 3,
	SAM4S_USB_DEV_CONFIGURED = 4,
	SAM4S_USB_DEV_SUSPENDED = 0x80,  /* not used directly, but or-ed */
	SAM4S_USB_DEV_SUSP_POWERED = SAM4S_USB_DEV_POWERED | SAM4S_USB_DEV_SUSPENDED,
	SAM4S_USB_DEV_SUSP_DEFAULT = SAM4S_USB_DEV_DEFAULT | SAM4S_USB_DEV_SUSPENDED,
	SAM4S_USB_DEV_SUSP_ADDRESSED = SAM4S_USB_DEV_ADDRESSED | SAM4S_USB_DEV_SUSPENDED,
	SAM4S_USB_DEV_SUSP_CONFIGURED = SAM4S_USB_DEV_CONFIGURED | SAM4S_USB_DEV_SUSPENDED
};

/* state of our endpoints and device */
enum sam4s_usb_ep_state sam4s_usb_ep_state[SAM4S_USB_NENDP];
enum sam4s_usb_dev_state sam4s_usb_dev_state;

/* last bank read from this endpoint? */
unsigned char sam4s_usb_lastbank[SAM4S_USB_NENDP];
unsigned char sam4s_usb_devaddr;

struct usb_ctrlreq sam4s_usb_ctrl; /* global buffer for control requests */
unsigned char sam4s_usb_ep0buf[64]; /* buffer for receiving payload of control transfers */
unsigned int sam4s_usb_ep0buf_len;  /* number of bytes used within buffer */

static inline void
sam4s_usb_cp_ep0buf(unsigned char *src, unsigned int len)
{
	unsigned char *dst = sam4s_usb_ep0buf + sam4s_usb_ep0buf_len;
	while (sam4s_usb_ep0buf_len <= sizeof(sam4s_usb_ep0buf) && len) {
		*dst++ = *src++;
		len--;
		sam4s_usb_ep0buf_len++;
	}
}

#define SAM4S_USB_CP_EP0BUF_OBJ(x) \
		sam4s_usb_cp_ep0buf((unsigned char*)&(x), sizeof(x))

/* some operations on the CSR register need a few NOPs */
static inline void
sam4s_usb_nops(unsigned int n) {
	while (n--)
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
sam4s_usb_ep_reset(unsigned int ep)
{
	if (ep >= SAM4S_USB_NENDP)
		return;

	UDP->UDP_IDR = UDP_IxR_EPnINT(ep);   /* disable interrupt */
	UDP->UDP_ICR = UDP_IxR_EPnINT(ep);   /* acknowledge interrupt */

	UDP->UDP_RST_EP = 1<<ep; /* reset endpoint fifo */
	UDP->UDP_CSR[ep] = 0;
	UDP->UDP_RST_EP = 0;     /* clear endpoint reset flag */

	sam4s_usb_ep_state[ep] = SAM4S_USB_EP_DISABLED;
}

static inline void
sam4s_usb_cp_to_fdr(unsigned int ep, const unsigned char *buf, unsigned int len)
{
	while (len--)
		UDP->UDP_FDR[ep] = *buf++;
}

static inline unsigned int
sam4s_usb_cp_from_fdr(unsigned int ep, unsigned char *dst, unsigned int rxbytecnt, unsigned int dstlen)
{
	unsigned int ret = 0;
	volatile char c;

	while (dstlen && rxbytecnt) {
		*dst++ = UDP->UDP_FDR[ep];
		dstlen--;
		rxbytecnt--;
		ret++;
	}

	while (rxbytecnt--)
		c = UDP->UDP_FDR[ep];

	return ret;
}

/* go either in the addressed (addr==0) or default (addr!=0)
   state, see state diagram §40.6.3, Fig 40-14 USB Device State Diagram */
static void
sam4s_usb_setaddr(unsigned int addr)
{
	if (addr) {
		/* to addressed state */
		UDP->UDP_FADDR = addr | UDP_FADDR_FEN;
		UDP->UDP_GLB_STAT = (UDP->UDP_GLB_STAT | UDP_GLB_STAT_FADDEN) &
			~UDP_GLB_STAT_CONFG;
		sam4s_usb_dev_state = SAM4S_USB_DEV_ADDRESSED;
	} else {
		UDP->UDP_FADDR = UDP_FADDR_FEN;
		UDP->UDP_GLB_STAT = UDP->UDP_GLB_STAT &
			~(UDP_GLB_STAT_CONFG | UDP_GLB_STAT_FADDEN);
		sam4s_usb_dev_state = SAM4S_USB_DEV_DEFAULT;
	}
}

/* handler for when we have recveived a SETUP transaction on our control
   endpoint, *or* the SETUP transaction followed by a data-out transfer
   the payload of which we have stored in sam4s_usb_ep0buf */
static void
sam4s_usb_handle_ep0_setup() {
	int wrlen = 0;   /* set to -1 to stall */

	TRACE_IRQ(sam4s_usb_ctrl.bmRequestType |
		(sam4s_usb_ctrl.bRequest << 8) |
		(sam4s_usb_ctrl.wLength << 16));

	/* only handle standard requests for now! */
	if (BMREQUESTTYPE_TYPE(sam4s_usb_ctrl.bmRequestType) !=
	    BMREQUESTTYPE_TYPE_STD
	) {
		TRACE_IRQ(0xffffffff);
		wrlen = -1; /* stall */
		goto out;
	}

	if (sam4s_usb_ctrl.bRequest == BREQUEST_STD_GETSTATUS) {
		if (sam4s_usb_ctrl.wValue != 0 ||
		    sam4s_usb_ctrl.wLength != 2
		) {
			TRACE_IRQ(0xffffffff);
			wrlen = -1;
		} else {
			uint16_t ep = 0, v;

			switch (BMREQUESTTYPE_RECP(sam4s_usb_ctrl.bmRequestType)) {
			case BMREQUESTTYPE_RECP_ENDP:
				ep = sam4s_usb_ctrl.wIndex;
				if (ep >= SAM4S_USB_NENDP)
					ep = 0;
				if (sam4s_usb_ep_state[ep] == SAM4S_USB_EP_STALLED)
					v = 1;
				break;
			case BMREQUESTTYPE_RECP_DEV:
				v = 0; /* could be REMOTEWAKEUP capable */
				break;
			case BMREQUESTTYPE_RECP_INTF:
				v = 0;
				break;
			}
			/* send back status */
			sam4s_usb_ep0buf_len = 0;
			SAM4S_USB_CP_EP0BUF_OBJ(v);
			wrlen = sizeof(v);
		}
	} else if (sam4s_usb_ctrl.bRequest == BREQUEST_STD_CLEAR_FEATURE) {
		TRACE_IRQ(0xffffffff);
		wrlen = -1;
		/* TODO */
	} else if (sam4s_usb_ctrl.bRequest == BREQUEST_STD_SET_FEATURE) {
		TRACE_IRQ(0xffffffff);
		wrlen = -1;
		/* TODO */
	} else if (sam4s_usb_ctrl.bRequest == BREQUEST_STD_SET_ADDRESS) {
		TRACE_IRQ( sam4s_usb_ctrl.wValue);
		sam4s_usb_devaddr = sam4s_usb_ctrl.wValue;
	} else if (sam4s_usb_ctrl.bRequest == BREQUEST_STD_GET_DESCRIPTOR) {
		/* descriptor type */
		uint8_t dt = sam4s_usb_ctrl.wValue >> 8;
		sam4s_usb_ep0buf_len = 0;
		TRACE_IRQ(dt);
		if (dt == LIBUSB_DT_DEVICE) {
			SAM4S_USB_CP_EP0BUF_OBJ(sam4s_usb_descr_dev);
			wrlen = sam4s_usb_ep0buf_len;
		} else if (dt == LIBUSB_DT_CONFIG) {
			SAM4S_USB_CP_EP0BUF_OBJ(sam4s_usb_descr_cfg);
			SAM4S_USB_CP_EP0BUF_OBJ(sam4s_usb_descr_int);
			SAM4S_USB_CP_EP0BUF_OBJ(sam4s_usb_descr_ep1);
			SAM4S_USB_CP_EP0BUF_OBJ(sam4s_usb_descr_ep2);
			((struct libusb_config_descriptor *)
			  (&sam4s_usb_ep0buf))->wTotalLength = sam4s_usb_ep0buf_len;
			wrlen = sam4s_usb_ep0buf_len;
		} else {
			wrlen = -1; /* error -> stall */
		}
	} else {
		/* not handled, stall endpoint */
		wrlen = -1;
	}

out:
	if (wrlen == -1) {
		sam4s_usb_ep_state[0] = SAM4S_USB_EP_STALLED;
		sam4s_usb_csr_set(0, UDP_CSR_FORCESTALL);
		return;
	}

	if (sam4s_usb_ctrl.bRequest == BREQUEST_STD_SET_ADDRESS)
		sam4s_usb_ep_state[0] = SAM4S_USB_EP_EP0_ADDRESS;
	else
		sam4s_usb_ep_state[0] = SAM4S_USB_EP_EP0_STATUS_IN;

	/* transmit packet */
	sam4s_usb_cp_to_fdr(0, sam4s_usb_ep0buf, wrlen);
	sam4s_usb_csr_set(0, UDP_CSR_TXPKTRDY);
	TRACE_IRQ(wrlen);
}

/* this is the main handler for data reception, which is double buffered
   (two banks) */
static void
sam4s_usb_handle_bankint(unsigned int ep, int bank)
{
#define UDP_CSR_RXBYTECNT_READ()
	uint32_t rxbytecnt = RXBYTECNT(ep);

	/* normal payload */
	if (sam4s_usb_ep_state[ep] == SAM4S_USB_EP_IDLE) {
		sam4s_usb_cp_from_fdr(ep, NULL, rxbytecnt, 0);
		/* TODO: what to do with the data?! */
	/* control transfer with additional data received */
	} else if (sam4s_usb_ep_state[ep] == SAM4S_USB_EP_EP0_DATA_OUT) {
		sam4s_usb_ep0buf_len = sam4s_usb_cp_from_fdr(ep,
					sam4s_usb_ep0buf, rxbytecnt,
					sizeof(sam4s_usb_ep0buf));
	} else {
		/* TODO */
	}

	sam4s_usb_lastbank[ep] = bank; /* remember used bank, clr irq flag */
	sam4s_usb_csr_clr(ep, bank ? UDP_CSR_RX_DATA_BK1 : UDP_CSR_RX_DATA_BK0);

	/* in case it was a control request with addtln data, process... */
	if (sam4s_usb_ep_state[ep] == SAM4S_USB_EP_EP0_DATA_OUT)
		sam4s_usb_handle_ep0_setup();
}

/* interrupt handler for the endpoint */
static void
sam4s_usb_handle_epint(unsigned int ep)
{
	uint32_t csr = UDP->UDP_CSR[ep];
	enum sam4s_usb_ep_state *state = & sam4s_usb_ep_state[ep];
	unsigned int bank0, bank1;
	char *p;

	/* §40.6.2.2 Data IN transactions are used ..
	   conduct the transfer of data from the device to the host */

	/* Data IN transaction is achieved, acknowledged by the Host */
	if (csr & UDP_CSR_TXCOMP) { /* transmission has completed */
		sam4s_usb_csr_clr(ep, UDP_CSR_TXCOMP);

		/* completion of a normal write request, or status for ctrl transfer */
		if (*state == SAM4S_USB_EP_SENDING ||
		    *state == SAM4S_USB_EP_EP0_STATUS_IN
		) {
			/* ... */
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
		} else /* if (*state != SAM4S_USB_EP_STALLED) */  {
			sam4s_usb_csr_clr(ep, UDP_CSR_FORCESTALL);			
		}
	}

	if (csr & UDP_CSR_RXSETUP) {
		sam4s_usb_cp_from_fdr(0, (unsigned char*)&sam4s_usb_ctrl,
			RXBYTECNT(ep), sizeof(sam4s_usb_ctrl));

		TRACE(sam4s_usb_ctrl.bmRequestType |
			(sam4s_usb_ctrl.bRequest << 8) |
			(sam4s_usb_ctrl.wLength << 16));

		/* out request, bmRequestType.D7 == 0, i.e. host->device
		   with additional data, so we have to wait..*/
		if ((BMREQUESTTYPE_DIR(sam4s_usb_ctrl.bmRequestType) ==
			BMREQUESTTYPE_DIR_HOST_TO_DEV) &&
			sam4s_usb_ctrl.wLength > 0
		){
			/* we will receive additional data */
			sam4s_usb_ep_state[ep] = SAM4S_USB_EP_EP0_DATA_OUT;
			sam4s_usb_csr_clr(ep, UDP_CSR_DIR); /* out */
			sam4s_usb_csr_clr(ep, UDP_CSR_RXSETUP);
		} else {
			/* out request without additional data, or a request
			   to send in data, which we process immediately */
			sam4s_usb_ep_state[ep] = SAM4S_USB_EP_IDLE;
			sam4s_usb_csr_set(ep, UDP_CSR_DIR); /* in */
			sam4s_usb_csr_clr(ep, UDP_CSR_RXSETUP);		
			sam4s_usb_handle_ep0_setup();
		}
	}
}

/* main interrupt handler for USB device peripheral */
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

			sam4s_usb_dev_state |= SAM4S_USB_DEV_SUSPENDED;
			break;
		}

		/* start of frame */
		if (irq_pending & UDP_ISR_SOFINT) {
			UDP->UDP_ICR = UDP_ICR_SOFINT;
			break;
		}

		if (irq_pending & (UDP_ISR_RXRSM|UDP_ISR_WAKEUP)) {
			/* acknowledge interrupt, disable wakeup, enable suspend */
			UDP->UDP_ICR = (UDP_ICR_WAKEUP|UDP_ICR_RXRSM|UDP_ICR_WAKEUP);
			UDP->UDP_IDR = UDP_IDR_RXRSM | UDP_IDR_WAKEUP;
			UDP->UDP_IER = UDP_IER_RXSUSP;

			/* fix me, should be state saved during suspend time */
			sam4s_usb_dev_state &= ~SAM4S_USB_DEV_SUSPENDED;
			break;
		}

		if (irq_pending & UDP_ISR_ENDBUSRES) {
			sam4s_usb_dev_state = SAM4S_USB_DEV_DEFAULT;
			UDP->UDP_ICR = UDP_ICR_ENDBUSRES;
		
			/* configure endpoint 0 as control endpoint */
			UDP->UDP_RST_EP = 1 << 0; /* reset, then enable... */
			UDP->UDP_CSR[0] = (UDP_CSR_EPTYPE_CTRL | UDP_CSR_EPEDS);
			UDP->UDP_RST_EP = 0;      /* clear reset flag */
			UDP->UDP_IER    = 1 << 0; /* enable interrupt */
			sam4s_usb_ep_state[0] =  SAM4S_USB_EP_IDLE;
			break;
		}

		/* handle endpoint interrupts */
		if (irq_pending & UDP_IxR_EPxINT) {
			for (i=0; i<SAM4S_USB_NENDP; i++) {
				if (irq_pending & UDP_IxR_EPnINT(i))
					sam4s_usb_handle_epint(i);
			}
			break;
		}
	}
}

void
sam4s_usb_init()
{
	int i;

	TRACE(1); /* init */

	NVIC_DisableIRQ(UDP_IRQn);

	UDP->UDP_TXVC = UDP_TXVC_TXVDIS; /* disable transceiver, disable pullups */
	UDP->UDP_IDR = UDP->UDP_ISR; /* disable all interrupts */
	UDP->UDP_ICR = UDP->UDP_ISR; /* acknowledge all interrupts */

	sam4s_clock_peripheral_onoff(ID_UDP, 1 /* on */);

	sam4s_usb_setaddr(0);
	sam4s_usb_dev_state = SAM4S_USB_DEV_DEFAULT;

	for (i=0; i<SAM4S_USB_NENDP; i++)
		sam4s_usb_ep_reset(i);

	/* Enable DDM(PB10), DDP(PB11), should be the default, but I got
	   confused along the way and better leave it in here explicitly. */
	MATRIX->CCFG_SYSIO &= ~(CCFG_SYSIO_SYSIO11|CCFG_SYSIO_SYSIO10);

	/* enable USB peripheral, enable pull up resistor */
	UDP->UDP_TXVC = UDP_TXVC_PUON;
	sam4s_usb_dev_state = SAM4S_USB_DEV_ATTACHED;

	/* now we should get a end-of-busreset interrupt */

	NVIC_SetPriority (UDP_IRQn, 1);
	NVIC_EnableIRQ(UDP_IRQn);

	TRACE(2); /* init */
}

void
sam4s_usb_off()
{
	int i;

	TRACE(3);

	NVIC_DisableIRQ(UDP_IRQn);

	UDP->UDP_IDR = UDP->UDP_ISR; /* disable all interrupts */
	UDP->UDP_ICR = UDP->UDP_ISR; /* acknowledge all interrupts */
	for (i=0; i<SAM4S_USB_NENDP; i++)
		sam4s_usb_ep_reset(i);

	UDP->UDP_TXVC = UDP_TXVC_TXVDIS; /* disable transceiver */
	sam4s_clock_peripheral_onoff(ID_UDP, 0 /* off */);

	TRACE(4);
}