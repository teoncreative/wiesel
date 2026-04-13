
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

#include "w_pch.h"

namespace wiesel {

class DeletionQueue {
 public:
  using DeleteFn = std::move_only_function<void()>;

  void Push(DeleteFn fn, uint32_t frames_to_wait = 2) {
    entries_.push_back({std::move(fn), frames_to_wait});
  }

  template <typename T>
  void Defer(std::unique_ptr<T> obj, uint32_t frames_to_wait = 2) {
    Push([held = std::move(obj)]() mutable { held.reset(); }, frames_to_wait);
  }

  // Call once per frame after presenting. Ticks down all counters and
  // executes deletions whose wait has expired.
  // Safe against re-entrant Push() calls from within fn().
  void Flush() {
    // Swap out so new entries pushed during execution don't invalidate iteration
    std::vector<Entry> current;
    std::swap(current, entries_);

    for (auto& entry : current) {
      if (entry.frames_remaining == 0) {
        entry.fn();
      } else {
        --entry.frames_remaining;
        entries_.push_back(std::move(entry));
      }
    }
  }

  // Force-delete everything immediately (e.g. on shutdown).
  // Keeps flushing until empty since deletions can cascade.
  void FlushAll() {
    while (!entries_.empty()) {
      std::vector<Entry> current;
      std::swap(current, entries_);
      for (auto& entry : current) {
        entry.fn();
      }
    }
  }

  size_t Size() const { return entries_.size(); }

 private:
  struct Entry {
    DeleteFn fn;
    uint32_t frames_remaining;
  };

  std::vector<Entry> entries_;
};

}  // namespace wiesel