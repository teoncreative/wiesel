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

  // We need valid TTF data for AddFont. Load SourceSans3 as the carrier
  // (the custom loader intercepts our PUA codepoints).
  auto font_file = Engine::vfs()->Open("engine://fonts/SourceSans3.ttf");
  if (!font_file) {
    LOG_ERROR("Failed to load SourceSans3.ttf for icon font");
    return;
  }

  void* font_data = IM_ALLOC(font_file.Size());
  memcpy(font_data, font_file.Data(), font_file.Size());

  ImFontConfig config;
  config.MergeMode = true;
  config.DstFont = io.FontDefault;
  config.FontDataOwnedByAtlas = true;
  config.FontLoader = IconFontLoader::GetLoader();
  // Match the default font's raw size (LoadFont uses size*2 with Scale=0.5)
  config.SizePixels = 38.0f;
  config.FontData = font_data;
  config.FontDataSize = static_cast<int>(font_file.Size());

  static const ImWchar ranges[] = {0xE000, 0xE0FF, 0};
  config.GlyphRanges = ranges;

  io.Fonts->AddFont(&config);
  LOG_INFO("Editor icon font registered ({} icons)", std::size(kEditorIcons));
}

}  // namespace Wiesel::Editor
