//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "wpak/wpak.h"

#include <urkern/buffer.h>

#include <algorithm>
#include <fstream>
#include <optional>
#include <vector>

namespace Wpak {

const char* ErrorToString(Error error) {
  switch (error) {
    case Error::kFileOpenFailed:
      return "Failed to open file";
    case Error::kInvalidMagic:
      return "Not a valid WPAK archive";
    case Error::kUnsupportedVersion:
      return "Unsupported WPAK version";
    case Error::kCompressionUnsupported:
      return "Compressed entries not yet supported";
    case Error::kDirectoryNotFound:
      return "Directory does not exist";
    case Error::kPathNotADirectory:
      return "Path is not a directory";
    case Error::kEmptyVfsPrefix:
      return "VFS prefix is required";
    case Error::kFileCreateFailed:
      return "Failed to create file";
    case Error::kFileReadFailed:
      return "Failed to read file";
  }
  return "Unknown error";
}

// Read exactly n bytes from `f` into a new Buffer positioned for reading.
// Returns nullopt on short read / IO error.
static std::optional<urkern::Buffer> ReadChunk(std::ifstream& f, size_t n) {
  urkern::Buffer buf;
  buf.ReserveExact(n);
  f.read(buf.data_mutable(), static_cast<std::streamsize>(n));
  if (!f) {
    return std::nullopt;
  }
  buf.set_write_cursor(n);
  return buf;
}

bool IsWpakFile(const std::filesystem::path& path) {
  if (!std::filesystem::exists(path) ||
      !std::filesystem::is_regular_file(path)) {
    return false;
  }
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    return false;
  }
  char magic[4] = {};
  file.read(magic, 4);
  return std::strncmp(magic, "WPAK", 4) == 0;
}

std::expected<Archive, Error> LoadArchive(
    const std::filesystem::path& archive_path) {
  std::ifstream file(archive_path, std::ios::binary);
  if (!file.is_open()) {
    return std::unexpected(Error::kFileOpenFailed);
  }

  // Header: magic (4) + version (u32) + entry_count (u32)
  auto header = ReadChunk(file, 4 + sizeof(uint32_t) + sizeof(uint32_t));
  if (!header) {
    return std::unexpected(Error::kFileReadFailed);
  }
  char magic[4];
  header->Read(magic, 4);
  if (std::strncmp(magic, "WPAK", 4) != 0) {
    return std::unexpected(Error::kInvalidMagic);
  }
  uint32_t version = header->ReadInt<uint32_t>();
  if (version != kCurrentVersion) {
    return std::unexpected(Error::kUnsupportedVersion);
  }
  uint32_t entry_count = header->ReadInt<uint32_t>();

  Archive archive;
  archive.path = std::filesystem::absolute(archive_path);
  archive.version = version;

  for (uint32_t i = 0; i < entry_count; i++) {
    auto name_len_buf = ReadChunk(file, sizeof(uint32_t));
    if (!name_len_buf) {
      return std::unexpected(Error::kFileReadFailed);
    }
    uint32_t name_length = name_len_buf->ReadInt<uint32_t>();

    size_t rest_size =
        name_length + sizeof(uint64_t) * 3 + sizeof(uint8_t);
    auto entry_buf = ReadChunk(file, rest_size);
    if (!entry_buf) {
      return std::unexpected(Error::kFileReadFailed);
    }

    ArchiveEntry entry;
    entry.name.resize(name_length);
    entry_buf->Read(entry.name.data(), name_length);
    entry.offset = entry_buf->ReadInt<uint64_t>();
    entry.size = entry_buf->ReadInt<uint64_t>();
    entry.compressed_size = entry_buf->ReadInt<uint64_t>();
    entry.compressed = entry_buf->ReadInt<uint8_t>() != 0;

    archive.entries[entry.name] = std::move(entry);
  }

  return archive;
}

std::expected<std::vector<uint8_t>, Error> ReadEntry(
    const Archive& archive, const ArchiveEntry& entry) {
  std::ifstream file(archive.path, std::ios::binary);
  if (!file.is_open()) {
    return std::unexpected(Error::kFileOpenFailed);
  }

  file.seekg(entry.offset);

  if (entry.compressed) {
    return std::unexpected(Error::kCompressionUnsupported);
  }

  std::vector<uint8_t> data(entry.size);
  file.read(reinterpret_cast<char*>(data.data()), entry.size);
  return data;
}

