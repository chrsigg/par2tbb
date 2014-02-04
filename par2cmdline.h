//  This file is part of par2cmdline (a PAR 2.0 compatible file verification and
//  repair tool). See http://parchive.sourceforge.net for details of PAR 2.0.
//
//  Copyright (c) 2003 Peter Brian Clements
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

#ifndef __PARCMDLINE_H__
#define __PARCMDLINE_H__


#ifdef WIN32
// Windows includes
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// System includes
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <io.h>
#include <fcntl.h>
#include <assert.h>

#define snprintf _snprintf

#define __LITTLE_ENDIAN 1234
#define __BIG_ENDIAN    4321
#define __PDP_ENDIAN    3412

#define __BYTE_ORDER __LITTLE_ENDIAN

typedef unsigned char    u8;
typedef unsigned short   u16;
typedef unsigned long    u32;
typedef unsigned __int64 u64;

#ifndef _SIZE_T_DEFINED
#  ifdef _WIN64
typedef unsigned __int64 size_t;
#  else
typedef unsigned int     size_t;
#  endif
#  define _SIZE_T_DEFINED
#endif

#ifndef ULONG_MAX
  enum { ULONG_MAX = 0xffffffffUL /* maximum unsigned long value */ };
#endif

  #define HAVE_ASYNC_IO 1

  struct aiocb_type : OVERLAPPED {
  public:
    typedef unsigned __int64 off_t;
  private:
    HANDLE hFile_;

    bool rw(HANDLE hFile, size_t sz, const void* buf, off_t off, bool want_write) {
      if (sz > ULONG_MAX)
        return false; // Win64 boundary case

      LARGE_INTEGER li;
      li.QuadPart = off;
      this->Offset = li.LowPart;        /* File offset */
      this->OffsetHigh = li.HighPart;        /* File offset */
      BOOL b = want_write ? ::WriteFile(hFile, buf, sz, NULL, this) :
                            ::ReadFile(hFile, const_cast<void*> (buf), sz, NULL, this);
      if (!b)
        b = ERROR_IO_PENDING == ::GetLastError(); // request has already completed or is pending
      if (b)
        hFile_ = hFile;
      return FALSE != b;
    }

  public:
    aiocb_type(void) : hFile_(INVALID_HANDLE_VALUE) {
      memset(this, 0, sizeof(OVERLAPPED));
      hEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
    }

    aiocb_type(const aiocb_type&) : hFile_(INVALID_HANDLE_VALUE) {
      memset(this, 0, sizeof(OVERLAPPED));
      hEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
    }

    aiocb_type& operator=(const aiocb_type&) { return *this; }

    ~aiocb_type(void) {
      if (NULL != hEvent) ::CloseHandle(hEvent);
    }

    bool read(HANDLE hFile, size_t sz, void* buf, off_t off) {
      return rw(hFile, sz, buf, off, false);
    }

    bool write(HANDLE hFile, size_t sz, const void* buf, off_t off) {
      return rw(hFile, sz, buf, off, true);
    }

    void suspend_until_completed(void) const {
      ::WaitForSingleObject(hEvent, INFINITE);
    }

    bool has_completed(void) const {
      return WAIT_OBJECT_0 == ::WaitForSingleObject(hEvent, 0);
    }

    bool completedOK(void) const {
      assert(has_completed());
      DWORD nbTransferred;
      return FALSE != ::GetOverlappedResult(hFile_, const_cast<aiocb_type*> (this), &nbTransferred, FALSE);
    }
  };

#else // WIN32
#ifdef HAVE_CONFIG_H

#include <config.h>

#ifdef HAVE_STDLIB_H
#  include <stdlib.h>
#endif

#ifdef HAVE_STDIO_H
#  include <stdio.h>
#endif

#if HAVE_DIRENT_H
#  include <dirent.h>
#  define NAMELEN(dirent) strlen((dirent)->d_name)
#else
#  define dirent direct
#  define NAMELEN(dirent) (dirent)->d_namelen
#  if HAVE_SYS_NDIR_H
#    include <sys/ndir.h>
#  endif
#  if HAVE_SYS_DIR_H
#    include <sys/dir.h>
#  endif
#  if HAVE_NDIR_H
#    include <ndir.h>
#  endif
#endif

