
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace wiesel {

class ThreadPool {
 public:
  explicit ThreadPool(uint32_t thread_count);
  ~ThreadPool();

  // Submit a task to the pool. Returns immediately.
  void Submit(std::function<void()> task);

  // Number of tasks currently queued (not yet started)
  size_t GetQueueSize() const;

  // Number of worker threads
  uint32_t GetThreadCount() const {
    return static_cast<uint32_t>(workers_.size());
  }

 private:
  std::vector<std::thread> workers_;
  std::queue<std::function<void()>> tasks_;

  mutable std::mutex mutex_;
  std::condition_variable condition_;
  bool stopping_ = false;
};

}  // namespace wiesel