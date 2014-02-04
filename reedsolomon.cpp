//  This file is part of par2cmdline (a PAR 2.0 compatible file verification and
//  repair tool). See http://parchive.sourceforge.net for details of PAR 2.0.
//
//  Copyright (c) 2003 Peter Brian Clements
//
//  Modifications for better scalar code generation using the Visual C++ compiler
//  are Copyright (c) 2007-2008 Vincent Tan.
//
//  MMX functions are based on code by Paul Houle (paulhoule.com) March 22, 2008,
//  and are Copyright (c) 2008 Paul Houle and Vincent Tan.
//
//  Modifications for GPGPU support using nVidia CUDA technology are
//  Copyright (c) 2008 Vincent Tan.
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

#include "par2cmdline.h"

#ifdef _MSC_VER
#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif
#endif

u32 gcd(u32 a, u32 b)
{
  if (a && b)
  {
    while (a && b)
    {
      if (a>b)
      {
        a = a%b;
      }
      else
      {
        b = b%a;
      }
    }

    return a+b;
  }
  else
  {
    return 0;
  }
}

template <> bool ReedSolomon<Galois8>::SetInput(const vector<bool> &present)
{
  inputcount = (u32)present.size();

  datapresentindex = new u32[inputcount];
  datamissingindex = new u32[inputcount];
  database         = new G::ValueType[inputcount];

  G::ValueType base = 1;

  for (unsigned int index=0; index<inputcount; index++)
  {
    // Record the index of the file in the datapresentindex array 
    // or the datamissingindex array
    if (present[index])
    {
      datapresentindex[datapresent++] = index;
    }
    else
    {
      datamissingindex[datamissing++] = index;
    }

    database[index] = base++;
  }

  return true;
}

template <> bool ReedSolomon<Galois8>::SetInput(u32 count)
{
  inputcount = count;

  datapresentindex = new u32[inputcount];
  datamissingindex = new u32[inputcount];
  database         = new G::ValueType[inputcount];

  G::ValueType base = 1;

  for (unsigned int index=0; index<count; index++)
  {
    // Record that the file is present
    datapresentindex[datapresent++] = index;

    database[index] = base++;
  }

  return true;
}

template <> bool ReedSolomon<Galois8>::InternalProcess(
  const Galois8 &factor, size_t size, buffer& ib, u32 outputindex, void *outputbuffer
  )
{
  const void *inputbuffer = ib.get();

#ifdef LONGMULTIPLY
  // The 8-bit long multiplication tables
  Galois8 *table = glmt->tables;

  // Split the factor into Low and High bytes
  unsigned int fl = (factor >> 0) & 0xff;

  // Get the four separate multiplication tables
  Galois8 *LL = &table[(0*256 + fl) * 256 + 0]; // factor.low  * source.low

  // Combine the four multiplication tables into two
  unsigned int L[256];

  unsigned int *pL = &L[0];

  for (unsigned int i=0; i<256; i++)
  {
    *pL = *LL;

    pL++;
    LL++;
  }

  // Treat the buffers as arrays of 32-bit unsigned ints.
  u32 *src4 = (u32 *)inputbuffer;
  u32 *end4 = (u32 *)&((u8*)inputbuffer)[size & ~3];
  u32 *dst4 = (u32 *)outputbuffer;

  // Process the data
  while (src4 < end4)
  {
    u32 s = *src4++;

    // Use the two lookup tables computed earlier
    *dst4++ ^= (L[(s >> 0) & 0xff]      )
            ^  (L[(s >> 8) & 0xff] << 8 )
            ^  (L[(s >> 16)& 0xff] << 16)
            ^  (L[(s >> 24)& 0xff] << 24);
  }

  // Process any left over bytes at the end of the buffer
  if (size & 3)
  {
    u8 *src1 = &((u8*)inputbuffer)[size & ~3];
    u8 *end1 = &((u8*)inputbuffer)[size];
    u8 *dst1 = &((u8*)outputbuffer)[size & ~3];

    // Process the data
    while (src1 < end1)
    {
      u8 s = *src1++;
      *dst1++ ^= L[s];
    }
  }
#else
  // Treat the buffers as arrays of 16-bit Galois values.

  Galois8 *src = (Galois8 *)inputbuffer;
  Galois8 *end = (Galois8 *)&((u8*)inputbuffer)[size];
  Galois8 *dst = (Galois8 *)outputbuffer;

  // Process the data
  while (src < end)
  {
    *dst++ += *src++ * factor;
  }
#endif

  return eSuccess;
}



