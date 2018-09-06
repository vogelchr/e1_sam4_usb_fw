#ifndef TRACE_UTIL_H
#define TRACE_UTIL_H

#include <stdint.h>

struct trace_util_data {
	uint32_t facility;
	uint32_t payload;
};

extern int trace_util_read(struct trace_util_data *p);

extern void trace_util_user(uint32_t facility, uint32_t payload);
extern void trace_util_in_irq(uint32_t facility, uint32_t payload);

#endif