/*
 ============================================================================
 Name        : hev-event-source-idle.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2013 everyone.
 Description : A idle event source
 ============================================================================
 */

#include <unistd.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>

#include "hev-event-source-idle.h"

static bool hev_event_source_idle_prepare (HevEventSource *source);
static bool hev_event_source_idle_check (HevEventSource *source, HevEventSourceFD *fd);
static bool hev_event_source_idle_dispatch (HevEventSource *source, HevEventSourceFD *fd,
			HevEventSourceFunc callback, void *data);
static void hev_event_source_idle_finalize (HevEventSource *source);

struct _HevEventSourceIdle
{
	HevEventSource parent;

	int event_fd;
};

static HevEventSourceFuncs hev_event_source_idle_funcs =
{
	.prepare = hev_event_source_idle_prepare,
	.check = hev_event_source_idle_check,
	.dispatch = hev_event_source_idle_dispatch,
	.finalize = hev_event_source_idle_finalize,
};

HevEventSource *
hev_event_source_idle_new (void)
{
	HevEventSource *source = hev_event_source_new (&hev_event_source_idle_funcs,
				sizeof (HevEventSourceIdle));
	if (source) {
		HevEventSourceIdle *self = (HevEventSourceIdle *) source;
		self->event_fd = eventfd (0, 0);
		hev_event_source_set_priority (source, INT32_MIN);
		hev_event_source_add_fd (source, self->event_fd, EPOLLIN | EPOLLET);
	}

	return source;
}

static bool
hev_event_source_idle_prepare (HevEventSource *source)
{
	HevEventSourceIdle *self = (HevEventSourceIdle *) source;
	eventfd_write (self->event_fd, 1);

	return true;
}

static bool
hev_event_source_idle_check (HevEventSource *source, HevEventSourceFD *fd)
{
	HevEventSourceIdle *self = (HevEventSourceIdle *) source;
	if (EPOLLIN & fd->revents) {
		eventfd_t val = 0;
		eventfd_read (self->event_fd, &val);
		return true;
	}

	return false;
}

static bool
hev_event_source_idle_dispatch (HevEventSource *source, HevEventSourceFD *fd,
			HevEventSourceFunc callback, void *data)
{
	return callback (data);
}

static void
hev_event_source_idle_finalize (HevEventSource *source)
{
	HevEventSourceIdle *self = (HevEventSourceIdle *) source;
	close (self->event_fd);
}
