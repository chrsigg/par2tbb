//  This file is part of par2cmdline (a PAR 2.0 compatible file verification and
//  repair tool). See http://parchive.sourceforge.net for details of PAR 2.0.
//
//  Copyright (c) 2008 Vincent Tan, created 2008-09-20. buffer.h
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

#undef DEBUG_BUFFERS

// implements a refcounted buffer

  #if GPGPU_CUDA
    #include "cuda.h"
  #endif

  class buffer {
  private:
    u8* buffer_;

  #if GPGPU_CUDA
    bool buffer_allocated_by_gpu_;
  #endif

  #if !defined(NDEBUG) && defined(DEBUG_BUFFERS)
  public:
    unsigned id_;
  private:
  #endif

    //buffer(buffer&);
    //buffer& operator=(const buffer&);

  public:
    buffer(void);
    ~buffer(void);
    bool alloc(size_t sz);

    const u8* get(void) const { return buffer_; }
    u8* get(void) { return buffer_; }
  }; // buffer

  #if WANT_CONCURRENT
  class rcbuffer : public buffer {
  private:
    tbb::atomic<int> refcount_;

    friend class pipeline_state_base;
    bool try_to_acquire(void);

  protected:
    rcbuffer(void);

  public:
    void add_ref(void);
    int release(void);
  }; // rcbuffer
  #endif
