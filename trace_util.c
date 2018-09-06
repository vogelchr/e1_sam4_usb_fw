#include "trace_util.h"
#include <sam4s8b.h>
#include <stddef.h>

#define TRACE_UTIL_NELEMS 128

uint32_t * volatile trace_util_writep;
uint32_t trace_util_buf[TRACE_UTIL_NELEMS];
static const uint32_t *trace_util_end = & trace_util_buf[TRACE_UTIL_NELEMS];

void
trace_util_init() {
	uint32_t *p;
	for (p=trace_util_buf; p != trace_util_end; p++)
		*p = TRACE_UTIL_TAG_EMPTY;
	trace_util_writep = trace_util_buf;
	*trace_util_buf = TRACE_UTIL_TAG_WRPTR;
}

int
trace_util_read(uint32_t **p, uint32_t *tag)
{
	if (*p == NULL)
		*p = trace_util_buf;

	if (*(volatile uint32_t *)*p == TRACE_UTIL_TAG_WRPTR)
		return 0;
	*tag = *(volatile uint32_t *)((*p)++);
	return 1;
}

void
trace_util(uint32_t tag) {
	uint32_t *p;

	__disable_irq();
	/* this is not IRQ save! */
	p = trace_util_writep;
	*p++ = tag;
	if (p == trace_util_end)
		p = trace_util_buf;
	*p = TRACE_UTIL_TAG_WRPTR;
	trace_util_writep = p;
	__enable_irq();
}

void
trace_util_in_irq(uint32_t tag) {
	uint32_t *p;

	/* this is not IRQ save! */
	p = trace_util_writep;
	*p++ = tag;
	if (p == trace_util_end)
		p = trace_util_buf;
	*p = TRACE_UTIL_TAG_WRPTR;
	trace_util_writep = p;
}