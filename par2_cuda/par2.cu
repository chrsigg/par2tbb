//  This file is part of par2cmdline (a PAR 2.0 compatible file verification and
//  repair tool). See http://parchive.sourceforge.net for details of PAR 2.0.
//
//  GPGPU support using nVidia CUDA technology. Copyright (c) 2008 Vincent Tan.
//  Created 2008-09-20. par2.cu
//
//  par2cmdline-0.4-tbb is available at http://chuchusoft.com/par2_tbb
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

/*
 * Copyright 1993-2007 NVIDIA Corporation.  All rights reserved.
 *
 * NOTICE TO USER:
 *
 * This source code is subject to NVIDIA ownership rights under U.S. and
 * international Copyright laws.  Users and possessors of this source code
 * are hereby granted a nonexclusive, royalty-free license to use this code
 * in individual and commercial software.
 *
 * NVIDIA MAKES NO REPRESENTATION ABOUT THE SUITABILITY OF THIS SOURCE
 * CODE FOR ANY PURPOSE.  IT IS PROVIDED "AS IS" WITHOUT EXPRESS OR
 * IMPLIED WARRANTY OF ANY KIND.  NVIDIA DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOURCE CODE, INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE.
 * IN NO EVENT SHALL NVIDIA BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL,
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS,  WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION,  ARISING OUT OF OR IN CONNECTION WITH THE USE
 * OR PERFORMANCE OF THIS SOURCE CODE.
 *
 * U.S. Government End Users.   This source code is a "commercial item" as
 * that term is defined at  48 C.F.R. 2.101 (OCT 1995), consisting  of
 * "commercial computer  software"  and "commercial computer software
 * documentation" as such terms are  used in 48 C.F.R. 12.212 (SEPT 1995)
 * and is provided to the U.S. Government only as a commercial end item.
 * Consistent with 48 C.F.R.12.212 and 48 C.F.R. 227.7202-1 through
 * 227.7202-4 (JUNE 1995), all U.S. Government End Users acquire the
 * source code with only those rights set forth herein.
 *
 * Any use of this source code in individual and commercial software must
 * include, in the user documentation and internal comments to the code,
 * the above Disclaimer and U.S. Government End Users Notice.
 */

// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <vector>

#define MULTIPLE_STREAMS 1

#include "par2_kernel.cu"

namespace cuda {
namespace internal {
  extern size_t CountDevices(void);
  extern bool SelectDevice(size_t i);

  extern size_t StreamCount(void);

  extern size_t AllocStream(void);
  extern bool DeallocStream(size_t streamidx);
  extern bool SyncStream(size_t streamidx);
  extern bool QueryStream(size_t streamidx);

  extern void* AllocHostMemory(size_t sz);
  extern bool DeallocHostMemory(void* ptr);
  extern void* AllocDeviceMemory(size_t sz); // returns devptr
  extern bool DeallocDeviceMemory(void* devptr);
  extern bool CopyFromHostToDeviceMemory(void* devptr, const void* hostptr, size_t sz, size_t streamidx);
  extern bool CopyFromDeviceToHostMemory(void* hostptr, const void* devptr, size_t sz, size_t streamidx);

  extern bool BindTextureToDeviceMemory(size_t streamidx, void* tex_dev_ptr, size_t tex_size);
  extern bool ProcessViaGPU(size_t streamidx, size_t len, // The number of u32's to process
                            unsigned* dev_obuf, const unsigned* inputbuffer, const unsigned short* host_lh,
                            unsigned* dev_ibuf, unsigned short* dev_lh);

  // up to 4 GPU streams are supported but testing on an 8600M GT showed no improvement above 2:
  enum { NUM_GPU_PROCESSORS = MULTIPLE_STREAMS ? 2 : 1 };

