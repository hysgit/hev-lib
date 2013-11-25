/*
 ============================================================================
 Name        : hev-event-source-signal.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2013 everyone.
 Description : A signal event source
 ============================================================================
 */

#include <unistd.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>

#include "hev-event-source-signal.h"

static bool hev_event_source_signal_check (HevEventSource *source, HevEventSourceFD *fd);
static void hev_event_source_signal_finalize (HevEventSource *source);

struct _HevEventSourceSignal
{
	HevEventSource parent;

	int signal_fd;
};

static HevEventSourceFuncs hev_event_source_signal_funcs =
{
	.prepare = NULL,
	.check = hev_event_source_signal_check,
	.dispatch = NULL,
	.finalize = hev_event_source_signal_finalize,
};

HevEventSource *
hev_event_source_signal_new (int signal)
{
	HevEventSource *source = hev_event_source_new (&hev_event_source_signal_funcs,
				sizeof (HevEventSourceSignal));
	if (source) {
		HevEventSourceSignal *self = (HevEventSourceSignal *) source;
		sigset_t mask;
		sigemptyset (&mask);
		sigaddset (&mask, signal);
		sigprocmask(SIG_BLOCK, &mask, NULL);
		self->signal_fd = signalfd (-1, &mask, SFD_NONBLOCK);
		hev_event_source_add_fd (source, self->signal_fd, EPOLLIN | EPOLLET, NULL);
	}

	return source;
}

static bool
hev_event_source_signal_check (HevEventSource *source, HevEventSourceFD *fd)
{
	HevEventSourceSignal *self = (HevEventSourceSignal *) source;
	if (EPOLLIN & fd->revents) {
		struct signalfd_siginfo siginfo;
		read (self->signal_fd, &siginfo, sizeof (struct signalfd_siginfo));
		return true;
	}

	return false;
}

static void
hev_event_source_signal_finalize (HevEventSource *source)
{
	HevEventSourceSignal *self = (HevEventSourceSignal *) source;
	close (self->signal_fd);
}

