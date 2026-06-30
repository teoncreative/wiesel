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

// Low-level drawing primitives that deeper UI widgets share. Nothing here
// creates items in ImGui's layout stack - these just paint on an ImDrawList.

namespace wiesel::editor::ui::draw {

// Compute the top-left point to AddText-render `text` so that it is visually
// centered inside the axis-aligned rect [rect_min, rect_max]. Uses the
// current ImGui font via CalcTextSize - pass `font_size <= 0` to use the
// default font size, or a positive value (e.g. from a scaled header row) to
// override. The returned ImVec2 is ready to pass straight to AddText.
ImVec2 GetCenteredTextPos(const ImVec2& rect_min, const ImVec2& rect_max,
                         const char* text, float font_size = 0.0f);

// Draw an icon + label inline on `dl` at vertical center `cy`, starting
// horizontally at `x_inout`. Advances `x_inout` past the icon + spacing so the
// caller can continue with additional inline content. Either `icon` or
// `label` may be null/empty (that part is skipped). Spacing between icon and
// label is 2 * ItemInnerSpacing.x (matches existing hand-rolled copies in
// CategoryRow, command palette, ClosableTreeNode).
void IconLabelInline(ImDrawList* dl, float& x_inout, float cy,
                     const char* icon, const char* label,
                     ImU32 icon_col, ImU32 label_col);

// Render a right-aligned "badge" pill at `right_x`, vertically centered at
// `cy`, with text rendered at `host_font_size * style::kBadgeScale`. Uses
// ui::style::kBadgeBg + kBadgeRounding + kBadgePadX for consistency.
// `font` must be a valid ImFont; use ImGui::GetFont() if there's no
// explicit mono font.
void DrawBadge(ImDrawList* dl, ImFont* font, const char* text, float cy,
               float right_x, float host_font_size);

// Width (in pixels) of the rect that DrawBadge would produce for `text` at
// `host_font_size`. Callers use this to place content to the left of a
// badge without overlapping it.
float BadgeWidth(ImFont* font, const char* text, float host_font_size);

// Draw a dashed rounded-rect outline. Walks the rounded path in small
// `dash_len`-long strokes separated by `gap_len`. Used for drop-target /
// highlighted region outlines.
void DashedRectRounded(ImDrawList* dl, const ImVec2& p_min, const ImVec2& p_max,
                       ImU32 col, float rounding, float thickness,
                       float dash_len, float gap_len);

}  // namespace wiesel::editor::ui::draw
