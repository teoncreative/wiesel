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
// Created by Metehan Gezer on 12.02.2026.
//

#include "util/w_vfs.hpp"
#include "util/w_logger.hpp"

namespace Wiesel {

VfsFile::VfsFile(std::vector<uint8_t> data, std::string path)
    : data_(std::move(data)), path_(std::move(path)) {}

size_t VfsFile::Read(void* buffer, size_t size) {
  size_t available = data_.size() - position_;
  size_t to_read = std::min(size, available);
  std::memcpy(buffer, data_.data() + position_, to_read);
  position_ += to_read;
  return to_read;
}

std::vector<uint8_t> VfsFile::ReadBytes(size_t count) {
  size_t available = data_.size() - position_;
  size_t to_read = std::min(count, available);
  std::vector<uint8_t> result(data_.begin() + position_,
                              data_.begin() + position_ + to_read);
  position_ += to_read;
  return result;
}

void VfsFile::Seek(size_t position) {
  position_ = std::min(position, data_.size());
}

void VfsFile::SeekRelative(int64_t offset) {
  int64_t new_pos = static_cast<int64_t>(position_) + offset;
  if (new_pos < 0) {
    new_pos = 0;
  }
  position_ = std::min(static_cast<size_t>(new_pos), data_.size());
}

size_t VfsFile::Tell() const {
  return position_;
}

size_t VfsFile::Size() const {
  return data_.size();
}

const std::string& VfsFile::Path() const {
  return path_;
}

bool VfsFile::IsEof() const {
  return position_ >= data_.size();
}

const uint8_t* VfsFile::Data() const {
  return data_.data();
}

std::istream& VfsFile::Stream() {
  if (!stream_) {
    membuf_ = std::make_unique<MemBuf>(data_.data(), data_.size());
    stream_ = std::make_unique<std::istream>(membuf_.get());
  }
  return *stream_;
}

std::vector<char> VfsFile::AsChars() const {
  return std::vector<char>(data_.begin(), data_.end());
}

std::vector<uint32_t> VfsFile::AsUint32() const {
  size_t count = data_.size() / sizeof(uint32_t);
  std::vector<uint32_t> result(count);
  std::memcpy(result.data(), data_.data(), count * sizeof(uint32_t));
  return result;
}

void VirtualFileSystem::Mount(const std::string& mount_point,
                              const std::filesystem::path& path, int priority) {
  std::filesystem::path abs_path = std::filesystem::absolute(path);

  if (!std::filesystem::exists(abs_path)) {
    throw std::runtime_error("Path does not exist: " + path.string());
  }

  std::string normalized_mount = NormalizePath(mount_point);

  if (std::filesystem::is_regular_file(abs_path)) {
    if (!Wpak::IsWpakFile(abs_path)) {
      throw std::runtime_error("Not a valid .pak archive: " + path.string());
    }

    auto result = Wpak::LoadArchive(abs_path);
    if (!result) {
      throw std::runtime_error(result.error.message);
    }

    archives_[normalized_mount] = std::move(result.value);

    MountPoint mp;
    mp.mount_point = normalized_mount;
    mp.physical_path = abs_path;
    mp.priority = priority;
    mp.is_archive = true;

    mount_points_.push_back(mp);
  } else {
    MountPoint mp;
    mp.mount_point = normalized_mount;
    mp.physical_path = abs_path;
    mp.priority = priority;
    mp.is_archive = false;

    mount_points_.push_back(mp);
  }

  std::sort(mount_points_.begin(), mount_points_.end());
}

VfsFile VirtualFileSystem::Open(const std::string& virtual_path) {
  std::string normalized = NormalizePath(virtual_path);

  for (const auto& mp : mount_points_) {
    if (normalized.find(mp.mount_point) != 0) {
      continue;
    }

    std::string relative_path = normalized.substr(mp.mount_point.length());
    if (!relative_path.empty() && relative_path[0] == '/') {
      relative_path = relative_path.substr(1);
    }

    if (mp.is_archive) {
      auto archive_it = archives_.find(mp.mount_point);
      if (archive_it != archives_.end()) {
        const auto& archive = archive_it->second;
        auto entry_it = archive.entries.find(relative_path);

        if (entry_it != archive.entries.end()) {
          auto result = Wpak::ReadEntry(archive, entry_it->second);
          if (result) {
            return VfsFile(std::move(result.value), virtual_path);
          }
          LOG_ERROR("VFS: failed to read from archive: {}",
                    result.error.message);
        }
      }
    } else {
      std::filesystem::path full_path = mp.physical_path / relative_path;

      if (std::filesystem::exists(full_path) &&
          std::filesystem::is_regular_file(full_path)) {
        std::ifstream file(full_path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
          continue;
        }

        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<uint8_t> buffer(size);
        file.read(reinterpret_cast<char*>(buffer.data()), size);

        return VfsFile(std::move(buffer), virtual_path);
      }
    }
  }

  LOG_WARN("VFS: file not found: {}", virtual_path);
  return {};
}

bool VirtualFileSystem::FileExists(const std::string& virtual_path) {
  std::string normalized = NormalizePath(virtual_path);

  for (const auto& mp : mount_points_) {
    if (normalized.find(mp.mount_point) != 0) {
      continue;
    }

    std::string relative_path = normalized.substr(mp.mount_point.length());
    if (!relative_path.empty() && relative_path[0] == '/') {
      relative_path = relative_path.substr(1);
    }

    if (mp.is_archive) {
      auto archive_it = archives_.find(mp.mount_point);
      if (archive_it != archives_.end()) {
        if (archive_it->second.entries.find(relative_path) !=
            archive_it->second.entries.end()) {
          return true;
        }
      }
    } else {
      std::filesystem::path full_path = mp.physical_path / relative_path;
      if (std::filesystem::exists(full_path)) {
        return true;
      }
    }
  }

  return false;
}

std::vector<std::string> VirtualFileSystem::ListFiles(
    const std::string& virtual_dir, bool recursive) {
  std::string normalized = NormalizePath(virtual_dir);
  std::vector<std::string> results;

  for (const auto& mp : mount_points_) {
    if (normalized.find(mp.mount_point) != 0) {
      continue;
    }

    std::string relative_path = normalized.substr(mp.mount_point.length());
    if (!relative_path.empty() && relative_path[0] == '/') {
      relative_path = relative_path.substr(1);
    }

    if (mp.is_archive) {
      auto archive_it = archives_.find(mp.mount_point);
      if (archive_it != archives_.end()) {
        for (const auto& [name, entry] : archive_it->second.entries) {
          if (relative_path.empty() || name.find(relative_path) == 0) {
            std::string full_virtual_path = mp.mount_point + "/" + name;
            results.push_back(full_virtual_path);
          }
        }
      }
    } else {
      std::filesystem::path full_path = mp.physical_path / relative_path;

      if (std::filesystem::exists(full_path) &&
          std::filesystem::is_directory(full_path)) {
        if (recursive) {
          for (const auto& entry :
               std::filesystem::recursive_directory_iterator(full_path)) {
            if (entry.is_regular_file()) {
              std::filesystem::path rel =
                  std::filesystem::relative(entry.path(), mp.physical_path);
              std::string virtual_path =
                  mp.mount_point + "/" + rel.generic_string();
              results.push_back(virtual_path);
            }
          }
        } else {
          for (const auto& entry :
               std::filesystem::directory_iterator(full_path)) {
            if (entry.is_regular_file()) {
              std::filesystem::path rel =
                  std::filesystem::relative(entry.path(), mp.physical_path);
              std::string virtual_path =
                  mp.mount_point + "/" + rel.generic_string();
              results.push_back(virtual_path);
            }
          }
        }
      }
    }
  }

  // Remove duplicates (same file from different mount points)
  std::sort(results.begin(), results.end());
  results.erase(std::unique(results.begin(), results.end()), results.end());

  return results;
}

std::optional<std::filesystem::path> VirtualFileSystem::GetPhysicalPath(
    const std::string& virtual_path) {
  std::string normalized = NormalizePath(virtual_path);

  for (const auto& mp : mount_points_) {
    if (normalized.find(mp.mount_point) != 0) {
      continue;
    }

    std::string relative_path = normalized.substr(mp.mount_point.length());
    if (!relative_path.empty() && relative_path[0] == '/') {
      relative_path = relative_path.substr(1);
    }

    if (mp.is_archive) {
      continue;
    }

    std::filesystem::path full_path = mp.physical_path / relative_path;
    if (std::filesystem::exists(full_path)) {
      return full_path;
    }
  }

  return std::nullopt;
}

void VirtualFileSystem::Unmount(const std::string& mount_point) {
  std::string normalized = NormalizePath(mount_point);

  mount_points_.erase(std::remove_if(mount_points_.begin(), mount_points_.end(),
                                     [&normalized](const MountPoint& mp) {
                                       return mp.mount_point == normalized;
                                     }),
                      mount_points_.end());

  archives_.erase(normalized);
}

void VirtualFileSystem::Clear() {
  mount_points_.clear();
  archives_.clear();
}

std::string VirtualFileSystem::NormalizePath(const std::string& path) {
  std::string normalized = path;

  // Replace backslashes with forward slashes
  std::replace(normalized.begin(), normalized.end(), '\\', '/');

  // Remove trailing slash
  if (!normalized.empty() && normalized.back() == '/') {
    normalized.pop_back();
  }

  // Ensure it starts with /
  if (normalized.empty() || normalized[0] != '/') {
    normalized = "/" + normalized;
  }

  return normalized;
}

}  // namespace Wiesel