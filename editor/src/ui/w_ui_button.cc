//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "ui/w_ui_button.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <cstring>

namespace wiesel::editor::ui::button {

namespace {

struct ToolbarGroupState {
  ImDrawListSplitter splitter;
  ImDrawList* draw_list = nullptr;
  float pad = 0.0f;
  bool active = false;
};

ToolbarGroupState& GroupState() {
  static ToolbarGroupState s{};
  return s;
}

}  // namespace

void BeginToolbarGroup(const char* id) {
  ToolbarGroupState& st = GroupState();
  IM_ASSERT(!st.active && "BeginToolbarGroup is not reentrant");
  st.active = true;
  st.pad = 2.0f;
  st.draw_list = ImGui::GetWindowDrawList();

  ImGui::PushID(id);
  st.splitter.Split(st.draw_list, 2);
  st.splitter.SetCurrentChannel(st.draw_list, 1);

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
  ImGui::BeginGroup();
}

void EndToolbarGroup() {
  ToolbarGroupState& st = GroupState();
  IM_ASSERT(st.active && "EndToolbarGroup without matching Begin");
  ImGui::EndGroup();
  ImGui::PopStyleVar();

  ImVec2 min = ImGui::GetItemRectMin();
  ImVec2 max = ImGui::GetItemRectMax();
  min.x -= st.pad;
  min.y -= st.pad;
  max.x += st.pad;
  max.y += st.pad;

  const ImGuiStyle& s = ImGui::GetStyle();
  st.splitter.SetCurrentChannel(st.draw_list, 0);
  st.draw_list->AddRectFilled(min, max,
                              ImGui::GetColorU32(ImGuiCol_FrameBg),
                              s.FrameRounding);
  if (s.FrameBorderSize > 0.0f) {
    st.draw_list->AddRect(min, max, ImGui::GetColorU32(ImGuiCol_Border),
                          s.FrameRounding, 0, s.FrameBorderSize);
  }
  st.splitter.Merge(st.draw_list);

  ImGui::PopID();
  st.active = false;
}

bool ToolbarButton(const char* label, bool active) {
  ImGuiContext& g = *GImGui;
  const float sz = ImGui::GetFrameHeight();
  const ImVec2 size(sz, sz);
  const ImVec2 pos = ImGui::GetCursorScreenPos();

  ImGui::PushID(label);
  const bool clicked = ImGui::InvisibleButton("##tbtn", size);
  const bool hovered = ImGui::IsItemHovered();
  const bool held = ImGui::IsItemActive();
  ImGui::PopID();

  ImU32 bg_col = 0;
  if (active || held) {
    bg_col = ImGui::GetColorU32(ImGuiCol_ButtonActive);
  } else if (hovered) {
    bg_col = ImGui::GetColorU32(ImGuiCol_ButtonHovered);
  }

  ImDrawList* dl = ImGui::GetWindowDrawList();
  if (bg_col != 0) {
    dl->AddRectFilled(pos, ImVec2(pos.x + sz, pos.y + sz), bg_col,
                      g.Style.FrameRounding);
  }

  // Strip any "##id" suffix from the visible label.
  const char* text_end = strstr(label, "##");
  if (!text_end) {
    text_end = label + strlen(label);
  }
  const ImVec2 text_sz = ImGui::CalcTextSize(label, text_end, true);
  const ImVec2 text_pos(pos.x + (sz - text_sz.x) * 0.5f,
                        pos.y + (sz - text_sz.y) * 0.5f);
  const ImU32 text_col = (hovered || active)
                             ? ImGui::GetColorU32(ImGuiCol_Text)
                             : ImGui::GetColorU32(ImGuiCol_TextDisabled);
  dl->AddText(text_pos, text_col, label, text_end);

  return clicked;
}

}  // namespace wiesel::editor::ui::button
