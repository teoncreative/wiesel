
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "w_pch.hpp"

namespace Wiesel {

class DeletionQueue {
 public:
  using DeleteFn = std::function<void()>;

  void Push(DeleteFn&& fn, uint32_t frames_to_wait = 2) {
    entries_.push_back({std::move(fn), frames_to_wait});
  }

  // Call once per frame after presenting. Ticks down all counters and
  // executes deletions whose wait has expired.
  void Flush() {
    for (auto it = entries_.begin(); it != entries_.end();) {
      if (it->frames_remaining == 0) {
        it->fn();
        it = entries_.erase(it);
      } else {
        --it->frames_remaining;
        ++it;
      }
    }
  }

  // Force-delete everything immediately (e.g. on shutdown).
  void FlushAll() {
    for (auto& entry : entries_) {
      entry.fn();
    }
    entries_.clear();
  }

 private:
  struct Entry {
    DeleteFn fn;
    uint32_t frames_remaining;
  };

  std::vector<Entry> entries_;
};

}  // namespace Wiesel