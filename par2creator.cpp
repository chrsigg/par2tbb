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

#include "par2cmdline.h"
#include <backward/auto_ptr.h>

#if WANT_CONCURRENT
  #if CONCURRENT_PIPELINE
    class create_buffer : public pipeline_buffer {
      friend class create_pipeline_state;
      friend class create_filter_read;
      friend class create_filter_process;
      Par2CreatorSourceFile* sourcefile_;
      u32                    sourceindex_;
    };

    class create_pipeline_state : public pipeline_state<create_buffer> {
    private:
      friend class create_filter_read;
      friend class create_filter_process;

      vector<Par2CreatorSourceFile*>&              sourcefiles_;
      // If we have defered computation of the file hash and block crc and hashes
      // sourcefile and sourceindex will be used to update them during
      // the main recovery block computation
      vector<Par2CreatorSourceFile*>::iterator     sourcefile_;
      u32                                          sourceindex_;
      bool                                         deferhashcomputation_;

      // Computing the MD5 hashes for each source file requires that the buffers be hashed in the
      // correct order, which is not a certainty when the buffers are processed concurrently. To
      // ensure that they are correctly hashed, the next-index for hashing is remembered in
      // shm_value_type.first. If a buffer arrives out of order (ahead of when it should be used
      // for hashing) then its hashing is deferred by inserting it into the idx_to_buffer_type
      // hash_map and later used after the buffer corresponding to the next-index is processed.
      typedef tbb::concurrent_hash_map< u32, create_buffer*, intptr_hasher<u32> > idx_to_buffer_type;
      typedef std::pair<u32, idx_to_buffer_type>                                  shm_value_type;
      typedef tbb::concurrent_hash_map<Par2CreatorSourceFile*, shm_value_type, intptr_hasher<Par2CreatorSourceFile*> > shm_type;
      shm_type                                     shm_;

#ifndef NDEBUG
// only for checking that the hashing order is correct:
typedef tbb::concurrent_hash_map< Par2CreatorSourceFile*, tbb::concurrent_vector<u32>, intptr_hasher<Par2CreatorSourceFile*> > record_type;
record_type record_;
#endif

      void try_to_update_hashes(create_buffer* ib);

    public:
      create_pipeline_state(
        size_t                                     max_tokens,
        u64                                        chunksize,
        u32                                        missingblockcount,
        size_t                                     blocklength,
        u64                                        blockoffset,
        vector<DataBlock*>&                        inputblocks,
        vector<Par2CreatorSourceFile*>&            sourcefiles,
        bool                                       deferhashcomputation) :
        pipeline_state<create_buffer>(max_tokens, chunksize, missingblockcount, blocklength, blockoffset, inputblocks),
        sourcefiles_(sourcefiles), sourcefile_(sourcefiles.begin()), sourceindex_(0),
        deferhashcomputation_(deferhashcomputation) {}

#ifndef NDEBUG
~create_pipeline_state(void) {
  for (record_type::const_iterator it = record_.begin(); it != record_.end(); ++it)
    for (tbb::concurrent_vector<u32>::const_iterator jt = (*it).second.begin(); jt != (*it).second.end(); ++jt) {
      if (*jt != u32(jt - (*it).second.begin())) printf("%s: %u\n", (*it).first->get_diskfilename().c_str(), *jt);
      assert(*jt == u32(jt - (*it).second.begin()));
    }
}
#endif
    };

      void create_pipeline_state::try_to_update_hashes(create_buffer* ib) {
        if (!deferhashcomputation_)
          return;

        assert(blockoffset() == 0 /* && blocklength() == blocksize */);
        //assert(ib->sourcefile_ != sourcefiles_.end());

        shm_type::accessor a;
        idx_to_buffer_type::accessor ia;
        if (shm_.insert(a, ib->sourcefile_)) {
          a->second = shm_value_type(0, idx_to_buffer_type());
#ifndef NDEBUG
//printf("(%s 0) inserted into shm_\n", ib->sourcefile_->get_diskfilename().c_str());
#endif
        }

#ifndef NDEBUG
//printf("(%s %u) vs %u -> ", ib->sourcefile_->get_diskfilename().c_str(), ib->sourceindex_, a->second.first);
#endif
        if (a->second.first == ib->sourceindex_) {
//printf("immed\n");
          for (bool ib_needs_releasing = false; ; ) {
            ib->sourcefile_->UpdateHashes(ib->sourceindex_, ib->get(), blocklength());
#ifndef NDEBUG
{
record_type::accessor ra;
record_.insert(ra, ib->sourcefile_);
ra->second.push_back(ib->sourceindex_);
}
#endif
            const u32 bc = ib->sourcefile_->BlockCount();
            if (ib_needs_releasing)
              release(ib);

            const u32 idx = ++a->second.first;
//printf("next_idx = %u\n", idx);
            if (idx == bc) {
              // there should be no buffers waiting to be hashed:
              assert(a->second.second.size() == 0);
              //assert(!a->second.second.find(ia, ib->sourceindex_));
#ifndef NDEBUG
              assert(shm_.erase(a));
#else
              (bool) shm_.erase(a);
#endif
              break;
            } else if (a->second.second.find(ia, idx)) { // can any deferred buffers now be used up?
//printf("found %u in deferred_list\n", idx);
              ib = ia->second;
              ib_needs_releasing = true;

              a->second.second.erase(ia);
            } else {
//printf("did not find %u in deferred_list\n", idx);
              break;
            }
          } // for
        } else if (a->second.first < ib->sourceindex_) { // buffer cannot be used for hash yet - defer its use until correct time
//printf("deferred (buffer %u)\n", ib->id_);
#ifndef NDEBUG
//if (a->second.second.find(ia, ib->sourceindex_)) printf("(%s %u) already in deferred_list\n", ib->sourcefile_->get_diskfilename().c_str(), ib->sourceindex_);
#endif
//assert(!a->second.second.find(ia, ib->sourceindex_) || ia->second == ib);
          assert(!a->second.second.find(ia, ib->sourceindex_));

          if (a->second.second.insert(ia, ib->sourceindex_)) {
#ifndef NDEBUG
//printf("inserted (%s %u) into deferred_list\n", ib->sourcefile_->get_diskfilename().c_str(), ib->sourceindex_);
#endif
            add_ref(*ib);
            ia->second = ib;
          }

        } else {
//printf("ignored\n");
        }
      }

    class create_filter_read : public filter_read_base<create_filter_read, create_buffer> {
    public:
      create_filter_read(pipeline_state<create_buffer>& s) : filter_read_base<create_filter_read, create_buffer>(s) {}

      void on_mutex_held(create_buffer* ib) {
        create_pipeline_state& s = static_cast<create_pipeline_state&> (state_);

        if (!s.deferhashcomputation_) return;

        ib->sourcefile_ = *s.sourcefile_;
        ib->sourceindex_ = s.sourceindex_;

        if (s.sourcefile_ != s.sourcefiles_.end()) {
          // Work out which source file the next block belongs to
          if (++s.sourceindex_ >= (*s.sourcefile_)->BlockCount()) {
            s.sourceindex_ = 0;
            ++s.sourcefile_;
          }
        }
      }

      bool on_inputbuffer_read(create_buffer* /* ib */) {
        // The last buffer for a sourcefile causes a 0 entry to be inserted into shm_ if
        // try_to_update_hashes() is called more than once for same buffer. The easiest
        // solution is to not call try_to_update_hashes() here.
        //static_cast<create_pipeline_state&> (state_).try_to_update_hashes(ib);
        return true;
      }
    };

    class create_filter_process : public filter_process_base<create_filter_process, create_buffer, Par2Creator> {
    public:
      create_filter_process(Par2Creator& delegate, pipeline_state<create_buffer>& s) :
        filter_process_base<create_filter_process, create_buffer, Par2Creator>(delegate, s) {}

      virtual void* operator()(void* ib) {
        // try_to_update_hashes() should be called first because if the superclass is first called, it will
        // call ib->release() which will make ib invalid for the call to try_to_update_hashes():
        static_cast<create_pipeline_state&> (state_).try_to_update_hashes(static_cast<create_buffer*> (ib));
        return filter_process_base<create_filter_process, create_buffer, Par2Creator>::operator()(ib);
      }
    };

  #endif
