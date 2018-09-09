#ifndef TRACE_UTIL_H
#define TRACE_UTIL_H

#include <stdint.h>

struct trace_util_data {
	char text[32];
	uint32_t a, b;
};

extern int trace_util_read(struct trace_util_data *p);
extern void trace_util_write(const struct trace_util_data p);

#endif