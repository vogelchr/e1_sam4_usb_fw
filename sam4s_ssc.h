#ifndef SAM4S_SCC_H
#define SAM4S_SCC_H

#define SAM4S_SSC_FRAME_BYTES 32
#define SAM4S_SSC_BUF_RX_NFRAMES 2
#define SAM4S_SSC_BUF_TX_NFRAMES 4

/* 
   after realign_reset, the next frame read will be rxbuf[0..(N-1)] and
   rx buffer will begin with align zero-bits before received and realigned
   frame data starts
 */
extern void sam4s_ssc_realign_reset(int align);

extern void sam4s_ssc_init();

extern unsigned char sam4s_ssc_rxbuf[SAM4S_SSC_BUF_RX_NFRAMES*SAM4S_SSC_FRAME_BYTES];
extern int sam4s_ssc_rx_last_frame;

extern unsigned char sam4s_ssc_txbuf[SAM4S_SSC_BUF_TX_NFRAMES*SAM4S_SSC_FRAME_BYTES];
extern int sam4s_ssc_tx_last_frame;

unsigned long sam4s_ssc_tx_irq_ctr;
unsigned long sam4s_ssc_rx_irq_ctr;

#endif

