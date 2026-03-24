#pragma once

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace Wpak {

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

// Error-as-value types

struct Error {
  std::string message;
};

template <typename T>
struct Result {
  bool success;
  T value;
  Error error;

  static Result Ok(T val) { return {true, std::move(val), {}}; }

  static Result Fail(std::string msg) {
    return {false, {}, Error{std::move(msg)}};
  }

  explicit operator bool() const { return success; }
};

struct Status {
  bool success;
  Error error;

  static Status Ok() { return {true, {}}; }

  static Status Fail(std::string msg) { return {false, Error{std::move(msg)}}; }

  explicit operator bool() const { return success; }
};

// --- Reading API ---

Result<Archive> LoadArchive(const std::filesystem::path& archive_path);

Result<std::vector<uint8_t>> ReadEntry(const Archive& archive,
                                       const ArchiveEntry& entry);

bool IsWpakFile(const std::filesystem::path& path);

// --- Writing API ---

struct PackEntry {
  std::string relative_path;
  std::filesystem::path full_path;
  uint64_t size;
};

Result<std::vector<PackEntry>> CollectFiles(
    const std::filesystem::path& input_dir);

using ProgressCallback =
    std::function<void(size_t index, size_t total, const std::string& name)>;

Status WriteArchive(const std::filesystem::path& output_path,
                    const std::vector<PackEntry>& files,
                    ProgressCallback progress = nullptr);

}  // namespace Wpak