#ifndef SAM4S_DAC_H
#define SAM4S_DAC_H

#define SAM4S_DAC_RANGE (1UL << 12)

extern void
sam4s_dac_init();

extern void
sam4s_dac_update(int ch, unsigned int value);

#endif