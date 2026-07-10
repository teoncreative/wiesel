//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "ui/w_ui_chip.h"

#include <imgui_internal.h>

#include "ui/w_ui_draw.h"
#include "util/imgui/imgui_lucide.h"

namespace wiesel::editor::ui::chip {

namespace {

constexpr float kLabelPadX = 8.0f;
constexpr float kInnerGap = 2.0f;
constexpr float kAddPadX = 12.0f;

// Muted fill for removable chips - a touch lighter than the editor's drawer
// bg so chips read against a dark card, but still recessed vs ImGuiCol_Button.
constexpr ImU32 kChipBg = IM_COL32(0x2c, 0x28, 0x27, 0xff);
constexpr ImU32 kChipBgHover = IM_COL32(0x36, 0x32, 0x31, 0xff);

}  // namespace

bool Chip(const char* label) {
  ImGuiWindow* window = ImGui::GetCurrentWindow();
  if (window->SkipItems) {
    return false;
  }

  const float h = ImGui::GetFrameHeight();
  const ImVec2 label_sz = ImGui::CalcTextSize(label);
  const float label_area_w = kLabelPadX + label_sz.x + kInnerGap;
  const float x_btn_w = h;  // square X hit area
  const float total_w = label_area_w + x_btn_w;

  const ImVec2 pos = ImGui::GetCursorScreenPos();

  ImGui::PushID(label);
  ImGui::Dummy(ImVec2(label_area_w, h));
  const bool chip_hovered = ImGui::IsItemHovered();
  ImGui::SameLine(0.0f, 0.0f);
  const bool x_clicked = ImGui::InvisibleButton("##x", ImVec2(x_btn_w, h));
  const bool x_hovered = ImGui::IsItemHovered();
  ImGui::PopID();

  ImDrawList* dl = window->DrawList;
  const ImVec2 p_max(pos.x + total_w, pos.y + h);
  const float rounding = h * 0.5f;
  const ImU32 bg_col = (chip_hovered || x_hovered) ? kChipBgHover : kChipBg;
  dl->AddRectFilled(pos, p_max, bg_col, rounding);

  const float cy = pos.y + h * 0.5f;
  dl->AddText(ImVec2(pos.x + kLabelPadX, cy - label_sz.y * 0.5f),
              ImGui::GetColorU32(ImGuiCol_Text), label);

  const ImU32 x_col = ImGui::GetColorU32(
      x_hovered ? ImGuiCol_Text : ImGuiCol_TextDisabled);
  const ImVec2 x_icon_sz = ImGui::CalcTextSize(ICON_LC_X);
  dl->AddText(ImVec2(pos.x + label_area_w + (x_btn_w - x_icon_sz.x) * 0.5f,
                     cy - x_icon_sz.y * 0.5f),
              x_col, ICON_LC_X);

  return x_clicked;
}

bool AddChipButton(const char* label) {
  ImGuiWindow* window = ImGui::GetCurrentWindow();
  if (window->SkipItems) {
    return false;
  }

  const float h = ImGui::GetFrameHeight();
  const ImVec2 label_sz = ImGui::CalcTextSize(label);
  const float total_w = label_sz.x + kAddPadX * 2.0f;

  const ImVec2 pos = ImGui::GetCursorScreenPos();

  const bool clicked = ImGui::InvisibleButton(label, ImVec2(total_w, h));
  const bool hovered = ImGui::IsItemHovered();

  ImDrawList* dl = window->DrawList;
  const ImVec2 p_max(pos.x + total_w, pos.y + h);
  const float rounding = h * 0.5f;

  if (hovered) {
    dl->AddRectFilled(pos, p_max, ImGui::GetColorU32(ImGuiCol_ButtonHovered),
                      rounding);
    dl->AddRect(pos, p_max, ImGui::GetColorU32(ImGuiCol_Border), rounding, 0,
                1.0f);
  } else {
    draw::DashedRectRounded(dl, pos, p_max,
                            ImGui::GetColorU32(ImGuiCol_Border), rounding,
                            1.0f, 4.0f, 3.0f);
  }

  const float cy = pos.y + h * 0.5f;
  const ImU32 text_col = ImGui::GetColorU32(
      hovered ? ImGuiCol_Text : ImGuiCol_TextDisabled);
  dl->AddText(ImVec2(pos.x + kAddPadX, cy - label_sz.y * 0.5f), text_col,
              label);

  return clicked;
}

}  // namespace wiesel::editor::ui::chip
