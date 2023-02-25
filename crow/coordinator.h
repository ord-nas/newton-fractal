#ifndef _CROW_FRACTAL_SERVER_COORDINATOR_
#define _CROW_FRACTAL_SERVER_COORDINATOR_

#include <mutex>
#include <condition_variable>

#include "thread_pool.h"

class Coordinator {
 public:
  explicit Coordinator(size_t num_threads) : thread_pool_(num_threads) {}

  template <typename F>
  void QueueComputation(F f) {
    Queue(f, computation_sync_);
  }

  template <typename F>
  void QueueLayout(F f) {
    Queue(f, layout_sync_);
  }

  void WaitUntilComputationDone() {
    WaitUntilDone(computation_sync_);
  }

  void WaitUntilLayoutDone() {
    WaitUntilDone(layout_sync_);
  }

 private:
  struct WorkerSynchronizer {
    std::mutex m;
    std::condition_variable cv;
    int outstanding_tasks = 0;
  };

  template <typename F>
  void Queue(F f, WorkerSynchronizer& sync) {
    // Increment the number of outstanding tasks.
    {
      std::scoped_lock lock(sync.m);
      ++sync.outstanding_tasks;
    }

    // Queue the task.
    thread_pool_.Queue([f, &sync]() {
      f();
      // After finishing, decrement the number of outstanding tasks.
      bool notify = false;
      {
	std::scoped_lock lock(sync.m);
	--sync.outstanding_tasks;
	notify = (sync.outstanding_tasks == 0);
      }
      // Notify any waiting threads if there are no more tasks.
      if (notify) {
	sync.cv.notify_all();
      }
    });
  }

  void WaitUntilDone(WorkerSynchronizer& sync) {
    std::unique_lock lock(sync.m);
    while (sync.outstanding_tasks > 0) {
      sync.cv.wait(lock);
    }
    lock.unlock();
  }

  ThreadPool thread_pool_;

  WorkerSynchronizer computation_sync_;
  WorkerSynchronizer layout_sync_;
};

#endif // _CROW_FRACTAL_SERVER_COORDINATOR_
