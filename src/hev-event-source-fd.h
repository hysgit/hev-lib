/*
 ============================================================================
 Name        : hev-event-source-fd.h
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2013 everyone.
 Description : An event source FD
 ============================================================================
 */

#include "hev-event-source.h"

#ifndef __HEV_EVENT_SOURCE_FD_H__
#define __HEV_EVENT_SOURCE_FD_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct _HevEventSourceFD HevEventSourceFD;

struct _HevEventSourceFD
{
	int fd;
	uint32_t _events;
	uint32_t revents;
	uint32_t _dispatched;
	unsigned int _ref_count;

	HevEventSource *source;
	void *data;
};

static inline HevEventSourceFD *
_hev_event_source_fd_new (HevEventSource *source, int fd, uint32_t events)
{
	HevEventSourceFD *self = HEV_MEMORY_ALLOCATOR_ALLOC (sizeof (HevEventSourceFD));
	if (self) {
		self->fd = fd;
		self->_events = events;
		self->revents = 0;
		self->_dispatched = 0;
		self->_ref_count = 1;
		self->source = source;
		self->data = NULL;
	}

	return self;
}

static inline HevEventSourceFD *
_hev_event_source_fd_ref (HevEventSourceFD *self)
{
	self->_ref_count ++;
	return self;
}

static inline void
_hev_event_source_fd_unref (HevEventSourceFD *self)
{
	self->_ref_count --;
	if (self->_ref_count)
	  return;

	HEV_MEMORY_ALLOCATOR_FREE (self);
}

static inline void
_hev_event_source_fd_dispatch (HevEventSourceFD *self)
{
	self->_dispatched = 1;
	_hev_event_source_fd_ref (self);
}

static inline void
_hev_event_source_fd_dispatch_finish (HevEventSourceFD *self)
{
	self->_dispatched = 0;
	_hev_event_source_fd_unref (self);
}

static inline void
hev_event_source_fd_set_data (HevEventSourceFD *self, void *data)
{
	self->data = data;
}

static inline void *
hev_event_source_fd_get_data (HevEventSourceFD *self)
{
	return self->data;
}

static inline void
_hev_event_source_fd_clear_source (HevEventSourceFD *self)
{
	self->source = NULL;
}

#endif /* __HEV_EVENT_SOURCE_H__ */

