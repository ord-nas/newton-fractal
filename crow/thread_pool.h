#include <mutex>
#include <condition_variable>

#include <boost/asio/io_service.hpp>
#include <boost/bind.hpp>
#include <boost/thread/thread.hpp>

class ThreadPool {
 public:
  ThreadPool(size_t num_threads) : work_(io_service_) {
    for (size_t i = 0; i < num_threads; ++i) {
      threads_.create_thread(boost::bind(&boost::asio::io_service::run, &io_service_));
    }
  }

  template <typename F>
  void Queue(F f) {
    // Increment the number of outstanding tasks.
    {
      std::scoped_lock lock(m_);
      ++outstanding_tasks_;
    }

    // Post the task.
    io_service_.post([f, this]() {
      f();
      // After finishing, decrement the number of outstanding tasks.
      {
	std::scoped_lock lock(m_);
	--outstanding_tasks_;
      }
      // Notify any waiting threads that maybe we're done.
      cv_.notify_all();
    });
  }

  void WaitUntilDone() {
    std::unique_lock lock(m_);
    while (outstanding_tasks_ > 0) {
      cv_.wait(lock);
    }
    lock.unlock();
  }

  ~ThreadPool() {
    io_service_.stop();
    threads_.join_all();
  }

 private:
  boost::asio::io_service io_service_;
  boost::asio::io_service::work work_;
  boost::thread_group threads_;

  std::mutex m_;
  std::condition_variable cv_;
  int outstanding_tasks_ = 0;
};
