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
	unsigned int _ref_count;

	HevEventSource *source;
	void *data;
};

inline HevEventSourceFD * hev_event_source_fd_new (HevEventSource *source, int fd, uint32_t events);

inline HevEventSourceFD * hev_event_source_fd_ref (HevEventSourceFD *self);
inline void hev_event_source_fd_unref (HevEventSourceFD *self);

static inline void
hev_event_source_fd_set_data (HevEventSourceFD *self, void *data)
{
	if (self)
	  self->data = data;
}

static inline void *
hev_event_source_fd_get_data (HevEventSourceFD *self)
{
	return self ? self->data : NULL;
}

#endif /* __HEV_EVENT_SOURCE_H__ */

