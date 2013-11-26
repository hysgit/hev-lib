/*
 ============================================================================
 Name        : object.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2013 everyone.
 Description : Object example
 ============================================================================
 */

#include <hev-lib.h>
#include <glib-object.h>

int
main (int argc, char *argv[])
{
	HevEventLoop *loop = NULL;
	GObject *object = NULL;

	loop = hev_event_loop_new ();
	object = g_object_new (G_TYPE_OBJECT, NULL);

	hev_event_loop_run (loop);

	g_object_unref (object);
	hev_event_loop_unref (loop);

	return 0;
}

