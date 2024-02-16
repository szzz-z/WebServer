#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool {
 public:
  explicit ThreadPool(size_t threadNumber) {
    for (size_t i = 0; i < threadNumber; ++i) {
      m_thread_.emplace_back([this]() {
        for (;;) {
          std::function<void()> task;
          {
            std::unique_lock<std::mutex> lock(m_mutex_);
            m_cv_.wait(lock, [this]() { return m_stop_ || !tasks_.empty(); });
            if (m_stop_ && tasks_.empty()) {
              return;
            }
            task = std::move(tasks_.front());
            tasks_.pop();
          }
          task();
        }
      });
    }
  }

  ThreadPool(const ThreadPool &) = delete;
  ThreadPool(ThreadPool &&) = delete;

  auto operator=(const ThreadPool &) -> ThreadPool & = delete;
  auto operator=(ThreadPool &&) -> ThreadPool & = delete;

  ~ThreadPool() {
    {
      std::unique_lock<std::mutex> lock(m_mutex_);
      m_stop_ = true;
    }
    m_cv_.notify_all();
    for (auto &threads : m_thread_) {
      threads.join();
    }
  }

  template <typename F, typename... Args>
  auto Submit(F &&f, Args &&...args) -> std::future<decltype(f(args...))> {
    auto task_ptr = std::make_shared<std::packaged_task<decltype(f(args...))()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    {
      std::unique_lock<std::mutex> lock(m_mutex_);
      if (m_stop_) {
        throw std::runtime_error("submit on stopped ThreadPool");
      }
      tasks_.emplace([task_ptr]() { (*task_ptr)(); });
    }
    m_cv_.notify_one();
    return task_ptr->get_future();
  }

 private:
  bool m_stop_{};
  std::vector<std::thread> m_thread_;
  std::queue<std::function<void()>> tasks_;
  std::mutex m_mutex_;
  std::condition_variable m_cv_;
};

#endif  // THREADPOOL_H
