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

void InitEditorIcons() {
  ImGuiIO& io = ImGui::GetIO();

  auto codicon_file = Engine::vfs()->Open("engine://fonts/codicon.ttf");
  if (codicon_file) {
    void* codicon_data = IM_ALLOC(codicon_file.Size());
    memcpy(codicon_data, codicon_file.Data(), codicon_file.Size());

    ImFontConfig codicon_config;
    codicon_config.MergeMode = true;
    codicon_config.DstFont = io.FontDefault;
    codicon_config.FontDataOwnedByAtlas = true;
    codicon_config.SizePixels = ImGui::Moonlight::kDefaultFontSize;
    codicon_config.FontData = codicon_data;
    codicon_config.FontDataSize = static_cast<int>(codicon_file.Size());
    codicon_config.GlyphOffset = ImVec2(0, 3);  // slight vertical offset

    static constexpr ImWchar codicon_ranges[] = {0xEA60, 0xF000, 0};
    codicon_config.GlyphRanges = codicon_ranges;

    io.Fonts->AddFont(&codicon_config);
    LOG_INFO("Codicon glyphs merged");
  }
}

}  // namespace Wiesel::Editor
