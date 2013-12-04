/*
 ============================================================================
 Name        : echo-server.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2013 everyone.
 Description : Echo server example
 ============================================================================
 */

#include <hev-lib.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static HevSList *client_list = NULL;

typedef struct _Client Client;

struct _Client
{
	HevEventSourceFD *fd;
	HevRingBuffer *buffer;
	bool idle;
};

static Client *
client_new (void)
{
	Client *client = HEV_MEMORY_ALLOCATOR_ALLOC (sizeof (Client));
	if (client) {
		client->fd = NULL;
		client->buffer = NULL;
		client->idle = false;
	}

	return client;
}

static void
client_free (Client *client)
{
	if (client) {
		close (client->fd->fd);
		hev_event_source_del_fd (client->fd->source, client->fd->fd);
		hev_ring_buffer_unref (client->buffer);
		HEV_MEMORY_ALLOCATOR_FREE (client);
	}
}

static bool
set_fd_nonblock (int fd, bool nonblock)
{
	int on = nonblock ? 1 : 0;
	if (0 > ioctl (fd, FIONBIO, (char *) &on))
	  return false;

	return  true;
}

static ssize_t
read_data (int fd, HevRingBuffer *buffer)
{
	struct msghdr mh;
	struct iovec iovec[2];
	size_t iovec_len = 0, inc_len = 0;
	ssize_t size = -2;

	iovec_len = hev_ring_buffer_writing (buffer, iovec);
	if (0 < iovec_len) {
		/* recv data */
		memset (&mh, 0, sizeof (mh));
		mh.msg_iov = iovec;
		mh.msg_iovlen = iovec_len;
		size = recvmsg (fd, &mh, 0);
		inc_len = (0 > size) ? 0 : size;
		hev_ring_buffer_write_finish (buffer, inc_len);
	}

	return size;
}

static ssize_t
write_data (int fd, HevRingBuffer *buffer)
{
	struct msghdr mh;
	struct iovec iovec[2];
	size_t iovec_len = 0, inc_len = 0;
	ssize_t size = -2;

	iovec_len = hev_ring_buffer_reading (buffer, iovec);
	if (0 < iovec_len) {
		/* send data */
		memset (&mh, 0, sizeof (mh));
		mh.msg_iov = iovec;
		mh.msg_iovlen = iovec_len;
		size = sendmsg (fd, &mh, 0);
		inc_len = (0 > size) ? 0 : size;
		hev_ring_buffer_read_finish (buffer, inc_len);
	}

	return size;
}

static bool
client_source_handler (HevEventSourceFD *fd, void *data)
{
	Client *client = NULL;
	ssize_t size = 0;
	bool trywrite = false;

	client = hev_event_source_fd_get_data (fd);

	if (EPOLLIN & fd->revents) {
		size = read_data (fd->fd, client->buffer);
		if (-2 < size) {
			if (-1 == size) {
				if (EAGAIN == errno)
				  fd->revents &= ~EPOLLIN;
				else
				  goto remove_client;
			} else if (0 == size) {
				goto remove_client;
			}
		}
		/* activate try write */
		trywrite = true;
	}

	if ((EPOLLOUT & fd->revents) || trywrite ) {
		/* try write */
		size = write_data (fd->fd, client->buffer);
		if (-2 < size) {
			if (-1 == size) {
				if (EAGAIN != errno)
				  goto remove_client;
			}
		} else {
			fd->revents &= ~EPOLLOUT;
		}
	}

	if ((EPOLLERR | EPOLLHUP) & fd->revents)
	  goto remove_client;

	client->idle = false;

	return true;

remove_client:
	printf ("Client %d leave\n", fd->fd);
	client_free (client);
	client_list = hev_slist_remove (client_list, client);

	return true;
}

