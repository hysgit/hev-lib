/*
 ============================================================================
 Name        : event-loop.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2013 everyone.
 Description : An event loop example
 ============================================================================
 */

#include <hev-lib.h>

int
main (int argc, char *argv[])
{
	HevEventLoop *loop = NULL;

	loop = hev_event_loop_new ();
	hev_event_loop_run (loop);
	hev_event_loop_unref (loop);

	return 0;
}