#endif

#ifdef _MSC_VER
#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif
#endif

#if defined(WIN64)
namespace std {
  using ::memset;
}
#endif

Par2Creator::Par2Creator(void)
: noiselevel(CommandLine::nlUnknown)
, blocksize(0)
, chunksize(0)
, outputbuffer(0)
#if WANT_CONCURRENT && CONCURRENT_PIPELINE
#else
//, inputbuffer(0)
#endif

, sourcefilecount(0)
, sourceblockcount(0)

, largestfilesize(0)
, recoveryfilescheme(CommandLine::scUnknown)
, recoveryfilecount(0)
, recoveryblockcount(0)
, firstrecoveryblock(0)

, mainpacket(0)
, creatorpacket(0)

, deferhashcomputation(false)

#if WANT_CONCURRENT
, concurrent_processing_level(ALL_CONCURRENT)
, last_cout(tbb::tick_count::now())
#endif
, create_dummy_par_files(false)
{
#if WANT_CONCURRENT
  cout_in_use = 0;
#endif
}

Par2Creator::~Par2Creator(void)
{
  delete mainpacket;
  delete creatorpacket;

#if WANT_CONCURRENT && CONCURRENT_PIPELINE && GPGPU_CUDA
  cuda::DeallocateResources();
#endif

#if WANT_CONCURRENT && CONCURRENT_PIPELINE
  if (outputbuffer)
    tbb::cache_aligned_allocator<u8>().deallocate((u8*)outputbuffer, 0);
#else
//delete [] (u8*)inputbuffer;
  delete [] (u8*)outputbuffer;
#endif

  vector<Par2CreatorSourceFile*>::iterator sourcefile = sourcefiles.begin();
  while (sourcefile != sourcefiles.end())
  {
    delete *sourcefile;
    ++sourcefile;
  }
}

Result Par2Creator::Process(const CommandLine &commandline)
{
  // Get information from commandline
  noiselevel = commandline.GetNoiseLevel();
  blocksize = commandline.GetBlockSize();
  sourceblockcount = commandline.GetBlockCount();
  const list<CommandLine::ExtraFile> &extrafiles = commandline.GetExtraFiles();
  sourcefilecount = (u32)extrafiles.size();
  float redundancy = commandline.GetRedundancy();
  recoveryblockcount = commandline.GetRecoveryBlockCount();
  recoveryfilecount = commandline.GetRecoveryFileCount();
  firstrecoveryblock = commandline.GetFirstRecoveryBlock();
  recoveryfilescheme = commandline.GetRecoveryFileScheme();
  string par2filename = commandline.GetParFilename();
  size_t memorylimit = commandline.GetMemoryLimit();
  largestfilesize = commandline.GetLargestSourceSize();

#if WANT_CONCURRENT
  concurrent_processing_level = commandline.GetConcurrentProcessingLevel();
  if (noiselevel > CommandLine::nlQuiet) {
    cout << "Processing ";
    if (ALL_SERIAL == concurrent_processing_level)
      cout << "checksums and Reed-Solomon data serially.";
    else if (CHECKSUM_SERIALLY_BUT_PROCESS_CONCURRENTLY == concurrent_processing_level)
      cout << "checksums serially and Reed-Solomon data concurrently.";
    else if (ALL_CONCURRENT == concurrent_processing_level)
      cout << "checksums and Reed-Solomon data concurrently.";
    else
      return eLogicError;
    cout << endl;
  }
#endif
  create_dummy_par_files = commandline.GetCreateDummyParFiles();

  // Compute block size from block count or vice versa depending on which was
  // specified on the command line
  if (!ComputeBlockSizeAndBlockCount(extrafiles))
    return eInvalidCommandLineArguments;

  // Determine how many recovery blocks to create based on the source block
  // count and the requested level of redundancy.
  if (redundancy > 0 && !ComputeRecoveryBlockCount(redundancy))
    return eInvalidCommandLineArguments;

  // Determine how much recovery data can be computed on one pass
  if (!CalculateProcessBlockSize(memorylimit))
    return eLogicError;

  // Determine how many recovery files to create.
  if (!ComputeRecoveryFileCount())
    return eInvalidCommandLineArguments;

  if (noiselevel > CommandLine::nlQuiet)
  {
    // Display information.
    cout << "Block size: " << blocksize << endl;
    cout << "Source file count: " << sourcefilecount << endl;
    cout << "Source block count: " << sourceblockcount << endl;
    if (redundancy>0 || recoveryblockcount==0)
      cout << "Redundancy: " << redundancy << '%' << endl;
    cout << "Recovery block count: " << recoveryblockcount << endl;
    cout << "Recovery file count: " << recoveryfilecount << endl;
    cout << endl;
  }

  // Open all of the source files, compute the Hashes and CRC values, and store
  // the results in the file verification and file description packets.
  if (!OpenSourceFiles(extrafiles))
    return eFileIOError;

  // Create the main packet and determine the setid to use with all packets
  if (!CreateMainPacket())
    return eLogicError;

  // Create the creator packet.
  if (!CreateCreatorPacket())
    return eLogicError;

  // Initialise all of the source blocks ready to start reading data from the source files.
  if (!CreateSourceBlocks())
    return eLogicError;

  // Create all of the output files and allocate all packets to appropriate file offets.
  if (!InitialiseOutputFiles(par2filename))
    return eFileIOError;

  if (!create_dummy_par_files && recoveryblockcount > 0)
  {
    // Allocate memory buffers for reading and writing data to disk.
    if (!AllocateBuffers())
      return eMemoryError;

    // Compute the Reed Solomon matrix
    if (!ComputeRSMatrix())
      return eLogicError;

    // Set the total amount of data to be processed.
    progress = 0;
    totaldata = blocksize * sourceblockcount * recoveryblockcount;

    // Start at an offset of 0 within a block.
    u64 blockoffset = 0;
    while (blockoffset < blocksize) // Continue until the end of the block.
    {
      // Work out how much data to process this time.
      size_t blocklength = (size_t)min((u64)chunksize, blocksize-blockoffset);

      // Read source data, process it through the RS matrix and write it to disk.
      if (!ProcessData(blockoffset, blocklength))
        return eFileIOError;

      blockoffset += blocklength;
    }

    if (noiselevel > CommandLine::nlQuiet)
      cout << "Writing recovery packets" << endl;

    // Finish computation of the recovery packets and write the headers to disk.
    if (!WriteRecoveryPacketHeaders())
      return eFileIOError;

    // Finish computing the full file hash values of the source files
    if (!FinishFileHashComputation())
      return eLogicError;
  }

  if (!create_dummy_par_files) {
    // Fill in all remaining details in the critical packets.
    if (!FinishCriticalPackets())
      return eLogicError;

    if (noiselevel > CommandLine::nlQuiet)
      cout << "Writing verification packets" << endl;

    // Write all other critical packets to disk.
    if (!WriteCriticalPackets())
      return eFileIOError;
  }

  // Close all files.
  if (!CloseFiles())
    return eFileIOError;

  if (noiselevel > CommandLine::nlSilent) {
    cout << "Done" << endl;
    if (create_dummy_par_files)
      cout << "WARNING: the par2 files do NOT contain valid verification data" << endl <<
              "(because dummy par file creation was requested)." << endl;
  }

  return eSuccess;
}

