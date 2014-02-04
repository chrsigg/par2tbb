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

#include "par2cmdline.h"

#ifdef _MSC_VER
#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif
#endif

// Open the file associated with the data block if is not already open
bool DataBlock::Open(void)
{
  if (diskfile == 0)
    return false;

  if (diskfile->IsOpen())
    return true;

  return diskfile->Open();
}

// Read some data at a specified position within a data block
// into a buffer in memory

bool DataBlock::ReadData(u64    position, // Position within the block
                         size_t size,     // Size of the memory buffer
                         void  *buffer)   // Pointer to memory buffer
{
  assert(diskfile != 0);

  // Check to see if the position from which data is to be read
  // is within the bounds of the data block
  if (length > position) 
  {
    // Compute the file offset and how much data to physically read from disk
    u64    fileoffset = offset + position;
    size_t want       = (size_t)min((u64)size, length - position);

    // Read the data from the file into the buffer
    if (!diskfile->Read(fileoffset, buffer, want))
      return false;

    // If the read extends beyond the end of the data block,
    // then the rest of the buffer is zeroed.
    if (want < size)
    {
      memset(&((u8*)buffer)[want], 0, size-want);
    }
  }
  else
  {
    // Zero the whole buffer
    memset(buffer, 0, size);
  }

  return true;
}

// Write some data at a specified position within a datablock
// from memory to disk

bool DataBlock::WriteData(u64         position, // Position within the block
                          size_t      size,     // Size of the memory buffer
                          const void *buffer,   // Pointer to memory buffer
                          size_t     &wrote)    // Amount actually written
{
  assert(diskfile != 0);

  wrote = 0;

  // Check to see if the position from which data is to be written
  // is within the bounds of the data block
  if (length > position)
  {
    // Compute the file offset and how much data to physically write to disk
    u64    fileoffset = offset + position;
    size_t have       = (size_t)min((u64)size, length - position);

    // Write the data from the buffer to disk
    if (!diskfile->Write(fileoffset, buffer, have))
      return false;

    wrote = have;
  }

  return true;
}

#if HAVE_ASYNC_IO
bool DataBlock::ReadDataAsync(aiocb_type& cb, u64 position, size_t size, void *buffer) {
  assert(NULL != diskfile);

  // Check to see if the position from which data is to be read
  // is within the bounds of the data block
  if (position >= length) {
//printf("DataBlock::ReadDataAsync(): position >= length\n");
    // Zero the whole buffer
    memset(buffer, 0, size);
    return false;
  }

  // Compute the file offset and how much data to physically read from disk
  u64    fileoffset = offset + position;
  size_t want       = (size_t)min((u64)size, length - position);

  // Read the data from the file into the buffer
  if (!diskfile->ReadAsync(cb, fileoffset, buffer, want))
    return false;

  // If the read extends beyond the end of the data block,
  // then the rest of the buffer is zeroed.
  if (want < size)
    memset(&((u8*)buffer)[want], 0, size-want);

  return true;
}

bool DataBlock::WriteDataAsync(aiocb_type& cb, u64 position, size_t size, const void *buffer, size_t &wrote) {
  assert(NULL != diskfile);

  wrote = 0;

  // Check to see if the position from which data is to be written
  // is within the bounds of the data block
  if (position >= length) {
//printf("DataBlock::WriteDataAsync(): position >= length\n");
    return false;
  }

  // Compute the file offset and how much data to physically write to disk
  u64    fileoffset = offset + position;
  size_t write_length = (size_t)min((u64)size, length - position);

  // Write the data from the buffer to disk
//printf("DataBlock::WriteDataAsync(): writing %lu bytes\n", write_length);
  if (!diskfile->WriteAsync(cb, fileoffset, buffer, write_length))
    return false;

  wrote = write_length;
  return true;
}
#endif
