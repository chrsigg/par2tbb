//  This file is part of par2cmdline (a PAR 2.0 compatible file verification and
//  repair tool). See http://parchive.sourceforge.net for details of PAR 2.0.
//
//  GPGPU support using nVidia CUDA technology. Copyright (c) 2008 Vincent Tan.
//  Created 2008-09-20. cuda.h
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

#ifndef __CUDA_H__
#define __CUDA_H__

class rcbuffer;

namespace cuda {

  bool Begin(void);
  void End(void);

  unsigned AllocateResources(u32 blockcount, size_t blocksize);
  void DeallocateResources(void);

  bool Process(size_t unsigned_int_count, rcbuffer& inputbuffer, const unsigned* lhTable,
               u32 outputindex);
  // how many times the GPU was invoked to process a block of data:
  unsigned GetProcessingCount(void);
  u32 GetDeviceOutputBufferCount(void);
  bool CopyDeviceOutputBuffer(u32 outputindex, u32* outputbuffer);

  // pagelocked memory
  void* AllocateHost(size_t blocksize);
  void DeallocateHost(void* hostptr);

  void Xor(u32* dst, const u32* src, size_t sz);
} // namespace cuda

#endif // __CUDA_H__