////////////////////////////////////////////////////////////////////////////////////////////



// Set which of the source files are present and which are missing
// and compute the base values to use for the vandermonde matrix.
template <> bool ReedSolomon<Galois16>::SetInput(const vector<bool> &present)
{
  inputcount = (u32)present.size();

  datapresentindex = new u32[inputcount];
  datamissingindex = new u32[inputcount];
  database         = new G::ValueType[inputcount];

  unsigned int logbase = 0;

  for (unsigned int index=0; index<inputcount; index++)
  {
    // Record the index of the file in the datapresentindex array 
    // or the datamissingindex array
    if (present[index])
    {
      datapresentindex[datapresent++] = index;
    }
    else
    {
      datamissingindex[datamissing++] = index;
    }

    // Determine the next useable base value.
    // Its log must must be relatively prime to 65535
    while (gcd(G::Limit, logbase) != 1)
    {
      logbase++;
    }
    if (logbase >= G::Limit)
    {
      cerr << "Too many input blocks for Reed Solomon matrix." << endl;
      return false;
    }
    G::ValueType base = G(logbase++).ALog();

    database[index] = base;
  }

  return true;
}

// Record that the specified number of source files are all present
// and compute the base values to use for the vandermonde matrix.
template <> bool ReedSolomon<Galois16>::SetInput(u32 count)
{
  inputcount = count;

  datapresentindex = new u32[inputcount];
  datamissingindex = new u32[inputcount];
  database         = new G::ValueType[inputcount];

  unsigned int logbase = 0;

  for (unsigned int index=0; index<count; index++)
  {
    // Record that the file is present
    datapresentindex[datapresent++] = index;

    // Determine the next useable base value.
    // Its log must must be relatively prime to 65535
    while (gcd(G::Limit, logbase) != 1)
    {
      logbase++;
    }
    if (logbase >= G::Limit)
    {
      cerr << "Too many input blocks for Reed Solomon matrix." << endl;
      return false;
    }
    G::ValueType base = G(logbase++).ALog();

    database[index] = base;
  }

  return true;
}


#ifdef LONGMULTIPLY

  #if __GNUC__ && (__i386__ || __x86_64__)
    #include <sys/types.h>
    #include <sys/sysctl.h>

    #if __x86_64__
      extern "C" void rs_process_x86_64_scalar(void* dst, const void* src, size_t size, const u32* LH);
    #else // __i386__
      extern "C" void rs_process_i386_scalar(void* dst, const void* src, size_t size, const u32* LH);
    #endif
  #endif

  namespace DetectVectorUnit {
    namespace internal {
      static bool HasVectorUnit(void);
    }

    static const bool hasVectorUnit = internal::HasVectorUnit();
  }

  #if __GNUC__ && (__i386__ || __x86_64__)
    #include <sys/types.h>
    #include <sys/sysctl.h>

    #if __x86_64__
  extern "C" void rs_process_x86_64_mmx(void* dst, const void* src, size_t size, const u32* LH);
//extern "C" void rs_process_x86_64_sse2(void* dst, const void* src, size_t size, const u32* LH);
    #else // __i386__
  extern "C" void rs_process_i686_mmx(void* dst, const void* src, size_t size, const u32* LH);
//extern "C" void rs_process_i686_sse2(void* dst, const void* src, size_t size, const u32* LH);
    #endif

