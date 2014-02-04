//  This file is part of par2cmdline (a PAR 2.0 compatible file verification and
//  repair tool). See http://parchive.sourceforge.net for details of PAR 2.0.
//
//  Copyright (c) 2008 Vincent Tan, created 2008-09-20. buffer.cpp
//
//  par2cmdline is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  par2cmdline is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//  Modifications for concurrent processing, async I/O, Unicode support, and
//  hierarchial directory support are Copyright (c) 2007-2008 Vincent Tan.
//  Search for "#if WANT_CONCURRENT" for concurrent code.
//  Concurrent processing utilises Intel Thread Building Blocks 2.0,
//  Copyright (c) 2007 Intel Corp.

#include "par2cmdline.h"

// implements refcounted buffer

buffer::buffer(void) : buffer_(NULL)
#if GPGPU_CUDA
  , buffer_allocated_by_gpu_(false)
#endif
{
}

buffer::~buffer(void) {
  if (buffer_) {
#if GPGPU_CUDA
    if (buffer_allocated_by_gpu_)
      cuda::DeallocateHost(buffer_);
    else
#endif
#if WANT_CONCURRENT
      tbb::cache_aligned_allocator<u8>().deallocate(buffer_, 0);
#else
      free(buffer_);
#endif
  }
}

bool
buffer::alloc(size_t sz) {
  assert(NULL == buffer_);
#if GPGPU_CUDA
  buffer_ = (u8*) cuda::AllocateHost(sz);
  if (buffer_)
    buffer_allocated_by_gpu_ = true;
  else
#endif
#if WANT_CONCURRENT
    buffer_ = tbb::cache_aligned_allocator<u8>().allocate(sz);//new u8[sz];
#else
    buffer_ = (u8*) malloc(sz);
#endif
  return NULL != buffer_;
}

#if WANT_CONCURRENT

rcbuffer::rcbuffer(void)
{
  refcount_ = 0;
}

bool
rcbuffer::try_to_acquire(void) {
  if (1 == ++refcount_) {
  #if !defined(NDEBUG) && defined(DEBUG_BUFFERS)
printf("%u try_to_acquire: rc = 1 SUCCESS\n", id_);
  #endif
//printf("%p try_to_acquire: rc = 1 SUCCESS\n", this);
    return true;
  }

  #if !defined(NDEBUG) && defined(DEBUG_BUFFERS)
  int rc = --refcount_;
printf("%u try_to_acquire: rc = %d FAILED\n", id_, rc);
  #else
  --refcount_;
  #endif
  return false;
}

void
rcbuffer::add_ref(void) {
  #if !defined(NDEBUG) && defined(DEBUG_BUFFERS)
  int rc = ++refcount_;
printf("%u add_ref: rc = %d\n", id_, rc);
  #else
//int rc = ++refcount_;
//printf("%p add_ref: rc = %d\n", this, rc);
  ++refcount_;
  #endif
}

int
rcbuffer::release(void) {
  assert(refcount_ > 0);
  #if !defined(NDEBUG) && defined(DEBUG_BUFFERS)
  int rc = --refcount_;
printf("%u release: rc = %d\n", id_, rc);
  return rc;
  #else
//int rc = --refcount_;
//printf("%p release: rc = %d\n", this, rc);
//return rc;
  return --refcount_;
  #endif
}

#endif // WANT_CONCURRENT
