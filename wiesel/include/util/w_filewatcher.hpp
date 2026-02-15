//
// Created by Metehan Gezer on 14.02.2026.
//

#pragma once

#include "w_pch.hpp"

namespace Wiesel {

class FileWatcher {
 public:
  FileWatcher() = default;

  void Watch(const std::filesystem::path& directory, bool recursive = true);

  // Returns true if any watched files have changed since the last poll.
  bool Poll();

  bool IsWatching() const { return !watch_dir_.empty(); }

 private:
  void ScanFiles();

  std::filesystem::path watch_dir_;
  bool recursive_ = true;
  std::unordered_map<std::string, std::filesystem::file_time_type> file_times_;
};

}  // namespace Wiesel
