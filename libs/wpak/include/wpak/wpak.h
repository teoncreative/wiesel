//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace Wpak {

// Soft size limit for archive splitting. A single archive will not exceed
// this unless an individual file is larger. Files are never split across
// archives.
static constexpr uint64_t kIdealArchiveSize = 256 * 1024 * 1024;  // 256 MB

static constexpr uint32_t kCurrentVersion = 2;

struct ArchiveEntry {
  std::string name;
  uint64_t offset;
  uint64_t size;
  uint64_t compressed_size;
  bool compressed;
};

struct Archive {
  std::filesystem::path path;
  uint32_t version = 0;
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

// Write one or more v2 archives into output_dir with prefixed entry names.
// Entries are named as prefix + relative_path (e.g. "engine://shaders/geo.frag").
// Archives are split at kIdealArchiveSize. Files are named pak_01.wpak, pak_02.wpak, etc.
// start_index controls the starting number (to allow appending to existing paks).
// Returns the list of created file paths.
Result<std::vector<std::filesystem::path>> WriteArchive(
    const std::filesystem::path& output_dir,
    const std::vector<PackEntry>& files, const std::string& vfs_prefix,
    int start_index = 1, ProgressCallback progress = nullptr);

}  // namespace Wpak
