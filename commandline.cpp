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
#include <backward/auto_ptr.h>

#if __APPLE__
  #include <sys/types.h>
  #include <sys/sysctl.h>
#endif

#include <set>

#ifdef _MSC_VER
#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif
#endif

#if defined(WIN32) || defined(__APPLE__)
  struct less_stri {
    bool operator()(const string& lhs, const string& rhs) const {
      return stricmp(lhs.c_str(), rhs.c_str()) < 0;
    }
  };
#endif

#ifdef WIN32
  #include <tchar.h>

  enum { OS_SEPARATOR = '\\', OTHER_OS_SEPARATOR = '/' };

  wstring
  UTF8_to_UTF16(const char* utf8_str, size_t utf8_length)
  {
    int i, j;

    // NTFS filenames are precomposed UCS-2 (UTF-16) characters
    // but par2 files require decomposed (aka composite) UCS-1
    // (UTF-8) characters, so first convert from UTF-8 and then
    // precompose the string

    if (utf8_length == 0) {
      i = 0;
    } else {
      i = ::MultiByteToWideChar(CP_UTF8, //    UTF-8
            0,                           //    character-type options
            utf8_str,                    //    string to map
            (int) utf8_length,           //    number of bytes in string
            NULL,                        //    wide-character buffer
            0                            //    size of buffer
          );
      if (i == 0)
        return wstring();
    }

    wstring tmp(i, '\0');
    if (!tmp.data())
      return wstring();

    if (utf8_length != 0) {
      j = ::MultiByteToWideChar(CP_UTF8, //    UTF-8
            0,                           //    character-type options
            utf8_str,                    //    string to map
            (int) utf8_length,           //    number of bytes in string
            &tmp[0],                     //    wide-character buffer
            i                            //    size of buffer
          );
      assert(j == i);
    }

    i = ::FoldString(MAP_PRECOMPOSED, tmp.c_str(), tmp.length(), NULL, 0);
    if (0 == i)
      return wstring();

    wstring res(i, '\0'); // i does not include the end '\0' char
    j = ::FoldString(MAP_PRECOMPOSED, tmp.c_str(), tmp.length(), &res[0], i);
    assert(j == i);
    if (0 == j || j != i)
      return wstring();

//cout << "UTF8_to_UTF16 translated '" << utf8_str << "' [" << utf8_length << "] char utf8 string to " << i << " char wide string." << endl;
    return res;
  }

  wstring
  UTF8_to_UTF16(const string& utf8)
  {
    return UTF8_to_UTF16(utf8.c_str(), utf8.length());
  }

  static
  string
  UTF16_to_console_code_page(const wstring& utf16)
  {
    int    i, j;
    UINT   cp = ::GetConsoleOutputCP();

    {
      i = ::WideCharToMultiByte(cp,      //    code page
            0,                           //    performance and mapping flags
            utf16.c_str(),               //    wide-character string
            utf16.length(),              //    number of chars in string
            NULL,                        //    buffer for new string
            0,                           //    size of buffer in bytes
            NULL, NULL);
      if (i == 0)
        return string();
    }

    string res(i, '\0');
    if (!res.data())
      return string();

//size_t sz = (size_t) (j-1);
    {
      j = ::WideCharToMultiByte(cp,      //    code page
            0,                           //    performance and mapping flags
            utf16.c_str(),               //    wide-character string
            utf16.length(),              //    number of chars in string
            &res[0],                     //    buffer for new string
            i,                           //    size of buffer in bytes
            NULL, NULL);
      assert(j == i);
    }
//cout << "UTF16_to_UTF8 translated " << sz << " char utf16 string to '" << res << "' [" << i << " chars]." << endl;
    return res;
  }

  string
  UTF8_string_to_cout_string(const string& utf8)
  {
    // on Win32, cout outputs to the console which is
    // DOS-compatible, meaning that it doesn't understand
    // UTF-8, which means that the input string needs
    // to be translated to the console's code page
    return ::UTF16_to_console_code_page(::UTF8_to_UTF16(utf8));
  }

  string
  UTF16_to_UTF8(const wchar_t* utf16)
  {
    int    i, j;

    // NTFS filenames are precomposed UCS-2 (UTF-16) characters
    // but par2 files require decomposed (aka composite) UCS-1
    // (UTF-8) characters, so first decompose the string then
    // convert it to UTF-8

    i = ::FoldString(MAP_COMPOSITE, utf16, -1, NULL, 0);
    if (0 == i)
      return string();

    wstring tmp(i, '\0'); // i includes the '\0' char
    j = ::FoldString(MAP_COMPOSITE, utf16, -1, &tmp[0], i);
    assert(j == i);
    if (0 == j || j != i)
      return string();

    {
      i = ::WideCharToMultiByte(CP_UTF8, //    UTF-8
            0,                           //    performance and mapping flags
            tmp.c_str(),                 //    wide-character string
            j-1,                         //    number of chars in string
            NULL,                        //    buffer for new string
            0,                           //    size of buffer in bytes
            NULL, NULL);
      if (i == 0)
        return string();
    }

    string res(i, '\0');
    if (!res.data())
      return string();

//size_t sz = (size_t) (j-1);
    {
      j = ::WideCharToMultiByte(CP_UTF8, //    UTF-8
            0,                           //    performance and mapping flags
            tmp.c_str(),                 //    wide-character string
            j-1,                         //    number of chars in string
            &res[0],                     //    buffer for new string
            i,                           //    size of buffer in bytes
            NULL, NULL);
      assert(j == i);
    }
//cout << "UTF16_to_UTF8 translated " << sz << " char utf16 string to '" << res << "' [" << i << " chars]." << endl;
    return res;
  }

