//  This file is part of par2cmdline (a PAR 2.0 compatible file verification and
//  repair tool). See http://parchive.sourceforge.net for details of PAR 2.0.
//
//  GPGPU support using nVidia CUDA technology. Copyright (c) 2008 Vincent Tan.
//  Created 2008-09-20. cuda.cpp
//
//  par2cmdline-0.4-tbb is available at http://chuchusoft.com/par2_tbb
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

#include "par2cmdline.h"

#ifdef _MSC_VER
#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif
#endif

#include "cuda.h"

#include "tbb/concurrent_queue.h"
#include "tbb/tbb_thread.h"

#if GPGPU_CUDA

namespace cuda {

  #if WIN32 || WIN64
  typedef DWORD thread_id;
  #else
  typedef pthread_t thread_id;
  #endif

  extern thread_id this_thread_id(void);

  typedef int cudaStream_t;
  class per_host_thread_resources {
  private:
    struct gpu_task {
      enum {
        INVALID, AVAILABLE, ACQUIRED, PROCESSING, // AVAILABLE -> ACQUIRED -> PROCESSING -> AVAILABLE
        FLAG_PROCESSING = 0x4000000
      };

      gpu_task(void) : host_lh(NULL), dev_lh(NULL), dev_ibuf(NULL), streamid(0) { state = INVALID; }
      ~gpu_task(void);

      bool alloc_resources(size_t blocksize);

      u16* host_lh; // points to 512 unsigned shorts in page-locked memory
      u16* dev_lh; // points to 512 unsigned shorts in device memory; the texture's memory
      u32* dev_ibuf;
      cudaStream_t streamid;
      tbb::atomic<int> state;
    };
    vector<gpu_task> tasks_;
    vector<u32*> dev_obufs_;
    size_t sizeof_one_host_obuf_;
    bool gpu_detected_;

  public:
    per_host_thread_resources(void) : sizeof_one_host_obuf_(0), gpu_detected_(false) {}

    bool begin(void);

    int alloc_resources(u32 blockcount, size_t blocksize);
    void dealloc_resources(void);

    int first_available_task(const unsigned* lhTable);
    bool process(size_t unsigned_int_count, buffer& inputbuffer, u32 outputindex, int taskindex);

    int update_task_status(void);
    bool copy_output_buffer(u32 outputindex, u32* outputbuffer);

    size_t count_obufs(void) const { return dev_obufs_.size(); }

    static size_t blockstride(size_t blocksize) { return (blocksize + 0x1FF) & ~0x1FF; }
  };

  // Functions to detect/use nVidia CUDA technology if hardware that supports
  // CUDA is present in the system.
  namespace internal {

    extern size_t CountDevices(void);
    extern bool SelectDevice(size_t i);

    extern size_t StreamCount(void);

    extern size_t AllocStream(void); // 0 is failure
    extern bool DeallocStream(size_t streamidx);
    extern bool SyncStream(size_t streamidx);
    extern bool QueryStream(size_t streamidx);

    extern void* AllocHostMemory(size_t sz);
    extern bool DeallocHostMemory(void* ptr);
    extern void* AllocDeviceMemory(size_t sz); // returns devptr
    extern bool DeallocDeviceMemory(void* devptr);
    extern bool CopyFromHostToDeviceMemory(void* devptr, const void* hostptr, size_t sz, size_t streamidx);
    extern bool CopyFromDeviceToHostMemory(void* hostptr, const void* devptr, size_t sz, size_t streamidx);

    extern bool BindTextureToDeviceMemory(size_t streamidx, void* tex_dev_ptr, size_t tex_size);
    extern bool ProcessViaGPU(size_t streamidx, size_t len, // len is the number of u32's to process
                              unsigned* dev_obuf, const unsigned* inputbuffer, const unsigned short* host_lh,
                              unsigned* dev_ibuf, unsigned short* dev_lh);
  }

  #if WIN32 || WIN64
  thread_id this_thread_id(void) { return ::GetCurrentThreadId(); }
  #else
  thread_id this_thread_id(void) { return ::pthread_self(); }
  #endif

