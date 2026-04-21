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

#include <imgui.h>
#include <functional>
#include <string>

namespace wiesel::editor {

// Shared "name this thing" popup used by every asset/folder/script create
// flow. Visually mirrors the command palette: centered, modal, dim
// background. Header shows the factory's icon + label; the input only
// accepts a base name; a preview line under it shows the full VFS path
// with extension so the user knows exactly what file will land where.
struct NamePromptRequest {
  std::string title;        // e.g. "Cursor Set"
  std::string icon;         // ICON_LC_* glyph; may be empty
  std::string base_dir;     // VFS dir, e.g. "app://cursors/" (must end in /)
  std::string extension;    // e.g. ".wcursorset" (empty for folders)
  std::string default_name;
  std::string hint;         // Placeholder shown in empty input
  // Called when the user confirms with a non-empty name. The two arguments
  // are the bare name (whatever was typed) and the full computed VFS path
  // (base_dir + name + extension).
  std::function<void(const std::string& name,
                     const std::string& full_vfs_path)>
      on_confirm;
};

class NamePromptPopup {
 public:
  // Open the popup with the given request. If a previous prompt is still
  // visible it gets replaced by this one.
  void Open(NamePromptRequest req);

  // Render. Call once per frame from the editor's draw loop.
  void Render();

  void SetMonoFont(ImFont* font) { mono_font_ = font; }

  bool IsOpen() const { return open_; }

 private:
  bool open_ = false;
  bool just_opened_ = false;
  NamePromptRequest req_;
  char name_buf_[256] = {};
  ImFont* mono_font_ = nullptr;
};

}  // namespace wiesel::editor
