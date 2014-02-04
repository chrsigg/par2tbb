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


#ifdef WIN32
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <tchar.h>

#define OffsetType __int64
#define MaxOffset 0x7fffffffffffffffI64
#define LengthType unsigned int
#define MaxLength 0xffffffffUL

DiskFile::DiskFile(void)
{
  //filename;
  filesize = 0;
  offset = 0;

  hFile = INVALID_HANDLE_VALUE;

  exists = false;

  blockcount = 0;
}

DiskFile::~DiskFile(void)
{
  if (hFile != INVALID_HANDLE_VALUE)
    ::CloseHandle(hFile);
}

// Create new file on disk and make sure that there is enough
// space on disk for it.
bool DiskFile::Create(string _filename, u64 _filesize, bool async)
{
  assert(hFile == INVALID_HANDLE_VALUE);

  filename = _filename;
  filesize = _filesize;

  // Create the file
  hFile = ::CreateFile(utf8_string_to_native_char_array(_filename),
                       GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_NEW,
#if HAVE_ASYNC_IO
                       async ? FILE_FLAG_OVERLAPPED : 0,
#else
                       0,
#endif
                       NULL);
  if (hFile == INVALID_HANDLE_VALUE)
  {
    DWORD error = ::GetLastError();

    cerr << "Could not create \"" << _filename << "\": " << ErrorMessage(error) << endl;

    return false;
  }

  if (filesize > 0)
  {
    // Seek to the end of the file
    LONG lowoffset = ((LONG*)&filesize)[0];
    LONG highoffset = ((LONG*)&filesize)[1];

    if (INVALID_SET_FILE_POINTER == SetFilePointer(hFile, lowoffset, &highoffset, FILE_BEGIN))
    {
      DWORD error = ::GetLastError();

      cerr << "Could not set size of \"" << _filename << "\": " << ErrorMessage(error) << endl;

      ::CloseHandle(hFile);
      hFile = INVALID_HANDLE_VALUE;
      ::DeleteFile(utf8_string_to_native_char_array(_filename));

      return false;
    }

    // Set the end of the file
    if (!::SetEndOfFile(hFile))
    {
      DWORD error = ::GetLastError();

      cerr << "Could not set size of \"" << _filename << "\": " << ErrorMessage(error) << endl;

      ::CloseHandle(hFile);
      hFile = INVALID_HANDLE_VALUE;
      ::DeleteFile(utf8_string_to_native_char_array(_filename));

      return false;
    }
  }

  offset = filesize;

  exists = true;
  return true;
}

// Write some data to disk

bool DiskFile::Write(u64 _offset, const void *buffer, size_t length)
{
  assert(hFile != INVALID_HANDLE_VALUE);

  if (offset != _offset)
  {
    LONG lowoffset = ((LONG*)&_offset)[0];
    LONG highoffset = ((LONG*)&_offset)[1];

    // Seek to the required offset
    if (INVALID_SET_FILE_POINTER == SetFilePointer(hFile, lowoffset, &highoffset, FILE_BEGIN))
    {
      DWORD error = ::GetLastError();

      cerr << "Could not write " << (u64)length << " bytes to \"" << filename << "\" at offset " << _offset << ": " << ErrorMessage(error) << endl;

      return false;
    }
    offset = _offset;
  }

  if (length > MaxLength)
  {
    cerr << "Could not write " << (u64)length << " bytes to \"" << filename << "\" at offset " << _offset << ": " << "Write too long" << endl;

    return false;
  }

  DWORD write = (LengthType)length;
  DWORD wrote;

  // Write the data
  if (!::WriteFile(hFile, buffer, write, &wrote, NULL))
  {
    DWORD error = ::GetLastError();

    cerr << "Could not write " << (u64)length << " bytes to \"" << filename << "\" at offset " << _offset << ": " << ErrorMessage(error) << endl;

    return false;
  }

  offset += length;

  if (filesize < offset)
  {
    filesize = offset;
  }

  return true;
}

#if HAVE_ASYNC_IO

bool DiskFile::ReadAsync(aiocb_type& cb, u64 offset, void *buffer, size_t length) {
  assert(INVALID_HANDLE_VALUE != hFile);
  return cb.read(hFile, length, buffer, (off_t) offset);
}

