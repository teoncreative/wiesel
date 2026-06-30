//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "ui/w_ui_draw.h"

#include <imgui_internal.h>

#include "ui/w_ui_style.h"

namespace wiesel::editor::ui::draw {

ImVec2 GetCenteredTextPos(const ImVec2& rect_min, const ImVec2& rect_max,
                         const char* text, float font_size) {
  const ImVec2 size = (font_size > 0.0f)
                          ? ImGui::GetFont()->CalcTextSizeA(
                                font_size, FLT_MAX, 0.0f, text)
                          : ImGui::CalcTextSize(text);
  return ImVec2(rect_min.x + (rect_max.x - rect_min.x - size.x) * 0.5f,
                rect_min.y + (rect_max.y - rect_min.y - size.y) * 0.5f);
}

void IconLabelInline(ImDrawList* dl, float& x_inout, float cy,
                     const char* icon, const char* label,
                     ImU32 icon_col, ImU32 label_col) {
  ImGuiContext& g = *GImGui;
  if (icon && icon[0]) {
    const ImVec2 icon_sz = ImGui::CalcTextSize(icon);
    dl->AddText(ImVec2(x_inout, cy - icon_sz.y * 0.5f), icon_col, icon);
    x_inout += icon_sz.x + g.Style.ItemInnerSpacing.x * 2.0f;
  }
  if (label && label[0]) {
    const ImVec2 label_sz = ImGui::CalcTextSize(label);
    dl->AddText(ImVec2(x_inout, cy - label_sz.y * 0.5f), label_col, label);
    x_inout += label_sz.x;
  }
}

void DrawBadge(ImDrawList* dl, ImFont* font, const char* text, float cy,
               float right_x, float host_font_size) {
  const float font_size = host_font_size * style::kBadgeScale;
  const ImVec2 text_sz = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, text);
  const float badge_h = host_font_size;
  const float badge_w = text_sz.x + style::kBadgePadX * 2.0f;
  const ImVec2 badge_max(right_x, cy + badge_h * 0.5f);
  const ImVec2 badge_min(badge_max.x - badge_w, cy - badge_h * 0.5f);
  dl->AddRectFilled(badge_min, badge_max, style::kBadgeBg,
                    style::kBadgeRounding);
  dl->AddRect(badge_min, badge_max, ImGui::GetColorU32(ImGuiCol_Border),
              style::kBadgeRounding, 0, 1.0f);
  dl->AddText(font, font_size,
              ImVec2(badge_min.x + style::kBadgePadX, cy - text_sz.y * 0.5f),
              ImGui::GetColorU32(ImGuiCol_TextDisabled), text);
}

float BadgeWidth(ImFont* font, const char* text, float host_font_size) {
  const float font_size = host_font_size * style::kBadgeScale;
  const float w = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, text).x;
  return w + style::kBadgePadX * 2.0f;
}

void DashedRectRounded(ImDrawList* dl, const ImVec2& p_min, const ImVec2& p_max,
                       ImU32 col, float rounding, float thickness,
                       float dash_len, float gap_len) {
  // Build the rounded-rect outline as a polyline and emit dashes along it.
  // The path is copied out because AddLine below reuses dl->_Path internally.
  dl->PathRect(p_min, p_max, rounding);
  ImVector<ImVec2> path;
  path.reserve(dl->_Path.Size);
  for (int i = 0; i < dl->_Path.Size; i++) {
    path.push_back(dl->_Path[i]);
  }
  dl->_Path.resize(0);

  const int point_count = path.Size;
  if (point_count < 2) {
    return;
  }

  bool drawing = true;
  float remaining = dash_len;
  for (int i = 0; i < point_count; i++) {
    const ImVec2 a = path[i];
    const ImVec2 b = path[(i + 1) % point_count];
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float seg_len = ImSqrt(dx * dx + dy * dy);
    if (seg_len < 0.001f) {
      continue;
    }
    const float inv_len = 1.0f / seg_len;
    const float nx = dx * inv_len;
    const float ny = dy * inv_len;
    float pos = 0.0f;
    while (pos < seg_len) {
      const float step = ImMin(remaining, seg_len - pos);
      if (drawing) {
        const ImVec2 s(a.x + nx * pos, a.y + ny * pos);
        const ImVec2 e(a.x + nx * (pos + step), a.y + ny * (pos + step));
        dl->AddLine(s, e, col, thickness);
      }
      pos += step;
      remaining -= step;
      if (remaining <= 0.0f) {
        drawing = !drawing;
        remaining = drawing ? dash_len : gap_len;
      }
    }
  }
}

}  // namespace wiesel::editor::ui::draw
