#include "e1_mgmt.h"

void
e1_mgmt_init() {

}

/* this is called in the ssc receive interrupt context */
void
e1_mgmt_rx_dblframe_irq(uint32_t *p, ) {

}

/* this is handled in the idle loop repeatedly */
void
e1_mgmt_poll() {
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
}

}