// Compute block size from block count or vice versa depending on which was
// specified on the command line
bool Par2Creator::ComputeBlockSizeAndBlockCount(const list<CommandLine::ExtraFile> &extrafiles)
{
  // Determine blocksize from sourceblockcount or vice-versa
  if (blocksize > 0)
  {
    u64 count = 0;

    for (ExtraFileIterator i=extrafiles.begin(); i!=extrafiles.end(); i++)
    {
      count += (i->FileSize() + blocksize-1) / blocksize;
    }

    if (count > 32768)
    {
      cerr << "Block size is too small. It would require " << count << "blocks." << endl;
      return false;
    }

    sourceblockcount = (u32)count;
  }
  else if (sourceblockcount > 0)
  {
    if (sourceblockcount < extrafiles.size())
    {
      // The block count cannot be less that the number of files.

      cerr << "Block count of " << sourceblockcount << " is too small: it must be at least " << extrafiles.size() << "." << endl; // 2008/06/27
      return false;
    }
    else if (sourceblockcount == extrafiles.size())
    {
      // If the block count is the same as the number of files, then the block
      // size is the size of the largest file (rounded up to a multiple of 4).

      u64 largestsourcesize = 0;
      for (ExtraFileIterator i=extrafiles.begin(); i!=extrafiles.end(); i++)
      {
        if (largestsourcesize < i->FileSize())
        {
          largestsourcesize = i->FileSize();
        }
      }

      blocksize = (largestsourcesize + 3) & ~3;
    }
    else
    {
      u64 totalsize = 0;
      for (ExtraFileIterator i=extrafiles.begin(); i!=extrafiles.end(); i++)
      {
        totalsize += (i->FileSize() + 3) / 4;
      }

      if (sourceblockcount > totalsize)
      {
        sourceblockcount = (u32)totalsize;
        blocksize = 4;
      }
      else
      {
        // Absolute lower bound and upper bound on the source block size that will
        // result in the requested source block count.
        u64 lowerBound = totalsize / sourceblockcount;
        u64 upperBound = (totalsize + sourceblockcount - extrafiles.size() - 1) / (sourceblockcount - extrafiles.size());

        u64 bestsize = lowerBound;
        u64 bestdistance = 1000000;
        u64 bestcount = 0;

        u64 count;
        u64 size;

        // Work out how many blocks you get for the lower bound block size
        {
          size = lowerBound;

          count = 0;
          for (ExtraFileIterator i=extrafiles.begin(); i!=extrafiles.end(); i++)
          {
            count += ((i->FileSize()+3)/4 + size-1) / size;
          }

          if (bestdistance > (count>sourceblockcount ? count-sourceblockcount : sourceblockcount-count))
          {
            bestdistance = (count>sourceblockcount ? count-sourceblockcount : sourceblockcount-count);
            bestcount = count;
            bestsize = size;
          }
        }

        // Work out how many blocks you get for the upper bound block size
        {
          size = upperBound;

          count = 0;
          for (ExtraFileIterator i=extrafiles.begin(); i!=extrafiles.end(); i++)
          {
            count += ((i->FileSize()+3)/4 + size-1) / size;
          }

          if (bestdistance > (count>sourceblockcount ? count-sourceblockcount : sourceblockcount-count))
          {
            bestdistance = (count>sourceblockcount ? count-sourceblockcount : sourceblockcount-count);
            bestcount = count;
            bestsize = size;
          }
        }

        // Use binary search to find best block size
        while (lowerBound+1 < upperBound)
        {
          size = (lowerBound + upperBound)/2;

          count = 0;
          for (ExtraFileIterator i=extrafiles.begin(); i!=extrafiles.end(); i++)
          {
            count += ((i->FileSize()+3)/4 + size-1) / size;
          }

          if (bestdistance > (count>sourceblockcount ? count-sourceblockcount : sourceblockcount-count))
          {
            bestdistance = (count>sourceblockcount ? count-sourceblockcount : sourceblockcount-count);
            bestcount = count;
            bestsize = size;
          }

          if (count < sourceblockcount)
          {
            upperBound = size;
          }
          else if (count > sourceblockcount)
          {
            lowerBound = size;
          }
          else
          {
            upperBound = size;
          }
        }

        size = bestsize;
        count = bestcount;

        if (count > 32768)
        {
          cerr << "Error calculating block size." << endl;
          return false;
        }

        sourceblockcount = (u32)count;
        blocksize = size*4;
      }
    }
  }

  return true;
}


// Determine how many recovery blocks to create based on the source block
// count and the requested level of redundancy.
bool Par2Creator::ComputeRecoveryBlockCount(float redundancy)
{
  // Determine recoveryblockcount
  recoveryblockcount = (u32) ((sourceblockcount * redundancy + 50.0f) / 100.0f);

  // Force valid values if necessary
  if (recoveryblockcount == 0 && redundancy > 0)
    recoveryblockcount = 1;

  if (recoveryblockcount > 65536)
  {
    cerr << "Too many recovery blocks requested: maximum is 65536 but ended up with " << recoveryblockcount << "." << endl; // 2008/06/27
    return false;
  }

  // Check that the last recovery block number would not be too large
  if (firstrecoveryblock + recoveryblockcount >= 65536)
  {
    cerr << "First recovery block number is too high." << endl;
    return false;
  }

  return true;
}

