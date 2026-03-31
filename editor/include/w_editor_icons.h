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

// Editor icons mapped to Private Use Area codepoints (U+E000+).
// These are merged into the default ImGui font via a custom ImFontLoader,
// so they can be used directly in any ImGui string (titles, menus, etc.).
//
// Usage: ImGui::Begin(ICON_HIERARCHY " Scene Hierarchy");

#define ICON_FOLDER "\xee\x80\x80"     // U+E000
#define ICON_CAMERA "\xee\x80\x81"     // U+E001
#define ICON_CONSOLE "\xee\x80\x82"    // U+E002
#define ICON_HIERARCHY "\xee\x80\x83"  // U+E003
#define ICON_BROWSER "\xee\x80\x84"    // U+E004

namespace Wiesel::Editor {

struct EditorIconDef {
  const char* name;
  const char* vfs_path;
  unsigned int codepoint;
};

inline constexpr EditorIconDef kEditorIcons[] = {
    {"folder", "engine://textures/ui/folder.png", 0xE000},
    {"camera", "engine://textures/ui/camera.png", 0xE001},
    {"console", "engine://textures/ui/console.png", 0xE002},
    {"hierarchy", "engine://textures/ui/scene_hierarchy.png", 0xE003},
    {"browser", "engine://textures/ui/browser.png", 0xE004},
};

// Initialize the editor icon font system. Call once in EditorLayer::OnAttach
// after all other fonts have been added.
void InitEditorIcons();

}  // namespace Wiesel::Editor
