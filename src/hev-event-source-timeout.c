/*
 ============================================================================
 Name        : hev-event-source-timeout.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2013 everyone.
 Description : A timeout event source
 ============================================================================
 */

#include <time.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>

#include "hev-event-source-timeout.h"

static bool hev_event_source_timeout_prepare (HevEventSource *source);
static bool hev_event_source_timeout_check (HevEventSource *source, HevEventSourceFD *fd);
static void hev_event_source_timeout_finalize (HevEventSource *source);

struct _HevEventSourceTimeout
{
	HevEventSource parent;

	int timer_fd;
	unsigned int interval;
};

static HevEventSourceFuncs hev_event_source_timeout_funcs =
{
	.prepare = hev_event_source_timeout_prepare,
	.check = hev_event_source_timeout_check,
	.dispatch = NULL,
	.finalize = hev_event_source_timeout_finalize,
};

HevEventSource *
hev_event_source_timeout_new (unsigned int interval)
{
	HevEventSource *source = hev_event_source_new (&hev_event_source_timeout_funcs,
				sizeof (HevEventSourceTimeout));
	if (source) {
		HevEventSourceTimeout *self = (HevEventSourceTimeout *) source;
		self->timer_fd = timerfd_create (CLOCK_MONOTONIC, TFD_NONBLOCK);
		self->interval = interval;
		hev_event_source_add_fd (source, self->timer_fd, EPOLLIN | EPOLLET, NULL);
	}

	return source;
}

static bool
hev_event_source_timeout_prepare (HevEventSource *source)
{
	HevEventSourceTimeout *self = (HevEventSourceTimeout *) source;
	uint64_t time = 0;
	struct itimerspec spec;
	spec.it_interval.tv_sec = 0;
	spec.it_interval.tv_nsec = 0;
	clock_gettime (CLOCK_MONOTONIC, &spec.it_value);
	time = spec.it_value.tv_sec * 1000000000 + spec.it_value.tv_nsec;
	time += self->interval * 1000000;
	spec.it_value.tv_sec = time / 1000000000;
	spec.it_value.tv_nsec = time % 1000000000;
	timerfd_settime (self->timer_fd, TFD_TIMER_ABSTIME, &spec, NULL);

	return true;
}

static bool
hev_event_source_timeout_check (HevEventSource *source, HevEventSourceFD *fd)
{
	HevEventSourceTimeout *self = (HevEventSourceTimeout *) source;
	if (EPOLLIN & fd->revents) {
		uint64_t time;
		read (self->timer_fd, &time, sizeof (uint64_t));
		return true;
	}

	return false;
}

static void
hev_event_source_timeout_finalize (HevEventSource *source)
{
	HevEventSourceTimeout *self = (HevEventSourceTimeout *) source;
	close (self->timer_fd);
}

