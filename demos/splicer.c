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

enum
{
	CLIENT_IN = (1 << 3),
	CLIENT_OUT = (1 << 2),
	REMOTE_IN = (1 << 1),
	REMOTE_OUT = (1 << 0),
};

static HevSList *session_list = NULL;

typedef struct _Session Session;

struct _Session
{
	HevEventSource *source;
	HevEventSourceFD *client_fd;
	HevEventSourceFD *remote_fd;
	HevRingBuffer *forward_buffer;
	HevRingBuffer *backward_buffer;
	uint8_t revents;
	bool idle;
};

static bool session_source_handler (HevEventSourceFD *fd, void *data);

static Session *
session_new (void)
{
	Session *session = HEV_MEMORY_ALLOCATOR_ALLOC (sizeof (Session));
	if (session) {
		session->source = hev_event_source_fds_new ();
		hev_event_source_set_callback (session->source,
					(HevEventSourceFunc) session_source_handler, session, NULL);
		session->client_fd = NULL;
		session->remote_fd = NULL;
		session->forward_buffer = NULL;
		session->backward_buffer = NULL;
		session->revents = 0;
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
			hev_event_source_del_fd (session->source, session->remote_fd->fd);
		}
		if (session->client_fd) {
			close (session->client_fd->fd);
			hev_event_source_del_fd (session->source, session->client_fd->fd);
		}
		hev_ring_buffer_unref (session->forward_buffer);
		hev_ring_buffer_unref (session->backward_buffer);
		hev_event_source_unref (session->source);
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
	Session *session = data;
	ssize_t size = 0;

	if ((EPOLLERR | EPOLLHUP) & fd->revents)
	  goto remove_session;

	if (fd == session->client_fd) {
		if (EPOLLIN & fd->revents)
		  session->revents |= CLIENT_IN;
		if (EPOLLOUT & fd->revents)
		  session->revents |= CLIENT_OUT;
	} else {
		if (EPOLLIN & fd->revents)
		  session->revents |= REMOTE_IN;
		if (EPOLLOUT & fd->revents)
		  session->revents |= REMOTE_OUT;
	}

	if (CLIENT_OUT & session->revents) {
		size = write_data (session->client_fd->fd, session->backward_buffer);
		if (-2 < size) {
			if (-1 == size) {
				if (EAGAIN == errno) {
					session->revents &= ~CLIENT_OUT;
					session->client_fd->revents &= ~EPOLLOUT;
				} else {
					goto remove_session;
				}
			}
		} else {
			session->client_fd->revents &= ~EPOLLOUT;
		}
	}

	if (REMOTE_OUT & session->revents) {
		size = write_data (session->remote_fd->fd, session->forward_buffer);
		if (-2 < size) {
			if (-1 == size) {
				if (EAGAIN == errno) {
					session->revents &= ~REMOTE_OUT;
					session->remote_fd->revents &= ~EPOLLOUT;
				} else {
					goto remove_session;
				}
			}
		} else {
			session->remote_fd->revents &= ~EPOLLOUT;
		}
	}

	if (CLIENT_IN & session->revents) {
		size = read_data (session->client_fd->fd, session->forward_buffer);
		if (-2 < size) {
			if (-1 == size) {
				if (EAGAIN == errno) {
					session->revents &= ~CLIENT_IN;
					session->client_fd->revents &= ~EPOLLIN;
				} else {
					goto remove_session;
				}
			} else if (0 == size) {
				goto remove_session;
			}
		} else {
			session->client_fd->revents &= ~EPOLLIN;
		}
	}

	if (REMOTE_IN & session->revents) {
		size = read_data (session->remote_fd->fd, session->backward_buffer);
		if (-2 < size) {
			if (-1 == size) {
				if (EAGAIN == errno) {
					session->revents &= ~REMOTE_IN;
					session->remote_fd->revents &= ~EPOLLIN;
				} else {
					goto remove_session;
				}
			} else if (0 == size) {
				goto remove_session;
			}
		} else {
			session->remote_fd->revents &= ~EPOLLIN;
		}
	}

	session->idle = false;

	return true;

remove_session:
	/* printf ("Remove session %p\n", session); */
	session_free (session);
	session_list = hev_slist_remove (session_list, session);

	return false;
}

static bool
listener_source_handler (HevEventSourceFD *fd, void *data)
{
	HevEventLoop *loop = data;
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
		hev_event_loop_add_source (loop, session->source);
		set_fd_nonblock (client_fd, true);
		session->client_fd = hev_event_source_add_fd (session->source, client_fd,
					EPOLLIN | EPOLLOUT | EPOLLET);
		remote_fd = socket (AF_INET, SOCK_STREAM, 0);
		set_fd_nonblock (remote_fd, true);
		session->remote_fd = hev_event_source_add_fd (session->source, remote_fd,
					EPOLLIN | EPOLLOUT | EPOLLET);
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
	HevEventLoop *loop = data;
	HevSList *list = NULL;
	for (list=session_list; list; list=hev_slist_next (list)) {
		Session *session = hev_slist_data (list);
		if (session->idle) {
			/* printf ("Remove timeout session %p\n", session); */
			hev_event_loop_del_source (loop, session->source);
			session_free (session);
			hev_slist_set_data (list, NULL);
		} else {
			session->idle = true;
		}
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
	HevEventSource *source = NULL, *listener_source = NULL;
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

	listener_source = hev_event_source_fds_new ();
	hev_event_source_set_priority (listener_source, 2);
	hev_event_source_add_fd (listener_source, fd, EPOLLIN | EPOLLET);
	hev_event_source_set_callback (listener_source,
				(HevEventSourceFunc) listener_source_handler, loop, NULL);
	hev_event_loop_add_source (loop, listener_source);
	hev_event_source_unref (listener_source);

	source = hev_event_source_timeout_new (10 * 1000);
	hev_event_source_set_priority (source, -1);
	hev_event_source_set_callback (source, timeout_handler, loop, NULL);
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

