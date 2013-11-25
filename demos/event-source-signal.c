/*
 ============================================================================
 Name        : event-source-signal.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2013 everyone.
 Description : An event source signal example
 ============================================================================
 */

#include <stdio.h>
#include <signal.h>
#include <hev-lib.h>

static bool
signal_handler (HevEventSourceFD *fd, void *data)
{
	printf ("signal\n");

	return true;
}

int
main (int argc, char *argv[])
{
	HevEventLoop *loop = NULL;
	HevEventSource *signal = NULL;

	loop = hev_event_loop_new ();

	signal = hev_event_source_signal_new (SIGQUIT);
	hev_event_source_set_callback (signal, signal_handler, NULL, NULL);
	hev_event_loop_add_source (loop, signal);
	hev_event_source_unref (signal);

	hev_event_loop_run (loop);

	hev_event_loop_unref (loop);

	return 0;
}

