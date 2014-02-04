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

#ifndef __COMMANDLINE_H__
#define __COMMANDLINE_H__

// The CommandLine object is responsible for understanding the format
// of the command line parameters are parsing the command line to
// extract details as to what the user wants to do.

class CommandLine
{
  static CommandLine* sInstance;

public:
  static CommandLine* get(void);
  static string FileOrPathForCout(const string& path);


  CommandLine(void);
  ~CommandLine(void);

  // Parse the supplied command line arguments. 
  bool Parse(int argc, TCHAR *argv[]);

  // Display details of the correct format for command line parameters.
  static void usage(void);

  // What operation will we be carrying out
  typedef enum
  {
    opNone = 0,
    opCreate,        // Create new PAR2 recovery volumes
    opVerify,        // Verify but don't repair damaged data files
    opRepair         // Verify and if possible repair damaged data files
  } Operation;

  typedef enum
  {
    verUnknown = 0,
    verPar1,         // Processing PAR 1.0 files
    verPar2          // Processing PAR 2.0 files
  } Version;

  typedef enum
  {
    scUnknown = 0,
    scVariable,      // Each PAR2 file will have 2x as many blocks as previous
    scLimited,       // Limit PAR2 file size
    scUniform        // All PAR2 files the same size
  } Scheme;

  typedef enum
  {
    nlUnknown = 0,
    nlSilent,       // Absolutely no output (other than errors)
    nlQuiet,        // Bare minimum of output
    nlNormal,       // Normal level of output
    nlNoisy,        // Lots of output
    nlDebug         // Extra debugging information
  } NoiseLevel;

  // Any extra files listed on the command line
  class ExtraFile
  {
  public:
    ExtraFile(void);
    ExtraFile(const ExtraFile&);
    ExtraFile& operator=(const ExtraFile&);

    ExtraFile(const string &name, u64 size);

    string FileName(void) const {return filename;}
    u64 FileSize(void) const {return filesize;}

  protected:
    string filename;
    u64    filesize;
  };

public:
  // Accessor functions for the command line parameters

  CommandLine::Operation GetOperation(void) const          {return operation;}
  CommandLine::Version   GetVersion(void) const            {return version;}
  u64                    GetBlockSize(void) const          {return blocksize;}
  u32                    GetBlockCount(void) const         {return blockcount;}
  float                  GetRedundancy(void) const         {return redundancy;}
  u32                    GetFirstRecoveryBlock(void) const {return firstblock;}
  u32                    GetRecoveryFileCount(void) const  {return recoveryfilecount;}
  u32                    GetRecoveryBlockCount(void) const {return recoveryblockcount;}
  CommandLine::Scheme    GetRecoveryFileScheme(void) const {return recoveryfilescheme;}
  size_t                 GetMemoryLimit(void) const        {return memorylimit;}
  u64                    GetLargestSourceSize(void) const  {return largestsourcesize;}
  u64                    GetTotalSourceSize(void) const    {return totalsourcesize;}
  CommandLine::NoiseLevel GetNoiseLevel(void) const        {return noiselevel;}

  // 2007/10/21 for hierarchial support:
  const string&          GetBaseDirectory(void) const      {return base_directory;}

#if WANT_CONCURRENT
  unsigned               GetConcurrentProcessingLevel(void) const { return concurrent_processing_level; }
#endif

  bool                   GetCreateDummyParFiles(void) const { return create_dummy_par_files; }

  string                              GetParFilename(void) const {return parfilename;}
  const list<CommandLine::ExtraFile>& GetExtraFiles(void) const  {return extrafiles;}

protected:
  Operation operation;         // The operation to be carried out.
  Version version;             // What version files will be processed.

  NoiseLevel noiselevel;       // How much display output should there be.

  u32 blockcount;              // How many blocks the source files should 
                               // be virtually split into.

  u64 blocksize;               // What virtual block size to use.

  u32 firstblock;              // What the exponent value for the first
                               // recovery block will be.

  Scheme recoveryfilescheme;   // How the the size of the recovery files should
                               // be calculated.

  u32 recoveryfilecount;       // How many recovery files should be created.

  u32 recoveryblockcount;      // How many recovery blocks should be created.
  bool recoveryblockcountset;  // Set if the recoveryblockcount as been specified

  float redundancy;            // What percentage of recovery data should
                               // be created.
  bool redundancyset;          // Set if the redundancy has been specified

  string parfilename;          // The name of the PAR2 file to create, or
                               // the name of the first PAR2 file to read
                               // when verifying or repairing.

  list<ExtraFile> extrafiles;  // The list of other files specified on the
                               // command line. When creating, this will be
                               // the source files, and when verifying or
                               // repairing, this will be additional PAR2
                               // files or data files to be examined.

  u64 totalsourcesize;         // Total size of the source files.

  u64 largestsourcesize;       // Size of the largest source file.

  size_t memorylimit;          // How much memory is permitted to be used
                               // for the output buffer when creating
                               // or repairing.

  // 2007/10/21 - added this to support hierarchial repair (eg, to protect a
  // hierarchy of folders of jpg files)

  // if non-empty then this is the base directory for paths stored in par2
  // files to support hierarchial repair
  string base_directory;

#if WANT_CONCURRENT
  // although it is possible to specify how many threads that TBB should
  // use, that functionality is not implemented (and the TBB documentation
  // recommends not using it in production code), so this is merely a bool:
  unsigned concurrent_processing_level; // whether to process serially or concurrently
#endif
  bool create_dummy_par_files; // so that final par2 size can be determined
};

typedef list<CommandLine::ExtraFile>::const_iterator ExtraFileIterator;

#endif // __COMMANDLINE_H__
