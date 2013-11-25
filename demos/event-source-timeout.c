/*
 ============================================================================
 Name        : event-source-timeout.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2013 everyone.
 Description : An event source timeout example
 ============================================================================
 */

#include <stdio.h>
#include <hev-lib.h>

static bool
timeout_handler (HevEventSourceFD *fd, void *data)
{
	printf ("timeout\n");
	return true;
}

int
main (int argc, char *argv[])
{
	HevEventLoop *loop = NULL;
	HevEventSource *timeout = NULL;

	loop = hev_event_loop_new ();

	timeout = hev_event_source_timeout_new (1000);
	hev_event_source_set_callback (timeout, timeout_handler, NULL, NULL);
	hev_event_loop_add_source (loop, timeout);
	hev_event_source_unref (timeout);

	hev_event_loop_run (loop);

	hev_event_loop_unref (loop);

	return 0;
}

