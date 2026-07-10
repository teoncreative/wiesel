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

// Row-style widgets: sidebar category rows, scene hierarchy entries,
// and the shared flat / full-width-highlight look.

namespace wiesel::editor::ui::row {

// Left-sidebar row used by settings-style popups (Project Settings, Editor
// Settings). Flat row (no rounding, no inset), highlight spans the whole
// window width ignoring WindowPadding, 2px accent hairline on the left when
// selected. Matches the scene hierarchy's entry style. Returns true on
// click this frame. `icon` is optional ICON_LC_* glyph.
bool CategoryRow(const char* label, bool selected,
                 const char* icon = nullptr);

// Draw a selection / hover highlight rect over a row the caller already
// submitted as an InvisibleButton. Matches ImGui's built-in row tinting:
// HeaderHovered on hover (wins over Selected so the active row under the
// mouse still lights up on hover), Header when only Selected.
//
// Pass `rounding = 0.0f` for flat/full-width rows, or FrameRounding for
// inset rows with rounded corners (command palette style).
void DrawRowHighlight(ImDrawList* dl, const ImVec2& min, const ImVec2& max,
                      bool selected, bool hovered, float rounding = 0.0f);

// Scene-hierarchy-style tree node:
//   - no rounding, flat
//   - highlight spans full window width (ignores WindowPadding on X)
//   - 2px accent hairline on the far left when Selected
// `static_tint`: when true, the row renders with a fixed tinted background
// and ignores hover/active highlight - useful for section headers (scene
// roots) where a click-highlight would be misleading.
bool HierarchyRow(const char* label, ImGuiTreeNodeFlags flags,
                  bool static_tint = false);

}  // namespace wiesel::editor::ui::row