// Determine how much recovery data can be computed on one pass
bool Par2Creator::CalculateProcessBlockSize(size_t memorylimit)
{
  // Are we computing any recovery blocks
  if (recoveryblockcount == 0)
  {
    deferhashcomputation = false;
  }
  else
  {
    // Would single pass processing use too much memory
    if (blocksize * recoveryblockcount > memorylimit)
    {
      // Pick a size that is small enough
      chunksize = ~3 & (memorylimit / recoveryblockcount);

      deferhashcomputation = false;
    }
    else
    {
      chunksize = (size_t)blocksize;

      deferhashcomputation = true;
    }
  }

  return true;
}

// Determine how many recovery files to create.
bool Par2Creator::ComputeRecoveryFileCount(void)
{
  // Are we computing any recovery blocks
  if (recoveryblockcount == 0)
  {
    recoveryfilecount = 0;
    return true;
  }
 
  switch (recoveryfilescheme)
  {
  case CommandLine::scUnknown:
    {
      assert(false);
      return false;
    }
    break;
  case CommandLine::scVariable:
  case CommandLine::scUniform:
    {
      if (recoveryfilecount == 0)
      {
        // If none specified then then filecount is roughly log2(blockcount)
        // This prevents you getting excessively large numbers of files
        // when the block count is high and also allows the files to have
        // sizes which vary exponentially.

        for (u32 blocks=recoveryblockcount; blocks>0; blocks>>=1)
        {
          recoveryfilecount++;
        }
      }
  
      if (recoveryfilecount > recoveryblockcount)
      {
        // You cannot have move recovery files that there are recovery blocks
        // to put in them.
        cerr << "Too many recovery files specified." << endl;
        return false;
      }
    }
    break;

  case CommandLine::scLimited:
    {
      // No recovery file will contain more recovery blocks than would
      // be required to reconstruct the largest source file if it
      // were missing. Other recovery files will have recovery blocks
      // distributed in an exponential scheme.

      u32 largest = (u32)((largestfilesize + blocksize-1) / blocksize);
      u32 whole = recoveryblockcount / largest;
      whole = (whole >= 1) ? whole-1 : 0;

      u32 extra = recoveryblockcount - whole * largest;
      recoveryfilecount = whole;
      for (u32 blocks=extra; blocks>0; blocks>>=1)
      {
        recoveryfilecount++;
      }
    }
    break;
  }

  return true;
}


#if WANT_CONCURRENT_PAR2_FILE_OPENING

Par2CreatorSourceFile* Par2Creator::OpenSourceFile(const CommandLine::ExtraFile &extrafile)
{
    std::auto_ptr<Par2CreatorSourceFile> sourcefile(new Par2CreatorSourceFile);

    if (noiselevel > CommandLine::nlSilent) {
      string  name(utf8_string_to_cout_parameter(CommandLine::FileOrPathForCout(
                   extrafile.FileName())));

      tbb::mutex::scoped_lock l(cout_mutex);
      cout << "Opening: " << name;
      if (create_dummy_par_files)
        cout << "\r";
      else
        cout << endl;
    }

    // Open the source file and compute its Hashes and CRCs.
    if (!sourcefile->Open(noiselevel, extrafile, blocksize, deferhashcomputation, cout_mutex, last_cout)) {
      string  name(utf8_string_to_cout_parameter(CommandLine::FileOrPathForCout(
                   extrafile.FileName())));

      tbb::mutex::scoped_lock l(cout_mutex);
      cerr << "error: could not open '" << name << "'" << endl;
      return NULL;
    }

    // Record the file verification and file description packets
    // in the critical packet list.
    sourcefile->RecordCriticalPackets(criticalpackets);

    // Add the source file to the sourcefiles array.
    //sourcefiles.push_back(sourcefile);

    // Close the source file until its needed
    sourcefile->Close();

    return sourcefile.release();
}

  #if 1

  class pipeline_state_open_source_file {
  public:
    pipeline_state_open_source_file(const vector<CommandLine::ExtraFile>& files,
      tbb::concurrent_vector<Par2CreatorSourceFile*>& res, Par2Creator& delegate) :
      files_(files), res_(res), delegate_(delegate), ok_(true) { idx_ = 0; }

    const vector<CommandLine::ExtraFile>&           files(void) const { return files_; }
    tbb::concurrent_vector<Par2CreatorSourceFile*>& res(void) const { return res_; }
    Par2Creator&                                    delegate(void) const { return delegate_; }
    unsigned                                        add_idx(int delta) { return idx_ += delta; }

    bool                                            is_ok(void) const { return ok_; }
    void                                            set_not_ok(void) { ok_ = false; }

  private:
    const vector<CommandLine::ExtraFile>&           files_;
    tbb::concurrent_vector<Par2CreatorSourceFile*>& res_;
    Par2Creator&                                    delegate_;
    tbb::atomic<unsigned>                           idx_;

    bool                                            ok_; // if an error or failure occurs then this becomes false
  };

  class filter_open_source_file : public tbb::filter {
  private:
    filter_open_source_file& operator=(const filter_open_source_file&); // assignment disallowed
  protected:
    pipeline_state_open_source_file& state_;
  public:
    filter_open_source_file(pipeline_state_open_source_file& s) :
      tbb::filter(false /* tbb::filter::parallel */), state_(s) {}
    virtual void* operator()(void*);
  };

  //virtual
  void* filter_open_source_file::operator()(void*) {
    if (!state_.is_ok())
        return NULL; // abort

    unsigned idx = state_.add_idx(1) - 1;
    const vector<CommandLine::ExtraFile>& files = state_.files();
    if (idx >= files.size()) {
      state_.add_idx(-1);
      return NULL; // done
    }

    Par2CreatorSourceFile* sf = state_.delegate().OpenSourceFile(files[idx]);
    if (!sf)
      state_.set_not_ok();
    else
      state_.res().push_back(sf);

    return this; // tell tbb::pipeline that there is more to process
  }

  #else

  class ApplyOpenSourceFile {
    Par2Creator* const _obj;
    const std::vector<CommandLine::ExtraFile>& _extrafiles;
    tbb::concurrent_vector<Par2CreatorSourceFile*>& _res;
    tbb::atomic<u32>& _ok;
  public:
    void operator()( const tbb::blocked_range<size_t>& r ) const {
      Par2Creator* obj = _obj;
      const std::vector<CommandLine::ExtraFile>& extrafiles = _extrafiles;
      tbb::concurrent_vector<Par2CreatorSourceFile*>& res = _res;
      tbb::atomic<u32>& ok = _ok;
      for ( size_t it = r.begin(); ok && it != r.end(); ++it ) {
        Par2CreatorSourceFile* sf = obj->OpenSourceFile(extrafiles[it]);
        if (!sf) {
          ok = false;
          break;
        } else
          res.push_back(sf);
      }
    }

    ApplyOpenSourceFile(Par2Creator* obj, const std::vector<CommandLine::ExtraFile>& v,
      tbb::concurrent_vector<Par2CreatorSourceFile*>& res, tbb::atomic<u32>& ok) :
      _obj(obj), _extrafiles(v), _res(res), _ok(ok) { _ok = true; }
  };

  #endif

#endif