  static cudaStream_t streams_[NUM_GPU_PROCESSORS] = {0};
  static size_t streams_size_ = 0;

size_t
CountDevices(void) {
  int count = 0;
  cudaError_t err = ::cudaGetDeviceCount(&count);
  if (cudaSuccess != err)
    return 0;
  return (size_t) count;
}

bool
SelectDevice(size_t i) {
  cudaDeviceProp deviceProp;
  cudaError_t err = ::cudaGetDeviceProperties(&deviceProp, (int) i);
//printf("cudaGetDeviceProperties -> %d, deviceProp.major = %d\n", err, deviceProp.major);
  if (deviceProp.major < 1)
    return false;
  err = ::cudaSetDevice((int) i);
//if (err) printf("cudaSetDevice(%d) -> %d\n", i, err);
  if (cudaSuccess != err)
    return false;

  err = ::cudaThreadSynchronize();
//if (err) printf("cudaThreadSynchronize -> %d\n", err);
  return true;//cudaSuccess == err;
}

size_t
StreamCount(void) {
  return NUM_GPU_PROCESSORS;
}

size_t
AllocStream(void) {
  if (streams_size_ == NUM_GPU_PROCESSORS)
    return 0; // no room left

#if !defined(WIN32)
  assert(streams_size_ < NUM_GPU_PROCESSORS);
  assert(0 == streams_[streams_size_]);
#endif

  cudaStream_t stream = 0;
  cudaError_t err = ::cudaStreamCreate(&stream);
  bool ok = cudaSuccess == err;
  if (ok) {
    streams_[streams_size_++] = stream;
  }
  return ok ? streams_size_ : 0;
}

bool
DeallocStream(size_t streamidx) {
  cudaError_t err = 0 < streamidx && streamidx <= streams_size_ ? ::cudaStreamDestroy(streams_[streamidx-1]) : cudaErrorInvalidResourceHandle;
  bool res = cudaSuccess == err;
  if (res)
    streams_[streamidx-1] = 0;
  return res;
}

bool
SyncStream(size_t streamidx) {
  cudaError_t err;
  if (streamidx)
    err = 0 < streamidx && streamidx <= streams_size_ ? ::cudaStreamSynchronize(streams_[streamidx-1]) : cudaErrorInvalidResourceHandle;
  else
    err = ::cudaThreadSynchronize();

//if (err) printf("cudaStreamSynchronize(%d) -> %d (%s)\n", streamidx, err, cudaGetErrorString(err));
  return cudaSuccess == err;
}

bool
QueryStream(size_t streamidx) {
  cudaError_t err = 0 < streamidx && streamidx <= streams_size_ ? ::cudaStreamQuery(streams_[streamidx-1]) : cudaErrorInvalidResourceHandle;
//if (!(cudaSuccess == err || cudaErrorNotReady == err)) printf("cudaStreamQuery(%d) -> %d (%s)\n", streamidx, err, cudaGetErrorString(err));
#if !defined(WIN32)
  assert(cudaSuccess == err || cudaErrorNotReady == err);
#endif
  return cudaSuccess == err;
}

void*
AllocHostMemory(size_t sz) {
  void* ptr = NULL;
  cudaError_t err = ::cudaMallocHost(&ptr, sz);
//if (err) printf("cudaMallocHost(%u) -> %d (%s) %p\n", (unsigned) sz, err, cudaGetErrorString(err), ptr);
  return cudaSuccess == err ? ptr : NULL;
}

bool
DeallocHostMemory(void* ptr) {
  cudaError_t err = ::cudaFreeHost(ptr);
//printf("cudaFreeHost(%p) -> %d (%s)\n", ptr, err, cudaGetErrorString(err));
  return cudaSuccess == err;
}

void*
AllocDeviceMemory(size_t sz) {
  void* devptr = NULL;
  cudaError_t err = ::cudaMalloc(&devptr, sz);
//if (err) printf("cudaMalloc(%u) -> %d (%s) %p\n", (unsigned) sz, err, cudaGetErrorString(err), devptr);
  return cudaSuccess == err ? devptr : NULL;
}

bool
DeallocDeviceMemory(void* devptr) {
  cudaError_t err = ::cudaFree(devptr);
//printf("cudaFree(%p) -> %d (%s)\n", devptr, err, cudaGetErrorString(err));
  return cudaSuccess == err;
}

bool
CopyFromHostToDeviceMemory(void* devptr, const void* hostptr, size_t sz, size_t streamidx) {
  cudaError_t err;
  if (streamidx)
    err = 0 < streamidx && streamidx <= streams_size_
          ? ::cudaMemcpyAsync(devptr, hostptr, sz, cudaMemcpyHostToDevice, streams_[streamidx-1])
          : cudaErrorInvalidResourceHandle;
  else
    err = ::cudaMemcpy(devptr, hostptr, sz, cudaMemcpyHostToDevice);

//if (err) printf("cudaMemcpyHostToDevice(%p, %p, %u, %d) -> %d (%s)\n", devptr, hostptr, (unsigned) sz, streamidx, err, cudaGetErrorString(err));
  return cudaSuccess == err;
}

bool
CopyFromDeviceToHostMemory(void* hostptr, const void* devptr, size_t sz, size_t streamidx) {
  cudaError_t err;
  if (streamidx)
    err = 0 < streamidx && streamidx <= streams_size_
          ? ::cudaMemcpyAsync(hostptr, devptr, sz, cudaMemcpyDeviceToHost, streams_[streamidx-1])
          : cudaErrorInvalidResourceHandle;
  else
    err = ::cudaMemcpy(hostptr, devptr, sz, cudaMemcpyDeviceToHost);

//if (err) printf("cudaMemcpyDeviceToHost(%p, %p, %u, %d) -> %d (%s)\n", devptr, hostptr, (unsigned) sz, streamidx, err, cudaGetErrorString(err));
  return cudaSuccess == err;
}

bool BindTextureToDeviceMemory(size_t streamidx, void* tex_dev_ptr, size_t tex_size) {
  // texRef MUST be global and should be declared in the *_kernel.cu file
  cudaError_t err;

  if (1 == streamidx)
    err = ::cudaBindTexture(NULL, texRef0, tex_dev_ptr, tex_size);
#if MULTIPLE_STREAMS
  else if (2 == streamidx)
    err = ::cudaBindTexture(NULL, texRef1, tex_dev_ptr, tex_size);
  else if (3 == streamidx)
    err = ::cudaBindTexture(NULL, texRef2, tex_dev_ptr, tex_size);
  else if (4 == streamidx)
    err = ::cudaBindTexture(NULL, texRef3, tex_dev_ptr, tex_size);
#endif
  else
    err = cudaErrorInvalidResourceHandle;
  return cudaSuccess == err;
}

bool
ProcessViaGPU(size_t streamidx, size_t len, // The number of u32's to process
              unsigned* dev_obuf, const unsigned* inputbuffer, const unsigned short* host_lh,
              unsigned* dev_ibuf, unsigned short* dev_lh) {
//printf("ProcessViaGPU(streamidx=%lu, len=%lx, dev_obuf=%p, inputbuffer=%p, host_lh=%p, dev_ibuf=%p, dev_lh=%p)\n", streamidx, len, dev_obuf, inputbuffer, host_lh, dev_ibuf, dev_lh);

  //assert(0 == (len & 127));
  if (0 == streamidx || streamidx > streams_size_)
    return false;

  const size_t mem_size = sizeof(unsigned) * len;
  //cudaError_t err;

  if (!CopyFromHostToDeviceMemory(dev_ibuf, inputbuffer, mem_size, streamidx))
    return false;

  if (!SyncStream(streamidx))
    return false;

  if (!CopyFromHostToDeviceMemory(dev_lh, host_lh, 512 * sizeof(unsigned short), streamidx))
    return false;

  //  NVIDIA recommend at least 64 threads so let's use 128 to ensure
  //  that the GPU is kept occupied. Then the number of blocks, b, is
  //  determined by the number, n, of uint32's that are to be processed
  //  (since the GPU operates using 32-bit registers as per section
  //  3.1 of the CUDA programming guide):
  //    b = n / 128
  //  For example, if n = 128K then b = 128K / 128 = 1K. So:
  //    dim3  grid( 1024, 1, 1);
  //    dim3  threads( 128 /* num_threads */, 1, 1);
  //    rsKernel<<< grid, threads, 0 /*mem_size*/, streamid >>>( dev_ibuf );
  //
  //  If the number of uint32 values is not evenly divisible by 128 then
  //  process the last block as 128 values but only copy back the remaining
  //  n mod 128 values.

  dim3 grid( unsigned(127+len)/128, 1, 1);
  dim3 threads( 128 /* num_threads */, 1, 1);

  // enqueue request to execute the kernel
  if (1 == streamidx)
    rsKernel0<<< grid, threads, 0 /*mem_size*/, streams_[streamidx-1] >>>( dev_obuf, dev_ibuf );
#if MULTIPLE_STREAMS
  else if (2 == streamidx)
    rsKernel1<<< grid, threads, 0 /*mem_size*/, streams_[streamidx-1] >>>( dev_obuf, dev_ibuf );
  else if (3 == streamidx)
    rsKernel2<<< grid, threads, 0 /*mem_size*/, streams_[streamidx-1] >>>( dev_obuf, dev_ibuf );
  else if (4 == streamidx)
    rsKernel3<<< grid, threads, 0 /*mem_size*/, streams_[streamidx-1] >>>( dev_obuf, dev_ibuf );
#endif
  else
    return false;

  return true;
}

} // namespace internal
} // namespace cuda