#if STDC_HEADERS
#  include <string.h>
#else
#  if !HAVE_STRCHR
#    define strchr index
#    define strrchr rindex
#  endif
char *strchr(), *strrchr();
#  if !HAVE_MEMCPY
#    define memcpy(d, s, n) bcopy((s), (d), (n))
#    define memove(d, s, n) bcopy((s), (d), (n))
#  endif
#endif

#if HAVE_MEMORY_H
#  include <memory.h>
#endif

#if !HAVE_STRICMP
#  if HAVE_STRCASECMP
#    define stricmp strcasecmp
#  endif
#endif

#if HAVE_INTTYPES_H
#  include <inttypes.h>
#endif

#if HAVE_STDINT_H
#  include <stdint.h>
typedef uint8_t            u8;
typedef uint16_t           u16;
typedef uint32_t           u32;
typedef uint64_t           u64;
#else
typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;
#endif

#if HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif

#if HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif

#if HAVE_UNISTD_H
#  include <unistd.h>
#endif

#define _MAX_PATH 255

#if HAVE_ENDIAN_H
#  include <endian.h>
#  ifndef __LITTLE_ENDIAN
#    ifdef _LITTLE_ENDIAN
#      define __LITTLE_ENDIAN _LITTLE_ENDIAN
#      define __LITTLE_ENDIAN _LITTLE_ENDIAN
#      define __BIG_ENDIAN _BIG_ENDIAN
#      define __PDP_ENDIAN _PDP_ENDIAN
#    else
#      error <endian.h> does not define __LITTLE_ENDIAN etc.
#    endif
#  endif
#else
#  define __LITTLE_ENDIAN 1234
#  define __BIG_ENDIAN    4321
#  define __PDP_ENDIAN    3412
#  if WORDS_BIGENDIAN
#    define __BYTE_ORDER __BIG_ENDIAN
#  else
#    define __BYTE_ORDER __LITTLE_ENDIAN
#  endif
#endif

// Using async I/O on FreeBSD causes a crash. Cause unknown.
#if HAVE_AIO_H && HAVE_ERRNO_H && !defined(PLATFORM_FREEBSD)
#  include <errno.h>
#  include <aio.h>
#  include <assert.h>

  #define HAVE_ASYNC_IO 1

  #ifndef NDEBUG
    extern bool want_printf(int res);
  #endif

  struct aiocb_type : aiocb {
//public:
//off_t off_;
//size_t len_;

  private:
    bool rw(int fildes, size_t sz, const void* buf, off_t off, bool want_write) {
      memset(this, 0, sizeof(aiocb));
      this->aio_fildes = fildes;        /* File descriptor */
      this->aio_offset = off;        /* File offset */
      this->aio_buf = static_cast<volatile void*> (const_cast<void*> (buf));        /* Location of buffer */
      this->aio_nbytes = sz;        /* Length of transfer */
//off_ = off;
//len_ = sz;
  #ifndef NDEBUG
      int res = want_write ? ::aio_write(this) : ::aio_read(this);
      if (-1 == res)
        res = errno;
      if (want_printf(res)) printf("aio_rw(%p) -> %d (%s)\n", this, res, strerror(res));
      return 0 == res;
  #else
      return 0 == (want_write ? ::aio_write(this) : ::aio_read(this));
  #endif
    }

  public:
    aiocb_type(void) {}

    bool read(int fildes, size_t sz, const void* buf, off_t off) {
      return rw(fildes, sz, buf, off, false);
    }

    bool write(int fildes, size_t sz, const void* buf, off_t off) {
      return rw(fildes, sz, buf, off, true);
    }

    void suspend_until_completed(void) const {
#ifndef NDEBUG
      int res = ::aio_error(this);
      if (EINPROGRESS != res) {
if (res) printf("suspend_until_completed: aio_error() => %d, errno = %d\n", res, errno);
        return;
      }
#endif
      const struct aiocb* l[1] = {this};
#ifndef NDEBUG
      res = ::aio_suspend(l, 1, NULL);
if (EINTR != res && 0 != res) printf("res = %d, errno = %d\n", res, errno);
      assert(EINTR == res || 0 == res);
//    if (!(EINTR == res || 0 == res)) printf("aio_suspend(%p) -> %d (%s)\n", this, res, strerror(res));
#else
      ::aio_suspend(l, 1, NULL);
#endif
    }

    bool has_completed(void) const {
      int res = ::aio_error(this);
      assert(EINVAL != res);
      return EINPROGRESS != res;
    }

    bool completedOK(void) const {
      int res = ::aio_error(this);
      assert(EINVAL != res && EINPROGRESS != res);
      return EINPROGRESS != res ? -1 != ::aio_return(const_cast<aiocb_type*> (this)) : res;
    }
  };

