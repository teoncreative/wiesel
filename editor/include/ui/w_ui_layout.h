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

// Window-geometry and split-pane helpers. Nothing here needs to know about
// what content goes inside - callers compose them with row/section/field
// helpers from the other ui/ sub-headers.

namespace wiesel::editor::ui::layout {

// Left and right screen-space X coordinates of the current window's content
// frame, ignoring WindowPadding. Use these when drawing a rect that should
// reach from window edge to window edge (component drawer borders, scene
// hierarchy row highlights, section fills, etc).
struct WindowEdges {
  float left;
  float right;
};
WindowEdges GetWindowEdges();

// Full-width horizontal separator. Ignores WindowPadding on X so the line runs
// edge-to-edge. Thickness = WindowBorderSize (or 1px if borders are off).
// Advances the cursor by the line thickness only - no ItemSpacing - so the
// next widget can sit flush.
void Separator();

// 1px vertical line at the current cursor position, spanning the remaining
// content height. Reserves 1px of horizontal space and leaves the cursor flush
// to the right edge of the line with `SameLine(0, 0)` so the next widget can
// start touching it. Handles the `Dummy(1, 0) + SameLine(0, 0)` gotcha in one
// place.
void VerticalDivider();

// Split the current window into a fixed-width sidebar on the left and a
// flexible body on the right, with a horizontal separator on top and a 1px
// vertical divider between them. Typical use:
//
//   if (ui::popup::Begin(...)) {
//     ui::layout::BeginSidebarBody(160.0f);
//       for (int i = 0; i < n; i++) {
//         if (ui::row::CategoryRow(labels[i], i == sel)) sel = i;
//       }
//     ui::layout::BeginBody();
//       // body widgets
//     ui::layout::EndSidebarBody();
//     ui::popup::End();
//   }
//
// Encapsulates:
//   - Unindent to reach the popup's left edge (popup::Begin indents WindowPad).
//   - Top separator.
//   - Sidebar child with padding=0, item-spacing.y=0, no rounding.
//   - Vertical divider between sidebar and body.
//   - Body child with kDrawerBg bg, no rounding, AlwaysUseWindowPadding (so
//     content inside gets the same inner inset as the popup title row).
//   - Matching re-indent so the popup's End Unindent stays balanced.
void BeginSidebarBody(float sidebar_width);
void BeginBody();
void EndSidebarBody();

// Set the cursor X so a widget of width `item_w` centers horizontally within
// the current window. Call after SameLine() when placing a centered group on
// the same row as left-aligned content.
void CenterCursorX(float item_w);

// Set the cursor X so a widget of width `item_w` right-aligns, leaving
// `right_pad` pixels of space from the right edge of the window. When
// `right_pad` is negative, uses the current style's WindowPadding.x.
void RightAlignCursorX(float item_w, float right_pad = -1.0f);

}  // namespace wiesel::editor::ui::layout
