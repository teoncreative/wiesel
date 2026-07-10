//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "ui/w_ui_row.h"

#include <imgui_internal.h>

#include "ui/w_ui_draw.h"
#include "ui/w_ui_layout.h"

namespace wiesel::editor::ui::row {

void DrawRowHighlight(ImDrawList* dl, const ImVec2& min, const ImVec2& max,
                      bool selected, bool hovered, float rounding) {
  if (!selected && !hovered) {
    return;
  }
  const ImU32 col = ImGui::GetColorU32(
      hovered ? ImGuiCol_HeaderHovered : ImGuiCol_Header);
  dl->AddRectFilled(min, max, col, rounding);
}

bool CategoryRow(const char* label, bool selected, const char* icon) {
  const ImGuiStyle& s = ImGui::GetStyle();
  ImGuiWindow* window = ImGui::GetCurrentWindow();

  const float row_h = ImGui::GetFrameHeight();
  const float avail_w = ImGui::GetContentRegionAvail().x;
  const ImVec2 pos = ImGui::GetCursorScreenPos();

  ImGui::PushID(label);
  const bool clicked =
      ImGui::InvisibleButton("##cat_row", ImVec2(avail_w, row_h));
  const bool hovered = ImGui::IsItemHovered();
  ImGui::PopID();

  ImDrawList* dl = window->DrawList;
  const layout::WindowEdges edges = layout::GetWindowEdges();

  if (selected || hovered) {
    const ImU32 bg_col = ImGui::GetColorU32(
        selected ? ImGuiCol_Header : ImGuiCol_HeaderHovered);
    dl->AddRectFilled(ImVec2(edges.left, pos.y),
                      ImVec2(edges.right, pos.y + row_h), bg_col);
  }
  if (selected) {
    dl->AddRectFilled(ImVec2(edges.left, pos.y),
                      ImVec2(edges.left + 2.0f, pos.y + row_h),
                      ImGui::GetColorU32(ImGuiCol_CheckMark));
  }

  const float cy = pos.y + row_h * 0.5f;
  float x = pos.x + s.FramePadding.x;
  draw::IconLabelInline(dl, x, cy, icon, label,
                        ImGui::GetColorU32(ImGuiCol_TextDisabled),
                        ImGui::GetColorU32(ImGuiCol_Text));

  return clicked;
}

bool HierarchyRow(const char* label, ImGuiTreeNodeFlags flags,
                  bool static_tint) {
  ImGuiWindow* window = ImGui::GetCurrentWindow();
  const bool is_selected = (flags & ImGuiTreeNodeFlags_Selected) != 0;

  // Flat highlight (no rounding, no border), always framed + full-width.
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

  ImVec4 tint4;
  if (static_tint) {
    ImVec4 bg = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
    tint4 = ImVec4(bg.x + 0.035f, bg.y + 0.035f, bg.z + 0.035f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Header, tint4);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, tint4);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, tint4);
  } else if (!is_selected) {
    // Hide the per-frame Header fill for unselected rows (Framed trees
    // otherwise always paint ImGuiCol_Header, which is accent-tinted).
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
  }

  flags |= ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanFullWidth;
  bool open = ImGui::TreeNodeEx(label, flags);

  if (static_tint) {
    ImGui::PopStyleColor(3);
  } else if (!is_selected) {
    ImGui::PopStyleColor();
  }
  ImGui::PopStyleVar(2);

  // Paint the left/right gutters so the highlight ignores WindowPadding.
  const ImVec2 item_min = ImGui::GetItemRectMin();
  const ImVec2 item_max = ImGui::GetItemRectMax();
  const layout::WindowEdges edges = layout::GetWindowEdges();
  ImDrawList* dl = window->DrawList;

  auto fill_gutters = [&](ImU32 col) {
    if (item_min.x > edges.left) {
      dl->AddRectFilled(ImVec2(edges.left, item_min.y),
                        ImVec2(item_min.x, item_max.y), col);
    }
    if (edges.right > item_max.x) {
      dl->AddRectFilled(ImVec2(item_max.x, item_min.y),
                        ImVec2(edges.right, item_max.y), col);
    }
  };

  if (static_tint) {
    fill_gutters(ImGui::ColorConvertFloat4ToU32(tint4));
    return open;
  }

  // Matches TreeNodeBehavior's internal color priority so the gutter fill
  // stays in sync with the work-rect bg.
  const bool hovered = ImGui::IsItemHovered();
  const bool held = ImGui::IsItemActive();
  if (is_selected || hovered) {
    ImU32 bg_col;
    if (held && hovered) {
      bg_col = ImGui::GetColorU32(ImGuiCol_HeaderActive);
    } else if (hovered) {
      bg_col = ImGui::GetColorU32(ImGuiCol_HeaderHovered);
    } else {
      bg_col = ImGui::GetColorU32(ImGuiCol_Header);
    }
    fill_gutters(bg_col);
  }
  if (is_selected) {
    // 2px accent line on the far left.
    dl->AddRectFilled(ImVec2(edges.left, item_min.y),
                      ImVec2(edges.left + 2.0f, item_max.y),
                      ImGui::GetColorU32(ImGuiCol_CheckMark));
  }

  return open;
}

}  // namespace wiesel::editor::ui::row
