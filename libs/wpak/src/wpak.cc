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
#include <fstream>

namespace Wpak {

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

Result<Archive> LoadArchive(const std::filesystem::path& archive_path) {
  std::ifstream file(archive_path, std::ios::binary);
  if (!file.is_open()) {
    return Result<Archive>::Fail("Failed to open archive: " +
                                 archive_path.string());
  }

  char magic[4];
  file.read(magic, 4);
  if (std::strncmp(magic, "WPAK", 4) != 0) {
    return Result<Archive>::Fail("Not a valid WPAK archive: " +
                                 archive_path.string());
  }

  uint32_t version;
  file.read(reinterpret_cast<char*>(&version), sizeof(version));
  if (version != 1) {
    return Result<Archive>::Fail("Unsupported WPAK version " +
                                 std::to_string(version) + " in " +
                                 archive_path.string());
  }

  uint32_t entry_count;
  file.read(reinterpret_cast<char*>(&entry_count), sizeof(entry_count));

  Archive archive;
  archive.path = std::filesystem::absolute(archive_path);

  for (uint32_t i = 0; i < entry_count; i++) {
    ArchiveEntry entry;

    uint32_t name_length;
    file.read(reinterpret_cast<char*>(&name_length), sizeof(name_length));

    std::string name(name_length, '\0');
    file.read(&name[0], name_length);
    entry.name = name;

    file.read(reinterpret_cast<char*>(&entry.offset), sizeof(entry.offset));
    file.read(reinterpret_cast<char*>(&entry.size), sizeof(entry.size));
    file.read(reinterpret_cast<char*>(&entry.compressed_size),
              sizeof(entry.compressed_size));

    uint8_t compressed;
    file.read(reinterpret_cast<char*>(&compressed), sizeof(compressed));
    entry.compressed = (compressed != 0);

    archive.entries[entry.name] = entry;
  }

  return Result<Archive>::Ok(std::move(archive));
}

Result<std::vector<uint8_t>> ReadEntry(const Archive& archive,
                                       const ArchiveEntry& entry) {
  std::ifstream file(archive.path, std::ios::binary);
  if (!file.is_open()) {
    return Result<std::vector<uint8_t>>::Fail("Failed to open archive: " +
                                              archive.path.string());
  }

  file.seekg(entry.offset);

  if (entry.compressed) {
    return Result<std::vector<uint8_t>>::Fail(
        "Compressed entries not yet supported");
  }

  std::vector<uint8_t> data(entry.size);
  file.read(reinterpret_cast<char*>(data.data()), entry.size);
  return Result<std::vector<uint8_t>>::Ok(std::move(data));
}

Result<std::vector<PackEntry>> CollectFiles(
    const std::filesystem::path& input_dir) {
  if (!std::filesystem::exists(input_dir)) {
    return Result<std::vector<PackEntry>>::Fail("Directory does not exist: " +
                                                input_dir.string());
  }
  if (!std::filesystem::is_directory(input_dir)) {
    return Result<std::vector<PackEntry>>::Fail("Not a directory: " +
                                                input_dir.string());
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

  return Result<std::vector<PackEntry>>::Ok(std::move(files));
}

Status WriteArchive(const std::filesystem::path& output_path,
                    const std::vector<PackEntry>& files,
                    ProgressCallback progress) {
  std::ofstream pak(output_path, std::ios::binary);
  if (!pak.is_open()) {
    return Status::Fail("Failed to create archive: " + output_path.string());
  }

  // Header
  pak.write("WPAK", 4);
  uint32_t version = 1;
  pak.write(reinterpret_cast<const char*>(&version), sizeof(version));

  uint32_t entry_count = static_cast<uint32_t>(files.size());
  pak.write(reinterpret_cast<const char*>(&entry_count), sizeof(entry_count));

  // Calculate data offset (after all metadata)
  uint64_t data_offset = 4 + sizeof(uint32_t) + sizeof(uint32_t);
  for (const auto& file : files) {
    data_offset += sizeof(uint32_t);             // name length
    data_offset += file.relative_path.length();  // name
    data_offset += sizeof(uint64_t) * 3;  // offset, size, compressed_size
    data_offset += sizeof(uint8_t);       // compressed flag
  }

  // Write metadata
  uint64_t current_offset = data_offset;
  for (const auto& file : files) {
    uint32_t name_length = static_cast<uint32_t>(file.relative_path.length());
    pak.write(reinterpret_cast<const char*>(&name_length), sizeof(name_length));
    pak.write(file.relative_path.c_str(), name_length);
    pak.write(reinterpret_cast<const char*>(&current_offset),
              sizeof(current_offset));
    pak.write(reinterpret_cast<const char*>(&file.size), sizeof(file.size));
    pak.write(reinterpret_cast<const char*>(&file.size), sizeof(file.size));
    uint8_t compressed = 0;
    pak.write(reinterpret_cast<const char*>(&compressed), sizeof(compressed));
    current_offset += file.size;
  }

  // Write file data
  for (size_t i = 0; i < files.size(); i++) {
    const auto& file = files[i];

    std::ifstream input(file.full_path, std::ios::binary);
    if (!input.is_open()) {
      return Status::Fail("Failed to read file: " + file.full_path.string());
    }

    std::vector<char> buffer(file.size);
    input.read(buffer.data(), file.size);
    pak.write(buffer.data(), file.size);

    if (progress) {
      progress(i, files.size(), file.relative_path);
    }
  }

  return Status::Ok();
}

}  // namespace Wpak