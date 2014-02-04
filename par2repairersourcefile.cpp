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
//  Modifications for concurrent processing, Unicode support, and hierarchial
//  directory support are Copyright (c) 2007-2008 Vincent Tan.
//  Search for "#if WANT_CONCURRENT" for concurrent code.
//  Concurrent processing utilises Intel Thread Building Blocks 2.0,
//  Copyright (c) 2007 Intel Corp.

#include "par2cmdline.h"

#ifdef WIN32
  #include <direct.h> // for mkdir
  enum { OS_SEPARATOR = '\\' };
#else
  #include <sys/types.h>
  #include <sys/stat.h> // for mkdir
  enum { OS_SEPARATOR = '/' };
#endif

#ifdef _MSC_VER
#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif
#endif

extern bool is_existing_folder(const string& dir);

static
bool
create_path(const string& path)
{
  string::size_type where = path.find_last_of(OS_SEPARATOR);
  if (string::npos == where)
    return false; // something is wrong

  string s(path.substr(0, where));
  if (!is_existing_folder(s) && !create_path(s))
    return false;

#ifdef WIN32
  return 0 == mkdir(path.c_str());
#else
  return 0 == mkdir(path.c_str(), 0000755);
#endif
}

Par2RepairerSourceFile::Par2RepairerSourceFile(DescriptionPacket *_descriptionpacket,
                                               VerificationPacket *_verificationpacket)
{
  firstblocknumber = 0;

  descriptionpacket = _descriptionpacket;
  verificationpacket = _verificationpacket;

//  verificationhashtable = 0;

  targetexists = false;
  targetfile = 0;
  completefile = 0;
}

Par2RepairerSourceFile::~Par2RepairerSourceFile(void)
{
  delete descriptionpacket;
  delete verificationpacket;

//  delete verificationhashtable;
}


void Par2RepairerSourceFile::SetDescriptionPacket(DescriptionPacket *_descriptionpacket)
{
  descriptionpacket = _descriptionpacket;
}

void Par2RepairerSourceFile::SetVerificationPacket(VerificationPacket *_verificationpacket)
{
  verificationpacket = _verificationpacket;
}

void Par2RepairerSourceFile::ComputeTargetFileName(string path) // path is the directory of the par2 file
{
  // Get a version of the filename compatible with the OS
  string filename = DiskFile::TranslateFilename(descriptionpacket->FileName());

  CommandLine* cl = CommandLine::get();
  if (!cl)
    return; // something is wrong

  string::size_type where;
  const string& bd = cl->GetBaseDirectory();
  if (bd.empty()) {
    // Strip the path from the filename
    if (string::npos != (where = filename.find_last_of(OS_SEPARATOR)))
      filename = filename.substr(where+1);

    targetfilename = path + filename;
  } else {
    filename = bd + filename; // always use base_directory

    where = filename.find_last_of(OS_SEPARATOR);
    if (string::npos == where)
      return; // something is wrong

    targetfilename = filename;
    filename.erase(where); // remove OS_SEPARATOR+filename

    // ensure that the directory where the file is supposed to be exists;
    // create it if it does not exist
    if (!is_existing_folder(filename))
      (bool) create_path(filename);
  }
}

string Par2RepairerSourceFile::TargetFileName(void) const
{
  return targetfilename;
}

void Par2RepairerSourceFile::SetTargetFile(DiskFile *diskfile)
{
  targetfile = diskfile;
}

DiskFile* Par2RepairerSourceFile::GetTargetFile(void) const
{
  return targetfile;
}

void Par2RepairerSourceFile::SetTargetExists(bool exists)
{
  targetexists = exists;
}

bool Par2RepairerSourceFile::GetTargetExists(void) const
{
  return targetexists;
}

void Par2RepairerSourceFile::SetCompleteFile(DiskFile *diskfile)
{
  completefile = diskfile;
}

DiskFile* Par2RepairerSourceFile::GetCompleteFile(void) const
{
  return completefile;
}

// Remember which source and target blocks will be used by this file
// and set their lengths appropriately
void Par2RepairerSourceFile::SetBlocks(u32 _blocknumber,
                                       u32 _blockcount,
                                       vector<DataBlock>::iterator _sourceblocks, 
                                       vector<DataBlock>::iterator _targetblocks,
                                       u64 blocksize)
{
  firstblocknumber = _blocknumber;
  blockcount = _blockcount;
  sourceblocks = _sourceblocks;
  targetblocks = _targetblocks;

  if (blockcount > 0)
  {
    u64 filesize = descriptionpacket->FileSize();

    vector<DataBlock>::iterator sb = sourceblocks;
    for (u32 blocknumber=0; blocknumber<blockcount; ++blocknumber, ++sb)
    {
      DataBlock &datablock = *sb;

      u64 blocklength = min(blocksize, filesize-(u64)blocknumber*blocksize);

      datablock.SetLength(blocklength);
    }
  }
}

// Determine the block count from the file size and block size.
void Par2RepairerSourceFile::SetBlockCount(u64 blocksize)
{
  if (descriptionpacket)
  {
    blockcount = (u32)((descriptionpacket->FileSize() + blocksize-1) / blocksize);
  }
  else
  {
    blockcount = 0;
  }
}
