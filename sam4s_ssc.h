#ifndef SAM4S_SCC_H
#define SAM4S_SCC_H

#define SAM4S_SSC_FRAME_BYTES 32
#define SAM4S_SSC_BUF_NFRAMES 4

extern void sam4s_ssc_init();

extern unsigned char sam4s_ssc_rxbuf[SAM4S_SSC_BUF_NFRAMES*SAM4S_SSC_FRAME_BYTES];
extern int sam4s_ssc_rx_last_frame;

extern unsigned char sam4s_ssc_txbuf[SAM4S_SSC_BUF_NFRAMES*SAM4S_SSC_FRAME_BYTES];
extern int sam4s_ssc_tx_last_frame;

unsigned long sam4s_ssc_tx_irq_ctr;
unsigned long sam4s_ssc_rx_irq_ctr;

#endif

