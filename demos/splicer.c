/*
 ============================================================================
 Name        : splicer.c
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

static HevSList *session_list = NULL;

typedef struct _Session Session;

struct _Session
{
	HevEventSourceFD *client_fd;
	HevEventSourceFD *remote_fd;
	HevRingBuffer *forward_buffer;
	HevRingBuffer *backward_buffer;
	bool idle;
};

static Session *
session_new (void)
{
	Session *session = HEV_MEMORY_ALLOCATOR_ALLOC (sizeof (Session));
	if (session) {
		session->client_fd = NULL;
		session->remote_fd = NULL;
		session->forward_buffer = NULL;
		session->backward_buffer = NULL;
		session->idle = false;
	}

	return session;
}

static void
session_free (Session *session)
{
	if (session) {
		if (session->remote_fd) {
			close (session->remote_fd->fd);
			hev_event_source_del_fd (session->remote_fd->source, session->remote_fd->fd);
		}
		if (session->client_fd) {
			close (session->client_fd->fd);
			hev_event_source_del_fd (session->client_fd->source, session->client_fd->fd);
		}
		hev_ring_buffer_unref (session->forward_buffer);
		hev_ring_buffer_unref (session->backward_buffer);
		HEV_MEMORY_ALLOCATOR_FREE (session);
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
session_source_handler (HevEventSourceFD *fd, void *data)
{
	Session *session = NULL;
	ssize_t size = 0;

	session = hev_event_source_fd_get_data (fd);

	if (EPOLLIN & fd->revents) {
		HevEventSourceFD *pair = NULL;
		HevRingBuffer *buffer = NULL;
		if (fd == session->client_fd) {
			pair = session->remote_fd;
			buffer = session->forward_buffer;
		} else {
			pair = session->client_fd;
			buffer = session->backward_buffer;
		}
		/* get write buffer */
		size = read_data (fd->fd, buffer);
		if (-2 < size) {
			if (-1 == size) {
				if (EAGAIN == errno)
				  fd->revents &= ~EPOLLIN;
				else
				  goto remove_session;
			} else if (0 == size) {
				goto remove_session;
			}
		}
		/* try write */
		size = write_data (pair->fd, buffer);
		if (-2 < size) {
			if (-1 == size) {
				if (EAGAIN != errno)
				  goto remove_session;
			}
		}
	}

	if (EPOLLOUT & fd->revents) {
		HevRingBuffer *buffer = NULL;
		if (fd == session->client_fd)
		  buffer = session->backward_buffer;
		else
		  buffer = session->forward_buffer;
		/* try write */
		size = write_data (fd->fd, buffer);
		if (-2 < size) {
			if (-1 == size) {
				if (EAGAIN != errno)
				  goto remove_session;
			}
		} else {
			fd->revents &= ~EPOLLOUT;
		}
	}

	if ((EPOLLERR | EPOLLHUP) & fd->revents)
	  goto remove_session;

	session->idle = false;

	return true;

remove_session:
	/* printf ("Remove session %p\n", session); */
	session_free (session);
	session_list = hev_slist_remove (session_list, session);

	return true;
}

static bool
listener_source_handler (HevEventSourceFD *fd, void *data)
{
	HevEventSource *session_source = data;
	struct sockaddr_in addr, raddr;
	socklen_t addr_len;
	int client_fd = -1, remote_fd = -1;

	addr_len = sizeof (addr);
	client_fd = accept (fd->fd, (struct sockaddr *) &addr,
				(socklen_t *) &addr_len);
	if (0 > client_fd) {
		if (EAGAIN == errno)
		  fd->revents &= ~EPOLLIN;
		else
		  printf ("Accept failed!\n");
	} else {
		Session *session = session_new ();
		set_fd_nonblock (client_fd, true);
		session->client_fd = hev_event_source_add_fd (session_source, client_fd,
					EPOLLIN | EPOLLOUT | EPOLLET);
		hev_event_source_fd_set_data (session->client_fd, session);
		remote_fd = socket (AF_INET, SOCK_STREAM, 0);
		set_fd_nonblock (remote_fd, true);
		session->remote_fd = hev_event_source_add_fd (session_source, remote_fd,
					EPOLLIN | EPOLLOUT | EPOLLET);
		hev_event_source_fd_set_data (session->remote_fd, session);
		memset (&raddr, 0, sizeof (raddr));
		raddr.sin_family = AF_INET;
		raddr.sin_addr.s_addr = inet_addr ("127.0.0.1");
		raddr.sin_port = htons (22);
		if ((0 > connect (remote_fd, (struct sockaddr *) &raddr,
						sizeof (raddr))) && (EINPROGRESS != errno)) {
			printf ("Connect to remote host failed, remove session %p\n", session);
			session_free (session);
			return true;
		}
		session->forward_buffer = hev_ring_buffer_new (2000);
		session->backward_buffer = hev_ring_buffer_new (2000);
		/* printf ("New session %p (%d, %d) enter from %s:%u\n", session,
			client_fd, remote_fd, inet_ntoa (addr.sin_addr), ntohs (addr.sin_port)); */
		session_list = hev_slist_append (session_list, session);
	}

	return true;
}

static bool
timeout_handler (void *data)
{
	HevSList *list = NULL;
	for (list=session_list; list; list=hev_slist_next (list)) {
		Session *session = hev_slist_data (list);
		if (session->idle) {
			/* printf ("Remove timeout session %p\n", session); */
			session_free (session);
			hev_slist_set_data (list, NULL);
		}
		session->idle = true;
	}
	session_list = hev_slist_remove_all (session_list, NULL);
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
	HevEventSource *source = NULL, *listener_source = NULL, *session_source = NULL;
	HevSList *list = NULL;
	int fd = -1, reuseaddr = 1;
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

	session_source = hev_event_source_fds_new ();
	hev_event_source_set_callback (session_source,
				(HevEventSourceFunc) session_source_handler, NULL, NULL);
	hev_event_loop_add_source (loop, session_source);
	hev_event_source_unref (session_source);

	listener_source = hev_event_source_fds_new ();
	hev_event_source_set_priority (listener_source, 2);
	hev_event_source_add_fd (listener_source, fd, EPOLLIN | EPOLLET);
	hev_event_source_set_callback (listener_source,
				(HevEventSourceFunc) listener_source_handler, session_source, NULL);
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

	for (list=session_list; list; list=hev_slist_next (list))
	  session_free (hev_slist_data (list));
	hev_slist_free (session_list);

	close (fd);
	hev_event_loop_unref (loop);

	return 0;
}

