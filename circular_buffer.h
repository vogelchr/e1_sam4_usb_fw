#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <string.h> /* memcpy */

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

static inline void *
circular_buffer_next_writep(const struct circular_buffer *p, size_t sz) {
	void *next = (void*)((char*)(p->writep)+sz);
	if (next == p->end)
		return p->start;
	return next;
}

static inline void *
circular_buffer_next_readp(const struct circular_buffer *p, size_t sz) {
	void *next = (void*)((char*)(p->readp)+sz);
	if (next == p->end)
		return p->start;
	return next;
}

static inline int
circular_buffer_put(struct circular_buffer *p, const void * element, size_t sz)
{
	void *next_writep = circular_buffer_next_writep(p, sz);
	if (next_writep == ((volatile struct circular_buffer *)p)->readp)
		return -1; /* full */
	memcpy(p->writep, element, sz);
	((volatile struct circular_buffer *)p)->writep = next_writep;
	return 0;
}

static inline int
circular_buffer_get(struct circular_buffer *p, void * element, size_t sz)
{
	void *next_readp = circular_buffer_next_readp(p, sz);
	if (p->readp == ((volatile struct circular_buffer *)p)->writep)
		return -1; /* empty */
	memcpy(element, p->readp, sz);
	memset(p->readp, '\xba', sz);  /* for debugging, clear */
	((volatile struct circular_buffer*)p)->readp = next_readp;
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
