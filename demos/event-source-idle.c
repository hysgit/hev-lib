/*
 ============================================================================
 Name        : event-source-idle.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2013 everyone.
 Description : An event source idle example
 ============================================================================
 */

#include <stdio.h>
#include <hev-lib.h>

static bool
idle_handler (void *data)
{
	printf ("idle\n");
	return true;
}

int
main (int argc, char *argv[])
{
	HevEventLoop *loop = NULL;
	HevEventSource *idle = NULL;

	loop = hev_event_loop_new ();

	idle = hev_event_source_idle_new ();
	hev_event_source_set_callback (idle, idle_handler, NULL, NULL);
	hev_event_loop_add_source (loop, idle);
	hev_event_source_unref (idle);

	hev_event_loop_run (loop);

	hev_event_loop_unref (loop);

	return 0;
}

