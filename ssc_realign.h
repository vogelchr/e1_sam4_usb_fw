#ifndef SSC_REALIGN_H
#define SSC_REALIGN_H

#define SSC_REALIGN_N_BYTES_P_FRAME 32

extern void ssc_realign_init(); /* this currently does nothing */

/* reinit the realignment code to delay output relative to input by n bits */
extern void ssc_realign_set_align(unsigned int nbits);

/* reads SSC_REALIGN_N_BYTES_P_FRAME from inp, writes the same amount of
   bytes to outp. Data will be delayed by nbits (set by _set_align() above). */
extern void ssc_realign_feed_frame(unsigned char *outp, const unsigned char *inp);

#endif