#else

  enum { OS_SEPARATOR = '/', OTHER_OS_SEPARATOR = '\\' };
  #include <dirent.h>

/*#include <CoreFoundation/CoreFoundation.h>

  extern void
  dump_utf8_as_utf16(const string& name);

  void
  dump_utf8_as_utf16(const string& name)
  {
    printf("name[utf8]: ");
    for (size_t z = 0; z != name.length(); ++z)
      printf("'%c' %02X ", (unsigned) (0xFF & name[z]), (unsigned) (0xFF & name[z]));
    printf("\n");

    CFStringRef  s  =  ::CFStringCreateWithBytes(NULL,
                         (const UInt8*) name.c_str(),
                         name.length(), kCFStringEncodingUTF8, false);
    size_t sz = (size_t) ::CFStringGetLength(s);

    UniChar* buf = (UniChar*) malloc(sizeof(UniChar) * sz);
    if (buf != NULL)
      ::CFStringGetCharacters(s, ::CFRangeMake(0, sz), buf);

    printf("name[utf16]: ");
    for (size_t z = 0; z != sz; ++z)
      printf("'%c' %02X ", buf[z] < 0x80 ? buf[z] : '?', buf[z]);
    printf("\n");

    free(buf);
  } */

  #define _tcstod strtod
#endif


extern bool is_existing_folder(const string& dir);

bool
is_existing_folder(const string& dir)
{
  struct_stat st;
  if (stat(utf8_string_to_native_char_array(dir), &st))
    return false;
  return (st.st_mode & S_IFDIR) != 0;
}

static
void
build_file_list_in_imp(string dir, list<string>* l)
{
#if WIN32
  dir += OS_SEPARATOR;
  dir += '*';

  WIN32_FIND_DATA fd;
  HANDLE h = ::FindFirstFile(utf8_string_to_native_char_array(dir), &fd);
  dir.erase(dir.length()-1);
  if (h == INVALID_HANDLE_VALUE)
    return;

  do {
    if (0 == _tcscmp/*strcmp*/(fd.cFileName, TEXT(".")) || 0 == _tcscmp/*strcmp*/(fd.cFileName, TEXT("..")))
      continue;
    if (fd.cFileName[0] == '.' ||
        (FILE_ATTRIBUTE_HIDDEN & fd.dwFileAttributes)) // ignore invisible files/folders
      continue;

    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
      build_file_list_in_imp(dir + native_char_array_to_utf8_string(fd.cFileName), l);
    else
      l->push_back(dir + native_char_array_to_utf8_string(fd.cFileName));
  } while (::FindNextFile(h, &fd));
  ::FindClose(h);
#else
  DIR *dirp = opendir(dir.c_str());
  if (dirp == 0)
    return;

  dir += OS_SEPARATOR;

  struct dirent *d;
  while ((d = readdir(dirp)) != 0) {
    string name = d->d_name;

    if (name == "." || name == "..")
      continue;
    if (name[0] == '.') // ignore invisible files/folders
      continue;

    if (d->d_type == DT_DIR)
      /* dump_utf8_as_utf16(name), */ build_file_list_in_imp(dir + name, l);
    else
      l->push_back(dir + name);
  }
  closedir(dirp);
#endif
}

