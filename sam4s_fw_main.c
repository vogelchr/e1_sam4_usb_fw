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

#include "trace_util.h"
#include "sam4s_uart0_console.h"
#include "sam4s_pinmux.h"
#include "sam4s_ssc.h"
#include "sam4s_clock.h"
#include "sam4s_spi.h"
#include "sam4s_usb.h"
#include "sam4s_usb_descriptors.h"
#include "sam4s_timer.h"
#include "gps_steer.h"
#include "trace_util.h"
#include "e1_mgmt.h"

#include <stdint.h>
#include <stdlib.h>
#include <sam4s8b.h>
#include <stdio.h>
#include <string.h>

unsigned char rxbuf[4] = {0, 0, 0, 0};
unsigned char txbuf[4] = {0, 0, 0, 0};

struct reg_pair {
	unsigned char regnum;
	unsigned char val;
};

/*
 * data on LUI inputs sampled falling edge of clk    TCF0.TCLK_SEL=0 {default}
 * data on LUI outputs updated on rising edge of clk RCF0.RCLK_SEL=0 {default}
 * 
 * ---+   +---+   +---+
 *    |   |   |   |   |   Clock
 *    +---+   +---+   +---
 * 
 * ------\ /-----\ /-----
 *   D    X       X       Data
 * ------/ \-----/ \-----
 * 
 * sam4 ssc:
 *   SSC_RCMR / SSC Receive Clock Mode Register, CKI Receive Clock Inversion
 *     0: data inputs are sampled on falling edge
 *   SSC_TCMR / SSC Transmit Clock Mode Register, CKI: Transmit Clock Inversion
 *     1: data outputs are shifted out on rising edge
 */

struct reg_pair idt82v2081_cfg[] = {
	{ 0x02, 0x01 }, /* Global Config: E1 Mode, Interrupt pin active driven */
	{ 0x03, 0x24 }, /*  TERM: Transmit and Receive Termination Configuration Register */
	{ 0x06, 0x01 }, /* TCF1: Transmitter Configuration Register 1: E1, 120Ohm */
	{ 0xff, 0xff }, /* END */
};


unsigned char flip_lsb_msb(unsigned char c) {
	c = ((c & 0x0f) << 4) | ((c & 0xf0) >> 4);
	c = ((c & 0x33) << 2) | ((c & 0xcc) >> 2);
	c = ((c & 0x55) << 1) | ((c & 0xaa) >> 1);
	return c;
}

static unsigned char
idt82v2081_reg(unsigned char regnum, int write)
{
	/* first ... .............last bit transfered */
	/* A0 A1 A2 A3 A4 R/#W 0 0 0 */
	return flip_lsb_msb((regnum & 0x1f) | (write?0x00:0x20));
}

struct trace_util_data trace;

static unsigned int rx_process_ctr;
static int last_dblfrm_processed;
static int enable_sync;

