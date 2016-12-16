/*
 ============================================================================
 Name        : hev-memory-allocator-slice.h
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2016 everyone.
 Description : Memory allocator sliced
 ============================================================================
 */

#ifndef __HEV_MEMORY_ALLOCATOR_SLICE__
#define __HEV_MEMORY_ALLOCATOR_SLICE__

#include "hev-memory-allocator.h"

typedef struct _HevMemoryAllocatorSlice HevMemoryAllocatorSlice;

HevMemoryAllocator * hev_memory_allocator_slice_new (void);

#endif /* __HEV_MEMORY_ALLOCATOR_SLICE__ */