/* GCC produces reasonably good code but it is not as good as the hand-written assembly because
   it produces too much register-to-stack-frame-and-back-again traffic, so it's disabled and
   the .s files are used instead. FWIW, VC++ definitely produces better code.

  #define rs_process_i686_mmx rs_process_simd

      typedef int v1di __attribute__ ((vector_size (8)));
      typedef v1di             mm_reg_type;
      #define mm_xor           __builtin_ia32_pxor
      #define mm_load32(x)     (mm_reg_type) __builtin_ia32_vec_init_v2si(x, 0)
      #define mm_store32(x)    __builtin_ia32_vec_ext_v2si(x, 0)
      #define mm_load64(src)   (*src)
      #define mm_store64(dst, src) (*dst) = (src)
      #define mm_unpacklo      __builtin_ia32_punpckldq
      #define mm_sr            __builtin_ia32_psrldi
      #define mm_empty         __builtin_ia32_emms()

  static void rs_process_simd(void *outputbuffer, const void *inputbuffer, size_t bsize, const unsigned *LH) {
    (u8*&) outputbuffer += bsize;
    (const u8*&) inputbuffer += bsize;
    for (bsize = -bsize; bsize; bsize += sizeof(u64)) {
      mm_reg_type s = mm_load64((const mm_reg_type*) (bsize + (const u8*) inputbuffer));
      u32 tmp = mm_store32(s);
      u16 sw = tmp >> 16;
      mm_reg_type s0 = mm_load32(LH[      u8(      tmp >> 0)]); // L
      mm_reg_type s1 = mm_load32(LH[512 + u8((u16) tmp >> 8)]); // H
      mm_reg_type s2 = mm_load32(LH[256 + u8(sw >> 0)]);        // preshifted L
      mm_reg_type s3 = mm_load32(LH[768 + u8(sw >> 8)]);        // preshifted H

      tmp = mm_store32(mm_sr(s, 32));
      sw = tmp >> 16;

      s = mm_load64((const mm_reg_type*) (bsize + (const u8*) outputbuffer));
      mm_store64((mm_reg_type*) (bsize + (u8*) outputbuffer), mm_xor(s,
        mm_xor(mm_xor(mm_unpacklo(s0, mm_load32(LH[      u8(      tmp >> 0)])),
                mm_unpacklo(s1, mm_load32(LH[512 + u8((u16) tmp >> 8)]))),
        mm_xor(mm_unpacklo(s2, mm_load32(LH[256 + u8(sw >> 0)])),
                mm_unpacklo(s3, mm_load32(LH[768 + u8(sw >> 8)]))))));
    }
    mm_empty;
  } */

  extern "C" int detect_mmx(void);

  namespace DetectVectorUnit {
    // The asm code for x86 and x64 processes in 8-byte chunks.
    enum { sizeof_work_unit = sizeof(u64) };
    //enum { sizeof_work_unit = 64 }; // sizeof 1 L1 cache line

    namespace internal {
      static bool HasVectorUnit(void) {
    #if __APPLE__ || __x86_64__
        // For Darwin/MacOSX, x86 always executes on MMX-capable CPUs. x64 CPUs always have MMX/SSE/SSE2.
        return true;
    #else // other 32-bit x86 POSIX systems:
        return 0 != detect_mmx();
    #endif
      }
    }
  }

  #elif defined(_MSC_VER) // Visual C++ compiler

    #if defined(WIN64)
      #include <emmintrin.h>
      typedef __m128i          mm_reg_type;
      #define mm_xor           _mm_xor_si128
      #define mm_load32        _mm_cvtsi32_si128
      #define mm_store32       _mm_cvtsi128_si32
      #define mm_load64        _mm_loadl_epi64
      #define mm_store64       _mm_storel_epi64
      #define mm_unpacklo      _mm_unpacklo_epi32
      #define mm_sr64          _mm_srli_epi64
      #define mm_sl32          _mm_slli_epi32
      #define mm_empty

  namespace DetectVectorUnit {
    // The asm code for x64 processes in 8-byte chunks.
    enum { sizeof_work_unit = sizeof(u64) };

    namespace internal {
      static bool HasVectorUnit(void) {
        return true;
      }
    }
  }
    #else // WIN32
      #include <mmintrin.h>
      typedef __m64            mm_reg_type;
      #define mm_xor           _mm_xor_si64
      #define mm_load32        _mm_cvtsi32_si64
      #define mm_store32       _mm_cvtsi64_si32
      #define mm_load64(src)   (*src)
      #define mm_store64(dst, src) (*dst) = (src)
      #define mm_unpacklo      _mm_unpacklo_pi32
      #define mm_sr64          _mm_srli_si64
      #define mm_sl32          _mm_slli_pi32
      #define mm_empty         _mm_empty()

  namespace DetectVectorUnit {
    // The asm code for x86 processes in 8-byte chunks.
    enum { sizeof_work_unit = sizeof(u64) };

    namespace internal {
      static bool HasVectorUnit(void) {
        return FALSE != IsProcessorFeaturePresent(PF_MMX_INSTRUCTIONS_AVAILABLE);
      }
    }
  }
    #endif

  // This function is based in part on code by Paul Houle.
  // The original code used inlined assembly, but this version uses the Visual C++ compiler
  // instrinsics so that the same function can be compiled for both x86 (using MMX) and x64
  // (using SSE2), because the x64 C++ compiler does not support inlined assembly. The VC++
  // compiler does a pretty good job of instruction scheduling - not quite as good as the
  // hand-written assembly (IMHO) but good enough. It certainly produces better code than GCC.
  static void rs_process_simd(void *outputbuffer, const void *inputbuffer, size_t bsize, const unsigned *LH) {
    (u8*&) outputbuffer += bsize;
    (const u8*&) inputbuffer += bsize;
    for (bsize = -bsize; bsize; bsize += sizeof(__m64)) {
      mm_reg_type s = mm_load64((const mm_reg_type*) (bsize + (const u8*) inputbuffer));
      u32 tmp = mm_store32(s);
      u16 sw = tmp >> 16;
    #if 1
      mm_reg_type s0 = mm_load32(LH[      u8(      tmp >> 0)]); // L
      mm_reg_type s1 = mm_load32(LH[256 + u8((u16) tmp >> 8)]); // H
      mm_reg_type s2 = mm_load32(LH[      u8(sw >> 0)]);        // L
      mm_reg_type s3 = mm_load32(LH[256 + u8(sw >> 8)]);        // H
    #else
      mm_reg_type s0 = mm_load32(LH[      u8(      tmp >> 0)]); // L
      mm_reg_type s1 = mm_load32(LH[512 + u8((u16) tmp >> 8)]); // H
      mm_reg_type s2 = mm_load32(LH[256 + u8(sw >> 0)]);        // preshifted L
      mm_reg_type s3 = mm_load32(LH[768 + u8(sw >> 8)]);        // preshifted H
    #endif
      s = mm_sr64(s, 32);
      tmp = mm_store32(s);
      sw = tmp >> 16;
    #if 1
      s0 = mm_unpacklo(s0, mm_load32(LH[      u8(      tmp >> 0)]));
      s1 = mm_unpacklo(s1, mm_load32(LH[256 + u8((u16) tmp >> 8)]));
      s2 = mm_unpacklo(s2, mm_load32(LH[      u8(sw >> 0)]));   // L
      s3 = mm_unpacklo(s3, mm_load32(LH[256 + u8(sw >> 8)]));   // H
    #else
      s0 = mm_unpacklo(s0, mm_load32(LH[      u8(      tmp >> 0)]));
      s1 = mm_unpacklo(s1, mm_load32(LH[512 + u8((u16) tmp >> 8)]));
      s2 = mm_unpacklo(s2, mm_load32(LH[256 + u8(sw >> 0)]));   // preshifted L
      s3 = mm_unpacklo(s3, mm_load32(LH[768 + u8(sw >> 8)]));   // preshifted H
    #endif

      s = mm_load64((const mm_reg_type*) (bsize + (const u8*) outputbuffer));
      s1 = mm_xor(s0, s1);
    #if 1
      s3 = mm_sl32(mm_xor(s2, s3), 16);
    #else
      s3 = mm_xor(s2, s3);
    #endif
      s3 = mm_xor(s1, s3);
      s = mm_xor(s, s3);
      mm_store64((mm_reg_type*) (bsize + (u8*) outputbuffer), s);
    }
    mm_empty;
  }

  #else
  namespace DetectVectorUnit {
    enum { sizeof_work_unit = sizeof(u8) };

    namespace internal {
      static bool HasVectorUnit(void) { return false; }
    }
  }
  #endif

