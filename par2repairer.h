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
//
//  Modifications for GPGPU support using nVidia CUDA technology are
//  Copyright (c) 2008 Vincent Tan.
//
//  Search for "#if WANT_CONCURRENT" for concurrent code.
//  Concurrent processing utilises Intel Thread Building Blocks 2.0,
//  Copyright (c) 2007 Intel Corp.
//
//  par2cmdline-0.4-tbb is available at http://chuchusoft.com/par2_tbb

#ifndef __PAR2REPAIRER_H__
#define __PAR2REPAIRER_H__

#define DSTOUT                0

#if WANT_CONCURRENT
  struct string_hasher {
    static  size_t  hash(const std::string& x) {
      size_t h = 0;
      for (const char* s = x.c_str(); *s; ++s)
        h = (h*17)^*s;
      return h;
    }
    static  bool  equal( const std::string& x, const std::string& y ) { return x == y; }
  };

  struct istring_hasher {
    static  size_t  hash(const std::string& x) {
      size_t h = 0;
      for (const char* s = x.c_str(); *s; ++s)
        h = (h*17)^ tolower(*s);
      return h;
    }
    static  bool  equal( const std::string& x, const std::string& y )
    { return x.length() == y.length() && 0 == stricmp(x.c_str(), y.c_str()); }
  };

  template <typename T>
  struct atomic_ptr : tbb::atomic<T> {
    // wow - C++ sometimes really is ugly...
    T  operator->(void) { return tbb::atomic<T>::operator typename tbb::atomic<T>::value_type(); }
    atomic_ptr<T>&  operator=(T t) { tbb::atomic<T>::operator=(t); return *this; }
  };

  class ConcurrentDiskFileMap {
  public:
  #if defined(WIN32) || defined(__APPLE_CC__)
    typedef tbb::concurrent_hash_map<string, DiskFile*, istring_hasher>  map_type;
  #else
    typedef tbb::concurrent_hash_map<string, DiskFile*, string_hasher>  map_type;
  #endif
    ConcurrentDiskFileMap(void) {}
    ~ConcurrentDiskFileMap(void) {
      map_type::iterator fi;
      for (fi = _diskfilemap.begin(); fi != _diskfilemap.end(); ++fi)
        delete (*fi).second;
    }

    bool  Insert(DiskFile *diskfile) {
      assert(!diskfile->FileName().empty());
      map_type::accessor  a;
      (bool) _diskfilemap.insert(a, diskfile->FileName());
      a->second = diskfile;
      return true;
    }
    void Remove(DiskFile *diskfile) {
      assert(!diskfile->FileName().empty());
      (bool) _diskfilemap.erase(diskfile->FileName());
    }
    DiskFile* Find(string filename) const {
      assert(!filename.empty());
      map_type::const_accessor  a;
      return _diskfilemap.find(a, filename) ?  a->second : NULL;
    }

  protected:
    map_type _diskfilemap;             // Map from filename to DiskFile
  };

  //#include  <ctype.h>
#endif

class Par2Repairer
{
public:
  Par2Repairer(void);
  ~Par2Repairer(void);

  Result Process(const CommandLine &commandline, bool dorepair);

protected:
  // Steps in verifying and repairing files:

#if WANT_CONCURRENT
public:
  #if WANT_CONCURRENT_SOURCE_VERIFICATION
  void VerifyOneSourceFile(Par2RepairerSourceFile *sourcefile, bool& finalresult);
  #endif
  void ProcessDataForOutputIndex(u32 outputstartindex, u32 outputendindex, size_t blocklength,
                                 u32 inputindex, buffer& inputbuffer);
  void ProcessDataConcurrently(size_t blocklength, u32 inputindex, buffer& inputbuffer);
#endif
  // Load packets from the specified file
  bool LoadPacketsFromFile(string filename);
#if WANT_CONCURRENT
protected:
  void* OutputBufferAt(u32 outputindex);
  bool ProcessDataForOutputIndex_(u32 outputindex, u32 outputendindex, size_t blocklength,
                                  u32 inputindex, buffer& inputbuffer);
#endif
  // Finish loading a recovery packet
  bool LoadRecoveryPacket(DiskFile *diskfile, u64 offset, PACKET_HEADER &header);
  // Finish loading a file description packet
  bool LoadDescriptionPacket(DiskFile *diskfile, u64 offset, PACKET_HEADER &header);
  // Finish loading a file verification packet
  bool LoadVerificationPacket(DiskFile *diskfile, u64 offset, PACKET_HEADER &header);
  // Finish loading the main packet
  bool LoadMainPacket(DiskFile *diskfile, u64 offset, PACKET_HEADER &header);
  // Finish loading the creator packet
  bool LoadCreatorPacket(DiskFile *diskfile, u64 offset, PACKET_HEADER &header);