// Open all of the source files, compute the Hashes and CRC values, and store
// the results in the file verification and file description packets.
bool Par2Creator::OpenSourceFiles(const list<CommandLine::ExtraFile> &extrafiles)
{
#if WANT_CONCURRENT_PAR2_FILE_OPENING
  if (ALL_CONCURRENT == concurrent_processing_level) {
    std::vector<CommandLine::ExtraFile> v;
    std::copy(extrafiles.begin(), extrafiles.end(), std::back_inserter(v));

    tbb::concurrent_vector<Par2CreatorSourceFile*> res;
  #if 1
    pipeline_state_open_source_file                s(v, res, *this);
    tbb::pipeline                                  p;
    filter_open_source_file                        fosf(s);
    p.add_filter(fosf);
    p.run(tbb::task_scheduler_init::default_num_threads());
    bool                                           ok = s.is_ok();
  #else
    tbb::parallel_for(tbb::blocked_range<size_t>(0, v.size()), ::ApplyOpenSourceFile(this, v, res, ok));
  #endif
    if (!ok)
      return false;
    std::copy(res.begin(), res.end(), std::back_inserter(sourcefiles));
  } else for (ExtraFileIterator extrafile = extrafiles.begin(); extrafile != extrafiles.end(); ++extrafile) {
    Par2CreatorSourceFile* sourcefile = OpenSourceFile(*extrafile);
    if (!sourcefile)
      return false;
    sourcefiles.push_back(sourcefile);
  }
#else
  ExtraFileIterator extrafile = extrafiles.begin();
  while (extrafile != extrafiles.end())
  {
    Par2CreatorSourceFile *sourcefile = new Par2CreatorSourceFile;

    if (noiselevel > CommandLine::nlSilent) {
      string  name(utf8_string_to_cout_parameter(CommandLine::FileOrPathForCout(
                   extrafile->FileName())));

      cout << "Opening: " << name << endl;
    }

    // Open the source file and compute its Hashes and CRCs.
    if (!sourcefile->Open(noiselevel, *extrafile, blocksize, deferhashcomputation)) {
      delete sourcefile;
      return false;
    }

    // Record the file verification and file description packets
    // in the critical packet list.
    sourcefile->RecordCriticalPackets(criticalpackets);

    // Add the source file to the sourcefiles array.
    sourcefiles.push_back(sourcefile);

    // Close the source file until its needed
    sourcefile->Close();

    ++extrafile;
  }
#endif

  if (noiselevel > CommandLine::nlSilent)
    cout << endl;

  return true;
}

// Create the main packet and determine the setid to use with all packets
bool Par2Creator::CreateMainPacket(void)
{
  // Construct the main packet from the list of source files and the block size.
  mainpacket = new MainPacket;

  // Add the main packet to the list of critical packets.
  criticalpackets.push_back(mainpacket);

  // Create the packet (sourcefiles will get sorted into FileId order).
  return mainpacket->Create(sourcefiles, blocksize);
}

// Create the creator packet.
bool Par2Creator::CreateCreatorPacket(void)
{
  // Construct the creator packet
  creatorpacket = new CreatorPacket;

  // Create the packet
  return creatorpacket->Create(mainpacket->SetId());
}

// Initialise all of the source blocks ready to start reading data from the source files.
bool Par2Creator::CreateSourceBlocks(void)
{
  // Allocate the array of source blocks
  sourceblocks.resize(sourceblockcount);

  vector<DataBlock>::iterator sourceblock = sourceblocks.begin();
  
  for (vector<Par2CreatorSourceFile*>::iterator sourcefile = sourcefiles.begin();
       sourcefile!= sourcefiles.end();
       sourcefile++)
  {
    // Allocate the appopriate number of source blocks to each source file.
    // sourceblock will be advanced.

    (*sourcefile)->InitialiseSourceBlocks(sourceblock, blocksize);
  }

  return true;
}

class FileAllocation
{
public:
  FileAllocation(void) 
  {
    filename = "";
    exponent = 0;
    count = 0;
  }

  string filename;
  u32 exponent;
  u32 count;
};

