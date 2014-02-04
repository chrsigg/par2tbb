//  This file is part of par2cmdline (a PAR 2.0 compatible file verification and
//  repair tool). See http://parchive.sourceforge.net for details of PAR 2.0.
//
//  GPGPU support using nVidia CUDA technology. Copyright (c) 2008 Vincent Tan.
//  Created 2008-09-20. par2_kernel.cu
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

#ifndef _PAR2_KERNEL_H_
  #define _PAR2_KERNEL_H_

  #include <stdio.h>

  #if 1

  __device__
  unsigned
  uc_to_ui(unsigned char i) {
    return i;
  }

  __device__
  unsigned short
  ui_to_us(unsigned i) {
    return i;
  }

  #define TEXREF(N) texRef ## N
  #define TEXFETCH(N, IDX) tex1Dfetch(TEXREF(N), IDX)
  #define LH(N, i) ((unsigned) (TEXFETCH(N, i & 0xFF) ^ TEXFETCH(N, 256U + uc_to_ui(i >> 8))))
  #define LHLH(N, i) LH(N, i) ^ (LH(N, ui_to_us(i >> 16)) << 16)
  #define RSKERNEL(N) \
__global__ \
void \
rsKernel ## N(unsigned* g_odata, const unsigned* g_idata) { \
  const unsigned int tid = threadIdx.x + blockIdx.x * blockDim.x; /* input/output index */ \
  const unsigned data = g_idata[tid]; \
  (unsigned int) atomicXor(g_odata + tid, LHLH(N, data)); \
}

texture<unsigned short, 1, cudaReadModeElementType> texRef0;
texture<unsigned short, 1, cudaReadModeElementType> texRef1;
texture<unsigned short, 1, cudaReadModeElementType> texRef2;
texture<unsigned short, 1, cudaReadModeElementType> texRef3;

RSKERNEL(0)
RSKERNEL(1)
RSKERNEL(2)
RSKERNEL(3)

  #else

texture<unsigned short, 1, cudaReadModeElementType> texRef0;

__device__
unsigned
lh(unsigned short i) {
  return (unsigned) (tex1Dfetch(texRef0, i & 0xFF) ^ tex1Dfetch(texRef0, 256U + ((unsigned) i >> 8)));
}

__device__
unsigned
lhlh(unsigned i) {
  return lh(i) ^ (lh(i >> 16) << 16);
}

__global__
void
rsKernel0(unsigned* g_odata, const unsigned* g_idata) {
  const unsigned int tid = threadIdx.x + blockIdx.x * blockDim.x; // input/output index
  (unsigned int) atomicXor(g_odata + tid, lhlh(g_idata[tid]));
}

  #endif

#endif // #ifndef _PAR2_KERNEL_H_
