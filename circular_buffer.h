#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <string.h> /* memcpy */
#include <stdint.h>
#include <cmsis_gcc.h>

struct circular_buffer {
	void *start;
	void *end;
	void *writep;
	void *readp;	
};

/*
 * WARNING! WE DO NOT SUPPORT HANDLING OBJECTS OF VARIABLE SIZES!
 * sz always must be sizeof(type) from when we were declaring the buffer!
 */

/* Advance the read pointer, to getr one element from the circular buffer.

   This should be lockless and work inside or outside irq context.

   If the buffer is empty, we return NULL.

   If the buffer is non-empty, advance the read-pointer and return
   the pointer before it was advanced (corresponding to the element
   that has been read. This element will not be overwritten because
   there is always one free spot between writep (next element to be
   written) and readp. */
static inline void *
circular_buffer_inc_readp_if_nonempty(struct circular_buffer *p, size_t sz) {
	void *rp, *nrp, *wp;

	do {
		rp = (void*)__LDREXW((uint32_t*)&p->readp);
		wp = *(void * volatile *)&p->writep;

		if (rp == wp) /* empty */
			return NULL;

		nrp = (void*)((char*)(rp)+sz);
		if (nrp == p->end)
			nrp = p->start;
	} while (__STREXW((uint32_t)nrp, (uint32_t*)&p->readp));

	return rp;
}

/* Advance the write pointer to get one element from the circular buffer.

   If the buffer is full, we return NULL.

   If the buffer is not full, we advance the write pointer and return the
   pointer before it has been advanced. This corresponds to the element that
   ought to be written.
   
   XXX FIXME: This currently leaves the possibility that the element
              is not written fast enough, so we will have to merge the inc
	      and put/get functions!
 */

static inline void *  __attribute__((always_inline))
circular_buffer_inc_writep_if_nonfull(struct circular_buffer *p, size_t sz) {
	void *rp, *wp, *nwp;

	do {
		rp = *(void * volatile *)&p->readp;
		wp = (void*)__LDREXW((uint32_t*)&p->writep);
		nwp = (void*)((char*)(wp)+sz);
		if (nwp == p->end)
			nwp = p->start;
		if (nwp == rp) /* full */
			return NULL;
	} while(__STREXW((uint32_t)nwp, (uint32_t*)&p->writep));
	return wp;
}

static inline int __attribute__((always_inline))
circular_buffer_put(struct circular_buffer *p, const void * element, size_t sz)
{
	void *wp;

	wp = circular_buffer_inc_writep_if_nonfull(p, sz);
	if (!wp)
		return -1; /* full */
	memcpy(wp, element, sz);
	return 0;
}

static inline int
circular_buffer_get(struct circular_buffer *p, void * element, size_t sz)
{
	void *rp = circular_buffer_inc_readp_if_nonempty(p, sz);
	if (!rp)
		return -1;
	memcpy(element, rp, sz);
	return 0;
}

#define CIRCULAR_BUFFER_INIT_STATIC(buf, total_sz) \
	{ .start=(void*)(buf), \
	  .end=(void*)(((char*)(buf))+(total_sz)), \
	  .writep=(void*)buf, \
	  .readp=(void*)buf }

#define CIRCULAR_BUFFER_INIT_STATIC_ARR(arr) CIRCULAR_BUFFER_INIT_STATIC((arr), sizeof(arr))

/* CIRCULAR_BUFFER_DECLARE(name, type, num_elements) declares
    - the buffer array: type name_data[num_elements];
    - the circular buffer structure struct circular buffer name;
    - inline functions name_put() and name_get()
 */

#define CIRCULAR_BUFFER_DECLARE(name, type, num_elements) \
	  static const size_t name ## _sz = sizeof(type); \
	  static type name ## _data[num_elements]; \
	  static struct circular_buffer name = \
	  	CIRCULAR_BUFFER_INIT_STATIC_ARR(name ## _data); \
	  static inline int name ## _put(type c) { \
	  	return circular_buffer_put(&name, &c, sizeof(type)); } \
	  static inline int name ## _get(type * c) { \
	  	return circular_buffer_get(&name, c, sizeof(type)); }

#endif
