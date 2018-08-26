#ifndef SAM4S_SCC_H
#define SAM4S_SCC_H

#define SAM4S_SSC_FRAME_BYTES 32
#define SAM4S_SSC_BUF_RX_NFRAMES 4
#define SAM4S_SSC_BUF_TX_NFRAMES 16

/* 
   after realign_reset, the next frame read will be rxbuf[0..(N-1)] and
   rx buffer will begin with align zero-bits before received and realigned
   frame data starts
 */
extern void sam4s_ssc_realign_reset(unsigned int align);

/* realign either in forwarwd (go_backwards=0) or backwards (go_backwords=1) direction */
extern unsigned int
sam4s_ssc_realign_adjust(int go_backwards);

extern void sam4s_ssc_init();

extern unsigned char sam4s_ssc_rx_buf[SAM4S_SSC_BUF_RX_NFRAMES*SAM4S_SSC_FRAME_BYTES];
extern int sam4s_ssc_rx_last_frame;

extern unsigned char sam4s_ssc_tx_buf[SAM4S_SSC_BUF_TX_NFRAMES*SAM4S_SSC_FRAME_BYTES];
extern int sam4s_ssc_tx_last_frame;

extern volatile unsigned long sam4s_ssc_rx_frame_ctr; /* number of received frames after last realignment clear */
extern unsigned long sam4s_ssc_tx_irq_ctr;   /* number of interrupts, tx */
extern unsigned long sam4s_ssc_rx_irq_ctr;   /* number of interrupts, rx */
extern unsigned long sam4s_ssc_irq_underflow_ctr; /* number of underflows detected in ISR */

#endif

