#ifndef TRACE_UTIL_H
#define TRACE_UTIL_H

#include <stdint.h>

#define TRACE_UTIL_TAG8(a,b)  (((a) & 0x000000ffUL) << 24) | ((b) & 0x00ffffffUL)
#define TRACE_UTIL_TAG16(a,b) (((a) & 0x0000ffffUL) << 16) | ((b) & 0x0000ffffUL)
#define TRACE_UTIL_TAG24(a,b) (((a) & 0x00ffffffUL) <<  8) | ((b) & 0x000000ffUL)

#define TRACE_UTIL_TAG_EMPTY   0x00000000UL
#define TRACE_UTIL_TAG_USB(v)  TRACE_UTIL_TAG8('u', v)
#define TRACE_UTIL_TAG_WRPTR   0xffffffffUL

extern void trace_util_init();
extern void trace_util(uint32_t tag);

#define TRACE_UTIL_USB(v) do { trace_util(TRACE_UTIL_TAG_USB(v)); } while(0)

#endif