    per_host_thread_resources::gpu_task::~gpu_task(void) {
      if (host_lh)
        (bool) internal::DeallocHostMemory(host_lh);
      if (dev_lh)
        (bool) internal::DeallocDeviceMemory(dev_lh);
      if (dev_ibuf)
        (bool) internal::DeallocDeviceMemory(dev_ibuf);
      if (streamid)
        (bool) internal::DeallocStream(streamid);
    }

    bool per_host_thread_resources::gpu_task::alloc_resources(size_t blocksize) {
//printf("allocating lh's\n");
      dev_lh = (u16*) internal::AllocDeviceMemory(512 * sizeof(u16));
      if (!dev_lh) {
//printf("\ntid %u: (3) host_lh=%p dev_lh=%p dev_ibuf=%p\n", (unsigned) this_thread_id(), host_lh, dev_lh, dev_ibuf);
        return false;
      }

      host_lh = (u16*) internal::AllocHostMemory(512 * sizeof(u16));
      if (!host_lh) {
//printf("\ntid %u: (4) host_lh=%p dev_lh=%p dev_ibuf=%p\n", (unsigned) this_thread_id(), host_lh, dev_lh, dev_ibuf);
        return false;
      }

      dev_ibuf = (u32*) internal::AllocDeviceMemory(per_host_thread_resources::blockstride(blocksize));
      if (!dev_ibuf) {
//printf("\ntid %u: (5) host_lh=%p dev_lh=%p dev_ibuf=%p\n", (unsigned) this_thread_id(), host_lh, dev_lh, dev_ibuf);
        return false;
      }

      streamid = internal::AllocStream();
      if (!streamid) {
//printf("\ntid %u: (7) host_lh=%p dev_lh=%p dev_ibuf=%p\n", (unsigned) this_thread_id(), host_lh, dev_lh, dev_ibuf);
        return false;
      }

      if (!internal::BindTextureToDeviceMemory(streamid, dev_lh, 512 * sizeof(u16))) {
//printf("\ntid %u: (3) host_lh=%p dev_lh=%p dev_ibuf=%p\n", (unsigned) this_thread_id(), host_lh, dev_lh, dev_ibuf);
        return false;
      }

//printf("\ntid %u: host_lh=%p dev_lh=%p dev_ibuf=%p\n", (unsigned) this_thread_id(), host_lh, dev_lh, dev_ibuf);
      state = AVAILABLE;
      return true;
    }

    bool per_host_thread_resources::begin(void) { // BEGIN
      size_t ndev = internal::CountDevices();
//printf("ndev=%u\n", ndev);
      gpu_detected_ = ndev && internal::SelectDevice(0); // always select the first device
//printf("gpu_detected_ = %u\n", gpu_detected_);
      return gpu_detected_;
    }

    int per_host_thread_resources::alloc_resources(u32 blockcount, size_t blocksize) { // ALLOCATE_RESOURCES
      if (!gpu_detected_)
        return 0;

      tasks_.resize(internal::StreamCount());
      for (vector<gpu_task>::iterator it = tasks_.begin(); it != tasks_.end(); ++it)
        if (!(*it).alloc_resources(blocksize))
          return 0;

      {
        u32* host_obuf = (u32*) internal::AllocHostMemory(blocksize);
        if (!host_obuf)
            return 0;
        memset(host_obuf, 0, blocksize);

        sizeof_one_host_obuf_ = blocksize;
        while (dev_obufs_.size() < blockcount) {
          u32* dev_obuf = (u32*) internal::AllocDeviceMemory(per_host_thread_resources::blockstride(blocksize));
          if (!dev_obuf)
            break;
          if (!internal::CopyFromHostToDeviceMemory(dev_obuf, host_obuf, blocksize, 0)) {
            (bool) internal::DeallocDeviceMemory(dev_obuf);
            break;
          }
          dev_obufs_.push_back(dev_obuf);
        }

        (bool) internal::DeallocHostMemory(host_obuf);
      }

      if (0 == dev_obufs_.size())
        return 0; // 0 buffers => can't use GPU for processing

      return tasks_.size();
    }

    void per_host_thread_resources::dealloc_resources(void) { // DEALLOCATE_RESOURCES
      tasks_.clear();
      for (vector<u32*>::iterator it = dev_obufs_.begin(); it != dev_obufs_.end(); ++it) {
        (bool) internal::DeallocDeviceMemory(*it);
      }
    }

