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

#include "util/imgui/imgui_lucide.h"
#include "util/imgui/imgui_theme.h"
#include "util/w_vfs.h"
#include "w_engine.h"
#include "w_icon_font_loader.h"
#include "w_icon_source.h"

namespace wiesel::editor {

void InitEditorIcons() {
  ImGuiIO& io = ImGui::GetIO();

  auto lucide_file = Engine::vfs()->Open("engine://fonts/lucide.ttf");
  if (lucide_file) {
    void* lucide_data = IM_ALLOC(lucide_file.Size());
    memcpy(lucide_data, lucide_file.Data(), lucide_file.Size());

    ImFontConfig lucide_config;
    lucide_config.MergeMode = true;
    lucide_config.DstFont = io.FontDefault;
    lucide_config.FontDataOwnedByAtlas = true;
    lucide_config.SizePixels = ImGui::Moonlight::kDefaultIconFontSize;
    lucide_config.FontData = lucide_data;
    lucide_config.FontDataSize = static_cast<int>(lucide_file.Size());
    lucide_config.GlyphMinAdvanceX = 16.0f;
    lucide_config.PixelSnapH = true;
    lucide_config.GlyphOffset = ImVec2(0, 3);

    static constexpr ImWchar lucide_ranges[] = {0xE000, 0xF8FF, 0};
    lucide_config.GlyphRanges = lucide_ranges;

    io.Fonts->AddFont(&lucide_config);
    LOG_INFO("Lucide glyphs merged");
  }

  // Substitute ImGui's built-in arrow primitives with Lucide chevrons.
  // Lucide is merged into the default font, so we reuse it as IconFont.
  ImGuiStyle& style = ImGui::GetStyle();
  style.IconFont = io.FontDefault;
  style.IconGlyphs[ImGuiIconId_ArrowUp]    = 0xE070;  // chevron-up
  style.IconGlyphs[ImGuiIconId_ArrowDown]  = 0xE06D;  // chevron-down
  style.IconGlyphs[ImGuiIconId_ArrowLeft]  = 0xE06E;  // chevron-left
  style.IconGlyphs[ImGuiIconId_ArrowRight] = 0xE06F;  // chevron-right
}

}  // namespace wiesel::editor
