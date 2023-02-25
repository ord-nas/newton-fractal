#ifndef _CROW_FRACTAL_SERVER_TASK_GROUP_
#define _CROW_FRACTAL_SERVER_TASK_GROUP_

#include <mutex>
#include <condition_variable>

#include "thread_pool.h"

class TaskGroup {
 public:
  // ThreadPool must outlive the TaskGroup.
  explicit TaskGroup(ThreadPool* thread_pool)
    : thread_pool_(*thread_pool) {}

  template <typename F>
  void Add(F f) {
    // Increment the number of outstanding tasks.
    {
      std::scoped_lock lock(m_);
      ++outstanding_tasks_;
    }

    // Queue the task.
    thread_pool_.Queue([f, this]() {
      f();
      // After finishing, decrement the number of outstanding tasks.
      bool notify = false;
      {
	std::scoped_lock lock(m_);
	--outstanding_tasks_;
	notify = (outstanding_tasks_ == 0);
      }
      // Notify any waiting threads if there are no more tasks.
      if (notify) {
	cv_.notify_all();
      }
    });
  }

  void WaitUntilDone() {
    std::unique_lock lock(m_);
    while (outstanding_tasks_ > 0) {
      cv_.wait(lock);
    }
    lock.unlock();
  }

 private:
  // Unowned.
  ThreadPool& thread_pool_;

  std::mutex m_;
  std::condition_variable cv_;
  int outstanding_tasks_ = 0;
};

#endif // _CROW_FRACTAL_SERVER_TASK_GROUP_