    int per_host_thread_resources::first_available_task(const unsigned* lhTable) { // EXECUTE
#ifndef NDEBUG
      assert(gpu_detected_);
#else
      if (!gpu_detected_) {
        return -1;
      }
#endif
      // find available task
      for (vector<gpu_task>::iterator it = tasks_.begin(); it != tasks_.end(); ++it) {
        if (gpu_task::AVAILABLE == (*it).state++) {
//printf("acquired task #%u - id %u\n", it - tasks_.begin(), (*it).streamid);
          assert(gpu_task::ACQUIRED <= (*it).state);

          for (size_t i = 0; i != 256*2; ++i)
            (*it).host_lh[i] = (u16) lhTable[i];

          return (int) (it - tasks_.begin());
        } else
          (*it).state--; // undo change by this fn
      }

      assert(false);
      return -1;
    }

    bool per_host_thread_resources::process(size_t unsigned_int_count, buffer& inputbuffer,
                                            u32 outputindex, int taskindex) { // EXECUTE
#ifndef NDEBUG
      assert(gpu_detected_ && taskindex >= 0 && (size_t) taskindex < tasks_.size() && outputindex < dev_obufs_.size());
#else
      if (!gpu_detected_ || taskindex < 0 || (size_t) taskindex >= tasks_.size() || outputindex >= dev_obufs_.size()) {
        return false;
      }
#endif

      gpu_task& task = tasks_[taskindex];
#ifndef NDEBUG
      bool qs = internal::QueryStream(task.streamid);
//printf("processing task #%u: state %u, id %u, qs %u, ib %p\n", taskindex, (int) task.state, task.streamid, qs, &inputbuffer);
      assert(gpu_task::ACQUIRED <= task.state);
      assert(qs);
#endif
      bool b = internal::ProcessViaGPU(task.streamid, unsigned_int_count, (unsigned*) dev_obufs_[outputindex],
                                       (const unsigned*) inputbuffer.get(), task.host_lh,
                                       (unsigned*) task.dev_ibuf, task.dev_lh);
      //assert(!internal::QueryStream(task.streamid)); <- does not hold
      if (b) {
        // +1 for ACQUIRED -> PROCESSING state change, +FLAG_PROCESSING for quick detection of PROCESSING state
        task.state += (1 + gpu_task::FLAG_PROCESSING);
        assert(gpu_task::PROCESSING <= (task.state & ~gpu_task::FLAG_PROCESSING));
      }
      return b;
    }

    int per_host_thread_resources::update_task_status(void) { // UPDATE_TASK_STATUS
      int res = 0;
      for (vector<gpu_task>::iterator it = tasks_.begin(); it != tasks_.end(); ++it) {
        if ((*it).state >= gpu_task::FLAG_PROCESSING && internal::QueryStream((*it).streamid)) {
//printf("released task #%u - id %u\n", it - tasks_.begin(), (*it).streamid);
          (*it).state += (gpu_task::AVAILABLE - gpu_task::PROCESSING - gpu_task::FLAG_PROCESSING);
          ++res;
        }
      }
      return res;
    }

    bool per_host_thread_resources::copy_output_buffer(u32 outputindex, u32* outputbuffer) { // COPY_OUTPUT_BUFFER
      assert(outputindex < dev_obufs_.size());
      if (outputindex >= dev_obufs_.size()) {
        return false;
      }

      if (!internal::SyncStream(0)) {
#ifndef NDEBUG
printf("SyncStream(0) failed\n");
#endif
        return false;
      }

      return internal::CopyFromDeviceToHostMemory(outputbuffer, dev_obufs_[outputindex], sizeof_one_host_obuf_, 0);
    }


  class CudaOperation {
  public:
    enum operation {
      NOP,
      BEGIN,
      END,
      ALLOCATE_RESOURCES,
      DEALLOCATE_RESOURCES,
      ALLOCATE_HOST,
      DEALLOCATE_HOST,
      EXECUTE,
      UPDATE_TASK_STATUS,
      COPY_OUTPUT_BUFFER,
	};

  //private:
    void* buf_;
    cuda::thread_id tid_;
    size_t unsigned_int_count_;
    size_t blocksize_;
    int result_;
    operation op_;

