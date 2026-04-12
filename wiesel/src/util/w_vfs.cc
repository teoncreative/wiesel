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

#include "util/w_vfs.h"

#include <ranges>
#include <set>
#include "util/w_logger.h"

namespace Wiesel {

// Join a base path and relative path without producing double slashes.
static std::string VfsJoin(const std::string& base,
                           const std::string& relative) {
  if (base.empty()) {
    return relative;
  }
  if (relative.empty()) {
    return base;
  }
  bool base_has_slash = (base.back() == '/');
  bool rel_has_slash = (relative.front() == '/');
  if (base_has_slash && rel_has_slash) {
    return base + relative.substr(1);
  }
  if (!base_has_slash && !rel_has_slash) {
    return base + "/" + relative;
  }
  return base + relative;
}

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

bool VirtualFileSystem::DirectoryExists(const std::string& virtual_path) {
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
      // Check if any archive entry starts with this prefix
      auto archive_it = archives_.find(mp.mount_point);
      if (archive_it != archives_.end()) {
        std::string prefix = relative_path.empty() ? "" : relative_path + "/";
        for (const auto& [name, entry] : archive_it->second.entries) {
          if (name.find(prefix) == 0) {
            return true;
          }
        }
      }
    } else {
      std::filesystem::path full_path = mp.physical_path / relative_path;
      if (std::filesystem::exists(full_path) &&
          std::filesystem::is_directory(full_path)) {
        return true;
      }
    }
  }
  return false;
}

