#include "trace_util.h"
#include <sam4s8b.h>
#include <stddef.h>

#include "circular_buffer.h"

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

/* this is a quick and dirty tracing facility using a ring-buffer */


CIRCULAR_BUFFER_DECLARE(trace_util, struct trace_util_data, 128)


extern int trace_util_read(struct trace_util_data *p);


int
trace_util_read(struct trace_util_data *p) {
	return trace_util_get(p);
}

void
trace_util_user(uint32_t facility, uint32_t payload) {
	struct trace_util_data tmp;
	tmp.facility = facility;
	tmp.payload = payload;

	__disable_irq();
	trace_util_put(tmp);
	__enable_irq();
}

void
trace_util_in_irq(uint32_t facility, uint32_t payload) {
	struct trace_util_data tmp;
	tmp.facility = facility;
	tmp.payload = payload;

	trace_util_put(tmp);
}