#endif

template <> bool ReedSolomon<Galois16>::InternalProcess(
  const Galois16 &factor, size_t size, buffer& ib, u32 outputindex, void *outputbuffer)
{
  const void *inputbuffer = ib.get();
#ifdef LONGMULTIPLY
  // The 8-bit long multiplication tables
  Galois16 *table = glmt->tables;

  // Split the factor into Low and High bytes
  unsigned int fl = (factor >> 0) & 0xff;
  unsigned int fh = (factor >> 8) & 0xff;

  // Get the four separate multiplication tables
  Galois16 *LL = &table[(0*256 + fl) * 256 + 0]; // factor.low  * source.low
  Galois16 *LH = &table[(1*256 + fl) * 256 + 0]; // factor.low  * source.high
  Galois16 *HL = &table[(1*256 + 0) * 256 + fh]; // factor.high * source.low
  Galois16 *HH = &table[(2*256 + fh) * 256 + 0]; // factor.high * source.high

  // Combine the four multiplication tables into two
  typedef unsigned int LHEntry;
//LHEntry L[512]; // Double the space required but
//LHEntry H[512]; // save ONE shift instruction.
  // mult tables (using an array of ints forces the compiler to align on a 4-byte boundary):
  LHEntry lhTable[256*2 *1];
  LHEntry* L = &lhTable[0];
  LHEntry* H = &lhTable[256];
//LHEntry lhTable[256*2 *2];
//LHEntry* L = &lhTable[0];
//LHEntry* H = &lhTable[512];

  #if __BYTE_ORDER == __LITTLE_ENDIAN
  LHEntry *pL = &L[0];
  LHEntry *pH = &H[0];
  #else
  LHEntry *pL = &H[0];
  LHEntry *pH = &L[0];
  #endif

  for (unsigned int i=0; i<256; i++)
  {
    LHEntry temp;
    {
      temp = *LL + *HL;
  #if __BYTE_ORDER == __LITTLE_ENDIAN
  #else
      temp = (temp >> 8) & 0xff | (temp << 8) & 0xff00;
  #endif

      *pL++ = temp;
      LL++;
      HL+=256;

      //pL[255] = temp << 16;
    }

    {
      temp = *LH + *HH;
  #if __BYTE_ORDER == __LITTLE_ENDIAN
  #else
      temp = (temp >> 8) & 0xff | (temp << 8) & 0xff00;
  #endif

      *pH++ = temp;
      LH++;
      HH++;

      //pH[255] = temp << 16;
    }
  }

  #if WANT_CONCURRENT && CONCURRENT_PIPELINE && GPGPU_CUDA
  if (has_gpu_ && size >= sizeof(u32) && 0 == (size & (sizeof(u32)-1))) {
    const size_t n = (size / sizeof(u32));
    // when called from pipeline_state in par2pipeline.h, ib will always be an instance
    // of pipeline_buffer and hence always an instance of rcbuffer:
    if (cuda::Process(n, static_cast<rcbuffer&> (ib), lhTable, outputindex)) // EXECUTE
      return eSuccess;
  }
  #endif

  if (DetectVectorUnit::hasVectorUnit) {
    enum { sizeof_work_unit = DetectVectorUnit::sizeof_work_unit };
    // asz = alignment size = # of bytes to process using scalar code before vector code can be used
    // vsz = vector size = # of bytes to process using vector code
    size_t asz = (sizeof_work_unit - (uintptr_t) inputbuffer) & (sizeof_work_unit-1); // 0...(sizeof_work_unit-1)
    size_t vsz = (size-asz) & ~(sizeof_work_unit-1);
    if (vsz) {
      if (asz) {
  #if __GNUC__ &&  __x86_64__
        rs_process_x86_64_scalar(outputbuffer, inputbuffer, asz, lhTable);
  #elif __GNUC__ &&  __i386__
        rs_process_i386_scalar(outputbuffer, inputbuffer, asz, lhTable);
  #else
        // Treat the buffers as arrays of 32-bit unsigned ints.
        u32 *src = (u32 *)inputbuffer;
        u32 *end = (u32 *)&((u8*)inputbuffer)[asz];
        u32 *dst = (u32 *)outputbuffer;
  
        // Process the data
        do {
          u32 s = *src++;

          // Use the two lookup tables computed earlier

          // Visual C++ generates better code with this version (mostly because of the casts):
          u16 sw = u16(s >> 16);
          u32 d  = L[u8(sw >> 0)];
              d ^= H[u8(sw >> 8)];
              d <<= 16;
              d ^= *dst ^ (L[u8(       s  >>  0)]      )
                        ^ (H[u8(((u16) s) >>  8)]      );
        //u32 d  = (L+256)[u8(sw >> 0)]; // use pre-shifted entries
        //    d ^= (H+256)[u8(sw >> 8)]; // use pre-shifted entries
        //    d ^= *dst ^ (L[u8(       s  >>  0)]      )
        //              ^ (H[u8(((u16) s) >>  8)]      );

          // the original version (too many shift's and and's):
        //u32 d = *dst ^ (L[(s >> 0) & 0xff]      )
        //             ^ (H[(s >> 8) & 0xff]      )
        //             ^ (L[(s >> 16)& 0xff] << 16)
        //             ^ (H[(s >> 24)& 0xff] << 16);
          *dst++ = d;
        } while (src < end);

  #endif
        (u8*&) outputbuffer += asz;
        (u8*&) inputbuffer  += asz;
        size -= asz;
      } // if (asz)

  #if __GNUC__ &&  __x86_64__
      rs_process_x86_64_mmx(outputbuffer, inputbuffer, vsz, lhTable);
  #elif __GNUC__ &&  __i386__
      rs_process_i686_mmx(outputbuffer, inputbuffer, vsz, lhTable);
    //rs_process_i686_sse2(outputbuffer, inputbuffer, vsz, lhTable);
  #elif defined(WIN64) || defined(WIN32)
      rs_process_simd(outputbuffer, inputbuffer, vsz, lhTable);
  #else
      vsz = 0; // no SIMD unit, so set vsz = 0
  #endif

      (u8*&) outputbuffer += vsz;
      (u8*&) inputbuffer  += vsz;
      size -= vsz;
    } // if (vsz)
  }

  if (size) {
  #if __GNUC__ && __x86_64__
    rs_process_x86_64_scalar(outputbuffer, inputbuffer, size, lhTable);
  #elif __GNUC__ &&  __i386__
    rs_process_i386_scalar(outputbuffer, inputbuffer, size, lhTable);
  #else // only Visual C++ produces decent x86 code for the following:
    // Treat the buffers as arrays of 32-bit unsigned ints.
    u32 *src = (u32 *)inputbuffer;
    u32 *end = (u32 *)&((u8*)inputbuffer)[size];
    u32 *dst = (u32 *)outputbuffer;
  
    // Process the data
    do {
      u32 s = *src++;

      // Use the two lookup tables computed earlier

      // Visual C++ generates better code with this version (mostly because of the casts):
      u16 sw = u16(s >> 16);
      u32 d  = L[u8(sw >> 0)];
          d ^= H[u8(sw >> 8)];
          d <<= 16;
          d ^= *dst ^ (L[u8(       s  >>  0)]      )
                    ^ (H[u8(((u16) s) >>  8)]      );
    /*u32 d  = (L+256)[u8(sw >> 0)]; // use pre-shifted entries
          d ^= (H+256)[u8(sw >> 8)]; // use pre-shifted entries
          d ^= *dst ^ (L[u8(       s  >>  0)]      )
                    ^ (H[u8(((u16) s) >>  8)]      )
                    ; // <- one shift instruction eliminated*/

      // the original version (too many shift's and and's):
    //u32 d = *dst ^ (L[(s >> 0) & 0xff]      )
    //             ^ (H[(s >> 8) & 0xff]      )
    //             ^ (L[(s >> 16)& 0xff] << 16)
    //             ^ (H[(s >> 24)& 0xff] << 16);
      *dst++ = d;
    } while (src < end);

  #endif
  }
#else
  // Treat the buffers as arrays of 16-bit Galois values.

  // BUG: This only works for __LITTLE_ENDIAN
  Galois16 *src = (Galois16 *)inputbuffer;
  Galois16 *end = (Galois16 *)&((u8*)inputbuffer)[size];
  Galois16 *dst = (Galois16 *)outputbuffer;

  // Process the data
  while (src < end)
  {
    *dst++ += *src++ * factor;
  }
#endif

  return eSuccess;
}
