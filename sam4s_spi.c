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

/* this file contains rudimentary code to control the SPI  */

#include "sam4s_spi.h"
#include "sam4s_pinmux.h"
#include "sam4s_clock.h"

#include <sam4s8b.h>
#include <stddef.h>

static const uint8_t pcs_by_csnum[] = {
	0x00, /* \CS0 */
	0x01, /* \CS1 */
	0x03, /* \CS2 */
	0x07  /* \CS3 */
};

#define TDR_VAL(csnum, len, data) (pcs_by_csnum[(cs)] | \
	((len)==1?SPI_TDR_LASTXFER:0) | (data))

void
sam4s_spi_transceive(unsigned char *rxbuf, const unsigned char *txbuf, unsigned int len)
{
	int cs = 1;

	/* activate the proper CS line */
	SPI->SPI_MR = (SPI->SPI_MR & ~SPI_MR_PCS_Msk) | SPI_MR_PCS(pcs_by_csnum[cs]);

	PDC_SPI->PERIPH_RPR = (uint32_t)rxbuf;
	PDC_SPI->PERIPH_RCR = len;
	PDC_SPI->PERIPH_TPR = (uint32_t)txbuf;
	PDC_SPI->PERIPH_TCR = len;
	PDC_SPI->PERIPH_PTCR = PERIPH_PTCR_TXTEN | PERIPH_PTCR_RXTEN;

	/* wait for finish */
	while (PDC_SPI->PERIPH_RCR);
}

void
sam4s_spi_init()
{
	sam4s_clock_peripheral_onoff(ID_SPI, 1 /* on */);

	sam4s_pinmux_function(SAM4S_PINMUX_PA(12), SAM4S_PINMUX_A); /* MISO */
	sam4s_pinmux_function(SAM4S_PINMUX_PA(13), SAM4S_PINMUX_A); /* MOSI */
	sam4s_pinmux_function(SAM4S_PINMUX_PA(14), SAM4S_PINMUX_A); /* SCK */
	sam4s_pinmux_function(SAM4S_PINMUX_PA(11), SAM4S_PINMUX_A); /* CS0 */
	sam4s_pinmux_function(SAM4S_PINMUX_PA(31), SAM4S_PINMUX_A); /* CS1 */
	sam4s_pinmux_function(SAM4S_PINMUX_PA(30), SAM4S_PINMUX_B); /* CS2 */


	/* IDT 82V2081 LIU, SCLKE=GND
	  Data is shifted on the rising edge of SCLK, data is captured
	  on the falling edge of SCLK. 
	*/
	SPI->SPI_CR = SPI_CR_SWRST;
	SPI->SPI_CR = SPI_CR_SPIEN;
	SPI->SPI_MR = SPI_MR_MSTR | SPI_MR_DLYBCS(255);
	SPI->SPI_CSR[1] = SPI_CSR_BITS(0) | SPI_CSR_SCBR(255)
		/* | SPI_CSR_CPOL */  | SPI_CSR_NCPHA;
}