// Create all of the output files and allocate all packets to appropriate file offets.
bool Par2Creator::InitialiseOutputFiles(string par2filename)
{
  // Allocate the recovery packets
  recoverypackets.resize(recoveryblockcount);

  // Choose filenames and decide which recovery blocks to place in each file
  vector<FileAllocation> fileallocations;
  fileallocations.resize(recoveryfilecount+1); // One extra file with no recovery blocks
  {
    // Decide how many recovery blocks to place in each file
    u32 exponent = firstrecoveryblock;
    if (recoveryfilecount > 0)
    {
      switch (recoveryfilescheme)
      {
      case CommandLine::scUnknown:
        {
          assert(false);
          return false;
        }
        break;
      case CommandLine::scUniform:
        {
          // Files will have roughly the same number of recovery blocks each.

          u32 base      = recoveryblockcount / recoveryfilecount;
          u32 remainder = recoveryblockcount % recoveryfilecount;

          for (u32 filenumber=0; filenumber<recoveryfilecount; filenumber++)
          {
            fileallocations[filenumber].exponent = exponent;
            fileallocations[filenumber].count = (filenumber<remainder) ? base+1 : base;
            exponent += fileallocations[filenumber].count;
          }
        }
        break;

      case CommandLine::scVariable:
        {
          // Files will have recovery blocks allocated in an exponential fashion.

          // Work out how many blocks to place in the smallest file
          u32 lowblockcount = 1;
          u32 maxrecoveryblocks = (1 << recoveryfilecount) - 1;
          while (maxrecoveryblocks < recoveryblockcount)
          {
            lowblockcount <<= 1;
            maxrecoveryblocks <<= 1;
          }

          // Allocate the blocks.
          u32 blocks = recoveryblockcount;
          for (u32 filenumber=0; filenumber<recoveryfilecount; filenumber++)
          {
            u32 number = min(lowblockcount, blocks);
            fileallocations[filenumber].exponent = exponent;
            fileallocations[filenumber].count = number;
            exponent += number;
            blocks -= number;
            lowblockcount <<= 1;
          }
        }
        break;

      case CommandLine::scLimited:
        {
          // Files will be allocated in an exponential fashion but the
          // Maximum file size will be limited.

          u32 largest = (u32)((largestfilesize + blocksize-1) / blocksize);
          u32 filenumber = recoveryfilecount;
          u32 blocks = recoveryblockcount;
         
          exponent = firstrecoveryblock + recoveryblockcount;

          // Allocate uniformly at the top
          while (blocks >= 2*largest && filenumber > 0)
          {
            filenumber--;
            exponent -= largest;
            blocks -= largest;

            fileallocations[filenumber].exponent = exponent;
            fileallocations[filenumber].count = largest;
          }
          assert(blocks > 0 && filenumber > 0);

          exponent = firstrecoveryblock;
          u32 count = 1;
          u32 files = filenumber;

          // Allocate exponentially at the bottom
          for (filenumber=0; filenumber<files; filenumber++)
          {
            u32 number = min(count, blocks);
            fileallocations[filenumber].exponent = exponent;
            fileallocations[filenumber].count = number;

            exponent += number;
            blocks -= number;
            count <<= 1;
          }
        }
        break;
      }
    }
     
     // There will be an extra file with no recovery blocks.
    fileallocations[recoveryfilecount].exponent = exponent;
    fileallocations[recoveryfilecount].count = 0;

    // Determine the format to use for filenames of recovery files
    char filenameformat[300];
    {
      u32 limitLow = 0;
      u32 limitCount = 0;
      for (u32 filenumber=0; filenumber<=recoveryfilecount; filenumber++)
      {
        if (limitLow < fileallocations[filenumber].exponent)
        {
          limitLow = fileallocations[filenumber].exponent;
        }
        if (limitCount < fileallocations[filenumber].count)
        {
          limitCount = fileallocations[filenumber].count;
        }
      }

      u32 digitsLow = 1;
      for (u32 t=limitLow; t>=10; t/=10)
      {
        digitsLow++;
      }
      
      u32 digitsCount = 1;
      for (u32 t=limitCount; t>=10; t/=10)
      {
        digitsCount++;
      }

      sprintf(filenameformat, "%%s.vol%%0%dd+%%0%dd.par2", digitsLow, digitsCount);
    }

    // Set the filenames
    for (u32 filenumber=0; filenumber<recoveryfilecount; filenumber++)
    {
      char filename[300];
      snprintf(filename, sizeof(filename), filenameformat, par2filename.c_str(), fileallocations[filenumber].exponent, fileallocations[filenumber].count);
      fileallocations[filenumber].filename = filename;
    }
    fileallocations[recoveryfilecount].filename = par2filename + ".par2";
  }

  // Allocate the recovery files
  {
    recoveryfiles.resize(recoveryfilecount+1);

    // Allocate packets to the output files
    {
      const MD5Hash &setid = mainpacket->SetId();
      vector<RecoveryPacket>::iterator recoverypacket = recoverypackets.begin();

      vector<DiskFile>::iterator recoveryfile = recoveryfiles.begin();
      vector<FileAllocation>::iterator fileallocation = fileallocations.begin();

      // For each recovery file:
      while (recoveryfile != recoveryfiles.end())
      {
        // How many recovery blocks in this file
        u32 count = fileallocation->count;

        // start at the beginning of the recovery file
        u64 offset = 0;

        if (count == 0)
        {
          // Write one set of critical packets
          list<CriticalPacket*>::const_iterator nextCriticalPacket = criticalpackets.begin();

          while (nextCriticalPacket != criticalpackets.end())
          {
            criticalpacketentries.push_back(CriticalPacketEntry(&*recoveryfile, 
                                                                offset, 
                                                                *nextCriticalPacket));
            offset += (*nextCriticalPacket)->PacketLength();

            ++nextCriticalPacket;
          }
        }
        else
        {
          // How many copies of each critical packet
          u32 copies = 0;
          for (u32 t=count; t>0; t>>=1)
          {
            copies++;
          }

          // Get ready to iterate through the critical packets
          u32 packetCount = 0;
          list<CriticalPacket*>::const_iterator nextCriticalPacket = criticalpackets.end();

          // What is the first exponent
          u32 exponent = fileallocation->exponent;

          // Start allocating the recovery packets
          u32 limit = exponent + count;
          while (exponent < limit)
          {
            // Add the next recovery packet
            recoverypacket->Create(&*recoveryfile, offset, blocksize, exponent, setid);

            offset += recoverypacket->PacketLength();
            ++recoverypacket;
            ++exponent;

            // Add some critical packets
            packetCount += copies * criticalpackets.size();
            while (packetCount >= count)
            {
              if (nextCriticalPacket == criticalpackets.end()) nextCriticalPacket = criticalpackets.begin();
              criticalpacketentries.push_back(CriticalPacketEntry(&*recoveryfile, 
                                                                  offset,
                                                                  *nextCriticalPacket));
              offset += (*nextCriticalPacket)->PacketLength();
              ++nextCriticalPacket;

              packetCount -= count;
            }
          }
        }

        // Add one copy of the creator packet
        criticalpacketentries.push_back(CriticalPacketEntry(&*recoveryfile, 
                                                            offset, 
                                                            creatorpacket));
        offset += creatorpacket->PacketLength();

        // Create the file on disk and make it the required size
        if (!recoveryfile->Create(fileallocation->filename, offset))
          return false;

        ++recoveryfile;
        ++fileallocation;
      }
    }
  }

  return true;
}

// Allocate memory buffers for reading and writing data to disk.
bool Par2Creator::AllocateBuffers(void)
{
#if GPGPU_CUDA
  // allocate the GPU output buffers
  if (rs.has_gpu() && 0 == cuda::AllocateResources(recoveryblockcount, (size_t) chunksize))
    rs.set_has_gpu(false);
#endif

#if WANT_CONCURRENT && CONCURRENT_PIPELINE
  typedef __TBB_TypeWithAlignmentAtLeastAsStrict(u8) element_type;
  const size_t aligned_chunksize = (sizeof(u8)*(size_t)chunksize+sizeof(element_type)-1)/sizeof(element_type);
  aligned_chunksize_ = aligned_chunksize;
  size_t sz = aligned_chunksize * recoveryblockcount;
  outputbuffer = tbb::cache_aligned_allocator<u8>().allocate(sz);//new u8[sz];
#else
  if (!inputbuffer.alloc(chunksize))
    return false;
//inputbuffer = new u8[chunksize];
  outputbuffer = new u8[chunksize * recoveryblockcount];
#endif

#if (WANT_CONCURRENT && CONCURRENT_PIPELINE) || DSTOUT
  outputbuffer_element_state_.resize(recoveryblockcount);
#endif

#if WANT_CONCURRENT && CONCURRENT_PIPELINE
  if (outputbuffer == NULL)
#else
  if (/* inputbuffer == NULL || */ outputbuffer == NULL)
#endif
  {
    cerr << "Could not allocate buffer memory." << endl;
    return false;
  }

  return true;
}

// Compute the Reed Solomon matrix
bool Par2Creator::ComputeRSMatrix(void)
{
  // Set the number of input blocks
  if (!rs.SetInput(sourceblockcount))
    return false;

  // Set the number of output blocks to be created
  if (!rs.SetOutput(false, 
                    (u16)firstrecoveryblock, 
                    (u16)firstrecoveryblock + (u16)(recoveryblockcount-1)))
    return false;

  // Compute the RS matrix
  if (!rs.Compute(noiselevel))
    return false;

  return true;
}


#if WANT_CONCURRENT

void* Par2Creator::OutputBufferAt(u32 outputindex) {
  #if CONCURRENT_PIPELINE
  // Select the appropriate part of the output buffer
  return &((u8*)outputbuffer)[aligned_chunksize_ * outputindex];
  #else
  // Select the appropriate part of the output buffer
  return &((u8*)outputbuffer)[chunksize * outputindex];
  #endif
}

