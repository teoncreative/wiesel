
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
#include <vector>

namespace wiesel::editor {

struct Command {
  // Unique id, e.g. "file.save_scene". Used as an internal key.
  std::string id;
  // Human-readable label shown in the palette row.
  std::string label;
  // Category the command belongs to. Shown as a section header when the
  // palette has no search filter active.
  std::string category;
  // Lucide glyph (UTF-8 string from imgui_lucide.h). May be empty.
  std::string icon;
  // Human-readable shortcut display text, e.g. "Ctrl+S". May be empty.
  std::string shortcut_text;
  // ImGui key chord used to dispatch the command via a global shortcut
  // (e.g. ImGuiMod_Ctrl | ImGuiKey_S). 0 = no bound shortcut.
  ImGuiKeyChord shortcut = 0;
  // Short label shown right-aligned on the row (e.g. "Entity",
  // "Texture", "Prefab"). Used by dynamic entries to disambiguate kinds.
  std::string trailing_label;
  // What to run when this command is invoked (palette click / shortcut).
  std::function<void()> action;
  // Optional: returns false to gray out the row and ignore the shortcut.
  std::function<bool()> enabled;
};

class CommandPalette {
 public:
  CommandPalette() = default;

  // Registry: commands are added once during editor init. Duplicate ids
  // replace any prior registration with the same id.
  void Register(Command cmd);

  // Monospace font used for the shortcut badge on each row. May be null —
  // the badge falls back to the default font.
  void SetMonoFont(ImFont* font) { mono_font_ = font; }

  // Provider that streams additional Commands matching the user's search.
  // Called only when the search box is non-empty (so huge collections like
  // the scene hierarchy or asset manager don't render wholesale).
  using ResultProvider =
      std::function<void(const std::string& filter_lower,
                         std::vector<Command>& out)>;
  void AddResultProvider(ResultProvider provider) {
    providers_.push_back(std::move(provider));
  }

  // Opens the palette centered on the viewport. Input is reset.
  void Open();

  bool is_open() const { return open_; }

  // Walks the registry, invokes any enabled command whose shortcut matches
  // the current key chord state. Should be called once per frame from the
  // editor's global shortcut pass. Returns true if something fired.
  bool DispatchShortcuts();

  // Draws the palette popup if open. Call once per frame.
  void Render();

 private:
  void RenderRow(const Command& cmd, bool selected, bool in_category_view);

  std::vector<Command> commands_;
  std::vector<ResultProvider> providers_;
  ImFont* mono_font_ = nullptr;
  bool open_ = false;
  bool just_opened_ = false;
  char search_[128] = "";
  int selected_index_ = 0;
};

}  // namespace wiesel::editor
