//
// Created by Metehan Gezer on 14.02.2026.
//

#include "util/w_filewatcher.hpp"
#include "util/w_logger.hpp"

namespace Wiesel {

void FileWatcher::Watch(const std::filesystem::path& directory, bool recursive) {
  watch_dir_ = directory;
  recursive_ = recursive;
  file_times_.clear();
  ScanFiles();
}

bool FileWatcher::Poll() {
  if (watch_dir_.empty() || !std::filesystem::exists(watch_dir_)) {
    return false;
  }

  bool changed = false;
  std::unordered_map<std::string, std::filesystem::file_time_type> current_times;

  auto scan = [&](auto iterator) {
    for (const auto& entry : iterator) {
      if (!entry.is_regular_file()) continue;
      std::string path_str = entry.path().string();
      std::filesystem::file_time_type write_time = entry.last_write_time();
      current_times[path_str] = write_time;

      auto it = file_times_.find(path_str);
      if (it == file_times_.end()) {
        // New file
        changed = true;
      } else if (it->second != write_time) {
        // Modified file
        changed = true;
      }
    }
  };

  if (recursive_) {
    scan(std::filesystem::recursive_directory_iterator(watch_dir_));
  } else {
    scan(std::filesystem::directory_iterator(watch_dir_));
  }

  // Check for removed files
  if (!changed) {
    for (const auto& [path, time] : file_times_) {
      if (current_times.find(path) == current_times.end()) {
        changed = true;
        break;
      }
    }
  }

  file_times_ = std::move(current_times);
  return changed;
}

void FileWatcher::ScanFiles() {
  if (watch_dir_.empty() || !std::filesystem::exists(watch_dir_)) {
    return;
  }

  auto scan = [&](auto iterator) {
    for (const auto& entry : iterator) {
      if (!entry.is_regular_file()) continue;
      file_times_[entry.path().string()] = entry.last_write_time();
    }
  };

  if (recursive_) {
    scan(std::filesystem::recursive_directory_iterator(watch_dir_));
  } else {
    scan(std::filesystem::directory_iterator(watch_dir_));
  }
}

}  // namespace Wiesel
