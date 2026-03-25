//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

//
// Created by Metehan Gezer on 14.02.2026.
//

#pragma once

#include <atomic>
#include <efsw/efsw.hpp>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>

namespace Wiesel {

class FileWatcher : public efsw::FileWatchListener {
 public:
  FileWatcher();
  ~FileWatcher() override;

  void Watch(const std::filesystem::path& directory, bool recursive = true);
  void Stop();

  // Returns true if any changes occurred since last call, then resets the flag.
  bool Poll();

  bool IsWatching() const { return watching_; }

  // efsw::FileWatchListener callback
  void handleFileAction(efsw::WatchID watch_id, const std::string& dir,
                        const std::string& filename, efsw::Action action,
                        std::string old_filename) override;

 private:
  std::unique_ptr<efsw::FileWatcher> watcher_;
  efsw::WatchID watch_id_ = -1;
  bool watching_ = false;
  std::atomic<bool> changed_{false};
};

}  // namespace Wiesel