#endif

#else // HAVE_CONFIG_H

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <assert.h>

#include <errno.h>

#define _MAX_PATH 255
#define stricmp strcasecmp
#define _stat stat

typedef   unsigned char        u8;
typedef   unsigned short       u16;
typedef   unsigned int         u32;
typedef   unsigned long long   u64;

#endif
#endif

#ifdef WIN32
  #define PATHSEP "\\"
  #define ALTPATHSEP "/"

  #ifdef UNICODE
    #define stat _wstati64 /* _stat64 */ /* 'i64' so that files >= 4GB can be processed, was: #define stat _stat */
    #define struct_stat struct _stati64 /* _stati64 */ /* _stat64 */ /* 'i64' so that files >= 4GB can be processed, was: #define stat _stat */
    // should probably rewrite these as inline functions instead of macros - but watch out for the '.c_str()' usage on tmp strings:
    #define utf8_string_to_native_char_array(x) ::UTF8_to_UTF16(x).c_str()
    #define utf8_char_array_to_native_char_array(x) ::UTF8_to_UTF16(x, strlen(x)).c_str()
    #define utf8_string_to_cout_parameter(x) ::UTF8_string_to_cout_string(x)
    #define native_char_array_to_utf8_string(x) ::UTF16_to_UTF8(x)
    #define native_char_array_to_utf8_char_array(x) ::UTF16_to_UTF8(x).c_str()
  #else
    #define stat _stati64 /* _stat64 */ /* 'i64' so that files >= 4GB can be processed, was: #define stat _stat */
    #define struct_stat struct _stati64 /* _stati64 */ /* _stat64 */ /* 'i64' so that files >= 4GB can be processed, was: #define stat _stat */
    // should probably rewrite these as inline functions instead of macros - but watch out for the '.c_str()' usage on tmp strings:
    #define utf8_string_to_native_char_array(x) x.c_str()
    #define utf8_char_array_to_native_char_array(x) x
    #define utf8_string_to_cout_parameter(x) x
    #define native_char_array_to_utf8_string(x) string(x)
    #define native_char_array_to_utf8_char_array(x) x
  #endif
#else
  #define PATHSEP "/"
  #define ALTPATHSEP "\\"

  #define struct_stat struct stat
  // should probably rewrite these as inline functions instead of macros - but watch out for the '.c_str()' usage on tmp strings:
  #define utf8_string_to_native_char_array(x) x.c_str()
  #define utf8_char_array_to_native_char_array(x) x
  #define utf8_string_to_cout_parameter(x) x
  #define native_char_array_to_utf8_string(x) string(x)
  #define native_char_array_to_utf8_char_array(x) x

  typedef char TCHAR;
  #define _tcschr strchr
#endif