bool DiskFile::WriteAsync(aiocb_type& cb, u64 offset, const void *buffer, size_t length) {
  assert(INVALID_HANDLE_VALUE != hFile);
  return cb.write(hFile, length, buffer, (off_t) offset);
}

#endif

// Open the file

bool DiskFile::Open(string _filename, u64 _filesize, bool async)
{
  assert(hFile == INVALID_HANDLE_VALUE);

  filename = _filename;
  filesize = _filesize;

  hFile = ::CreateFile(utf8_string_to_native_char_array(_filename), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
#if HAVE_ASYNC_IO
                       async ? FILE_FLAG_OVERLAPPED : 0,
#else
                       0,
#endif
                       NULL);
  if (hFile == INVALID_HANDLE_VALUE)
  {
    DWORD error = ::GetLastError();

    switch (error)
    {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
      break;
    default:
      cerr << "Could not open \"" << _filename << "\": " << ErrorMessage(error) << endl;
    }

    return false;
  }

  offset = 0;
  exists = true;

  return true;
}

// Read some data from disk

bool DiskFile::Read(u64 _offset, void *buffer, size_t length)
{
  assert(hFile != INVALID_HANDLE_VALUE);

  if (offset != _offset)
  {
    LONG lowoffset = ((LONG*)&_offset)[0];
    LONG highoffset = ((LONG*)&_offset)[1];

    // Seek to the required offset
    if (INVALID_SET_FILE_POINTER == SetFilePointer(hFile, lowoffset, &highoffset, FILE_BEGIN))
    {
      DWORD error = ::GetLastError();

      cerr << "Could not read " << (u64)length << " bytes from \"" << filename << "\" at offset " << _offset << ": " << ErrorMessage(error) << endl;

      return false;
    }
    offset = _offset;
  }

  if (length > MaxLength)
  {
    cerr << "Could not read " << (u64)length << " bytes from \"" << filename << "\" at offset " << _offset << ": " << "Read too long" << endl;

    return false;
  }

  DWORD want = (LengthType)length;
  DWORD got;

  // Read the data
  if (!::ReadFile(hFile, buffer, want, &got, NULL))
  {
    DWORD error = ::GetLastError();

    cerr << "Could not read " << (u64)length << " bytes from \"" << filename << "\" at offset " << _offset << ": " << ErrorMessage(error) << endl;

    return false;
  }

  offset += length;

  return true;
}

void DiskFile::Close(void)
{
  if (hFile != INVALID_HANDLE_VALUE)
  {
    ::CloseHandle(hFile);
    hFile = INVALID_HANDLE_VALUE;
  }
}

string DiskFile::GetCanonicalPathname(string filename)
{
#ifdef UNICODE
  typedef wstring TSTRING;
#else
  typedef string TSTRING;
#endif

  TCHAR fullname[MAX_PATH];
  TCHAR *filepart;

  // Resolve a relative path to a full path
  int length = ::GetFullPathName(utf8_string_to_native_char_array(filename),
                                 sizeof(fullname)/sizeof(TCHAR), fullname, &filepart);
  if (length <= 0 || (sizeof(fullname)/sizeof(TCHAR)) < length)
    return filename;

  // Make sure the drive letter is upper case.
  fullname[0] = toupper(fullname[0]);

  // Translate all /'s to \'s
  TCHAR *current = _tcschr/*strchr*/(fullname, '/');
  while (current)
  {
    *current++ = '\\';
    current  = _tcschr/*strchr*/(current, '/');
  }

  // Copy the root directory to the output string
  TSTRING longname(fullname, 3);

  // Start processing at the first path component
  current = &fullname[3];
  TCHAR *limit = &fullname[length];

  // Process until we reach the end of the full name
  while (current < limit)
  {
    TCHAR *tail;

    // Find the next \, or the end of the string
    (tail = _tcschr/*strchr*/(current, '\\')) || (tail = limit);
    *tail = 0;

    // Create a wildcard to search for the path
    TSTRING wild = longname + current;
    WIN32_FIND_DATA finddata;
    HANDLE hFind = ::FindFirstFile(wild.c_str(), &finddata);
    if (hFind == INVALID_HANDLE_VALUE)
    {
      // If the component was not found then just copy the rest of the path to the
      // output buffer verbatim.
      longname += current;
      break;
    }
    ::FindClose(hFind);

    // Copy the component found to the output
    longname += finddata.cFileName;

    current = tail + 1;

    // If we have not reached the end of the name, add a "\"
    if (current < limit)
      longname += '\\';
  }

  return native_char_array_to_utf8_string(longname.c_str());
}

