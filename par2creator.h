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

#ifndef __PAR2CREATOR_H__
#define __PAR2CREATOR_H__

class MainPacket;
class CreatorPacket;
class CriticalPacket;

class Par2Creator
{
public:
  Par2Creator(void);
  ~Par2Creator(void);

  // Create recovery files from the source files specified on the command line
  Result Process(const CommandLine &commandline);

protected:
  // Steps in the creation process:

#if WANT_CONCURRENT
public:
  void ProcessDataForOutputIndex(u32 outputstartindex, u32 outputendindex, size_t blocklength, u32 inputblock, buffer& ib);
  void ProcessDataConcurrently(size_t blocklength, u32 inputblock, buffer& ib);
  #if WANT_CONCURRENT_PAR2_FILE_OPENING
  Par2CreatorSourceFile* OpenSourceFile(const CommandLine::ExtraFile &extrafile);
  #endif
protected:
  void* OutputBufferAt(u32 outputindex);
  bool ProcessDataForOutputIndex_(u32 outputblock, u32 outputendblock, size_t blocklength, u32 inputblock, buffer& ib);
#endif

  // Compute block size from block count or vice versa depending on which was
  // specified on the command line
  bool ComputeBlockSizeAndBlockCount(const list<CommandLine::ExtraFile> &extrafiles);

  // Determine how many recovery blocks to create based on the source block
  // count and the requested level of redundancy.
  bool ComputeRecoveryBlockCount(float redundancy);

  // Determine how much recovery data can be computed on one pass
  bool CalculateProcessBlockSize(size_t memorylimit);

  // Determine how many recovery files to create.
  bool ComputeRecoveryFileCount(void);

  // Open all of the source files, compute the Hashes and CRC values, and store
  // the results in the file verification and file description packets.
  bool OpenSourceFiles(const list<CommandLine::ExtraFile> &extrafiles);

  // Create the main packet and determine the set_id_hash to use with all packets
  bool CreateMainPacket(void);

  // Create the creator packet.
  bool CreateCreatorPacket(void);

  // Initialise all of the source blocks ready to start reading data from the source files.
  bool CreateSourceBlocks(void);

  // Create all of the output files and allocate all packets to appropriate file offets.
  bool InitialiseOutputFiles(string par2filename);

  // Allocate memory buffers for reading and writing data to disk.
  bool AllocateBuffers(void);

  // Compute the Reed Solomon matrix
  bool ComputeRSMatrix(void);

  // Read source data, process it through the RS matrix and write it to disk.
  bool ProcessData(u64 blockoffset, size_t blocklength);

  // Finish computation of the recovery packets and write the headers to disk.
  bool WriteRecoveryPacketHeaders(void);

  // Finish computing the full file hash values of the source files
  bool FinishFileHashComputation(void);

  // Fill in all remaining details in the critical packets.
  bool FinishCriticalPackets(void);

  // Write all other critical packets to disk.
  bool WriteCriticalPackets(void);

  // Close all files.
  bool CloseFiles(void);

protected:
  CommandLine::NoiseLevel noiselevel; // How noisy we should be

  u64 blocksize;      // The size of each block.
  size_t chunksize;   // How much of each block will be processed at a 
                      // time (due to memory constraints).

//void *inputbuffer;  // chunksize
  void *outputbuffer; // chunksize * recoveryblockcount
  
  u32 sourcefilecount;   // Number of source files for which recovery data will be computed.
  u32 sourceblockcount;  // Total number of data blocks that the source files will be
                         // virtualy sliced into.

  u64 largestfilesize;   // The size of the largest source file

  CommandLine::Scheme recoveryfilescheme;  // What scheme will be used to select the
                                           // sizes for the recovery files.
  
  u32 recoveryfilecount;  // The number of recovery files that will be created
  u32 recoveryblockcount; // The number of recovery blocks that will be placed
                          // in the recovery files.

  u32 firstrecoveryblock; // The lowest exponent value to use for the recovery blocks.

  MainPacket    *mainpacket;    // The main packet
  CreatorPacket *creatorpacket; // The creator packet

  vector<Par2CreatorSourceFile*> sourcefiles;  // Array containing details of the source files
                                               // as well as the file verification and file
                                               // description packets for them.
  vector<DataBlock>          sourceblocks;     // Array with one entry for every source block.

  vector<DiskFile>           recoveryfiles;    // Array with one entry for every recovery file.
  vector<RecoveryPacket>     recoverypackets;  // Array with one entry for every recovery packet.

  list<CriticalPacket*>      criticalpackets;  // A list of all of the critical packets.
  list<CriticalPacketEntry>  criticalpacketentries; // A list of which critical packet will
                                                    // be written to which recovery file.

  ReedSolomon<Galois16> rs;   // The Reed Solomon matrix.

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
  u64 totaldata;    // Total amount of data to be processed.

  bool deferhashcomputation; // If we have enough memory to compute all recovery data
                             // in one pass, then we can defer the computation of
                             // the full file hash and block crc and hashes until
                             // the recovery data is computed.

#if WANT_CONCURRENT
  unsigned                  concurrent_processing_level;
  tbb::mutex                cout_mutex;
  tbb::atomic<u32>          cout_in_use; // this is used to display % done w/o blocking a thread
  tbb::tick_count           last_cout;   // when cout was used for output
#endif

  bool create_dummy_par_files;
};

#endif // __PAR2CREATOR_H__