static
list<string>*
build_file_list_in(const char* dir)
{
  string s(dir);
  if (s.empty())
    return NULL;

  size_t i = s.length()-1;
  char c = s[i];
  if (OTHER_OS_SEPARATOR == c || OS_SEPARATOR == c)
    s.erase(i);

  std::auto_ptr< list<string> > res(new list<string>);
  if (!res.get())
    return NULL;

  build_file_list_in_imp(s, res.get());
  return res.release();
}

CommandLine::ExtraFile::ExtraFile(void)
: filename()
, filesize(0)
{
}

CommandLine::ExtraFile::ExtraFile(const CommandLine::ExtraFile &other)
: filename(other.filename)
, filesize(other.filesize)
{
}

CommandLine::ExtraFile& CommandLine::ExtraFile::operator=(const CommandLine::ExtraFile &other)
{
  filename = other.filename;
  filesize = other.filesize;

  return *this;
}

CommandLine::ExtraFile::ExtraFile(const string &name, u64 size)
: filename(name)
, filesize(size)
{
}


/* static */ CommandLine* CommandLine::sInstance = NULL;

// static
CommandLine*
CommandLine::get(void)
{
  return sInstance;
}


//	static
string
CommandLine::FileOrPathForCout(const string& path)
{
  CommandLine* cl = get();
  if (!cl) {
    cerr << "error: missing cmd line - this should not happen!" << endl;
    return string(); // something is wrong
  }

  const string& bd = cl->GetBaseDirectory();
  if (bd.empty()) {
    string parent_dir;
    string file_name;
    DiskFile::SplitFilename(path, parent_dir, file_name);

    return file_name;
  }
  return path;
}


CommandLine::CommandLine(void)
: operation(opNone)
, version(verUnknown)
, noiselevel(nlUnknown)
, blockcount(0)
, blocksize(0)
, firstblock(0)
, recoveryfilescheme(scUnknown)
, recoveryfilecount(0)
, recoveryblockcount(0)
, recoveryblockcountset(false)
, redundancy(0.0f)
, redundancyset(false)
, parfilename()
, extrafiles()
, totalsourcesize(0)
, largestsourcesize(0)
, memorylimit(0)
#if WANT_CONCURRENT
, concurrent_processing_level(ALL_CONCURRENT) // whether to process everything serially or concurrently
, numthreads(0)
#endif
, create_dummy_par_files(false)
{
  sInstance = this;
}

CommandLine::~CommandLine(void)
{
  sInstance = NULL;
}

void CommandLine::usage(void)
{
  cout << 
    "\n"
    "Usage:\n"
    "\n"
    "  par2 c(reate) [options] <par2 file> [files] : Create PAR2 files\n"
    "  par2 v(erify) [options] <par2 file> [files] : Verify files using PAR2 file\n"
    "  par2 r(epair) [options] <par2 file> [files] : Repair files using PAR2 files\n"
    "\n"
    "You may also leave out the \"c\", \"v\", and \"r\" commands by using \"parcreate\",\n"
    "\"par2verify\", or \"par2repair\" instead.\n"
    "\n"
    "Options:\n"
    "\n"
    "  -b<n>  : Set the Block-Count\n"
    "  -s<n>  : Set the Block-Size (Don't use both -b and -s)\n"
    "  -r<n>  : Level of Redundancy (%)\n"
    "  -c<n>  : Recovery block count (Don't use both -r and -c)\n"
    "  -f<n>  : First Recovery-Block-Number\n"
    "  -u     : Uniform recovery file sizes\n"
    "  -l     : Limit size of recovery files (Don't use both -u and -l)\n"
    "  -n<n>  : Number of recovery files (Don't use both -n and -l)\n"
    "  -m<n>  : Memory (in MB) to use\n"
    "  -v [-v]: Be more verbose\n"
    "  -q [-q]: Be more quiet (-q -q gives silence)\n"
#if WANT_CONCURRENT
    "  -t<+|0|->: Threaded processing. The options are:\n"
	"     -t+ to checksum and create/repair concurrently - uses multiple threads - good for hard disk files - [default]\n"
	"     -t0 to checksum serially but create/repair concurrently - good for slow media such as CDs/DVDs\n"
	"     -t- to checksum/create/repair serially - uses a single thread - good for testing this program\n"
    "  -p<n>  : Set the number of threads for parallel processing\n"
#endif
    // 2007/10/21
    "  -d<dir>: root directory for paths to be put in par2 files OR root directory for files to repair from par2 files\n"
    // 2008/07/07
    "  -0     : create dummy par2 files - for getting actual final par2 files sizes without doing any computing\n"
    "  --     : Treat all remaining CommandLine as filenames\n"
    "\n"
    "If you wish to create par2 files for a single source file, you may leave\n"
    "out the name of the par2 file from the command line.\n";
}

