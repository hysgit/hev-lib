/*
 ============================================================================
 Name        : hev-event-loop.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2013 everyone.
 Description : An event loop
 ============================================================================
 */

#include <stddef.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>

#include "hev-slist.h"
#include "hev-event-loop.h"

struct _HevEventLoop
{
	int epoll_fd;
	unsigned int ref_count;

	bool run;
	HevEventSource *source_quit;
	HevSList *sources;
};

typedef struct _HevEventSourceQuit HevEventSourceQuit;

struct _HevEventSourceQuit
{
	HevEventSource parent;
	int event_fd;
};

static bool hev_event_source_quit_check (HevEventSource *source,
			HevEventSourceFD *fd);
static void hev_event_source_quit_finalize (HevEventSource *source);
static bool hev_event_source_quit_handler (void *data);

static HevEventSourceFuncs hev_event_source_quit_funcs =
{
	.prepare = NULL,
	.check = hev_event_source_quit_check,
	.dispatch = NULL,
	.finalize = hev_event_source_quit_finalize,
};

HevEventLoop *
hev_event_loop_new (void)
{
	HevEventLoop *self = HEV_MEMORY_ALLOCATOR_ALLOC (sizeof (HevEventLoop));

	if (self) {
		self->epoll_fd = epoll_create (1024);
		self->ref_count = 1;
		self->run = true;
		self->sources = NULL;

		/* quit event source */
		HevEventSourceQuit *quit = NULL;
		self->source_quit = hev_event_source_new (&hev_event_source_quit_funcs,
					sizeof (HevEventSourceQuit));
		quit = (HevEventSourceQuit *) self->source_quit;
		hev_event_source_set_callback (self->source_quit, hev_event_source_quit_handler,
					self, NULL);
		quit->event_fd = eventfd (0, 0);
		hev_event_source_add_fd (self->source_quit, quit->event_fd, EPOLLIN | EPOLLET | EPOLLONESHOT);
		hev_event_loop_add_source (self, self->source_quit);
		hev_event_source_unref (self->source_quit);
	}

	return self;
}

HevEventLoop *
hev_event_loop_ref (HevEventLoop *self)
{
	if (self)
	  self->ref_count ++;

	return self;
}

void
hev_event_loop_unref (HevEventLoop *self)
{
	if (self) {
		self->ref_count --;
		if (0 == self->ref_count) {
			HevSList *list = NULL;
			for (list=self->sources; list; list=hev_slist_next (list))
			  hev_event_source_unref (hev_slist_data (list));
			hev_slist_free (self->sources);
			close (self->epoll_fd);
			HEV_MEMORY_ALLOCATOR_FREE (self);
		}
	}
}

void
hev_event_loop_run (HevEventLoop *self)
{
	if (!self)
	  return;

	while (self->run) {
		struct epoll_event events[256];
		int i = 0, nfds = 0;
		HevSList *invalid_sources = NULL;

		/* poll */
		nfds = epoll_wait (self->epoll_fd,
					events, 256, -1);
		/* check & dispatch */
		for (i=0; i<nfds; i++) {
			HevEventSourceFD *fd = events[i].data.ptr;
			HevEventSource *source = fd->source;
			fd->revents = events[i].events;
			if (source->funcs.check (source, fd)) {
				if (source->funcs.dispatch (source, fd,
					source->callback.callback, source->callback.data)) {
					source->funcs.prepare (source);
				} else {
					invalid_sources = hev_slist_append (invalid_sources, source);
				}
			}
		}
		/* delete invalid sources */
		if (invalid_sources) {
			HevSList *list = NULL;
			for (list=invalid_sources; list; list=hev_slist_next (list))
			  hev_event_loop_del_source (self, hev_slist_data (list));
			hev_slist_free (invalid_sources);
		}
	}
}

void
hev_event_loop_quit (HevEventLoop *self)
{
	if (self) {
		HevEventSourceQuit *quit = (HevEventSourceQuit *) self->source_quit;
		eventfd_write (quit->event_fd, 1);
	}
}

bool
hev_event_loop_add_source (HevEventLoop *self, HevEventSource *source)
{
	if (self && source) {
		HevSList *list = NULL;
		self->sources = hev_slist_append (self->sources, hev_event_source_ref (source));
		for (list=source->fds; list; list=hev_slist_next (list)) {
			HevEventSourceFD *fd = hev_slist_data (list);
			_hev_event_loop_add_fd (self, fd);
		}
		source->funcs.prepare (source);
	}

	return false;
}

bool
hev_event_loop_del_source (HevEventLoop *self, HevEventSource *source)
{
	if (self && source) {
		HevSList *list = NULL;
		self->sources = hev_slist_remove (self->sources, source);
		for (list=source->fds; list; list=hev_slist_next (list)) {
			HevEventSourceFD *fd = hev_slist_data (list);
			_hev_event_loop_del_fd (self, fd);
		}
		hev_event_source_unref (source);
	}

	return false;
}

bool
_hev_event_loop_add_fd (HevEventLoop *self, HevEventSourceFD *fd)
{
	if (self && fd) {
		struct epoll_event event;
		event.events = fd->events;
		event.data.ptr = fd;
		return (0 == epoll_ctl (self->epoll_fd,
					EPOLL_CTL_ADD, fd->fd, &event));
	}

	return false;
}

bool
_hev_event_loop_del_fd (HevEventLoop *self, HevEventSourceFD *fd)
{
	if (self && fd) {
		return (0 == epoll_ctl (self->epoll_fd,
					EPOLL_CTL_DEL, fd->fd, NULL));
	}

	return false;
}

static bool
hev_event_source_quit_check (HevEventSource *source,
			HevEventSourceFD *fd)
{
	if (EPOLLIN & fd->revents)
	  return true;
	return false;
}

static void
hev_event_source_quit_finalize (HevEventSource *source)
{
	HevEventSourceQuit *quit = (HevEventSourceQuit *) source;
	close (quit->event_fd);
}

static bool
hev_event_source_quit_handler (void *data)
{
	HevEventLoop *loop = data;
	loop->run = false;
	return false;
}

