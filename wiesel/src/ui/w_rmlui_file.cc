//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "ui/w_rmlui_file.h"

#include "util/w_vfs.h"
#include "w_engine.h"

namespace Wiesel {

// Internal state for an opened VFS file with a read cursor.
struct RmlVfsFile {
  std::vector<uint8_t> data;
  size_t cursor = 0;
};

Rml::FileHandle RmlFileInterface::Open(const Rml::String& path) {
  auto file = Engine::vfs()->Open(path);
  if (!file) {
    LOG_WARN("[RmlUi] Failed to open file: {}", path);
    return 0;
  }

  auto* vfs_file = new RmlVfsFile();
  vfs_file->data.assign(file.Data(), file.Data() + file.Size());
  return reinterpret_cast<Rml::FileHandle>(vfs_file);
}

void RmlFileInterface::Close(Rml::FileHandle file) {
  delete reinterpret_cast<RmlVfsFile*>(file);
}

size_t RmlFileInterface::Read(void* buffer, size_t size, Rml::FileHandle file) {
  auto* vfs_file = reinterpret_cast<RmlVfsFile*>(file);
  size_t available = vfs_file->data.size() - vfs_file->cursor;
  size_t to_read = std::min(size, available);
  memcpy(buffer, vfs_file->data.data() + vfs_file->cursor, to_read);
  vfs_file->cursor += to_read;
  return to_read;
}

bool RmlFileInterface::Seek(Rml::FileHandle file, long offset, int origin) {
  auto* vfs_file = reinterpret_cast<RmlVfsFile*>(file);
  size_t new_pos = 0;
  switch (origin) {
    case SEEK_SET:
      new_pos = static_cast<size_t>(offset);
      break;
    case SEEK_CUR:
      new_pos = vfs_file->cursor + offset;
      break;
    case SEEK_END:
      new_pos = vfs_file->data.size() + offset;
      break;
    default:
      return false;
  }
  if (new_pos > vfs_file->data.size()) {
    return false;
  }
  vfs_file->cursor = new_pos;
  return true;
}

size_t RmlFileInterface::Tell(Rml::FileHandle file) {
  auto* vfs_file = reinterpret_cast<RmlVfsFile*>(file);
  return vfs_file->cursor;
}

size_t RmlFileInterface::Length(Rml::FileHandle file) {
  auto* vfs_file = reinterpret_cast<RmlVfsFile*>(file);
  return vfs_file->data.size();
}

}  // namespace Wiesel
