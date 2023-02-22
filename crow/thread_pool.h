#include <mutex>
#include <condition_variable>

#include <boost/asio/io_service.hpp>
#include <boost/bind.hpp>
#include <boost/thread/thread.hpp>

class ThreadPool {
 public:
  ThreadPool(size_t num_threads) : work_(io_service_) {
    for (size_t i = 0; i < num_threads; ++i) {
      auto* thread = new boost::thread(boost::bind(&boost::asio::io_service::run, &io_service_));
      std::cout << "ThreadPool created thread with handle: " << thread->native_handle() << std::endl;

      // Uncomment the code below to set a CPU affinity for the given thread.
      // Experiments show this doesn't seem to help much.
      //
      // If you do turn this on, care must be taken that the CPU requested
      // actually exists on this machine. Also these are *logical* CPUs, watch
      // out for Hyperthreading! I.e. you may want to try one thread per
      // *physical core* and then set the affinities so that each thread gets
      // its own core.
      //
      // For more details, see this very helpful blog post:
      // https://eli.thegreenplace.net/2016/c11-threads-affinity-and-hyperthreading

      // size_t cpu = (i < 4) ? (2*i) : (2*(i-4)+1);
      // cpu_set_t cpuset;
      // CPU_ZERO(&cpuset);
      // CPU_SET(cpu, &cpuset);
      // int rc = pthread_setaffinity_np(thread->native_handle(), sizeof(cpu_set_t), &cpuset);
      // std::cout << "Tried to set CPU affinity of thread " << thread->native_handle()
      // 		<< " to " << cpu << " and got: " << rc << std::endl;

      threads_.add_thread(thread);
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