  // Load packets from other PAR2 files with names based on the original PAR2 file
  bool LoadPacketsFromOtherFiles(string filename);

  // Load packets from any other PAR2 files whose names are given on the command line
  bool LoadPacketsFromExtraFiles(const list<CommandLine::ExtraFile> &extrafiles);

  // Check that the packets are consistent and discard any that are not
  bool CheckPacketConsistency(void);

  // Use the information in the main packet to get the source files
  // into the correct order and determine their filenames
  bool CreateSourceFileList(void);

  // Determine the total number of DataBlocks for the recoverable source files
  // The allocate the DataBlocks and assign them to each source file
  bool AllocateSourceBlocks(void);

  // Create a verification hash table for all files for which we have not
  // found a complete version of the file and for which we have
  // a verification packet
  bool PrepareVerificationHashTable(void);

  // Compute the table for the sliding CRC computation
  bool ComputeWindowTable(void);

  // Attempt to verify all of the source files
  bool VerifySourceFiles(void);

  // Scan any extra files specified on the command line
  bool VerifyExtraFiles(const list<CommandLine::ExtraFile> &extrafiles);

  // Attempt to match the data in the DiskFile with the source file
  bool VerifyDataFile(DiskFile *diskfile, Par2RepairerSourceFile *sourcefile);

  // Perform a sliding window scan of the DiskFile looking for blocks of data that 
  // might belong to any of the source files (for which a verification packet was
  // available). If a block of data might be from more than one source file, prefer
  // the one specified by the "sourcefile" parameter. If the first data block
  // found is for a different source file then "sourcefile" is changed accordingly.
  bool ScanDataFile(DiskFile                *diskfile,   // [in]     The file being scanned
                    Par2RepairerSourceFile* &sourcefile, // [in/out] The source file matched
                    MatchType               &matchtype,  // [out]    The type of match
                    MD5Hash                 &hashfull,   // [out]    The full hash of the file
                    MD5Hash                 &hash16k,    // [out]    The hash of the first 16k
                    u32                     &count);     // [out]    The number of blocks found

  // Find out how much data we have found
  void UpdateVerificationResults(void);

  // Check the verification results and report the results 
  bool CheckVerificationResults(void);

  // Rename any damaged or missnamed target files.
  bool RenameTargetFiles(void);

  // Work out which files are being repaired, create them, and allocate
  // target DataBlocks to them, and remember them for later verification.
  bool CreateTargetFiles(void);

  // Work out which data blocks are available, which need to be copied
  // directly to the output, and which need to be recreated, and compute
  // the appropriate Reed Solomon matrix.
  bool ComputeRSmatrix(void);

  // Allocate memory buffers for reading and writing data to disk.
  bool AllocateBuffers(size_t memorylimit);

  // Read source data, process it through the RS matrix and write it to disk.
  bool ProcessData(u64 blockoffset, size_t blocklength);

  // Verify that all of the reconstructed target files are now correct
  bool VerifyTargetFiles(void);

  // Delete all of the partly reconstructed files
  bool DeleteIncompleteTargetFiles(void);

protected:
  CommandLine::NoiseLevel   noiselevel;              // OnScreen display

  string                    searchpath;              // Where to find files on disk

  bool                      firstpacket;             // Whether or not a valid packet has been found.
  MD5Hash                   setid;                   // The SetId extracted from the first packet.
#if WANT_CONCURRENT
  tbb::concurrent_hash_map<u32, RecoveryPacket*, u32_hasher> recoverypacketmap;       // One recovery packet for each exponent value.
  ::atomic_ptr<MainPacket*>    mainpacket;           // One copy of the main packet.
  ::atomic_ptr<CreatorPacket*> creatorpacket;        // One copy of the creator packet.

