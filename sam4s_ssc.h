#ifndef SAM4S_SCC_H
#define SAM4S_SCC_H

#include <stdint.h>

/*
 * 
 * One e1 frame is 32 bytes = 256 bits = 8 (32bit) long words, to lower
 * the IRQ rate we generate a frame clock corresponding to *two* frames,
 * called a double-frame DBLFRM, that's conveniently the maximum the SSC
 * peripheral supports :-).
 *
 */

#define SAM4S_SSC_DBLFRM_LONGWORDS 16
#define SAM4S_SSC_BITS_PER_LONGWORD 32
#define SAM4S_SSC_BUF_DBLFRAMES 4

extern void sam4s_ssc_init();

extern uint32_t sam4s_ssc_rx_buf[SAM4S_SSC_DBLFRM_LONGWORDS*SAM4S_SSC_BUF_DBLFRAMES];
extern volatile int sam4s_ssc_rx_last_dblfrm;

extern uint32_t sam4s_ssc_tx_buf[SAM4S_SSC_DBLFRM_LONGWORDS*SAM4S_SSC_BUF_DBLFRAMES];
extern volatile int sam4s_ssc_tx_last_dblfrm;

extern unsigned int sam4s_ssc_tx_irq_ctr;
extern unsigned int sam4s_ssc_rx_irq_ctr;
extern unsigned int sam4s_ssc_irq_underflow_ctr;
extern unsigned int sam4s_ssc_irq_overflow_ctr;

#endif

