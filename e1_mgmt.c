#include "e1_mgmt.h"
#include "sam4s_ssc.h"
#include "sam4s_timer.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*
 * G.704 (10/98) Table 5/A G.704 – Allocation of bits 1 to 8 of the frame
 *
 *                       MSB                         LSB
 *                        1                           8
 * Frame containing     +---+---+---+---+---+---+---+---+
 * the frame alignment  | Si| 0 | 0 | 1 | 1 | 0 | 1 | 1 |
 * signal:              +---+---+---+---+---+---+---+---+
 *
 * Frame not containing +---+---+---+---+---+---+---+---+
 * the frame alignment  | Si| 1 | A |Sa4|Sa5|Sa6|Sa7|Sa8|
 * signal:              +---+---+---+---+---+---+---+---+
 */

#define G704_FAS_MSK   0x7f
#define G704_FAS_BITS   0x1b
#define G704_NOFAS_MSK 0xc0
#define G704_NOFAS_BITS 0x40

#define CHK_LW_MSB_OCTET(c,m,b) (((c) & ((m) << 24)) == ((b) << 24))
#define CHK_G704_FAS_LW(c) CHK_LW_MSB_OCTET((c), G704_FAS_MSK, G704_FAS_BITS)
#define CHK_G704_NOFAS_LW(c) CHK_LW_MSB_OCTET((c), G704_NOFAS_MSK, G704_NOFAS_BITS)

struct e1_mgmt_irqstats {
	unsigned int dblfrm;
	unsigned int n_dblframes_bad_fas;
};

void
e1_mgmt_init() {
	int i;

	/* create idle pattern for tx */
	memset(sam4s_ssc_rx_buf, '\0', sizeof(sam4s_ssc_rx_buf));
	memset(sam4s_ssc_tx_buf, '\0', sizeof(sam4s_ssc_tx_buf));

	for (i=0; i<SAM4S_SSC_BUF_DBLFRAMES; i++) {
		/* even C 0 0 1 1 0 1 1 */
		sam4s_ssc_tx_buf[i*SAM4S_SSC_DBLFRM_LONGWORDS] = 0x1b000000;
		/* odd  0 1 A S S S S S */
		sam4s_ssc_tx_buf[i*SAM4S_SSC_DBLFRM_LONGWORDS+8] = 0x40000000;  
	}

}

/*
 * this is called in the ssc receive interrupt context, p points to
 * a *doubleframe*, which is 64 bytes = 16 longwords and should be
 * aligned such that the MSB of p[0] is bit numbered 1 of an "even numbered"
 * E1 frame with a frame alignment signal and the MSB of p[8] corresponds
 * to bit 1 of an "odd numbered" E1 frame.
 */
 
void
e1_mgmt_rx_dblfrm_irq(uint32_t *p) {
	e1_mgmt_irqstats.n_dblframes++;

	if (!(CHK_G704_FAS_LW(p[0]) && CHK_G704_NOFAS_LW(p[8]))) {
		e1_mgmt_irqstats.n_dblframes_bad_fas;
	}
}

/* this is handled in the idle loop repeatedly */
void
e1_mgmt_poll() {
#if 0
	if (last_dblfrm_processed == sam4s_ssc_rx_last_dblfrm)
		return;

	/* possibly process a single dblframe */

	/* wrap-around happened? */
	if (last_dblfrm_processed > sam4s_ssc_rx_last_dblfrm)
		goto out; /* not yet */

	rx_process_ctr++;
	if (rx_process_ctr > 200) { /* give two dblframes (4ms) to sync */
		int i;
		for (i=0; i<SAM4S_SSC_BUF_DBLFRAMES; i++) {
			uint32_t lwe, lwo; /* even and odd longwords */
			lwe = sam4s_ssc_rx_buf[i*SAM4S_SSC_DBLFRM_LONGWORDS];
			lwo = sam4s_ssc_rx_buf[i*SAM4S_SSC_DBLFRM_LONGWORDS+8];

			if ((lwe & 0x7f000000) != 0x1b000000 ||
			    (lwo & 0xc0000000) != 0x40000000) {
				if (enable_sync)
					printf("%d: lwe=0x%08x lwo=0x%08x\r\n", i, lwe, lwo);
				break;

			    }
		}
		/* sync markers didn't match up */
		if (i != SAM4S_SSC_BUF_DBLFRAMES) {
			rx_process_ctr = 0;

			if (enable_sync) {
				sam4s_timer_e1_phase_adj(1);
			}
		}
	}
out:
	last_dblfrm_processed = sam4s_ssc_rx_last_dblfrm;
#endif
}
