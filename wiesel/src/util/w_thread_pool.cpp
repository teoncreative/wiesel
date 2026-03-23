
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "util/w_thread_pool.hpp"

namespace Wiesel {

ThreadPool::ThreadPool(uint32_t thread_count) {
  workers_.reserve(thread_count);
  for (uint32_t i = 0; i < thread_count; i++) {
    workers_.emplace_back([this]() {
      while (true) {
        std::function<void()> task;
        {
          std::unique_lock<std::mutex> lock(mutex_);
          condition_.wait(lock,
                          [this]() { return stopping_ || !tasks_.empty(); });
          if (stopping_ && tasks_.empty()) {
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

ThreadPool::~ThreadPool() {
  {
    std::unique_lock<std::mutex> lock(mutex_);
    stopping_ = true;
  }
  condition_.notify_all();
  for (auto& worker : workers_) {
    if (worker.joinable()) {
      worker.join();
    }
  }
}

void ThreadPool::Submit(std::function<void()> task) {
  {
    std::unique_lock<std::mutex> lock(mutex_);
    tasks_.push(std::move(task));
  }
  condition_.notify_one();
}

size_t ThreadPool::GetQueueSize() const {
  std::unique_lock<std::mutex> lock(mutex_);
  return tasks_.size();
}

}  // namespace Wiesel