static bool
listener_source_handler (HevEventSourceFD *fd, void *data)
{
	HevEventSource *client_source = data;
	struct sockaddr_in addr;
	socklen_t addr_len;
	int client_fd = 0;

	addr_len = sizeof (addr);
	client_fd = accept (fd->fd, (struct sockaddr *) &addr,
				(socklen_t *) &addr_len);
	if (0 > client_fd) {
		if (EAGAIN == errno)
		  fd->revents &= ~EPOLLIN;
		else
		  printf ("Accept failed!\n");
	} else {
		Client *client = client_new ();
		client->buffer = hev_ring_buffer_new (1024);
		printf ("New client %d enter from %s:%u\n",
			client_fd, inet_ntoa (addr.sin_addr), ntohs (addr.sin_port));
		set_fd_nonblock (client_fd, true);
		client->fd = hev_event_source_add_fd (client_source, client_fd, EPOLLIN | EPOLLOUT | EPOLLET);
		hev_event_source_fd_set_data (client->fd, client);
		client_list = hev_slist_append (client_list, client);
	}

	return true;
}

static bool
timeout_handler (void *data)
{
	HevSList *list = NULL;
	for (list=client_list; list; list=hev_slist_next (list)) {
		Client *client = hev_slist_data (list);
		if (client->idle) {
			printf ("Remove timeout client %d\n", client->fd->fd);
			client_free (client);
			hev_slist_set_data (list, NULL);
		} else {
			client->idle = true;
		}
	}
	client_list = hev_slist_remove_all (client_list, NULL);
	return true;
}

static bool
signal_pipe_handler (void *data)
{
	return true;
}

static bool
signal_int_handler (void *data)
{
	HevEventLoop *loop = data;

	printf ("Quiting...\n");
	hev_event_loop_quit (loop);

	return true;
}

int
main (int argc, char *argv[])
{
	HevEventLoop *loop = NULL;
	HevEventSource *source = NULL, *listener_source = NULL, *client_source = NULL;
	HevSList *list = NULL;
	int fd = 0, reuseaddr = 1;
	struct sockaddr_in addr;

	loop = hev_event_loop_new ();

	fd = socket (AF_INET, SOCK_STREAM, 0);
	if (0 > fd)
	  exit (1);
	setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof (reuseaddr));
	set_fd_nonblock (fd, true);
	memset (&addr, 0, sizeof (addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr ("0.0.0.0");
	addr.sin_port = htons (8000);
	if (0 > bind (fd, (struct sockaddr *) &addr, (socklen_t) sizeof (addr)))
	  exit (2);
	if (0 > listen (fd, 100))
	  exit (3);

	client_source = hev_event_source_fds_new ();
	hev_event_source_set_callback (client_source,
				(HevEventSourceFunc) client_source_handler, NULL, NULL);
	hev_event_loop_add_source (loop, client_source);
	hev_event_source_unref (client_source);

	listener_source = hev_event_source_fds_new ();
	hev_event_source_set_priority (listener_source, 2);
	hev_event_source_add_fd (listener_source, fd, EPOLLIN | EPOLLET);
	hev_event_source_set_callback (listener_source,
				(HevEventSourceFunc) listener_source_handler, client_source, NULL);
	hev_event_loop_add_source (loop, listener_source);
	hev_event_source_unref (listener_source);

	source = hev_event_source_timeout_new (30 * 1000);
	hev_event_source_set_priority (source, 1);
	hev_event_source_set_callback (source, timeout_handler, NULL, NULL);
	hev_event_loop_add_source (loop, source);
	hev_event_source_unref (source);

	source = hev_event_source_signal_new (SIGINT);
	hev_event_source_set_priority (source, 3);
	hev_event_source_set_callback (source, signal_int_handler, loop, NULL);
	hev_event_loop_add_source (loop, source);
	hev_event_source_unref (source);

	source = hev_event_source_signal_new (SIGPIPE);
	hev_event_source_set_callback (source, signal_pipe_handler, NULL, NULL);
	hev_event_loop_add_source (loop, source);
	hev_event_source_unref (source);

	hev_event_loop_run (loop);

	for (list=client_list; list; list=hev_slist_next (list))
	  client_free (hev_slist_data (list));
	hev_slist_free (client_list);

	close (fd);
	hev_event_loop_unref (loop);

	return 0;
}

