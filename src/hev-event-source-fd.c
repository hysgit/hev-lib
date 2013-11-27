/*
 ============================================================================
 Name        : hev-event-source-fd.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2013 everyone.
 Description : An event source FD
 ============================================================================
 */

#include "hev-event-source-fd.h"

inline HevEventSourceFD *
hev_event_source_fd_new (HevEventSource *source, int fd, uint32_t events)
{
	HevEventSourceFD *self = HEV_MEMORY_ALLOCATOR_ALLOC (sizeof (HevEventSourceFD));
	if (self) {
		self->fd = fd;
		self->_events = events;
		self->revents = 0;
		self->_ref_count = 1;
		self->source = source;
		self->data = NULL;
	}

	return self;
}

inline HevEventSourceFD *
hev_event_source_fd_ref (HevEventSourceFD *self)
{
	if (self) {
		self->_ref_count ++;
		return self;
	}

	return NULL;
}

inline void
hev_event_source_fd_unref (HevEventSourceFD *self)
{
	if (self) {
		self->_ref_count --;
		if (0 == self->_ref_count) {
			HEV_MEMORY_ALLOCATOR_FREE (self);
		}
	}
}