list<string>* DiskFile::FindFiles(string path, string wildcard)
{
  list<string> *matches = new list<string>;

  wildcard = path + wildcard;
  WIN32_FIND_DATA fd;
  HANDLE h = ::FindFirstFile(utf8_string_to_native_char_array(wildcard), &fd);
  if (h != INVALID_HANDLE_VALUE)
  {
    do
    {
      if (0 == (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
      {
        matches->push_back(path + native_char_array_to_utf8_string(fd.cFileName));
      }
    } while (::FindNextFile(h, &fd));
    ::FindClose(h);
  }

  return matches;
}




/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#else // !WIN32
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef HAVE_FSEEKO
# define OffsetType off_t
# define MaxOffset ((off_t)0x7fffffffffffffffULL)
# define fseek fseeko
#else
# if _FILE_OFFSET_BITS == 64
#  define OffsetType unsigned long long
#  define MaxOffset 0x7fffffffffffffffULL
# else
#  define OffsetType long
#  define MaxOffset 0x7fffffffUL
# endif
#endif

#define LengthType unsigned int
#define MaxLength 0xffffffffUL

DiskFile::DiskFile(void)
{
  //filename;
  filesize = 0;
  offset = 0;

  file = 0;

  exists = false;

  blockcount = 0;
}

DiskFile::~DiskFile(void)
{
  if (file != 0)
    fclose(file);
}

// Create new file on disk and make sure that there is enough
// space on disk for it.
bool DiskFile::Create(string _filename, u64 _filesize, bool /* async */)
{
  assert(file == 0);

  filename = _filename;
  filesize = _filesize;

  file = fopen(_filename.c_str(), "wb");
  if (file == 0)
  {
    cerr << "Could not create: " << _filename << endl;

    return false;
  }

  if (_filesize > (u64)MaxOffset)
  {
    cerr << "Requested file size for " << _filename << " is too large." << endl;
    return false;
  }

  if (_filesize > 0)
  {
    if (fseek(file, (OffsetType)_filesize-1, SEEK_SET))
    {
      fclose(file);
      file = 0;
      ::remove(filename.c_str());
      
      cerr << "Could not set end of file: " << _filename << endl;
      return false;
    }

    if (1 != fwrite(&_filesize, 1, 1, file))
    {
      fclose(file);
      file = 0;
      ::remove(filename.c_str());
      
      cerr << "Could not set end of file: " << _filename << endl;
      return false;
    }

#if HAVE_ASYNC_IO
    // 20090202 bug fix: async writes to the newly created file can
    // fail for the last byte because the file's state hasn't been
    // updated. This problem definitely occurs under Mac OS X
    // 10.4/10.5, so fix this by flushing the new file.
    // Closing and then re-opening the new file also works.
    if (true) {
      if (fflush(file)) {
        fclose(file);
        file = 0;
        ::remove(filename.c_str());
      
        cerr << "Could not flush newly created file: " << _filename << endl;
        return false;
      }
    } else {
      if (fclose(file)) {
        file = 0;
        ::remove(filename.c_str());
      
        cerr << "Could not close created file: " << _filename << endl;
        return false;
      }

      file = fopen(_filename.c_str(), "r+b"); // do NOT truncate file
      if (file == 0) {
        cerr << "Could not re-open newly created file: " << _filename << endl;

        return false;
      }
    }
#endif
  }

  offset = filesize;

  exists = true;
  return true;
}

// Write some data to disk

bool DiskFile::Write(u64 _offset, const void *buffer, size_t length)
{
  assert(file != 0);

  if (offset != _offset)
  {
    if (_offset > (u64)MaxOffset)
    {
      cerr << "Could not write " << (u64)length << " bytes to " << filename << " at offset " << _offset << endl;
      return false;
    }


    if (fseek(file, (OffsetType)_offset, SEEK_SET))
    {
      cerr << "Could not write " << (u64)length << " bytes to " << filename << " at offset " << _offset << endl;
      return false;
    }
    offset = _offset;
  }

  if (length > MaxLength)
  {
    cerr << "Could not write " << (u64)length << " bytes to " << filename << " at offset " << _offset << endl;
    return false;
  }

  if (1 != fwrite(buffer, (LengthType)length, 1, file))
  {
    cerr << "Could not write " << (u64)length << " bytes to " << filename << " at offset " << _offset << endl;
    return false;
  }

  offset += length;

  if (filesize < offset)
  {
    filesize = offset;
  }

  return true;
}

#if HAVE_ASYNC_IO

bool DiskFile::ReadAsync(aiocb_type& cb, u64 offset, void *buffer, size_t length) {
  assert(NULL != file);
  return cb.read(fileno(file), length, buffer, (off_t) offset);
}

bool DiskFile::WriteAsync(aiocb_type& cb, u64 offset, const void *buffer, size_t length) {
  assert(NULL != file);
  return cb.write(fileno(file), length, buffer, (off_t) offset);
}

#endif

// Open the file

bool DiskFile::Open(string _filename, u64 _filesize, bool /* async */)
{
  assert(file == 0);

  filename = _filename;
  filesize = _filesize;

  if (_filesize > (u64)MaxOffset)
  {
    cerr << "File size for " << _filename << " is too large." << endl;
    return false;
  }

  file = fopen(filename.c_str(), "rb");
  if (file == 0)
  {
    return false;
  }

  offset = 0;
  exists = true;

  return true;
}

// Read some data from disk

bool DiskFile::Read(u64 _offset, void *buffer, size_t length)
{
  assert(file != 0);

  if (offset != _offset)
  {
    if (_offset > (u64)MaxOffset)
    {
      cerr << "Could not read " << (u64)length << " bytes from " << filename << " at offset " << _offset << endl;
      return false;
    }


    if (fseek(file, (OffsetType)_offset, SEEK_SET))
    {
      cerr << "Could not read " << (u64)length << " bytes from " << filename << " at offset " << _offset << endl;
      return false;
    }
    offset = _offset;
  }

  if (length > MaxLength)
  {
    cerr << "Could not read " << (u64)length << " bytes from " << filename << " at offset " << _offset << endl;
    return false;
  }

  if (1 != fread(buffer, (LengthType)length, 1, file))
  {
    cerr << "Could not read " << (u64)length << " bytes from " << filename << " at offset " << _offset << endl;
    return false;
  }

  offset += length;

  return true;
}

void DiskFile::Close(void)
{
  if (file != 0)
  {
    fclose(file);
    file = 0;
  }
}

// Attempt to get the full pathname of the file
string DiskFile::GetCanonicalPathname(string filename)
{
  // Is the supplied path already an absolute one
  if (filename.size() == 0 || filename[0] == '/')
    return filename;

  // Get the current directory
  char curdir[1000];
  if (0 == getcwd(curdir, sizeof(curdir)))
  {
    return filename;
  }

#if HAVE_REALPATH
  if (!realpath(filename.c_str(), curdir))
    return string();
  return curdir;
#else
  // Allocate a work buffer and copy the resulting full path into it.
  char *work = new char[strlen(curdir) + filename.size() + 2];
  strcpy(work, curdir);
  if (work[strlen(work)-1] != '/')
    strcat(work, "/");
  strcat(work, filename.c_str());

  char *in = work;
  char *out = work;

  while (*in)
  {
    if (*in == '/')
    {
      if (in[1] == '.' && in[2] == '/')
      {
        // skip the input past /./
        in += 2;
      }
      else if (in[1] == '.' && in[2] == '.' && in[3] == '/')
      {
        // backtrack the output if /../ was found on the input
        in += 3;
        if (out > work)
        {
          do
          {
            out--;
          } while (out > work && *out != '/');
        }
      }
      else
      {
        *out++ = *in++;
      }
    }
    else
    {
      *out++ = *in++;
    }
  }
  *out = 0;

  string result = work;
  delete [] work;

  return result;
#endif
}

list<string>* DiskFile::FindFiles(string path, string wildcard)
{
  list<string> *matches = new list<string>;

  string::size_type where;

  if ((where = wildcard.find_first_of('*')) != string::npos ||
      (where = wildcard.find_first_of('?')) != string::npos)
  {
    string front = wildcard.substr(0, where);
    bool multiple = wildcard[where] == '*';
    string back = wildcard.substr(where+1);

    DIR *dirp = opendir(path.c_str());
    if (dirp != 0)
    {
      struct dirent *d;
      while ((d = readdir(dirp)) != 0)
      {
        string name = d->d_name;

        if (name == "." || name == "..")
          continue;

        if (multiple)
        {
          if (name.size() >= wildcard.size() &&
              name.substr(0, where) == front &&
              name.substr(name.size()-back.size()) == back)
          {
            matches->push_back(path + name);
          }
        }
        else
        {
          if (name.size() == wildcard.size())
          {
            string::const_iterator pw = wildcard.begin();
            string::const_iterator pn = name.begin();
            while (pw != wildcard.end())
            {
              if (*pw != '?' && *pw != *pn)
                break;
              ++pw;
              ++pn;
            }

            if (pw == wildcard.end())
            {
              matches->push_back(path + name);
            }
          }
        }

      }
      closedir(dirp);
    }
  }
  else
  {
    struct stat st;
    string fn = path + wildcard;
    if (stat(fn.c_str(), &st) == 0)
    {
      matches->push_back(path + wildcard);
    }
  }

  return matches;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif























bool DiskFile::Open(bool async)
{
  string _filename = filename;

  return Open(_filename, async);
}

bool DiskFile::Open(string _filename, bool async)
{
  return Open(_filename, GetFileSize(_filename), async);
}









// Delete the file

bool DiskFile::Delete(void)
{
#ifdef WIN32
  assert(hFile == INVALID_HANDLE_VALUE);
#else
  assert(file == 0);
#endif

  if (filename.size() > 0 && 0 == unlink(filename.c_str()))
  {
    return true;
  }
  else
  {
    cerr << "Cannot delete " << filename << endl;

    return false;
  }
}









//string DiskFile::GetPathFromFilename(string filename)
//{
//  string::size_type where;
//
//  if (string::npos != (where = filename.find_last_of('/')) ||
//      string::npos != (where = filename.find_last_of('\\')))
//  {
//    return filename.substr(0, where+1);
//  }
//  else
//  {
//    return "." PATHSEP;
//  }
//}

void DiskFile::SplitFilename(string filename, string &path, string &name)
{
  string::size_type where;

  if (string::npos != (where = filename.find_last_of('/')) ||
      string::npos != (where = filename.find_last_of('\\')))
  {
    path = filename.substr(0, where+1);
    name = filename.substr(where+1);
  }
  else
  {
    path = "." PATHSEP;
    name = filename;
  }
}

#define SUPPORT_BLOCK_DEVICES 0

#if SUPPORT_BLOCK_DEVICES
  #include <sys/ioctl.h>
  #include <sys/mount.h>
#endif

bool DiskFile::FileExists(string filename)
{
  struct_stat st;
#if SUPPORT_BLOCK_DEVICES
  return ((0 == stat(filename.c_str(), &st)) &&
          (0 != (st.st_mode & (S_IFREG|S_IFBLK))));
#else
  return ((0 == stat(utf8_string_to_native_char_array(filename), &st)) && (0 != (st.st_mode & S_IFREG)));
#endif
}

u64 DiskFile::GetFileSize(string filename)
{
  struct_stat st;
  if (0 == stat(utf8_string_to_native_char_array(filename), &st)) {
    if (st.st_mode & S_IFREG)
    {
      return st.st_size;
    }
#if SUPPORT_BLOCK_DEVICES
    else if (st.st_mode & S_IFBLK)
    {
      FILE *fp;
      u64 size = 0;
      if ( NULL != (fp = fopen(filename.c_str(), "rb")) )
      {
        ioctl(fileno(fp), BLKGETSIZE64, &size);
        fclose(fp);
      }
      return size;
    }
#endif
  }

  return 0;
}



// Take a filename from a PAR2 file and replace any characters
// which would be illegal for a file on disk
string DiskFile::TranslateFilename(string filename)
{
  string result;

  string::iterator p = filename.begin();
  while (p != filename.end())
  {
    unsigned char ch = *p;

    bool ok = true;
#ifdef WIN32
    if (ch < 32)
    {
      ok = false;
    }
    else
    {
      switch (ch)
      {
      case '/':
      case '\\': {
        CommandLine* cl = CommandLine::get();
        if (cl && !cl->GetBaseDirectory().empty()) {
          ch = '\\'; // if '/' was stored in the par2 file, change it to '\\'
          break;
        }
      }
      case '"':
      case '*':
      case ':':
      case '<':
      case '>':
      case '?':
      case '|':
        ok = false;
      }
    }
#else
    if (ch < 32)
    {
      ok = false;
    }
    else if ('/' == ch || '\\' == ch)
    {
      CommandLine* cl = CommandLine::get();
      if (cl && !cl->GetBaseDirectory().empty())
        ch = '/'; // if '\\' was stored in the par2 file, change it to '/'
      else
        ok = false;
    }
#endif


    if (ok)
    {
      result += ch;
    }
    else
    {
      // convert problem characters to hex
      result += ((ch >> 4) < 10) ? (ch >> 4) + '0' : (ch >> 4) + 'A'-10;
      result += ((ch & 0xf) < 10) ? (ch & 0xf) + '0' : (ch & 0xf) + 'A'-10;
    }

    ++p;
  }

  return result;
}

bool DiskFile::Rename(void)
{
  char newname[_MAX_PATH+1];
  u32 index = 0;

  struct_stat st;

  do
  {
    int length = snprintf(newname, _MAX_PATH, "%s.%u", filename.c_str(), ++index);
    if (length < 0)
    {
      cerr << filename << " cannot be renamed." << endl;
      return false;
    }
    newname[length] = 0;
  } while (stat(utf8_char_array_to_native_char_array(newname), &st) == 0);

  return Rename(newname);
}

bool DiskFile::Rename(string _filename)
{
#ifdef WIN32
  assert(hFile == INVALID_HANDLE_VALUE);
#else
  assert(file == 0);
#endif

  if (::rename(filename.c_str(), _filename.c_str()) == 0)
  {
    filename = _filename;

    return true;
  }
  else
  {
    cerr << filename << " cannot be renamed to " << _filename << endl;

    return false;
  }
}

#ifdef WIN32
string DiskFile::ErrorMessage(DWORD error)
{
  string result;

  LPVOID lpMsgBuf;
  if (::FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       NULL,
                       error,
                       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       (LPSTR)&lpMsgBuf,
                       0,
                       NULL))
  {
    result = (char*)lpMsgBuf;
    LocalFree(lpMsgBuf);
  }
  else
  {
    char message[40];
    _snprintf(message, sizeof(message), "Unknown error code (%d)", error);
    result = message;
  }

  return result;
}
#endif

DiskFileMap::DiskFileMap(void)
{
}

DiskFileMap::~DiskFileMap(void)
{
  map<string, DiskFile*>::iterator fi = diskfilemap.begin();
  while (fi != diskfilemap.end())
  {
    delete (*fi).second;

    ++fi;
  }
}

bool DiskFileMap::Insert(DiskFile *diskfile)
{
  string filename = diskfile->FileName();
  assert(filename.length() != 0);

  pair<map<string,DiskFile*>::const_iterator,bool> location = diskfilemap.insert(pair<string,DiskFile*>(filename, diskfile));

  return location.second;
}

void DiskFileMap::Remove(DiskFile *diskfile)
{
  string filename = diskfile->FileName();
  assert(filename.length() != 0);

  diskfilemap.erase(filename);
}

DiskFile* DiskFileMap::Find(string filename) const
{
  assert(filename.length() != 0);

  map<string, DiskFile*>::const_iterator f = diskfilemap.find(filename);

  return (f != diskfilemap.end()) ?  f->second : 0;
}
