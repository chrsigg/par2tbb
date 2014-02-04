//  This file is part of par2cmdline (a PAR 2.0 compatible file verification and
//  repair tool). See http://parchive.sourceforge.net for details of PAR 2.0.
//
//  Copyright (c) 2008 Vincent Tan, created 2008-09-17. pipeline.h
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

#undef DEBUG_BUFFERS
#undef DEBUG_ASYNC_WRITE // #define

// implements async I/O using a TBB pipeline

#include "buffer.h"

#if WANT_CONCURRENT && CONCURRENT_PIPELINE

  #include "tbb/tbb_thread.h"
  #include "tbb/tick_count.h"

  class pipeline_buffer : public rcbuffer {
  public:
    enum WRITE_STATUS { NONE, ASYNC_WRITE };
#ifdef DEBUG_ASYNC_WRITE
    vector<DataBlock*>::iterator inputblock_;
#endif
  private:
    aiocb_type aiocb_;
    u32 inputindex_;
    WRITE_STATUS write_status_;

  public:
    pipeline_buffer(void) : inputindex_(0), write_status_(NONE) {}

    aiocb_type& get_aiocb(void) { return aiocb_; }

    u32 get_inputindex(void) const { return inputindex_; }
    void set_inputindex(u32 ii) { inputindex_ = ii; }

    void set_write_status(WRITE_STATUS ws) { write_status_ = ws; }
    WRITE_STATUS get_write_status(void) const { return write_status_; }
  };

  class pipeline_state_base {
  public:
    // DiskFile* -> # of data-blocks in the DiskFile yet to be read in
    typedef tbb::concurrent_hash_map<DiskFile*, u32, intptr_hasher<DiskFile*> >  DiskFile_map_type;

  private:
    const u64                                    chunksize_;
    const u32                                    missingblockcount_;

    const size_t                                 blocklength_;
    const u64                                    blockoffset_;

    vector<DataBlock*>&                          inputblocks_;

    u32                                          inputindex_;
    vector<DataBlock*>::iterator                 inputblock_;
    tbb::mutex                                   inputblock_mutex_; // locks inputblock_ and copyblock_

    #if __GNUC__ &&  __ppc__
    // this won't cause any data corruption - it might only cause an incorrect total value to be printed
    u64                                          totalwritten_;
    #else
    tbb::atomic<u64>                             totalwritten_;
    #endif

    DiskFile_map_type                            openfiles_;

    bool                                         ok_; // if an error or failure occurs then this becomes false

  protected:
    static bool try_to_acquire(rcbuffer& b) { return b.try_to_acquire(); }
    static void add_ref(rcbuffer& b) { return b.add_ref(); }
    static int release(rcbuffer& b) { return b.release(); }

  public:
    pipeline_state_base(
      u64                                        chunksize,
      u32                                        missingblockcount,
      size_t                                     blocklength,
      u64                                        blockoffset,
      vector<DataBlock*>&                        inputblocks) :
      chunksize_(chunksize), missingblockcount_(missingblockcount),
      blocklength_(blocklength), blockoffset_(blockoffset),
      inputblocks_(inputblocks), inputindex_(0), inputblock_(inputblocks.begin()),
      ok_(true) {
      totalwritten_ = 0;
    }

    ~pipeline_state_base(void) {}

    bool is_ok(void) const { return ok_; }
    void set_not_ok(void) { ok_ = false; }

    u64 totalwritten(void) const { return totalwritten_; }
    void add_to_totalwritten(u64 d) { totalwritten_ += d; }

    const size_t                                 blocklength(void) const { return blocklength_; }
    const u64                                    blockoffset(void) const { return blockoffset_; }

    tbb::mutex&                                  inputblock_mutex(void) { return inputblock_mutex_; }
    vector<DataBlock*>::iterator                 inputblock(void) { return inputblock_; }
    vector<DataBlock*>::iterator                 inputblocks_end(void) { return inputblocks_.end(); }
    u32                                          get_and_inc_inputindex(void) {
      ++inputblock_;
      return inputindex_++;
	}

    template <typename ACCESSOR>
    bool   find_diskfile(ACCESSOR& a, DiskFile* key) { return openfiles_.find(a, key); }
    template <typename ACCESSOR>
    bool   insert_diskfile(ACCESSOR& a, DiskFile* key) { return openfiles_.insert(a, key); }
    template <typename ACCESSOR>
    bool   remove_diskfile(ACCESSOR& a) { return openfiles_.erase(a); }
  #ifndef NDEBUG
    size_t open_diskfile_count(void) const { return openfiles_.size(); }
  #endif
  };

  template <typename BUFFER>
  class pipeline_state : public pipeline_state_base {
  private:
    std::vector< BUFFER, tbb::cache_aligned_allocator<BUFFER> > inputbuffers_;
    size_t                                                      inputbuffersidx_; // where to start searching for next buffer

  public:
    pipeline_state(
      size_t                                     max_tokens,
      u64                                        chunksize,
      u32                                        missingblockcount,
      size_t                                     blocklength,
      u64                                        blockoffset,
      vector<DataBlock*>&                        inputblocks) :
      pipeline_state_base(chunksize, missingblockcount, blocklength, blockoffset, inputblocks),
      inputbuffersidx_(0) {
      inputbuffers_.resize(max_tokens);
      for (size_t i = 0; i != max_tokens; ++i) {
        if (!inputbuffers_[i].alloc((size_t)chunksize))
          throw 1;

  #if !defined(NDEBUG) && defined(DEBUG_BUFFERS)
        inputbuffers_[i].id_ = i;
  #endif
      }
    }

    BUFFER* first_available_buffer(void) {
      for (;;) {
        size_t off = inputbuffersidx_;
  #if !defined(NDEBUG) && defined(DEBUG_BUFFERS)
        size_t off__ = off;
  #endif
        size_t max_tokens = inputbuffers_.size();
        for (size_t i = 0; i != max_tokens; ++i) {
          if (try_to_acquire(inputbuffers_[off])) {
  #if !defined(NDEBUG) && defined(DEBUG_BUFFERS)
printf("%u acquired\n", (unsigned) off);
  #endif
            return &inputbuffers_[off];
          }

          if (max_tokens == ++off) off = 0;
        }

  #if !defined(NDEBUG) && defined(DEBUG_BUFFERS)
printf("pausing: off__=%u off=%u max_tokens=%u...\r", (unsigned) off__, (unsigned) off, (unsigned) max_tokens);
  #endif
        tbb::this_tbb_thread::sleep( tbb::tick_count::interval_t(0.001 /*0.001 = 1ms*/) ); // pause for 1ms
      }
      //assert(false);
      //return NULL;
    }

    void release(BUFFER* b) {
      int rc = pipeline_state_base::release(*b);
  #if !defined(NDEBUG) && defined(DEBUG_BUFFERS)
printf("%u released -> rc=%d\n", b - &inputbuffers_[0], rc);
  #endif
      if (0 == rc) {
        inputbuffersidx_ = b - &inputbuffers_[0];
  #if !defined(NDEBUG) && defined(DEBUG_BUFFERS)
printf("inputbuffersidx_ := %u\n", b - &inputbuffers_[0]);
  #endif
      }
    }
  };

  template <typename SUBCLASS, typename BUFFER>
  class filter_read_base : public tbb::filter {
  private:
    filter_read_base& operator=(const filter_read_base&); // assignment disallowed
  protected:
    typedef pipeline_state<BUFFER> state_type;
    state_type& state_;
  public:
    // Because async reads don't reliably work (the call to aio_suspend() sometimes never returns),
    // reads are now done synchronously, and thus this stage is now serial (because reading from
    // the same file from two or more threads at the same time is undefined behaviour).
    filter_read_base(state_type& s) :
      tbb::filter(true /* tbb::filter::serial */ /* false tbb::filter::parallel */), state_(s) {}
    virtual void* operator()(void*);
  };

  template <typename SUBCLASS, typename BUFFER>
  //virtual
  void* filter_read_base<SUBCLASS, BUFFER>::operator()(void*) {
    if (!state_.is_ok())
        return NULL; // abort

    vector<DataBlock*>::iterator inputblock;
    BUFFER*                      inputbuffer;

    // try to acquire a buffer (this should always succeed)
    inputbuffer = state_.first_available_buffer();
    assert(NULL != inputbuffer);

    {
      u32 inputindex;

      {
        tbb::mutex::scoped_lock l(state_.inputblock_mutex());

        inputblock = state_.inputblock();
        if (inputblock == state_.inputblocks_end())
          return NULL; // finished

        inputindex = state_.get_and_inc_inputindex();

        static_cast<SUBCLASS*> (this)->on_mutex_held(inputbuffer);
      }

//printf("inputindex=%u\n", inputindex);

      inputbuffer->set_inputindex(inputindex);
    }

    // For each input block

    { // if the file is not opened then do only one open call
      DiskFile* df = (*inputblock)->GetDiskFile();

      typename state_type::DiskFile_map_type::accessor fa;
      while (!state_.find_diskfile(fa, df)) {
//printf("opening DiskFile %s\n", df->FileName().c_str());
        // if this thread was the one that inserted df into the map then open the file
        // (otherwise the file is double opened)

        // There was a race condition here: df is not open, thread 1 queries the hash_map and
        // finds no entry so it calls insert(), thread 2 then queries the hash_map and finds
        // the entry so it then tries to read from the file which thread 1 has yet to open.
        // This race is avoided by using a write-accessor during the insert() call: this will
        // block other threads from trying to use the not-yet-opened-file.
        typename state_type::DiskFile_map_type::accessor ia; // "insert accessor"
        if (state_.insert_diskfile(ia, df)) {
          // The winner gets here and is the one responsible for opening file;
          // other threads trying to access 'df' will now block in the find() call above.
          if (!df->Open(false/*true*/)) { // open file for sync I/O
//printf("opening DiskFile %s failed\n", df->FileName().c_str());
  #ifndef NDEBUG
{int err = errno; fprintf(stderr, "error %d: %s, # of open files = %u\n", err, strerror(err), (unsigned) state_.open_diskfile_count()); fflush(stderr);}
  #endif
            cerr << "unable to open " << df->FileName() << endl;
            state_.set_not_ok();
            return NULL;
          }
          ia->second = df->GetBlockCount(); // how many blocks to read from the DiskFile

          // Release the accessor lock 'ia' and thus allow other threads to access the
          // now-open file. Now that df is in the map, the 'fa' accessor can acquire it.
        } else {
          // The loser must try again until it has access to the key 'df' via the above
          // find(), because although the file is now in the hash_map, it may not yet be open.
        }
      }

      // (if the data were read asynchronously, 'fa' can be released)
    }

    {
      // Read data from the current input block
  #if 1
#ifdef DEBUG_ASYNC_WRITE
printf("reading off=%llu len=%lu\n", (*inputblock)->GetOffset() + state_.blockoffset(), state_.blocklength());
#endif
      if (!(*inputblock)->ReadData(state_.blockoffset(), state_.blocklength(), inputbuffer->get()) ||
          !static_cast<SUBCLASS*> (this)->on_inputbuffer_read(inputbuffer)) {
        state_.set_not_ok();
        state_.release(inputbuffer);
        return NULL;
      }
  #else
      // on Mac OS X 10.5.5, suspend_until_completed() does not return if async requests are made
      // too frequently (it smells like an OS bug because when the requests occur further apart in
      // time, the suspension does end), so this code block is disabled:
      if (!(*inputblock)->ReadDataAsync(inputbuffer->get_aiocb(), state_.blockoffset(),
                                        state_.blocklength(), inputbuffer->get())) {
//printf("start reading DiskFile %s failed\n", (*inputblock)->GetDiskFile()->FileName().c_str());
    #ifndef NDEBUG
{int err = errno; fprintf(stderr, "\nerror %d: %s, # of open files = %u\n", err, strerror(err), (unsigned) state_.open_diskfile_count()); fflush(stderr);}
    #endif
        cerr << "unable to request async read of " << (*inputblock)->GetDiskFile()->FileName() << endl;
        state_.set_not_ok();
        state_.release(inputbuffer);
        return NULL;
      }

      // at this point, returning to caller is possible if another pipeline stage is inserted: it
      // would allow another async read to be requested or other processing to occur.
printf("%u suspending for read %lu bytes @ %llu\n", inputbuffer->id_, inputbuffer->get_aiocb().len_, inputbuffer->get_aiocb().off_);
      inputbuffer->get_aiocb().suspend_until_completed();
printf("%u suspending completed\n", inputbuffer->id_);
      if (!inputbuffer->get_aiocb().completedOK() || !static_cast<SUBCLASS*> (this)->on_inputbuffer_read(inputbuffer)) {
//printf("completion of reading DiskFile %s failed\n", (*inputblock)->GetDiskFile()->FileName().c_str());
    #ifndef NDEBUG
{int err = errno; fprintf(stderr, "error %d: %s, # of open files = %u\n", err, strerror(err), (unsigned) state_.open_diskfile_count()); fflush(stderr);}
    #endif
        cerr << "unable to complete async read of " << (*inputblock)->GetDiskFile()->FileName() << endl;
        state_.set_not_ok();
        state_.release(inputbuffer);
        return NULL;
      }
  #endif

      { // decr block count
        DiskFile* df = (*inputblock)->GetDiskFile();

        // the count is currently stored in the DiskFile_map_type but it could also be in the DiskFile class;
        // using an accessor here ensures mutual exclusion to the decrement; if moved to DiskFile, the count
        // should be changed to a tbb::atomic<u32> instead of a bare u32.
        typename state_type::DiskFile_map_type::accessor fa;
        if (!state_.find_diskfile(fa, df)) {
          cerr << "unable to decrement " << (*inputblock)->GetDiskFile()->FileName() << " (this should not occur)" << endl;
          state_.set_not_ok();
          state_.release(inputbuffer);
          return NULL;
        }

//printf("%s --bc => %u\n", df->FileName().c_str(), fa->second - 1);
        if (0 == --fa->second) { // last block was just read in so file can be closed
          df->Close();
//printf("%s was closed\n", df->FileName().c_str());
          state_.remove_diskfile(fa);
        }
      }
    }

#ifdef DEBUG_ASYNC_WRITE
    inputbuffer->inputblock_ = inputblock;
#endif

    return inputbuffer;
  }

  template <typename SUBCLASS, typename BUFFER, typename DELEGATE>
  class filter_process_base : public tbb::filter {
    typedef DELEGATE delegate_type;
    delegate_type& delegate_;
  protected:
    typedef pipeline_state<BUFFER> state_type;
    state_type& state_;
  public:
    filter_process_base(delegate_type& delegate, state_type& s) :
      tbb::filter(false /* SERIAL tbb::filter::parallel */), delegate_(delegate), state_(s) {}
    virtual void* operator()(void*);
  };

  template <typename SUBCLASS, typename BUFFER, typename DELEGATE>
  //virtual
  void* filter_process_base<SUBCLASS, BUFFER, DELEGATE>::operator()(void* item) {
    BUFFER* inputbuffer = static_cast<BUFFER*> (item);
    assert(NULL != inputbuffer);
//printf("filter_process_base::operator()\n");

//printf("inputbuffer->get_inputindex()=%u\n", inputbuffer->get_inputindex());
    delegate_.ProcessDataConcurrently(state_.blocklength(), inputbuffer->get_inputindex(), *inputbuffer);

    if (pipeline_buffer::ASYNC_WRITE == inputbuffer->get_write_status()) {
#ifdef DEBUG_ASYNC_WRITE
printf("waiting for async write at off=%llu to complete\n", (*inputbuffer->inputblock_)->GetOffset());
// used to debug the problem where the last byte of the file was not being written to - the bug fix is
// in diskfile.cpp: DiskFile::Create()
if (102387 == (*inputbuffer->inputblock_)->GetOffset()) {
  for (unsigned i = 0; i != state_.blocklength(); ++i)
    printf("%02x ", ((const unsigned char*) inputbuffer->get())[i]);
  printf("\n");
}
#endif
      inputbuffer->get_aiocb().suspend_until_completed();
      if (!inputbuffer->get_aiocb().completedOK()) {
        state_.set_not_ok();
//printf("writing inputbuffer=%p completed unsuccessfully\n", inputbuffer);
      }
      inputbuffer->set_write_status(pipeline_buffer::NONE);
    }

    state_.release(inputbuffer);
    return NULL;
  }

#endif // WANT_CONCURRENT && CONCURRENT_PIPELINE