// Return type of par2cmdline
typedef enum Result
{
  eSuccess                     = 0,

  eRepairPossible              = 1,  // Data files are damaged and there is
                                     // enough recovery data available to
                                     // repair them.

  eRepairNotPossible           = 2,  // Data files are damaged and there is
                                     // insufficient recovery data available
                                     // to be able to repair them.

  eInvalidCommandLineArguments = 3,  // There was something wrong with the
                                     // command line arguments

  eInsufficientCriticalData    = 4,  // The PAR2 files did not contain sufficient
                                     // information about the data files to be able
                                     // to verify them.

  eRepairFailed                = 5,  // Repair completed but the data files
                                     // still appear to be damaged.


  eFileIOError                 = 6,  // An error occured when accessing files
  eLogicError                  = 7,  // In internal error occurred
  eMemoryError                 = 8,  // Out of memory

} Result;

#define LONGMULTIPLY

// STL includes
#include <string>
#include <list>
#include <vector>
#include <map>
#include <algorithm>

#include <ctype.h>
#include <iostream>
#include <iomanip>

#include <cassert>

using namespace std;

#ifdef WIN32
  extern wstring UTF8_to_UTF16(const char* utf8_str, size_t utf8_length);
  extern wstring UTF8_to_UTF16(const string& utf8);
  extern string  UTF8_string_to_cout_string(const string& utf8);
  extern string  UTF16_to_UTF8(const wchar_t* utf16);
#endif

#ifdef offsetof
#undef offsetof
#endif
#define offsetof(TYPE, MEMBER) ((size_t) ((char*)(&((TYPE *)1)->MEMBER) - (char*)1))

#define WANT_CONCURRENT                     1
#define WANT_CONCURRENT_PAR2_FILE_OPENING   1
#define WANT_CONCURRENT_SOURCE_VERIFICATION 1

#if WANT_CONCURRENT
  #include "tbb/task_scheduler_init.h"
  #include "tbb/atomic.h"
  #include "tbb/concurrent_hash_map.h"
  #include "tbb/concurrent_vector.h"
  #include "tbb/tick_count.h"
  #include "tbb/blocked_range.h"
  #include "tbb/parallel_for.h"
  #include "tbb/mutex.h"
  #include "tbb/pipeline.h"

  class CTimeInterval {
  public:
    CTimeInterval(const std::string& label) :
      _label(label), _start(tbb::tick_count::now()), _done(false) {}
    ~CTimeInterval(void) {  emit();  }
    void  suppress_emission(void) { _done = true; }
    void  emit(void) {
      if (!_done) {
        _done  =  true;
        tbb::tick_count  end  =  tbb::tick_count::now();
        cout << _label << " took " << (end-_start).seconds() << " seconds." << endl;
      }
    }
  private:
    std::string     _label;
    tbb::tick_count _start;
    bool            _done;
  };

  template <typename T>
  struct intptr_hasher {
    static  size_t  hash(T i) { return static_cast<size_t> (31 + (size_t) i * 17); }
    static  bool  equal( T x, T y ) { return x == y; }
  };

  typedef intptr_hasher<u32>    u32_hasher;
//typedef intptr_hasher<size_t> size_t_hasher;

  #if HAVE_ASYNC_IO
    #define CONCURRENT_PIPELINE 1
  #endif

  enum { ALL_SERIAL, CHECKSUM_SERIALLY_BUT_PROCESS_CONCURRENTLY, ALL_CONCURRENT };
#endif

#include "letype.h"
// par2cmdline includes

#include "galois.h"
#include "crc.h"
#include "md5.h"
#include "par2fileformat.h"
#include "commandline.h"
#include "reedsolomon.h"

#include "diskfile.h"
#include "datablock.h"

#include "criticalpacket.h"
#include "par2creatorsourcefile.h"

#include "mainpacket.h"
#include "creatorpacket.h"
#include "descriptionpacket.h"
#include "verificationpacket.h"
#include "recoverypacket.h"

#include "par2repairersourcefile.h"

#include "filechecksummer.h"
#include "verificationhashtable.h"

#include "pipeline.h"

#include "par2creator.h"
#include "par2repairer.h"

#include "par1fileformat.h"
#include "par1repairersourcefile.h"
#include "par1repairer.h"

// Heap checking 
#ifdef _MSC_VER
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#define DEBUG_NEW new(_NORMAL_BLOCK, THIS_FILE, __LINE__)
#endif

#endif // __PARCMDLINE_H__