  public:
    CudaOperation(operation op = NOP, u32* buf = NULL, size_t unsigned_int_count = 0) :
      buf_(buf),
      tid_(cuda::this_thread_id()),
      unsigned_int_count_(unsigned_int_count),
      blocksize_(0),
      result_(-1),
      op_(op)
      {}

    CudaOperation(operation op, size_t unsigned_int_count, size_t blocksize) :
      buf_(NULL),
      tid_(cuda::this_thread_id()),
      unsigned_int_count_(unsigned_int_count),
      blocksize_(blocksize),
      result_(-1),
      op_(op)
      {}

    CudaOperation(size_t unsigned_int_count, rcbuffer& inputbuffer, u32 output_index, int task_index) :
      buf_(&inputbuffer),
      tid_(cuda::this_thread_id()),
      unsigned_int_count_(unsigned_int_count),
      blocksize_(output_index),
      result_(task_index),
      op_(EXECUTE)
      {}
  };

  typedef tbb::concurrent_hash_map< thread_id, tbb::concurrent_queue<CudaOperation>*,
                                    intptr_hasher<thread_id> >          cuda_replies;
  static cuda_replies cuda_replies_;
  static tbb::concurrent_queue<CudaOperation> cuda_requests_;

  // number of available GPU streams (ie, logical threads or logical processors); max = internal::StreamCount()
  static tbb::atomic<int> available_gpu_tasks_;
  static size_t gpu_obufs_ = 0;
  static tbb::atomic<int> updating_task_status_;

  // how many times the GPU was invoked to process a block of data
  static unsigned blocks_processed_by_gpu_ = 0;

  static per_host_thread_resources phtr_;

  static void cuda_thread_main(void);