bool CommandLine::Parse(int argc, TCHAR *argv[])
{
  if (argc<1)
  {
    return false;
  }

  // Split the program name into path and filename
  string path, name;
  DiskFile::SplitFilename(native_char_array_to_utf8_string(argv[0]), path, name);
  argc--;
  argv++;

  // Strip ".exe" from the end
  if (name.size() > 4 && 0 == stricmp(".exe", name.substr(name.length()-4).c_str()))
  {
    name = name.substr(0, name.length()-4);
  }

  // Check the resulting program name
  if (0 == stricmp("par2create", name.c_str()))
  {
    operation = opCreate;
  } 
  else if (0 == stricmp("par2verify", name.c_str()))
  {
    operation = opVerify;
  }
  else if (0 == stricmp("par2repair", name.c_str()))
  {
    operation = opRepair;
  }

  // Have we determined what operation we want?
  if (operation == opNone)
  {
    if (argc<2)
    {
      cerr << "Not enough command line arguments." << endl;
      return false;
    }

    switch (tolower(argv[0][0]))
    {
    case 'c':
      if (argv[0][1] == 0 || 0 == stricmp(native_char_array_to_utf8_char_array(argv[0]), "create"))
        operation = opCreate;
      break;
    case 'v':
      if (argv[0][1] == 0 || 0 == stricmp(native_char_array_to_utf8_char_array(argv[0]), "verify"))
        operation = opVerify;
      break;
    case 'r':
      if (argv[0][1] == 0 || 0 == stricmp(native_char_array_to_utf8_char_array(argv[0]), "repair"))
        operation = opRepair;
      break;
    }
    if (operation == opNone)
    {
      cerr << "Invalid operation specified: " << argv[0] << endl;
      return false;
    }
    argc--;
    argv++;
  }

  bool options = true;
#if defined(WIN32) || defined(__APPLE__)
  std::set<string, less_stri>  accepted_filenames;
#else
  std::set<string>  accepted_filenames;
#endif

  while (argc>0)
  {
    if (argv[0][0])
    {
      if (options && argv[0][0] != '-')
        options = false;

      if (options)
      {
        switch (tolower(argv[0][1]))
        {
        case 'b':  // Set the block count
          {
            if (operation != opCreate)
            {
              cerr << "Cannot specify block count unless creating." << endl;
              return false;
            }
            if (blockcount > 0)
            {
              cerr << "Cannot specify block count twice." << endl;
              return false;
            }
            else if (blocksize > 0)
            {
              cerr << "Cannot specify both block count and block size." << endl;
              return false;
            }
            
            TCHAR *p = &argv[0][2];
            while (blockcount <= 3276 && *p && isdigit(*p))
            {
              blockcount = blockcount * 10 + (*p - '0');
              p++;
            }
            if (0 == blockcount || blockcount > 32768 || *p)
            {
              cerr << "Invalid block count option: " << argv[0] << endl;
              return false;
            }
          }
          break;

        case 's':  // Set the block size
          {
            if (operation != opCreate)
            {
              cerr << "Cannot specify block size unless creating." << endl;
              return false;
            }
            if (blocksize > 0)
            {
              cerr << "Cannot specify block size twice." << endl;
              return false;
            }
            else if (blockcount > 0)
            {
              cerr << "Cannot specify both block count and block size." << endl;
              return false;
            }

            TCHAR *p = &argv[0][2];
            while (blocksize <= 429496729 && *p && isdigit(*p))
            {
              blocksize = blocksize * 10 + (*p - '0');
              p++;
            }
            if (*p || blocksize == 0)
            {
              cerr << "Invalid block size option: " << argv[0] << endl;
              return false;
            }
            if (blocksize & 3)
            {
              cerr << "Block size must be a multiple of 4." << endl;
              return false;
            }
          }
          break;

        case 'r':  // Set the amount of redundancy required
          {
            if (operation != opCreate)
            {
              cerr << "Cannot specify redundancy unless creating." << endl;
              return false;
            }
            if (redundancyset)
            {
              cerr << "Cannot specify redundancy twice." << endl;
              return false;
            }
            else if (recoveryblockcountset)
            {
              cerr << "Cannot specify both redundancy and recovery block count." << endl;
              return false;
            }

            TCHAR *p = &argv[0][2];
#if 1
            redundancy = (float) _tcstod(p, &p);
#else
            while (redundancy <= 10 && *p && isdigit(*p))
            {
              redundancy = redundancy * 10 + (*p - '0');
              p++;
            }
#endif
            if (redundancy > 100.0f || *p)
            {
              cerr << "Invalid redundancy option: " << argv[0] << endl;
              return false;
            }
            if (redundancy == 0.0f && recoveryfilecount > 0)
            {
              cerr << "Cannot set redundancy to 0 and file count > 0" << endl;
              return false;
            }
            redundancyset = true;
          }
          break;

        case 'c': // Set the number of recovery blocks to create
          {
            if (operation != opCreate)
            {
              cerr << "Cannot specify recovery block count unless creating." << endl;
              return false;
            }
            if (recoveryblockcountset)
            {
              cerr << "Cannot specify recovery block count twice." << endl;
              return false;
            }
            else if (redundancyset)
            {
              cerr << "Cannot specify both recovery block count and redundancy." << endl;
              return false;
            }

            TCHAR *p = &argv[0][2];
            while (recoveryblockcount <= 32768 && *p && isdigit(*p))
            {
              recoveryblockcount = recoveryblockcount * 10 + (*p - '0');
              p++;
            }
            if (recoveryblockcount > 32768 || *p)
            {
              cerr << "Invalid recoveryblockcount option: " << argv[0] << endl;
              return false;
            }
            if (recoveryblockcount == 0 && recoveryfilecount > 0)
            {
              cerr << "Cannot set recoveryblockcount to 0 and file count > 0" << endl;
              return false;
            }
            recoveryblockcountset = true;
          }
          break;

        case 'f':  // Specify the First block recovery number
          {
            if (operation != opCreate)
            {
              cerr << "Cannot specify first block number unless creating." << endl;
              return false;
            }
            if (firstblock > 0)
            {
              cerr << "Cannot specify first block twice." << endl;
              return false;
            }

            TCHAR *p = &argv[0][2];
            while (firstblock <= 3276 && *p && isdigit(*p))
            {
              firstblock = firstblock * 10 + (*p - '0');
              p++;
            }
            if (firstblock > 32768 || *p)
            {
              cerr << "Invalid first block option: " << argv[0] << endl;
              return false;
            }
          }
          break;

        case 'u':  // Specify uniformly sized recovery files
          {
            if (operation != opCreate)
            {
              cerr << "Cannot specify uniform files unless creating." << endl;
              return false;
            }
            if (argv[0][2])
            {
              cerr << "Invalid option: " << argv[0] << endl;
              return false;
            }
            if (recoveryfilescheme != scUnknown)
            {
              cerr << "Cannot specify two recovery file size schemes." << endl;
              return false;
            }

            recoveryfilescheme = scUniform;
          }
          break;

        case 'l':  // Limit the size of the recovery files
          {
            if (operation != opCreate)
            {
              cerr << "Cannot specify limit files unless creating." << endl;
              return false;
            }
            if (argv[0][2])
            {
              cerr << "Invalid option: " << argv[0] << endl;
              return false;
            }
            if (recoveryfilescheme != scUnknown)
            {
              cerr << "Cannot specify two recovery file size schemes." << endl;
              return false;
            }
            if (recoveryfilecount > 0)
            {
              cerr << "Cannot specify limited size and number of files at the same time." << endl;
              return false;
            }

            recoveryfilescheme = scLimited;
          }
          break;

        case 'n':  // Specify the number of recovery files
          {
            if (operation != opCreate)
            {
              cerr << "Cannot specify recovery file count unless creating." << endl;
              return false;
            }
            if (recoveryfilecount > 0)
            {
              cerr << "Cannot specify recovery file count twice." << endl;
              return false;
            }
            if (redundancyset && redundancy == 0)
            {
              cerr << "Cannot set file count when redundancy is set to 0." << endl;
              return false;
            }
            if (recoveryblockcountset && recoveryblockcount == 0)
            {
              cerr << "Cannot set file count when recovery block count is set to 0." << endl;
              return false;
            }
            if (recoveryfilescheme == scLimited)
            {
              cerr << "Cannot specify limited size and number of files at the same time." << endl;
              return false;
            }

            TCHAR *p = &argv[0][2];
            while (*p && isdigit(*p))
            {
              recoveryfilecount = recoveryfilecount * 10 + (*p - '0');
              p++;
            }
            if (recoveryfilecount == 0 || *p)
            {
              cerr << "Invalid recovery file count option: " << argv[0] << endl;
              return false;
            }
          }
          break;

        case 'm':  // Specify how much memory to use for output buffers
          {
            if (memorylimit > 0)
            {
              cerr << "Cannot specify memory limit twice." << endl;
              return false;
            }

            TCHAR *p = &argv[0][2];
            while (*p && isdigit(*p))
            {
              memorylimit = memorylimit * 10 + (*p - '0');
              p++;
            }
            if (memorylimit == 0 || *p)
            {
              cerr << "Invalid memory limit option: " << argv[0] << endl;
              return false;
            }
          }
          break;

        case 'v':
          {
            switch (noiselevel)
            {
            case nlUnknown:
              {
                if (argv[0][2] == 'v')
                  noiselevel = nlDebug;
                else
                  noiselevel = nlNoisy;
              }
              break;
            case nlNoisy:
            case nlDebug:
              noiselevel = nlDebug;
              break;
            default:
              cerr << "Cannot use both -v and -q." << endl;
              return false;
              break;
            }
          }
          break;

        case 'q':
          {
            switch (noiselevel)
            {
            case nlUnknown:
              {
                if (argv[0][2] == 'q')
                  noiselevel = nlSilent;
                else
                  noiselevel = nlQuiet;
              }
              break;
            case nlQuiet:
            case nlSilent:
              noiselevel = nlSilent;
              break;
            default:
              cerr << "Cannot use both -v and -q." << endl;
              return false;
              break;
            }
          }
          break;

        case 'd': {
          base_directory = DiskFile::GetCanonicalPathname(native_char_array_to_utf8_string(2 + argv[0]));
          if (base_directory.empty()) {
            cerr << "base directory for hierarchy support must specify a folder" << endl;
            return false;
          } else if (operation == opCreate && !is_existing_folder(base_directory)) {
            cerr << "the base directory (" << base_directory << ") for hierarchy support must specify an accessible and existing folder" << endl;
            return false;
          }
          if (base_directory[base_directory.length()-1] != OS_SEPARATOR)
            base_directory += OS_SEPARATOR;
          break;
        }

#if WANT_CONCURRENT
        case 't':
          {
            switch (argv[0][2]) {
            case '-':
              concurrent_processing_level = ALL_SERIAL;
              break;
            case '0':
              concurrent_processing_level = CHECKSUM_SERIALLY_BUT_PROCESS_CONCURRENTLY;
              break;
            case '+':
              concurrent_processing_level = ALL_CONCURRENT;
              break;
            default:
              cerr << "Expected -t+ (use multiple cores) or -t0 (checksum serially, process concurrently) or -t- (use single core)." << endl;
              return false;
            }
          }
          break;
                
         case 'p':  // Set the number of threads
            {
                TCHAR *p = &argv[0][2];
                while (numthreads <= 3276 && *p && isdigit(*p))
                {
                    numthreads = numthreads * 10 + (*p - '0');
                    p++;
                }
                if (numthreads > 32768 || *p)
                {
                    cerr << "Invalid number of threads option: " << argv[0] << endl;
                    return false;
                }
            }
            break;
#endif

        case '0':
          create_dummy_par_files = true;
          break;

        case '-':
          {
            argc--;
            argv++;
            options = false;
            continue;
          }
          break;
        default:
          {
            cerr << "Invalid option specified: " << argv[0] << endl;
            return false;
          }
        }
      }
      else
      {
        list<string> *filenames;

        // If the argument includes wildcard characters, 
        // search the disk for matching files
        if (_tcschr/*strchr*/(argv[0], '*') || _tcschr/*strchr*/(argv[0], '?'))
        {
          string path;
          string name;
          DiskFile::SplitFilename(native_char_array_to_utf8_string(argv[0]), path, name);

          filenames = DiskFile::FindFiles(path, name);
        } else if (is_existing_folder(native_char_array_to_utf8_string(argv[0]))) {
          filenames = build_file_list_in(native_char_array_to_utf8_string(argv[0]).c_str());
        } else {
          filenames = new list<string>;
          filenames->push_back(native_char_array_to_utf8_string(argv[0]));
        }

        for (list<string>::iterator fn = filenames->begin(); fn != filenames->end(); ++fn)
        {
          // Convert filename from command line into a full path + filename
          string filename = DiskFile::GetCanonicalPathname(*fn);

          // filename can be empty if the realpath() API in GetCanonicalPathname() returns NULL
          if (filename.empty())
            filename = *fn;

          // If this is the first file on the command line, then it
          // is the main PAR2 file.
          if (parfilename.length() == 0)
          {
            // If we are verifying or repairing, the PAR2 file must
            // already exist
            if (operation != opCreate)
            {
              // Find the last '.' in the filename
              string::size_type where = filename.find_last_of('.');
              if (where != string::npos)
              {
                // Get what follows the last '.'
                string tail = filename.substr(where+1);

                if (0 == stricmp(tail.c_str(), "par2"))
                {
                  parfilename = filename;
                  version = verPar2;
                }
                else if (0 == stricmp(tail.c_str(), "par") ||
                         (tail.size() == 3 &&
                         tolower(tail[0]) == 'p' &&
                         isdigit(tail[1]) &&
                         isdigit(tail[2])))
                {
                  parfilename = filename;
                  version = verPar1;
                }
              }

              // If we haven't figured out which version of PAR file we
              // are using from the file extension, then presumable the
              // files filename was actually the name of a data file.
              if (version == verUnknown)
              {
                // Check for the existence of a PAR2 of PAR file.
                if (DiskFile::FileExists(filename + ".par2"))
                {
                  version = verPar2;
                  parfilename = filename + ".par2";
                }
                else if (DiskFile::FileExists(filename + ".PAR2"))
                {
                  version = verPar2;
                  parfilename = filename + ".PAR2";
                }
                else if (DiskFile::FileExists(filename + ".par"))
                {
                  version = verPar1;
                  parfilename = filename + ".par";
                }
                else if (DiskFile::FileExists(filename + ".PAR"))
                {
                  version = verPar1;
                  parfilename = filename + ".PAR";
                }
              }
              else
              {
                // Does the specified PAR or PAR2 file exist
                if (!DiskFile::FileExists(filename))
                {
                  version = verUnknown;
                }
              }

              if (version == verUnknown)
              {
                cerr << "The recovery file does not exist: " << filename << endl;
                return false;
              }
            }
            else
            {
              // We are creating a new file
              version = verPar2;
              parfilename = filename;
            }
          }
          else
          {
            // Originally, all specified files were supposed to exist, or the program
            // would stop with an error message. This was not practical, for example in
            // a directory with files appearing and disappearing (an active download directory).
            // So the new rule is: when a specified file doesn't exist, it is silently skipped.
            if (!DiskFile::FileExists(filename)) {
              cout << "Ignoring non-existent source file: " << filename << endl;
            } else {
              u64 filesize = DiskFile::GetFileSize(filename);

              // Ignore all 0 byte files and duplicate file names
              if (filesize == 0)
                cout << "Skipping 0 byte file: " << filename << endl;
              else if (accepted_filenames.end() != accepted_filenames.find(filename))
                cout << "Skipping duplicate filename: " << filename << endl;
              else /* if (accepted_filenames.end() == accepted_filenames.find(filename)) */ {
                accepted_filenames.insert(filename);
                extrafiles.push_back(ExtraFile(filename, filesize));

                // track the total size of the source files and how
                // big the largest one is.
                totalsourcesize += filesize;
                if (largestsourcesize < filesize)
                  largestsourcesize = filesize;
              } // if
            } // if
          } // if
        } // for
        delete filenames;
      }
    }

    argc--;
    argv++;
  }

  if (parfilename.length() == 0)
  {
    cerr << "You must specify a Recovery file." << endl;
    return false;
  }

  // Default noise level
  if (noiselevel == nlUnknown)
  {
    noiselevel = nlNormal;
  }

  // If we a creating, check the other parameters
  if (operation == opCreate)
  {
    // If no recovery file size scheme is specified then use Variable
    if (recoveryfilescheme == scUnknown)
    {
      recoveryfilescheme = scVariable;
    }

    // If neither block count not block size is specified
    if (blockcount == 0 && blocksize == 0)
    {
      // Use a block count of 2000
      blockcount = 2000;
    }

    // If we are creating, the source files must be given.
    if (extrafiles.size() == 0)
    {
      // Does the par filename include the ".par2" on the end?
      if (parfilename.length() > 5 && 0 == stricmp(parfilename.substr(parfilename.length()-5, 5).c_str(), ".par2"))
      {
        // Yes it does.
        cerr << "You must specify a list of files when creating." << endl;
        return false;
      }
      else
      {
        // No it does not.

        // In that case check to see if the file exists, and if it does
        // assume that you wish to create par2 files for it.

        u64 filesize = 0;
        if (DiskFile::FileExists(parfilename) &&
            (filesize = DiskFile::GetFileSize(parfilename)) > 0)
        {
          extrafiles.push_back(ExtraFile(parfilename, filesize));

          // track the total size of the source files and how
          // big the largest one is.
          totalsourcesize += filesize;
          if (largestsourcesize < filesize)
            largestsourcesize = filesize;
        }
        else
        {
          // The file does not exist or it is empty.

          cerr << "You must specify a list of files when creating." << endl;
          return false;
        }
      }
    }

    // Strip the ".par2" from the end of the filename of the main PAR2 file.
    if (parfilename.length() > 5 && 0 == stricmp(parfilename.substr(parfilename.length()-5, 5).c_str(), ".par2"))
    {
      parfilename = parfilename.substr(0, parfilename.length()-5);
    }

    // Assume a redundancy of 5% if neither redundancy or recoveryblockcount were set.
    if (!redundancyset && !recoveryblockcountset)
    {
      redundancy = 5;
    }
  }

  // Assume a memory limit of 16MB if not specified.
  if (memorylimit == 0)
  {
#if defined(WIN32) || defined(WIN64)
    u64 TotalPhysicalMemory = 0;

    HMODULE hLib = ::LoadLibraryA("kernel32.dll");
    if (NULL != hLib)
    {
      BOOL (WINAPI *pfn)(LPMEMORYSTATUSEX) = (BOOL (WINAPI*)(LPMEMORYSTATUSEX))::GetProcAddress(hLib, "GlobalMemoryStatusEx");

      if (NULL != pfn)
      {
        MEMORYSTATUSEX mse;
        mse.dwLength = sizeof(mse);
        if (pfn(&mse))
        {
          TotalPhysicalMemory = mse.ullTotalPhys;
        }
      }

      ::FreeLibrary(hLib);
    }

    if (TotalPhysicalMemory == 0)
    {
      MEMORYSTATUS ms;
      ::ZeroMemory(&ms, sizeof(ms));
      ::GlobalMemoryStatus(&ms);

      TotalPhysicalMemory = ms.dwTotalPhys;
    }

    if (TotalPhysicalMemory == 0)
    {
      // Assume 128MB
      TotalPhysicalMemory = 128 * 1048576;
    }

    // Half of total physical memory
    memorylimit = (size_t)(TotalPhysicalMemory / 1048576 / 2);

#elif __APPLE__

    int name[2] = {CTL_HW, HW_USERMEM};
    int usermem_bytes;
    size_t size = sizeof(usermem_bytes);
    sysctl( name, 2, &usermem_bytes, &size, NULL, 0 );
    memorylimit = usermem_bytes / (2048 * 1024);

#else
  #if WANT_CONCURRENT
    // Assume 128MB (otherwise processing is slower)
    memorylimit = 64;
  #else
    memorylimit = 16;
  #endif
#endif
  }
  memorylimit *= 1048576;

  return true;
}
