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

#include "sam4s_uart0_console.h"
#include "circular_buffer.h"
#include <sam4s4c.h>

#include "sam4s_clock.h"
#include "sam4s_pinmux.h"

/* hardcoded serial handler for UART0, the bare minimum */

#define BAUDRATE 115200

CIRCULAR_BUFFER_DECLARE(rxbuf, char, 32)
CIRCULAR_BUFFER_DECLARE(txbuf, char, 32)

void
UART0_Handler()
{
	/* reading status register will clear "irq has fired" bits */
	uint32_t sr = UART0->UART_SR;

	/* receive char */
	if (sr & UART_SR_RXRDY) {
		char c = UART0->UART_RHR;
		rxbuf_put(c);
	}

	if (sr & UART_SR_TXEMPTY) {
		char c;
		if (txbuf_get(&c) != -1)      /* is there data to send? */
			UART0->UART_THR = c;  /*  -> send data */
		else                          /* if not, disable IRQ */
			UART0->UART_IDR = UART_IER_TXEMPTY;
	}
}

/*
 * default UART, as used by SAM-BA: PA9 == URXD0, PA10 == UTXD0
 */
void
sam4s_uart0_console_init() {
	sam4s_clock_peripheral_onoff(ID_UART0, 1/*on*/);
	sam4s_pinmux_function(SAM4S_PINMUX_PA(9), SAM4S_PINMUX_A);
	sam4s_pinmux_function(SAM4S_PINMUX_PA(10), SAM4S_PINMUX_A);

	UART0->UART_MR = UART_MR_PAR_NO; /* no parity, no loopback */
	UART0->UART_BRGR = (F_MCK_HZ / (16 * BAUDRATE)) + 0.5;

	UART0->UART_IER = UART_IER_RXRDY;
	NVIC_EnableIRQ(UART0_IRQn);

	UART0->UART_CR = UART_CR_RXEN|UART_CR_TXEN;
}

/* may only happen within non-irq context! */
void
sam4s_uart0_console_tx(unsigned char c)
{
	/* I guess the __disable/enable_irq() are not strictly needed */
	for (;;) {
		__disable_irq();
		if (txbuf_put(c) == 0)
			break;  /* successfully queued in txbuf, else... */
		__enable_irq(); /* txbuf full, allow IRQ hdlr to drain queue */
	}
	UART0->UART_IER = UART_IER_TXEMPTY;
	__enable_irq();
}

int
sam4s_uart0_console_rx()
{
	int ret;
	char c;

	ret = rxbuf_get(&c);
	if (ret == -1)
		return -1;
	return c;
}