  static tbb::tbb_thread cuda_thread_(&cuda_thread_main);




void cuda_thread_main(void) {
  for (;;) {
    CudaOperation item;
    cuda_requests_.pop(item);
    switch (item.op_) {
      case CudaOperation::NOP:
        break;
      case CudaOperation::BEGIN:
        item.result_ = false != phtr_.begin();
        break;
      case CudaOperation::END:
//printf("CUDA thread is about to quit\n");
        return;
      case CudaOperation::ALLOCATE_RESOURCES:
        available_gpu_tasks_ = phtr_.alloc_resources(item.unsigned_int_count_, item.blocksize_);
        gpu_obufs_ = phtr_.count_obufs();
        updating_task_status_ = 0;
        break;
      case CudaOperation::DEALLOCATE_RESOURCES:
        phtr_.dealloc_resources();
        break;
      case CudaOperation::ALLOCATE_HOST:
        item.buf_ = internal::AllocHostMemory(item.blocksize_);
        break;
      case CudaOperation::DEALLOCATE_HOST:
        item.result_ = false != internal::DeallocHostMemory(item.buf_);
        break;
      case CudaOperation::EXECUTE:
        assert(item.buf_);
        if (phtr_.process(item.unsigned_int_count_, *static_cast<buffer*> (item.buf_),
                          (u32) item.blocksize_, item.result_)) {
          ++blocks_processed_by_gpu_;
//printf("blocks_processed_by_gpu_ = %u\n", blocks_processed_by_gpu_);
        } else {
          cerr << "GPU processing has failed. This is bad. Sorry." << endl
          << "Possible solutions: try again, reboot your computer to reset the GPU, or use the CPU-only version of this program." << endl
          << endl
          << "You may need to delete the incompletely repaired files and rename the .1 files to their original names, "
              "eg, \"del part1.zip && ren part1.zip.1 part1.zip\"." << endl;
          exit(3);
        }
//printf("release(%p)\n", item.buf_);
        static_cast<rcbuffer*> (item.buf_)->release(); // must always release the buffer
        available_gpu_tasks_ += phtr_.update_task_status(); // take the opportunity to free up any gpu_tasks that have finished
        continue;
      case CudaOperation::UPDATE_TASK_STATUS:
        available_gpu_tasks_ += phtr_.update_task_status();
        --updating_task_status_;
        continue;
      case CudaOperation::COPY_OUTPUT_BUFFER:
        item.result_ = false != phtr_.copy_output_buffer(item.unsigned_int_count_, (u32*) item.buf_);
        break;
	}

    {
      cuda_replies::const_accessor ca;
      if (cuda_replies_.find(ca, item.tid_)) {
        tbb::concurrent_queue<CudaOperation>* reply_queue = ca->second;
        ca.release();

        reply_queue->push(item);
      }
    }
  }
}






static void DoCudaRequestWithReply(CudaOperation& co) {
  cuda_replies::accessor a;
  if (cuda_replies_.find(a, this_thread_id()) || cuda_replies_.insert(a, this_thread_id())) {
    if (NULL == a->second)
      a->second = new tbb::concurrent_queue<CudaOperation>();

    cuda_requests_.push(co);

    tbb::concurrent_queue<CudaOperation>* reply_queue = a->second;
    a.release();

    reply_queue->pop(co);
  } else
    assert(false); // should never occur?
}


bool Begin(void) {
  CudaOperation co(CudaOperation::BEGIN);
  DoCudaRequestWithReply(co);
  return 0 != co.result_;
}


void End(void) {
//printf("asking CUDA thread to quit...\n");
  cuda_requests_.push(CudaOperation(CudaOperation::END));
//printf("waiting for CUDA thread to quit...\n");
  cuda_thread_.join();
//printf("CUDA thread has quit\n");

  for (cuda_replies::iterator it = cuda_replies_.begin(); it != cuda_replies_.end(); ++it)
    delete (*it).second;
}


unsigned AllocateResources(u32 blockcount, size_t blocksize) {
  CudaOperation co(CudaOperation::ALLOCATE_RESOURCES, (size_t) blockcount, blocksize);
  DoCudaRequestWithReply(co);

  cout << "GPU processing ";
  if (available_gpu_tasks_ && gpu_obufs_)
    cout << "is enabled for " << gpu_obufs_ << " data/recovery blocks." << endl;
  else
    cout << "is not available." << endl;

  return available_gpu_tasks_;
}


void DeallocateResources(void) {
  CudaOperation co(CudaOperation::DEALLOCATE_RESOURCES);
  DoCudaRequestWithReply(co);
}


bool Process(size_t unsigned_int_count, rcbuffer& inputbuffer, const unsigned* lhTable,
             u32 outputindex) {
  if (outputindex >= gpu_obufs_)
    return false;

  if (--available_gpu_tasks_ < 0) {
    ++available_gpu_tasks_;
//printf("no GPU tasks available\n");

    // only one UPDATE_TASK_STATUS is ever enqueued
    if (0 == updating_task_status_++) {
      CudaOperation co(CudaOperation::UPDATE_TASK_STATUS);
      cuda_requests_.push(co);
    } else
      updating_task_status_--;
    return false;
  }

  int taskindex = phtr_.first_available_task(lhTable);
  assert(taskindex >= 0);
  if (taskindex < 0) {
    ++available_gpu_tasks_;
    return false;
  }

  // hold extra reference until after inputbuffer's contents have been copied to video card's memory
  inputbuffer.add_ref();
//printf("add_ref(%p)\n", &inputbuffer);

  CudaOperation co(unsigned_int_count, inputbuffer, outputindex, taskindex); // EXECUTE
  cuda_requests_.push(co);

  return true;
}

unsigned GetProcessingCount(void) {
  return blocks_processed_by_gpu_;
}


u32 GetDeviceOutputBufferCount(void) {
  return gpu_obufs_;
}

bool CopyDeviceOutputBuffer(u32 outputindex, u32* outputbuffer) {
  CudaOperation co(CudaOperation::COPY_OUTPUT_BUFFER, outputbuffer, (size_t) outputindex);
  DoCudaRequestWithReply(co);
  return 0 != co.result_;
}


void* AllocateHost(size_t blocksize) {
  CudaOperation co(CudaOperation::ALLOCATE_HOST, (size_t) 0, blocksize);
  DoCudaRequestWithReply(co);
  return co.buf_;
}


void DeallocateHost(void* hostptr) {
  CudaOperation co(CudaOperation::DEALLOCATE_HOST, (u32*) hostptr);
  DoCudaRequestWithReply(co);
}


void Xor(u32* dst, const u32* src, size_t sz) {
  for (; sz; sz -= sizeof(u32)) {
    *dst++ ^= *src++;
  }
}


} // namespace cuda

#endif
