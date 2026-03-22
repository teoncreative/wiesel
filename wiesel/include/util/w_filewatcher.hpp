//
// Created by Metehan Gezer on 14.02.2026.
//

#pragma once

#include <efsw/efsw.hpp>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <filesystem>

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