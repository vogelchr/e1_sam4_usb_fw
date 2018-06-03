#ifndef SAM4S_SPI_H
#define SAM4S_SPI_H

extern void sam4s_spi_init();
extern void sam4s_spi_transceive(unsigned char *rxbuf, const unsigned char *txbuf, unsigned int len);

#endif