//
// Created by Metehan Gezer on 14.02.2026.
//

#include "util/w_filewatcher.hpp"
#include "util/w_logger.hpp"

namespace Wiesel {

FileWatcher::FileWatcher() = default;

FileWatcher::~FileWatcher() {
  Stop();
}

void FileWatcher::Watch(const std::filesystem::path& directory,
                        bool recursive) {
  Stop();

  if (!std::filesystem::exists(directory)) {
    LOG_WARN("FileWatcher: directory does not exist: {}", directory.string());
    return;
  }

  watcher_ = std::make_unique<efsw::FileWatcher>();
  watch_id_ = watcher_->addWatch(directory.string(), this, recursive);
  if (watch_id_ < 0) {
    LOG_ERROR("FileWatcher: failed to watch: {}", directory.string());
    watcher_.reset();
    return;
  }

  watcher_->watch();
  watching_ = true;
  changed_.store(false);
  LOG_INFO("FileWatcher: watching {}", directory.string());
}

void FileWatcher::Stop() {
  if (watcher_ && watch_id_ >= 0) {
    watcher_->removeWatch(watch_id_);
  }
  watcher_.reset();
  watch_id_ = -1;
  watching_ = false;
  changed_.store(false);
}

bool FileWatcher::Poll() {
  return changed_.exchange(false);
}

void FileWatcher::handleFileAction(efsw::WatchID watch_id,
                                   const std::string& dir,
                                   const std::string& filename,
                                   efsw::Action action,
                                   std::string old_filename) {
  changed_.store(true);
}

}  // namespace Wiesel