bool Par2Creator::ProcessDataForOutputIndex_(u32 outputblock, u32 outputendblock, size_t blocklength,
                                             u32 inputblock, buffer& inputbuffer)
{
  #if CONCURRENT_PIPELINE
    int val = (outputbuffer_element_state_[outputblock] -= 2); // 0 -> -2, 1 -> -1
    if (val < -2) { // index is already in use: defer its processing
      outputbuffer_element_state_[outputblock] += 2; // undo my changes
//printf("deferring %u\n", outputblock);
      return false;
    }
    assert(val == -2 || val == -1); // ie, hold lock

    // Select the appropriate part of the output buffer
    void *outbuf = OutputBufferAt(outputblock);//&((u8*)outputbuffer)[aligned_chunksize_ * outputblock];
  #else
    // Select the appropriate part of the output buffer
    void *outbuf = OutputBufferAt(outputblock);//&((u8*)outputbuffer)[chunksize * outputblock];
  #endif

    // Process the data through the RS matrix
    rs.Process(blocklength, inputblock, inputbuffer, outputblock, outbuf);

  #if CONCURRENT_PIPELINE
    assert(val < 0);
    outputbuffer_element_state_[outputblock] += 2; // undo my changes, ie, release lock
  #endif

    if (noiselevel > CommandLine::nlQuiet) {
      tbb::tick_count now = tbb::tick_count::now();
      if ((now - last_cout).seconds() >= 0.1) { // only update every 0.1 seconds
// when building with -O3 under Darwin, using u32 instead of u64 causes incorrect values to be printed :-(
// I believe it's a compiler codegen bug, but this work-around (using u64 instead of u32) is "good enough".
        // Update a progress indicator
        u64 oldfraction = (u64)(1000 * progress / totaldata);
        progress += blocklength;
        u64 newfraction = (u64)(1000 * progress / totaldata);

        if (oldfraction != newfraction) {
//        tbb::mutex::scoped_lock l(cout_mutex);
          if (0 == cout_in_use.compare_and_swap(outputendblock, 0)) { // <= this version doesn't block - only need 1 thread to write to cout
            last_cout = now;
#ifndef NDEBUG
            cout << "GPU=" << cuda::GetProcessingCount() << " - " << "Processing: " << newfraction/10 << '.' << newfraction%10 << "%\r" << flush;
#else
            cout << "Processing: " << newfraction/10 << '.' << newfraction%10 << "%\r" << flush;
#endif
            cout_in_use = 0;
          }
        }
      } else
        progress += blocklength;
    }
    return true;
}

void Par2Creator::ProcessDataForOutputIndex(u32 outputblock, u32 outputendblock, size_t blocklength,
                                            u32 inputblock, buffer& inputbuffer)
{
  std::vector<u32> v; // which indexes need deferred processing
  v.reserve(outputendblock - outputblock);

  for( ; outputblock != outputendblock; ++outputblock )
    if (!ProcessDataForOutputIndex_(outputblock, outputendblock, blocklength, inputblock, inputbuffer))
      v.push_back(outputblock);

  // try to process all deferred indexes
  std::vector<u32> d;
  do {
    for (std::vector<u32>::const_iterator vit = v.begin(); vit != v.end(); ++vit) { // which indexes need deferred processing
      const u32 oi = *vit;
//printf("trying to process deferred %u\n", oi);
      if (!ProcessDataForOutputIndex_(oi, outputendblock, blocklength, inputblock, inputbuffer)) {
        d.push_back(oi); // failed -> try again
      }
    }
    v = d; d.clear();
  } while (!v.empty());
}

class ApplyPar2CreatorRSProcess {
public:
  ApplyPar2CreatorRSProcess(Par2Creator* obj, size_t blocklength, u32 inputblock, buffer& inputbuffer) :
    _obj(obj), _blocklength(blocklength), _inputblock(inputblock), _inputbuffer(inputbuffer) {}
  void operator()(const tbb::blocked_range<u32>& r) const {
    _obj->ProcessDataForOutputIndex(r.begin(), r.end(), _blocklength, _inputblock, _inputbuffer);
  }
private:
  Par2Creator* _obj;
  size_t       _blocklength;
  u32          _inputblock;
  buffer&      _inputbuffer;
};

void Par2Creator::ProcessDataConcurrently(size_t blocklength, u32 inputblock, buffer& inputbuffer)
{
  if (ALL_SERIAL != concurrent_processing_level) {
    static tbb::affinity_partitioner ap;
    tbb::parallel_for(tbb::blocked_range<u32>(0, recoveryblockcount),
      ::ApplyPar2CreatorRSProcess(this, blocklength, inputblock, inputbuffer), ap);
  } else
    ProcessDataForOutputIndex(0, recoveryblockcount, blocklength, inputblock, inputbuffer);
}

#endif


