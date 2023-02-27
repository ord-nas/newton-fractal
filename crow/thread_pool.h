#ifndef _CROW_FRACTAL_SERVER_THREAD_POOL_
#define _CROW_FRACTAL_SERVER_THREAD_POOL_

#include <boost/asio/io_service.hpp>
#include <boost/bind.hpp>
#include <boost/thread/thread.hpp>

class ThreadPool {
 public:
  explicit ThreadPool(size_t num_threads)
    : work_(io_service_), size_(num_threads) {
    for (size_t i = 0; i < num_threads; ++i) {
      auto* thread = new boost::thread(boost::bind(&boost::asio::io_service::run,
						   &io_service_));
      std::cout << "ThreadPool created thread with handle: "
		<< thread->native_handle() << std::endl;

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

  size_t size() const {
    return size_;
  }

  template <typename F>
  void Queue(F f) {
    // Post the task.
    io_service_.post(f);
  }

  ~ThreadPool() {
    io_service_.stop();
    threads_.join_all();
  }

 private:
  boost::asio::io_service io_service_;
  boost::asio::io_service::work work_;
  boost::thread_group threads_;
  size_t size_;
};

#endif // _CROW_FRACTAL_SERVER_THREAD_POOL_
