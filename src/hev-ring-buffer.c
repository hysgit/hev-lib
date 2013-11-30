/*
 ============================================================================
 Name        : hev-ring-buffer.c
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2013 everyone.
 Description : Ring buffer data structure
 ============================================================================
 */

#include <stdint.h>

#include "hev-ring-buffer.h"
#include "hev-memory-allocator.h"

struct _HevRingBuffer
{
	uint8_t *buffer;
	unsigned int ref_count;

	size_t wp;
	size_t rp;
	size_t len;
	bool full;
};

HevRingBuffer *
hev_ring_buffer_new (size_t len)
{
	HevRingBuffer *self = NULL;
	self = HEV_MEMORY_ALLOCATOR_ALLOC (sizeof (HevRingBuffer) + len);
	if (self) {
		self->wp = 0;
		self->rp = 0;
		self->len = len;
		self->full = false;
		self->buffer = ((void *) self) + sizeof (HevRingBuffer);
	}

	return self;
}

HevRingBuffer *
hev_ring_buffer_ref (HevRingBuffer *self)
{
	if (self)
	  self->ref_count ++;

	return self;
}

void
hev_ring_buffer_unref (HevRingBuffer *self)
{
	if (self) {
		self->ref_count --;
		if (0 == self->ref_count)
		  HEV_MEMORY_ALLOCATOR_FREE (self);
	}
}

size_t
hev_ring_buffer_reading (HevRingBuffer *self, struct iovec *iovec)
{
	if (self && iovec) {
		ssize_t len = self->wp - self->rp;

		if ((0 > len) || ((0 == len) && self->full)) {
			iovec[0].iov_base = self->buffer + self->rp;
			iovec[0].iov_len = self->len - self->rp;
			iovec[1].iov_base = self->buffer;
			iovec[1].iov_len = self->wp;
			return 2;
		} else if (0 < len) {
			iovec[0].iov_base = self->buffer + self->rp;
			iovec[0].iov_len = len;
			return 1;
		}
	}

	return 0;
}

void
hev_ring_buffer_read_finish (HevRingBuffer *self, size_t inc_len)
{
	if (self) {
		if (0 < inc_len) {
			size_t p = inc_len + self->rp;
			if (self->len < p)
			  self->rp = p - self->len;
			else
			  self->rp = p;
		}
		if (self->wp == self->rp)
		  self->full = false;
	}
}

size_t
hev_ring_buffer_writing (HevRingBuffer *self, struct iovec *iovec)
{
	if (self) {
		ssize_t len = self->rp - self->wp;

		if ((0 > len) || ((0 == len) && !self->full)) {
			iovec[0].iov_base = self->buffer + self->wp;
			iovec[0].iov_len = self->len - self->wp;
			iovec[1].iov_base = self->buffer;
			iovec[1].iov_len = self->rp;
			return 2;
		} else if (0 < len) {
			iovec[0].iov_base = self->buffer + self->wp;
			iovec[0].iov_len = len;
			return 1;
		}
	}

	return 0;
}

void
hev_ring_buffer_write_finish (HevRingBuffer *self, size_t inc_len)
{
	if (self && (0 < inc_len)) {
		size_t p = inc_len + self->wp;
		if (self->len < p)
		  self->wp = p - self->len;
		else
		  self->wp = p;
		if (self->wp == self->rp)
		  self->full = true;
	}
}