std::expected<std::vector<PackEntry>, Error> CollectFiles(
    const std::filesystem::path& input_dir) {
  if (!std::filesystem::exists(input_dir)) {
    return std::unexpected(Error::kDirectoryNotFound);
  }
  if (!std::filesystem::is_directory(input_dir)) {
    return std::unexpected(Error::kPathNotADirectory);
  }

  std::vector<PackEntry> files;
  for (const auto& dir_entry :
       std::filesystem::recursive_directory_iterator(input_dir)) {
    if (dir_entry.is_regular_file()) {
      PackEntry fe;
      fe.full_path = dir_entry.path();
      fe.relative_path = std::filesystem::relative(dir_entry.path(), input_dir)
                             .generic_string();
      fe.size = std::filesystem::file_size(dir_entry.path());
      files.push_back(fe);
    }
  }

  return files;
}

// Write a single archive from a subset of files.
static std::expected<void, Error> WriteSingleArchive(
    const std::filesystem::path& output_path,
    const std::vector<const PackEntry*>& files, const std::string& vfs_prefix,
    ProgressCallback progress, size_t global_offset, size_t global_total) {
  std::ofstream pak(output_path, std::ios::binary);
  if (!pak.is_open()) {
    return std::unexpected(Error::kFileCreateFailed);
  }

  std::vector<std::string> prefixed_names;
  prefixed_names.reserve(files.size());
  for (const auto* file : files) {
    prefixed_names.push_back(vfs_prefix + file->relative_path);
  }

  // Compute where the data section begins (right after all metadata).
  uint64_t data_offset = 4 + sizeof(uint32_t) + sizeof(uint32_t);
  for (size_t i = 0; i < files.size(); i++) {
    data_offset += sizeof(uint32_t);            // name length
    data_offset += prefixed_names[i].length();  // name
    data_offset += sizeof(uint64_t) * 3;        // offset, size, compressed_size
    data_offset += sizeof(uint8_t);             // compressed flag
  }

  // Build the entire metadata blob in a buffer, then dump at once.
  urkern::Buffer meta;
  meta.Write("WPAK", 4);
  meta.WriteInt<uint32_t>(kCurrentVersion);
  meta.WriteInt<uint32_t>(static_cast<uint32_t>(files.size()));

  uint64_t current_offset = data_offset;
  for (size_t i = 0; i < files.size(); i++) {
    meta.WriteInt<uint32_t>(static_cast<uint32_t>(prefixed_names[i].length()));
    meta.Write(prefixed_names[i].data(), prefixed_names[i].length());
    meta.WriteInt<uint64_t>(current_offset);
    meta.WriteInt<uint64_t>(files[i]->size);
    meta.WriteInt<uint64_t>(files[i]->size);  // compressed_size == size
    meta.WriteInt<uint8_t>(0);                // compressed flag
    current_offset += files[i]->size;
  }

  pak.write(meta.data(), static_cast<std::streamsize>(meta.size()));

  // Stream file payloads directly from source files into the archive.
  for (size_t i = 0; i < files.size(); i++) {
    const auto* file = files[i];

    std::ifstream input(file->full_path, std::ios::binary);
    if (!input.is_open()) {
      return std::unexpected(Error::kFileReadFailed);
    }

    std::vector<char> buffer(file->size);
    input.read(buffer.data(), file->size);
    pak.write(buffer.data(), file->size);

    if (progress) {
      progress(global_offset + i, global_total, prefixed_names[i]);
    }
  }

  return {};
}

std::expected<std::vector<std::filesystem::path>, Error> WriteArchive(
    const std::filesystem::path& output_dir,
    const std::vector<PackEntry>& files, const std::string& vfs_prefix,
    int start_index, ProgressCallback progress) {
  if (vfs_prefix.empty()) {
    return std::unexpected(Error::kEmptyVfsPrefix);
  }

  std::filesystem::create_directories(output_dir);

  // Partition files into chunks that respect kIdealArchiveSize
  std::vector<std::vector<const PackEntry*>> chunks;
  uint64_t current_chunk_size = 0;
  chunks.emplace_back();

  for (const auto& file : files) {
    if (!chunks.back().empty() &&
        current_chunk_size + file.size > kIdealArchiveSize) {
      chunks.emplace_back();
      current_chunk_size = 0;
    }
    chunks.back().push_back(&file);
    current_chunk_size += file.size;
  }

  std::vector<std::filesystem::path> output_paths;
  size_t global_offset = 0;

  for (size_t i = 0; i < chunks.size(); i++) {
    char name[32];
    std::snprintf(name, sizeof(name), "pak_%02d.wpak",
                  start_index + static_cast<int>(i));
    auto chunk_path = output_dir / name;

    auto status = WriteSingleArchive(chunk_path, chunks[i], vfs_prefix,
                                     progress, global_offset, files.size());
    if (!status) {
      return std::unexpected(status.error());
    }
    output_paths.push_back(chunk_path);
    global_offset += chunks[i].size();
  }

  return output_paths;
}

}  // namespace Wpak