int
main()
{
	int i;

	/* disable watchdog */
	WDT->WDT_MR = WDT_MR_WDDIS;

	sam4s_pinmux_init();
	/* Port PB1 is VCXO_EN */
	sam4s_pinmux_function(SAM4S_PINMUX_PB(1), SAM4S_PINMUX_GPIO);
	sam4s_pinmux_gpio_oe(SAM4S_PINMUX_PB(1), 1);
	sam4s_pinmux_gpio_set(SAM4S_PINMUX_PB(1), 1);

	sam4s_clock_init();

	/* debugging LEDs */
	sam4s_pinmux_function(SAM4S_PINMUX_PA(24), SAM4S_PINMUX_GPIO);
	sam4s_pinmux_gpio_oe(SAM4S_PINMUX_PA(24), 1);
	sam4s_pinmux_gpio_set(SAM4S_PINMUX_PA(24), 1);

	sam4s_pinmux_function(SAM4S_PINMUX_PA(25), SAM4S_PINMUX_GPIO);
	sam4s_pinmux_gpio_oe(SAM4S_PINMUX_PA(25), 1);
	sam4s_pinmux_gpio_set(SAM4S_PINMUX_PA(25), 1);

	sam4s_pinmux_function(SAM4S_PINMUX_PA(26), SAM4S_PINMUX_GPIO);
	sam4s_pinmux_gpio_oe(SAM4S_PINMUX_PA(26), 1);
	sam4s_pinmux_gpio_set(SAM4S_PINMUX_PA(26), 1);

	/* we had intended to use Port PA21 for CLK2048 (Peripheral B, PCK1),
	   but PLL limitation combined with Chris' stupidity prevents that.
	   So, keep it High-Z for now. */
	sam4s_pinmux_function(SAM4S_PINMUX_PA(21), SAM4S_PINMUX_GPIO);
	sam4s_pinmux_gpio_oe(SAM4S_PINMUX_PA(21), 0);

	/* we had intended to use Port PB3 for providing TX_CLK
	   (Peripheral B, PCK2), but PLL limitation combined with
	   Chris' stupidity prevents that. So, keep it High-Z for now. */
	sam4s_pinmux_function(SAM4S_PINMUX_PB(3), SAM4S_PINMUX_GPIO);
	sam4s_pinmux_gpio_oe(SAM4S_PINMUX_PB(3), 0);

	__enable_irq();

	sam4s_uart0_console_init();

	sam4s_ssc_init();
	sam4s_spi_init();
	sam4s_usb_init();
	sam4s_timer_init();

	gps_steer_init();
	e1_mgmt_init();

	printf("=============\r\n");
	printf("Hello, world.\r\n");
	printf("=============\r\n\r\n");

	for(;;) {
		int k;

		gps_steer_poll();
		e1_mgmt_poll();

		if(!trace_util_read(&trace) ) {
			/* "precision" for string must match
			    sizeof(struct trace_util_data.text)! */
			printf("%.32s 0x%08lx 0x%08lx\r\n",
				trace.text,trace.a,trace.b);
		}

		k = sam4s_uart0_console_rx();
		if (k == -1)
			continue;

		if (k == 'c') {
			struct reg_pair *p;
			for (p = idt82v2081_cfg; p->regnum != 0xff; p++) {
				printf("write reg 0x%02x = 0x%02x\r\n",
					p->regnum, p->val);
				txbuf[0] = idt82v2081_reg(p->regnum, 1);
				txbuf[1] = p->val;
				sam4s_spi_transceive(rxbuf, txbuf, 2);
			}
		}

		if (k == 'a') {
			printf("idt82v2081 registers:\r\n");
			for (i=0; i<0x1d; i++) {
				if ((i % 8) == 0)
					printf("0x%02x:", i);

				txbuf[0] = idt82v2081_reg(i, 0);
				txbuf[1] = 0;
				sam4s_spi_transceive(rxbuf, txbuf, 2);

				printf(" %02x", flip_lsb_msb(rxbuf[1]));
				if ((i % 8) == 7)
					printf("\r\n");
			}

			printf("\r\n");
		}

		if (k == 'b') {
			i = 0x0b;
			txbuf[0] = idt82v2081_reg(i, 0);
			txbuf[1] = 0xaa;
			sam4s_spi_transceive(rxbuf, txbuf, 2);
				printf("0x%02x -> 0x%02x 0x%02x\r\n",
					i, flip_lsb_msb(rxbuf[0]),
					flip_lsb_msb(rxbuf[1]));
		}

		if (k == 'r') {
			uint32_t *p = sam4s_ssc_rx_buf;

#if 0
			printf("last rx: %d/tx %d, rx_irq %u tx_irq %u over %u under %u\r\n",
				sam4s_ssc_rx_last_dblfrm,
				sam4s_ssc_tx_last_dblfrm,
				sam4s_ssc_rx_irq_ctr,
				sam4s_ssc_tx_irq_ctr,
				sam4s_ssc_irq_overflow_ctr,
				sam4s_ssc_irq_underflow_ctr);
#endif

			for (i=0; i<SAM4S_SSC_DBLFRM_LONGWORDS*SAM4S_SSC_BUF_DBLFRAMES; i++) {
				if ((i % 8) == 0)
					printf("Frame %d:", i / 8);
				printf(" %08lx", *p++);
				if ((i % 8) == 7)
					printf("\r\n");
			}

			for (i=0; i<SAM4S_SSC_BUF_DBLFRAMES; i++) {
				uint32_t lwe, lwo; /* even and odd longwords */
				lwe = sam4s_ssc_rx_buf[i*SAM4S_SSC_DBLFRM_LONGWORDS];
				lwo = sam4s_ssc_rx_buf[i*SAM4S_SSC_DBLFRM_LONGWORDS+8];

				printf("%d: lwe=0x%08lx lwo=0x%08lx\r\n",i,
					lwe & 0x7f000000,
					lwo & 0xc0000000);
			}
		}

		if (k == 'u')
			sam4s_usb_init();
		if (k == 'U')
			sam4s_usb_off();
		if (k == 't') {
			printf("\r\n\r\nTimer Status\r\n------------\r\n");
			for (i=0; i<3; i++) {
				printf("TC0-Ch%d: TC_CMR=0x%08lx SMMR=0x%08lx SR=0x%08lx\r\n",
					i, TC0->TC_CHANNEL[i].TC_CMR,
					TC0->TC_CHANNEL[i].TC_SMMR,
					TC0->TC_CHANNEL[i].TC_SR);
				printf("         cv=%-5lu ra=%-5lu rb=%-5lu rc=%-5lu\r\n",
					TC0->TC_CHANNEL[i].TC_CV,
					TC0->TC_CHANNEL[i].TC_RA,
					TC0->TC_CHANNEL[i].TC_RB,
					TC0->TC_CHANNEL[i].TC_RC);
			}
		}
		if (k == 's')
			enable_sync++;
		if (k== 'S')
			enable_sync=0;

		if (k == '<' || k == '>') {
			i = sam4s_timer_e1_phase_adj(k == '>');
			printf("phase_adj: %d\r\n",i);
		}
	}

	while(1);

}
