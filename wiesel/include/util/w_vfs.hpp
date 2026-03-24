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
#include <stdexcept>
#include <streambuf>
#include <string>
#include <vector>

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

class VirtualFileSystem {
 public:
  // Auto-detects whether path is a directory or a .pak archive.
  // For archives, validates the WPAK magic before mounting.
  void Mount(const std::string& mount_point,
             const std::filesystem::path& path, int priority = 0);

  VfsFile Open(const std::string& virtual_path);
  bool FileExists(const std::string& virtual_path);
  std::vector<std::string> ListFiles(const std::string& virtual_dir,
                                     bool recursive = false);

  std::optional<std::filesystem::path> GetPhysicalPath(
      const std::string& virtual_path);

  void Unmount(const std::string& mount_point);
  void Clear();

 private:
  struct MountPoint {
    std::string mount_point;
    std::filesystem::path physical_path;
    int priority;
    bool is_archive;

    bool operator<(const MountPoint& other) const {
      return priority > other.priority;  // Higher priority first
    }
  };

  struct ArchiveEntry {
    std::string name;
    uint64_t offset;
    uint64_t size;
    uint64_t compressed_size;
    bool compressed;
  };

  struct Archive {
    std::filesystem::path path;
    std::map<std::string, ArchiveEntry> entries;
  };

  std::vector<MountPoint> mount_points_;
  std::map<std::string, Archive> archives_;

  std::string NormalizePath(const std::string& path);
  bool LoadArchive(const std::filesystem::path& archive_path, Archive& archive);
  std::vector<uint8_t> ReadFromArchive(const Archive& archive,
                                       const ArchiveEntry& entry);
};

}  // namespace Wiesel

#endif  //WIESEL_PARENT_W_VFS_HPP