  ConcurrentDiskFileMap     diskFileMap;
#else
  map<u32, RecoveryPacket*> recoverypacketmap;       // One recovery packet for each exponent value.
  MainPacket               *mainpacket;              // One copy of the main packet.
  CreatorPacket            *creatorpacket;           // One copy of the creator packet.

  DiskFileMap               diskFileMap;
#endif

  map<MD5Hash,Par2RepairerSourceFile*> sourcefilemap;// Map from FileId to SourceFile
  vector<Par2RepairerSourceFile*>      sourcefiles;  // The source files
  vector<Par2RepairerSourceFile*>      verifylist;   // Those source files that are being repaired

  u64                       blocksize;               // The block size.
  u64                       chunksize;               // How much of a block can be processed.
  u32                       sourceblockcount;        // The total number of blocks
  u32                       availableblockcount;     // How many undamaged blocks have been found
  u32                       missingblockcount;       // How many blocks are missing

  bool                      blocksallocated;         // Whether or not the DataBlocks have been allocated
  vector<DataBlock>         sourceblocks;            // The DataBlocks that will be read from disk
  vector<DataBlock>         targetblocks;            // The DataBlocks that will be written to disk

  u32                       windowtable[256];        // Table for sliding CRCs
  u32                       windowmask;              // Maks for sliding CRCs

  bool                            blockverifiable;         // Whether and files can be verified at the block level
  VerificationHashTable           verificationhashtable;   // Hash table for block verification
  list<Par2RepairerSourceFile*>   unverifiablesourcefiles; // Files that are not block verifiable

  u32                       completefilecount;       // How many files are fully verified
  u32                       renamedfilecount;        // How many files are verified but have the wrong name
  u32                       damagedfilecount;        // How many files exist but are damaged
  u32                       missingfilecount;        // How many files are completely missing

  vector<DataBlock*>        inputblocks;             // Which DataBlocks will be read from disk
  vector<DataBlock*>        copyblocks;              // Which DataBlocks will copied back to disk
  vector<DataBlock*>        outputblocks;            // Which DataBlocks have to calculated using RS

  ReedSolomon<Galois16>     rs;                      // The Reed Solomon matrix.

  void                     *outputbuffer;            // Buffer for writing DataBlocks (chunksize * missingblockcount)

#if WANT_CONCURRENT
  #if CONCURRENT_PIPELINE
  // low bit: which half of each entry in outputbuffer contains valid data (if DSTOUT is 1)
  // high bit: whether entry in outputbuffer is in use (0 = available, 1 = in-use)
  std::vector< tbb::atomic<int> > outputbuffer_element_state_; // state of each entry of outputbuffer
  size_t                   aligned_chunksize_;
  #else
  buffer                    inputbuffer;
//void                     *inputbuffer;             // Buffer for reading DataBlocks (chunksize)
  #endif

  // 32-bit PowerPC does not support tbb::atomic<u64> because it requires the ldarx
  // instruction which is only available for 64-bit PowerPC CPUs, so...
  #if __GNUC__ &&  __ppc__
  // this won't cause any data corruption - it will only cause (possibly) incorrect progress values to be printed
  u64                       progress;                // How much data has been processed.
  #else
  tbb::atomic<u64>          progress;                // How much data has been processed.
  #endif
#else
  buffer                    inputbuffer;
//void                     *inputbuffer;             // Buffer for reading DataBlocks (chunksize)
  #if DSTOUT
  std::vector<int>          outputbuffer_element_state_; // state of each entry of outputbuffer
  #endif

  u64                       progress;                // How much data has been processed.
#endif

  u64                       totaldata;               // Total amount of data to be processed.

#if WANT_CONCURRENT
  unsigned                  concurrent_processing_level;
  tbb::mutex                cout_mutex;
  tbb::atomic<u32>          cout_in_use;             // when repairing, this is used to display % done w/o blocking a thread
  tbb::tick_count           last_cout;   // when cout was used for output
#endif
};

#endif // __PAR2REPAIRER_H__
