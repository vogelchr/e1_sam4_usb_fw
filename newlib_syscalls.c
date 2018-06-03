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

/* This file include small glue code to allow the newlib libc to work. */

#include <unistd.h>
#include <sys/stat.h>

#include "sam4s_uart0_console.h"

/* provided by linker, no particular type */
extern char _sheap, _eheap;
static void  *heap_ptr = &_sheap;

/* dynamic memory */
void *
_sbrk(ptrdiff_t incr)
{
	void *ret;

	/* multiple of 32 bits */
	incr = (incr + (sizeof(uint32_t)-1)) & ~sizeof(uint32_t);

	/* reached end of heap? */
	if ((uintptr_t)heap_ptr + incr > (uintptr_t)(&_eheap))
		return (void *)-1;

	ret = (void*)heap_ptr;
	heap_ptr = (void*)((char*)heap_ptr + incr);
	return ret;
}

/* handling of console, allowing printf, puts, putchar, etc... */

int
_close(int fd) {
	return 0;
}


int
_lseek(int fd, _off_t offs, int whence)
{
	return 0;
}

int
_write(int fd, const void *buf, size_t nbyte)
{
	const char *src = buf;
	const char *end = src + nbyte;

	if (fd != 1 && fd != 2)
		return -1;
	while (src != end) {
		sam4s_uart0_console_tx(*src++);
	}
	return nbyte;
}

int
_read (int fd, void *buf, size_t nbyte)
{
	char *dst = buf;
	char *end = dst + nbyte;

	if (fd != 0)
		return -1;

	while (dst != end) {
		int c = sam4s_uart0_console_rx();
		if (c == -1)
			continue;
		*dst++ = c;
	}
	return nbyte;
}

int
_fstat (int fd, struct stat *buf )
{
	return 0; /* dummy */
}

/* stdin, stdout, stderr are tty */
int
_isatty(int filedes)
{
	if (filedes >= 0 && filedes < 3)
		return 1;
	return 0;
}
