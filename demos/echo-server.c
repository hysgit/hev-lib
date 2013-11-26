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
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

typedef struct _RingBuffer RingBuffer;

struct _RingBuffer
{
	unsigned int wp;
	unsigned int rp;
	unsigned int len;
	bool full;
	uint8_t *buffer;
};

RingBuffer *
ring_buffer_new (unsigned int len)
{
	RingBuffer *buf = NULL;
	buf = HEV_MEMORY_ALLOCATOR_ALLOC (sizeof (RingBuffer) + len);
	if (buf) {
		buf->wp = 0;
		buf->rp = 0;
		buf->len = len;
		buf->full = false;
		buf->buffer = ((void *) buf) + sizeof (RingBuffer);
	}

	return buf;
}

void
ring_buffer_free (RingBuffer *self)
{
	HEV_MEMORY_ALLOCATOR_FREE (self);
}

int
ring_buffer_reading (RingBuffer *self, struct iovec *iovec)
{
	if (self && iovec) {
		int len = self->wp - self->rp;

		if (0 <= len) {
			iovec[0].iov_base = self->buffer + self->rp;
			iovec[0].iov_len = len;
			return 1;
		} else {
			iovec = HEV_MEMORY_ALLOCATOR_ALLOC (sizeof (struct iovec) * 2);
			iovec[0].iov_base = self->buffer + self->rp;
			iovec[0].iov_len = self->len - self->rp;
			iovec[1].iov_base = self->buffer;
			iovec[1].iov_len = self->wp;
			return 2;
		}
	}

	return 0;
}

void
ring_buffer_read_finish (RingBuffer *self, unsigned int inc_len)
{
	if (self) {
		unsigned int p = inc_len + self->rp;
		if (self->len < p) {
			self->rp = p - self->len;
		} else {
			self->rp = p;
		}
		if (self->wp == self->rp) {
			self->rp = 0;
			self->wp = 0;
			self->full = false;
		}
	}
}

int
ring_buffer_writing (RingBuffer *self, struct iovec *iovec)
{
	if (self) {
		int len = self->rp - self->wp;

		if (0 <= len) {
			if ((0 == len) && !self->full) {
				iovec[0].iov_base = self->buffer;
				iovec[0].iov_len = self->len;
			} else {
				iovec[0].iov_base = self->buffer + self->wp;
				iovec[0].iov_len = len;
			}
			return 1;
		} else {
			iovec[0].iov_base = self->buffer + self->wp;
			iovec[0].iov_len = self->len - self->wp;
			iovec[1].iov_base = self->buffer;
			iovec[1].iov_len = self->rp;
			return 2;
		}
	}

	return 0;
}

void
ring_buffer_write_finish (RingBuffer *self, unsigned int inc_len)
{
	if (self) {
		unsigned int p = inc_len + self->wp;
		if (self->len < p) {
			self->wp = p - self->len;
		} else {
			self->wp = p;
		}
		if (self->wp == self->rp)
		  self->full = true;
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

static bool
client_source_handler (HevEventSourceFD *fd, void *data)
{
	RingBuffer *buffer = NULL;
	struct msghdr mh;
	struct iovec iovec[2];
	int iovec_len = 0;
	unsigned int inc_len = 0;
	ssize_t size = 0;

	memset (&mh, 0, sizeof (mh));
	buffer = hev_event_source_fd_get_data (fd);
	if (EPOLLIN & fd->revents) {
		iovec_len = ring_buffer_writing (buffer, iovec);
		mh.msg_iov = iovec;
		mh.msg_iovlen = iovec_len;
		size = recvmsg (fd->fd, &mh, 0);
		inc_len = (0 > size) ? 0 : size;
		ring_buffer_write_finish (buffer, inc_len);

		if (0 == size) {
			printf ("Client %d leave\n", fd->fd);
			close (fd->fd);
			hev_event_source_del_fd (fd->source, fd->fd);
			ring_buffer_free (buffer);
		}

		fd->revents &= ~EPOLLIN;
	}
	if (EPOLLOUT & fd->revents) {
		iovec_len = ring_buffer_reading (buffer, iovec);
		mh.msg_iov = iovec;
		mh.msg_iovlen = iovec_len;
		size = sendmsg (fd->fd, &mh, 0);
		inc_len = (0 > size) ? 0 : size;
		ring_buffer_read_finish (buffer, inc_len);

		fd->revents &= ~EPOLLOUT;
	}

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
		printf ("Accept failed!\n");
	} else {
		RingBuffer *buffer = NULL;
		HevEventSourceFD *_fd = NULL;
		printf ("New client %d enter from %s:%u\n",
			client_fd, inet_ntoa (addr.sin_addr), ntohs (addr.sin_port));
		buffer = ring_buffer_new (1024);
		set_fd_nonblock (client_fd, true);
		_fd = hev_event_source_add_fd (client_source, client_fd, EPOLLIN | EPOLLOUT | EPOLLET);
		hev_event_source_fd_set_data (_fd, buffer);
	}

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
	int fd = 0;
	struct sockaddr_in addr;

	loop = hev_event_loop_new ();

	fd = socket (AF_INET, SOCK_STREAM, 0);
	set_fd_nonblock (fd, true);
	if (0 > fd)
	  exit (1);
	memset (&addr, 0, sizeof (addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr ("0.0.0.0");
	addr.sin_port = htons (8000);
	if (0 > bind (fd, (struct sockaddr *) &addr, (socklen_t) sizeof (addr)))
	  exit (2);
	if (0 > listen (fd, 5))
	  exit (3);

	client_source = hev_event_source_fds_new ();
	hev_event_source_set_callback (client_source,
				(HevEventSourceFunc) client_source_handler, NULL, NULL);
	hev_event_loop_add_source (loop, client_source);
	hev_event_source_unref (client_source);

	listener_source = hev_event_source_fds_new ();
	hev_event_source_add_fd (listener_source, fd, EPOLLIN | EPOLLET);
	hev_event_source_set_callback (listener_source,
				(HevEventSourceFunc) listener_source_handler, client_source, NULL);
	hev_event_loop_add_source (loop, listener_source);
	hev_event_source_unref (listener_source);

	source = hev_event_source_signal_new (SIGINT);
	hev_event_source_set_callback (source, signal_int_handler, loop, NULL);
	hev_event_loop_add_source (loop, source);
	hev_event_source_unref (source);

	source = hev_event_source_signal_new (SIGPIPE);
	hev_event_source_set_callback (source, signal_pipe_handler, NULL, NULL);
	hev_event_loop_add_source (loop, source);
	hev_event_source_unref (source);

	hev_event_loop_run (loop);

	hev_event_loop_unref (loop);

	return 0;
}

