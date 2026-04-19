
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

#include "imgui.h"
#include "w_pch.h"

namespace wiesel {

class Texture;

std::string PrefixLabel(const char* label);

// Render a small texture thumbnail with hover preview tooltip.
void RenderTexturePreview(const char* label, Texture* tex);

}  // namespace wiesel

namespace ImGui {
// Sets an icon glyph that the next ClosableTreeNode call on this thread
// will prepend to its label. Pass nullptr/"" to clear. The setting is
// consumed by the next matching ClosableTreeNode call (it auto-clears).
void SetNextTreeNodeIcon(const char* icon);
bool ClosableTreeNode(const char* label, bool* visible);

// State exposed by ClosableTreeNode to callers wrapping a component drawer.
// Set by ClosableTreeNode, read by the wrapper after its matching
// RenderSelf call.
struct ComponentHeaderState {
  bool header_rendered;
  bool open;
  float header_bottom_y;  // screen-space Y where drawer content starts
};
void ResetComponentHeaderState();
const ComponentHeaderState& GetComponentHeaderState();

// TreeNodeEx with extra vertical padding for more comfortable row height.
bool PaddedTreeNodeEx(const char* label, ImGuiTreeNodeFlags flags,
                      float padding_y = 3.0f, float rounding = 4.0f);

// Scene-hierarchy-style tree node:
//   - non-rounded highlight
//   - highlight spans full window width (ignores WindowPadding on X)
//   - 2px accent hairline on the left when the Selected flag is set
// `static_tint`: when true, the row renders with a fixed tinted background
// and ignores hover/active highlighting — useful for section headers
// (e.g., scene roots) where click-highlight would be misleading.
bool HierarchyTreeNodeEx(const char* label, ImGuiTreeNodeFlags flags,
                         bool static_tint = false);

// Like ImGui::Separator but ignores WindowPadding on X, so the line runs
// from the window's left edge to its right edge. Thickness = WindowBorderSize
// (or 1px when borders are off). Reserves the same vertical item space as
// the native separator so layout stays consistent.
void FullWidthSeparator();

// Draws a rounded rectangle border using dashes. Walks the rounded path and
// emits short line segments of `dash_len` separated by `gap_len`.
void DashedRectRounded(ImDrawList* draw_list, const ImVec2& p_min,
                       const ImVec2& p_max, ImU32 col, float rounding,
                       float thickness, float dash_len, float gap_len);

// Begin a framed group of buttons sharing a single input-style background
// and border. Auto-sizes to its contents. Pair each BeginToolbarGroup with
// EndToolbarGroup.
void BeginToolbarGroup(const char* id);
void EndToolbarGroup();
// Square icon button tailored for BeginToolbarGroup: transparent bg,
// muted label by default, lifts to ImGuiCol_ButtonHovered + white label on
// hover, and uses ButtonActive bg when `active` is true or held.
bool ToolbarButton(const char* label, bool active = false);
}  // namespace ImGui