// Read source data, process it through the RS matrix and write it to disk.
bool Par2Creator::ProcessData(u64 blockoffset, size_t blocklength)
{
//CTimeInterval  ti_pdlo("ProcessDataLoopOuter");

#if WANT_CONCURRENT && CONCURRENT_PIPELINE
  // Clear the output buffer
  memset(outputbuffer, 0, aligned_chunksize_ * recoveryblockcount);

  for (size_t i = 0; i != recoveryblockcount; ++i) {
    // when outputbuffer_element_state_ contains tbb::atomic<> objects,
    // they must be manually initialized to zero:
    outputbuffer_element_state_[i] = 0;
  }

//cout << "Creating using async I/O." << endl;
    assert(sourceblocks.size() == sourceblockcount);
    vector<DataBlock*>         sourceblocks_;
    sourceblocks_.resize(sourceblockcount);
    for (size_t i = 0; i != sourceblockcount; ++i)
      sourceblocks_[i] = &sourceblocks[i];

    const size_t max_tokens = ALL_SERIAL == concurrent_processing_level ? 1 : tbb::task_scheduler_init::default_num_threads();
    create_pipeline_state s(max_tokens, chunksize, recoveryblockcount, blocklength, blockoffset,
                            sourceblocks_, sourcefiles, deferhashcomputation);

    tbb::pipeline p;
    create_filter_read cfr(s);
    p.add_filter(cfr);
    create_filter_process cfp(*this, s);
    p.add_filter(cfp);
    //create_filter_write cfw(*this, s);
    //p.add_filter(cfw);

    p.run(max_tokens);

  #if GPGPU_CUDA
    if (rs.has_gpu()) {
    #ifndef NDEBUG
      CTimeInterval gpp("GPU postprocessing");
    #endif

      // do xor's of outputbuffers that are on the video card
      create_buffer* buf = s.first_available_buffer();
      if (!buf) {
        cerr << "Failed to acquire a buffer for copying back data from video card. Creation failed." << endl;
        return false;
      }

      const u32 n = cuda::GetDeviceOutputBufferCount();
      for (u32 i = 0; i != n; ++i) {
        if (!cuda::CopyDeviceOutputBuffer(i, (u32*) buf->get())) {
          cerr << "Failed to copy back data from video card. Creation failed." << endl;
          return false;
        }
        cuda::Xor((u32*) OutputBufferAt(i), (const u32*) buf->get(), (size_t) chunksize);
      }
      s.release(buf);

    #ifndef NDEBUG
      gpp.emit();
    #endif

      const unsigned pc = cuda::GetProcessingCount();
      if (0 == pc)
        cout << "The GPU was not used for processing." << endl;
      else {
        u64 fraction = (1000 * (u64) pc) / ((u64) sourceblockcount * (u64) recoveryblockcount);
        cout << "The GPU was used for " << fraction/10 << '.' << fraction%10 << "% of the processing (" <<
          pc << " out of " << ((u64) sourceblockcount * (u64) recoveryblockcount) << " processing blocks)." << endl;
      }
    }
  #endif

    p.clear();

    if (!s.is_ok()) {
      cerr << "Creation of recovery file(s) has failed." << endl;
      return false;
    }
#else
  // Clear the output buffer
  memset(outputbuffer, 0, chunksize * recoveryblockcount);

  // If we have defered computation of the file hash and block crc and hashes
  // sourcefile and sourceindex will be used to update them during
  // the main recovery block computation
  vector<Par2CreatorSourceFile*>::iterator sourcefile = sourcefiles.begin();
  u32 sourceindex = 0;

  vector<DataBlock>::iterator sourceblock;
  u32 inputblock;

  DiskFile *lastopenfile = NULL;

  // For each input block
  for ((sourceblock=sourceblocks.begin()),(inputblock=0);
       sourceblock != sourceblocks.end();
       ++sourceblock, ++inputblock)
  {
    // Are we reading from a new file?
    if (lastopenfile != (*sourceblock).GetDiskFile())
    {
      // Close the last file
      if (lastopenfile != NULL)
      {
        lastopenfile->Close();
      }

      // Open the new file
      lastopenfile = (*sourceblock).GetDiskFile();
      if (!lastopenfile->Open())
      {
        return false;
      }
    }

    {
      // Read data from the current input block
      if (!sourceblock->ReadData(blockoffset, blocklength, inputbuffer.get()))
        return false;

      if (deferhashcomputation) {
        assert(blockoffset == 0 && blocklength == blocksize);
        assert(sourcefile != sourcefiles.end());

        (*sourcefile)->UpdateHashes(sourceindex, inputbuffer.get(), blocklength);
      }

//CTimeInterval  ti_pdli("ProcessDataLoopInner");
  #if WANT_CONCURRENT
      ProcessDataConcurrently(blocklength, inputblock, inputbuffer);
  #else
      // For each output block
      for (u32 outputblock=0; outputblock<recoveryblockcount; outputblock++)
      {
        // Select the appropriate part of the output buffer
        void *outbuf = &((u8*)outputbuffer)[chunksize * outputblock];

        // Process the data through the RS matrix
        rs.Process(blocklength, inputblock, inputbuffer, outputblock, outbuf);

        if (noiselevel > CommandLine::nlQuiet)
        {
// when building with -O3 under Darwin, using u32 instead of u64 causes incorrect values to be printed :-(
// I believe it's a compiler codegen bug, but this work-around (using u64 instead of u32) is "good enough".
          // Update a progress indicator
          u64 oldfraction = (u64)(1000 * progress / totaldata);
          progress += blocklength;
          u64 newfraction = (u64)(1000 * progress / totaldata);

          if (oldfraction != newfraction) {
            cout << "Processing: " << newfraction/10 << '.' << newfraction%10 << "%\r" << flush;
          }
        }
      }
  #endif
    }

    // Work out which source file the next block belongs to
    if (++sourceindex >= (*sourcefile)->BlockCount())
    {
      sourceindex = 0;
      ++sourcefile;
    }
  }

  // Close the last file
  if (lastopenfile != NULL)
  {
    lastopenfile->Close();
  }
#endif

//ti_pdlo.emit();

  if (noiselevel > CommandLine::nlQuiet)
    cout << "Writing recovery packets\r";

  // For each output block
  for (u32 outputblock=0; outputblock<recoveryblockcount;outputblock++)
  {
#if WANT_CONCURRENT && CONCURRENT_PIPELINE
    // Select the appropriate part of the output buffer
    char *outbuf = &((char*)outputbuffer)[aligned_chunksize_ * outputblock];
#else
    // Select the appropriate part of the output buffer
    char *outbuf = &((char*)outputbuffer)[chunksize * outputblock];
#endif
    // Write the data to the recovery packet
    if (!recoverypackets[outputblock].WriteData(blockoffset, blocklength, outbuf))
      return false;
  }

  if (noiselevel > CommandLine::nlQuiet)
    cout << "Wrote " << recoveryblockcount * blocklength << " bytes to disk" << endl;

  return true;
}

// Finish computation of the recovery packets and write the headers to disk.
bool Par2Creator::WriteRecoveryPacketHeaders(void)
{
  // For each recovery packet
  for (vector<RecoveryPacket>::iterator recoverypacket = recoverypackets.begin();
       recoverypacket != recoverypackets.end();
       ++recoverypacket)
  {
    // Finish the packet header and write it to disk
    if (!recoverypacket->WriteHeader())
      return false;
  }

  return true;
}

bool Par2Creator::FinishFileHashComputation(void)
{
  // If we defered the computation of the full file hash, then we finish it now
  if (deferhashcomputation)
  {
    // For each source file
    vector<Par2CreatorSourceFile*>::iterator sourcefile = sourcefiles.begin();

    while (sourcefile != sourcefiles.end())
    {
      (*sourcefile)->FinishHashes();

      ++sourcefile;
    }
  }

  return true;
}

// Fill in all remaining details in the critical packets.
bool Par2Creator::FinishCriticalPackets(void)
{
  // Get the setid from the main packet
  const MD5Hash &setid = mainpacket->SetId();

  for (list<CriticalPacket*>::iterator criticalpacket=criticalpackets.begin(); 
       criticalpacket!=criticalpackets.end(); 
       criticalpacket++)
  {
    // Store the setid in each of the critical packets
    // and compute the packet_hash of each one.

    (*criticalpacket)->FinishPacket(setid);
  }

  return true;
}

// Write all other critical packets to disk.
bool Par2Creator::WriteCriticalPackets(void)
{
  list<CriticalPacketEntry>::const_iterator packetentry = criticalpacketentries.begin();

  // For each critical packet
  while (packetentry != criticalpacketentries.end())
  {
    // Write it to disk
    if (!packetentry->WritePacket())
      return false;

    ++packetentry;
  }

  return true;
}

// Close all files.
bool Par2Creator::CloseFiles(void)
{
//  // Close each source file.
//  for (vector<Par2CreatorSourceFile*>::iterator sourcefile = sourcefiles.begin();
//       sourcefile != sourcefiles.end();
//       ++sourcefile)
//  {
//    (*sourcefile)->Close();
//  }

  // Close each recovery file.
  for (vector<DiskFile>::iterator recoveryfile = recoveryfiles.begin();
       recoveryfile != recoveryfiles.end();
       ++recoveryfile)
  {
    recoveryfile->Close();
  }

  return true;
}
