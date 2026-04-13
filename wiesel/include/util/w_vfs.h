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

#ifndef WIESEL_PARENT_W_VFS_HPP
#define WIESEL_PARENT_W_VFS_HPP

#pragma once

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <istream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <vector>

#include <wpak/wpak.h>

namespace Wiesel {

class VfsFile {
 public:
  VfsFile() = default;
  VfsFile(std::vector<uint8_t> data, std::string path);

  // Returns true if the file was opened successfully (has data)
  explicit operator bool() const { return !data_.empty(); }

  // Stream-like reading
  template <typename T>
  T Read();

  size_t Read(void* buffer, size_t size);
  std::vector<uint8_t> ReadBytes(size_t count);

  // Positioning
  void Seek(size_t position);
  void SeekRelative(int64_t offset);
  size_t Tell() const;

  // Info
  size_t Size() const;
  const std::string& Path() const;
  bool IsEof() const;

  // Bulk access
  const uint8_t* Data() const;
  std::vector<char> AsChars() const;
  std::vector<uint32_t> AsUint32() const;

  // std::istream compatibility
  std::istream& Stream();

 private:
  class MemBuf : public std::streambuf {
   public:
    MemBuf(const uint8_t* data, size_t size) {
      char* p = const_cast<char*>(reinterpret_cast<const char*>(data));
      setg(p, p, p + size);
    }
  };

  std::vector<uint8_t> data_;
  std::string path_;
  size_t position_ = 0;

  std::unique_ptr<MemBuf> membuf_;
  std::unique_ptr<std::istream> stream_;
};

// Template implementation
template <typename T>
T VfsFile::Read() {
  if (position_ + sizeof(T) > data_.size()) {
    throw std::runtime_error("VfsFile::Read past end of file: " + path_);
  }
  T value;
  std::memcpy(&value, data_.data() + position_, sizeof(T));
  position_ += sizeof(T);
  return value;
}

struct VfsEntry {
  std::string name;      // filename only, e.g. "player.gltf"
  std::string vfs_path;  // full VFS path, e.g. "app://models/player.gltf"
  bool is_dir = false;
};

class VirtualFileSystem {
 public:
  // Mount a physical directory to a VFS prefix.
  void Mount(const std::string& mount_point, const std::filesystem::path& path,
             int priority = 0);

  void MountPak(const std::filesystem::path& wpak_path, int priority = 0);

  VfsFile Open(const std::string& virtual_path);
  bool FileExists(const std::string& virtual_path);
  bool DirectoryExists(const std::string& virtual_path);
  std::vector<std::string> ListFiles(const std::string& virtual_dir,
                                     bool recursive = false);

  // List files and directories in a single directory level.
  // Filters out .meta sidecar files.
  std::vector<VfsEntry> ListDirectory(const std::string& virtual_dir);

  std::optional<std::filesystem::path> GetPhysicalPath(
      const std::string& virtual_path);

  // File operations via VFS paths. Only work for physical (non-archive) mounts.
  // Returns true on success.
  bool RenameFile(const std::string& old_vfs_path,
                  const std::string& new_vfs_path);
  bool CopyFile(const std::string& src_vfs_path,
                const std::string& dest_vfs_path);
  bool CopyDirectory(const std::string& src_vfs_path,
                     const std::string& dest_vfs_path);
  bool DeleteFile(const std::string& vfs_path);
  bool DeleteDirectory(const std::string& vfs_path);
  bool CreateDirectory(const std::string& vfs_path);

  // Write string content to a VFS path. Creates parent directories.
  // Only works for physical (non-archive) mounts.
  bool WriteFile(const std::string& vfs_path, const std::string& content);

  // Register a virtual file entry so ListDirectory can show assets that
  // only exist in memory (e.g. built-in primitives). Creates intermediate
  // virtual directories automatically.
  void RegisterVirtualEntry(const std::string& vfs_path);

  void Unmount(const std::string& mount_point);
  void Clear();

  // VFS path utilities (pure string operations, no VFS state needed)

  // "app://scenes/main.wscene" -> "main.wscene"
  static std::string FileName(const std::string& vfs_path);
  // "app://scenes/main.wscene" -> "main"
  static std::string Stem(const std::string& vfs_path);
  // "app://scenes/main.wscene" -> ".wscene"
  static std::string Extension(const std::string& vfs_path);
  // "app://scenes/main.wscene" -> "app://scenes"
  static std::string Parent(const std::string& vfs_path);

 private:
  struct MountPoint {
    std::string mount_point;  // VFS prefix for dirs, archive path for paks
    std::filesystem::path physical_path;
    int priority;
    bool is_archive;

    bool operator<(const MountPoint& other) const {
      return priority > other.priority;  // Higher priority first
    }
  };

  std::vector<MountPoint> mount_points_;
  std::map<std::string, Wpak::Archive> archives_;
  std::set<std::string> virtual_entries_;

  std::string NormalizePath(const std::string& path);

  // Archive entry lookup by full VFS path. Returns nullptr if not found.
  const Wpak::ArchiveEntry* FindArchiveEntry(const MountPoint& mp,
                                             const std::string& normalized);

  // For directory mounts: check prefix match and return the physical path.
  // Returns nullopt if the mount doesn't match the VFS path.
  std::optional<std::filesystem::path> ResolveToPhysical(
      const MountPoint& mp, const std::string& normalized);

  // Collect VfsEntry items from entries that start with prefix.
  // Used by ListDirectory for both archives and virtual entries.
  static void CollectDirectoryEntries(const std::string& prefix,
                                      const std::string& normalized,
                                      std::set<std::string>& seen,
                                      std::vector<VfsEntry>& results,
                                      auto&& key_range);

 public:
  std::optional<std::filesystem::path> ResolvePhysicalPath(
      const std::string& vfs_path);

 private:
};

}  // namespace Wiesel

#endif  //WIESEL_PARENT_W_VFS_HPP