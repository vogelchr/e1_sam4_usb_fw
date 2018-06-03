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

#include "ssc_realign.h"

#include <stdint.h>
#include <string.h>

/* this is very convoluted code to re-align a buffer by individual bits */

/*
 * bits are numbered from 1 .. 8 per frame, and frames are numbered 0..31
 * align is the number of bits the input buffer is advanced relative
 * to the output buffer.
 * 
 * The MSB is transmitted first.
 * 
 * given two consecutive input octet
 *    a = in[i-1]
 *    b = in[i]
 * and one output octet
 *    o = out[j]
 * and an fractional alignment of l bits l = align % 8, here l == 3
 * we use a input shift register w = (a << 8) | b:
 * 
 *      a was transmitted earlier      b was transmitted later
 *     |<-  l  ->|
 * In  a7  a6  a5  a4  a3  a2  a1  a0  b7  b6  b5  b4  b3  b2  b1  b0
 * Out             o7  o6  o5  o4  o3  o2  o1  o0
 *     s2  s1  s0
 * 
 * output buffer     input buffer
 *   |       reframe   |
 *   |       buffer    |
 *   |         |       |
 *   V         V       V
 * 
 * out[0]    ref[0]             < copy in k octest
 * out[:]    ref[:]             <
 * out[k-1]  ref[k-1]   in[0]   <  < then realign input bits, first to
 * out[:]               in[:]      < output buffer..
 * out[N-1]             in[:]      <
 *           ref[0]     in[:]      < ..then later to the reframing buffer
 *           ref[:]     in[:]      <
 *           ref[k-1]   in[N-1]    <
 *
 *   k = (align + 7) / 8
 * 
 * if align == 0 ->
 *    last_in[N-1] -> out[0]
 *         in[0]   -> out[1]
 *         in[N-2] -> out[N-1]
 *         in[N-1] -> saved
 * 
 * if align == 1 ->  (LSB is sent last)
 *    (saved[D7])
 *    last_in[N-1,D0]     -> out[0,D7]      MSB is sent first
 *         in[0,D7..D1]   -> out[0,D6..D0]  LSB is sent last
 *         in[0,D0]       -> out[1,D7]
 *         in[1,D7..D1]   -> out[1,D6..D0]
 *         ..
 *         in[N-2,D0]     -> out[N-1, D7]
 *         in[N-1,D7..D1] -> out[N-1,D6..D0]
 *         in[N-1,D0]     -> saved[D7]
 * 
 * if align == 3 ->
 *    (saved[D7..D5])
 *    last_in[N-1,D2..D0] -> out[0,D7..D5]
 *         in[0,D7..D3]   -> out[0,D4..D0]
 *         in[0,D2..D0]   -> out[1,D7..D5]
 *         in[1,D7..D3]   -> out[1,D4..D0]
 *         ..
 *         in[N-1,D7..D3] -> out[N-1, D4..D0]
 *         in[N-1,D2..D0] -> saved[0, D7..D5]
 */

/* position of start of unaligned frame in aligned frame */
static unsigned int ssc_realign_align = 0;
unsigned char ssc_realign_buf[SSC_REALIGN_N_BYTES_P_FRAME+1];

void
ssc_realign_init() {
}

void
ssc_realign_set_align(unsigned int nbits)
{
	unsigned int j;
	for (j=0; j<SSC_REALIGN_N_BYTES_P_FRAME; j++)
		ssc_realign_buf[j] = 0;
	ssc_realign_align = nbits % (SSC_REALIGN_N_BYTES_P_FRAME * 8);
}

void
ssc_realign_feed_frame(
	unsigned char * restrict outp,
	const unsigned char * restrict inp
) {
	uint32_t shiftreg;
	const unsigned char *p;
	unsigned char *q;
	const unsigned char *end_inp = inp + SSC_REALIGN_N_BYTES_P_FRAME;
	unsigned char *end_outp = outp + SSC_REALIGN_N_BYTES_P_FRAME;

	unsigned int align_bits = ssc_realign_align;

	/* == copy overlapping region from reframe_buffer to output == */

	p = ssc_realign_buf;
	q = outp;
	while (align_bits >= 8) {
		*q++ = *p++;
		align_bits-= 8;
	}

	/* the last unread char in the ssc_realign_buf will hold a partial octet */
	shiftreg = *p;

	/* now start consuming unaligned data from the input buffer */
	p = inp;

	while (p != end_inp) {
		uint32_t c = *p++; /* consume input */
		/* output is input shifted by n. bits | data from last octet */
		*q++ = (c >> align_bits) | shiftreg;
		shiftreg = c << (8-align_bits); /* LSBs saved for next octet */
		if (q == end_outp) /* end of aligned output buffer reached.. */
			q = ssc_realign_buf; /* .. the rest goes into the reframe buffer */
	}

	if (align_bits)
		*q = shiftreg;
}