std::vector<VfsEntry> VirtualFileSystem::ListDirectory(
    const std::string& virtual_dir) {
  std::string normalized = NormalizePath(virtual_dir);
  std::vector<VfsEntry> results;
  std::set<std::string> seen;

  for (const MountPoint& mp : mount_points_) {
    if (normalized.find(mp.mount_point) != 0) {
      continue;
    }

    std::string relative_path = normalized.substr(mp.mount_point.length());
    if (!relative_path.empty() && relative_path[0] == '/') {
      relative_path = relative_path.substr(1);
    }

    if (mp.is_archive) {
      auto archive_it = archives_.find(mp.mount_point);
      if (archive_it == archives_.end()) {
        continue;
      }
      std::string prefix = relative_path.empty() ? "" : relative_path + "/";
      std::set<std::string> archive_dirs;
      for (const std::string& name :
           archive_it->second.entries | std::views::keys) {
        if (!prefix.empty() && name.find(prefix) != 0) {
          continue;
        }
        std::string remainder = name.substr(prefix.length());
        size_t slash = remainder.find('/');
        if (slash != std::string::npos) {
          // This is a subdirectory entry
          std::string dir_name = remainder.substr(0, slash);
          if (archive_dirs.insert(dir_name).second && !seen.count(dir_name)) {
            seen.insert(dir_name);
            VfsEntry e;
            e.name = dir_name;
            e.vfs_path = VfsJoin(normalized, dir_name);
            e.is_dir = true;
            results.push_back(std::move(e));
          }
        } else {
          // Direct file in this directory
          if (seen.count(remainder)) {
            continue;
          }
          // Skip .meta files
          if (remainder.size() > 5 &&
              remainder.substr(remainder.size() - 5) == ".meta") {
            continue;
          }
          seen.insert(remainder);
          VfsEntry e;
          e.name = remainder;
          e.vfs_path = VfsJoin(normalized, remainder);
          e.is_dir = false;
          results.push_back(std::move(e));
        }
      }
    } else {
      std::filesystem::path full_path = mp.physical_path / relative_path;
      if (!std::filesystem::exists(full_path) ||
          !std::filesystem::is_directory(full_path)) {
        continue;
      }
      for (const auto& entry : std::filesystem::directory_iterator(full_path)) {
        std::string name = entry.path().filename().string();
        if (seen.contains(name)) {
          continue;
        }
        // Skip .meta files
        if (!entry.is_directory() && entry.path().extension() == ".meta") {
          continue;
        }
        seen.insert(name);
        VfsEntry e;
        e.name = name;
        std::filesystem::path rel =
            std::filesystem::relative(entry.path(), mp.physical_path);
        e.vfs_path = VfsJoin(mp.mount_point, rel.generic_string());
        e.is_dir = entry.is_directory();
        results.push_back(std::move(e));
      }
    }
  }

  // Include virtual entries (memory-only assets like built-in primitives)
  {
    std::string prefix = normalized;
    if (!prefix.empty() && prefix.back() != '/') {
      prefix += '/';
    }
    std::set<std::string> virtual_dirs;
    for (const std::string& vpath : virtual_entries_) {
      if (vpath.find(prefix) != 0) {
        continue;
      }
      std::string remainder = vpath.substr(prefix.length());
      size_t slash = remainder.find('/');
      if (slash != std::string::npos) {
        std::string dir_name = remainder.substr(0, slash);
        if (virtual_dirs.insert(dir_name).second && !seen.count(dir_name)) {
          seen.insert(dir_name);
          VfsEntry e;
          e.name = dir_name;
          e.vfs_path = VfsJoin(normalized, dir_name);
          e.is_dir = true;
          results.push_back(std::move(e));
        }
      } else {
        if (seen.count(remainder)) {
          continue;
        }
        seen.insert(remainder);
        VfsEntry e;
        e.name = remainder;
        e.vfs_path = VfsJoin(normalized, remainder);
        e.is_dir = false;
        results.push_back(std::move(e));
      }
    }
  }

  return results;
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
            std::string full_virtual_path = VfsJoin(mp.mount_point, name);
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
                  VfsJoin(mp.mount_point, rel.generic_string());
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
                  VfsJoin(mp.mount_point, rel.generic_string());
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

// Resolve a VFS path to a physical path even if the target doesn't exist yet.
// Falls back to mount point resolution when GetPhysicalPath returns nullopt.
std::optional<std::filesystem::path> VirtualFileSystem::ResolvePhysicalPath(
    const std::string& vfs_path) {
  auto physical = GetPhysicalPath(vfs_path);
  if (physical.has_value()) {
    return physical;
  }
  std::string normalized = NormalizePath(vfs_path);
  for (const auto& mp : mount_points_) {
    if (mp.is_archive || normalized.find(mp.mount_point) != 0) {
      continue;
    }
    std::string relative = normalized.substr(mp.mount_point.length());
    if (!relative.empty() && relative[0] == '/') {
      relative = relative.substr(1);
    }
    return mp.physical_path / relative;
  }
  return std::nullopt;
}

bool VirtualFileSystem::RenameFile(const std::string& old_vfs_path,
                                   const std::string& new_vfs_path) {
  auto old_physical = GetPhysicalPath(old_vfs_path);
  auto new_physical = ResolvePhysicalPath(new_vfs_path);
  if (!old_physical || !new_physical) {
    return false;
  }
  std::error_code ec;
  std::filesystem::rename(*old_physical, *new_physical, ec);
  return !ec;
}

bool VirtualFileSystem::CopyFile(const std::string& src_vfs_path,
                                 const std::string& dest_vfs_path) {
  auto src = GetPhysicalPath(src_vfs_path);
  auto dest = ResolvePhysicalPath(dest_vfs_path);
  if (!src || !dest) {
    return false;
  }
  std::error_code ec;
  std::filesystem::copy_file(*src, *dest, ec);
  return !ec;
}

bool VirtualFileSystem::CopyDirectory(const std::string& src_vfs_path,
                                      const std::string& dest_vfs_path) {
  auto src = GetPhysicalPath(src_vfs_path);
  auto dest = ResolvePhysicalPath(dest_vfs_path);
  if (!src || !dest) {
    return false;
  }
  std::error_code ec;
  std::filesystem::copy(*src, *dest, std::filesystem::copy_options::recursive,
                        ec);
  return !ec;
}

bool VirtualFileSystem::DeleteFile(const std::string& vfs_path) {
  auto physical = GetPhysicalPath(vfs_path);
  if (!physical) {
    return false;
  }
  std::error_code ec;
  std::filesystem::remove(*physical, ec);
  return !ec;
}

bool VirtualFileSystem::DeleteDirectory(const std::string& vfs_path) {
  auto physical = GetPhysicalPath(vfs_path);
  if (!physical) {
    return false;
  }
  std::error_code ec;
  std::filesystem::remove_all(*physical, ec);
  return !ec;
}

bool VirtualFileSystem::CreateDirectory(const std::string& vfs_path) {
  auto physical = ResolvePhysicalPath(vfs_path);
  if (!physical) {
    return false;
  }
  std::error_code ec;
  std::filesystem::create_directories(*physical, ec);
  return !ec;
}

bool VirtualFileSystem::WriteFile(const std::string& vfs_path,
                                  const std::string& content) {
  auto physical = ResolvePhysicalPath(vfs_path);
  if (!physical) {
    return false;
  }
  // Ensure parent directory exists
  std::filesystem::create_directories(physical->parent_path());
  std::ofstream out(*physical);
  if (!out.is_open()) {
    return false;
  }
  out << content;
  return out.good();
}

void VirtualFileSystem::RegisterVirtualEntry(const std::string& vfs_path) {
  virtual_entries_.insert(NormalizePath(vfs_path));
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

  // Remove trailing slash (but preserve "scheme://")
  while (normalized.size() > 1 && normalized.back() == '/') {
    // Don't strip the slash that's part of "://"
    if (normalized.size() >= 3 && normalized[normalized.size() - 2] == '/' &&
        normalized[normalized.size() - 3] == ':') {
      break;
    }
    normalized.pop_back();
  }

  // Collapse triple slashes from scheme + path joining (e.g. "app:///foo" -> "app://foo")
  size_t scheme_end = normalized.find("://");
  if (scheme_end != std::string::npos) {
    size_t after_scheme = scheme_end + 3;
    while (after_scheme < normalized.size() &&
           normalized[after_scheme] == '/') {
      normalized.erase(after_scheme, 1);
    }
  }

  return normalized;
}

std::string VirtualFileSystem::FileName(const std::string& vfs_path) {
  auto slash = vfs_path.rfind('/');
  if (slash == std::string::npos) {
    return vfs_path;
  }
  return vfs_path.substr(slash + 1);
}

std::string VirtualFileSystem::Stem(const std::string& vfs_path) {
  std::string name = FileName(vfs_path);
  auto dot = name.rfind('.');
  if (dot == std::string::npos || dot == 0) {
    return name;
  }
  return name.substr(0, dot);
}

std::string VirtualFileSystem::Extension(const std::string& vfs_path) {
  std::string name = FileName(vfs_path);
  auto dot = name.rfind('.');
  if (dot == std::string::npos || dot == 0) {
    return "";
  }
  return name.substr(dot);
}

std::string VirtualFileSystem::Parent(const std::string& vfs_path) {
  auto slash = vfs_path.rfind('/');
  if (slash == std::string::npos) {
    return "";
  }
  return vfs_path.substr(0, slash);
}

}  // namespace Wiesel