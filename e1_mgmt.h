#ifndef E1_MGMT_H
#define E1_MGMT_H

extern void e1_mgmt_init();
extern void e1_mgmt_poll();
extern void e1_mgmt_rx_dblfrm_irq(uint32_t *p); /* called in irq context! */

#endif