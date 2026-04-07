//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "w_editor_icons.h"

#include <imgui.h>

#include "util/imgui/imgui_theme.h"
#include "util/w_vfs.h"
#include "w_engine.h"
#include "w_icon_font_loader.h"
#include "w_icon_source.h"

namespace Wiesel::Editor {

static PngIconSource s_png_source;

void InitEditorIcons() {
  // Load all PNG icons
  for (const auto& def : kEditorIcons) {
    s_png_source.AddIcon(def.codepoint, def.vfs_path);
  }

  // Set up the font loader
  IconFontLoader::SetSource(&s_png_source);

  ImGuiIO& io = ImGui::GetIO();

  // We need valid TTF data for AddFont. Use codicon as carrier since it
  // has no glyphs in our PUA range (0xE000-0xE0FF), avoiding conflicts
  // with fonts like Inter that map alternate glyphs to PUA codepoints.
  const char* carrier_path = "engine://fonts/codicon.ttf";
  auto font_file = Engine::vfs()->Open(carrier_path);
  if (!font_file) {
    LOG_ERROR("Failed to load {} for icon font", carrier_path);
    return;
  }

  void* font_data = IM_ALLOC(font_file.Size());
  memcpy(font_data, font_file.Data(), font_file.Size());

  ImFontConfig config;
  config.MergeMode = true;
  config.DstFont = io.FontDefault;
  config.FontDataOwnedByAtlas = true;
  config.FontLoader = IconFontLoader::GetLoader();
  config.SizePixels = 38.0f;
  config.FontData = font_data;
  config.FontDataSize = static_cast<int>(font_file.Size());

  static const ImWchar ranges[] = {0xE000, 0xE0FF, 0};
  config.GlyphRanges = ranges;

  io.Fonts->AddFont(&config);
  LOG_INFO("Editor icon font registered ({} icons)", std::size(kEditorIcons));
}

}  // namespace Wiesel::Editor
