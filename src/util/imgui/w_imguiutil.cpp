
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "util/imgui/w_imguiutil.hpp"

#include <imgui_internal.h>

namespace Wiesel {

  std::string PrefixLabel(const char* label) {
    float width = ImGui::CalcItemWidth();

    float x = ImGui::GetCursorPosX();
    ImGui::Text("%s", label);
    ImGui::SameLine();
    ImGui::SetCursorPosX(x + width * 0.5f + ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::SetNextItemWidth(-1);

    std::string labelID = "##";
    labelID += label;

    return labelID;
  }

}// namespace Wiesel

namespace ImGui {

  bool ClosableTreeNode(const char* label, bool* p_visible) {
    unsigned int id = ImGui::GetID(label);
    ImGuiTreeNodeFlags flags = 0;
    flags |= ImGuiTreeNodeFlags_Framed;
    if (p_visible)
      flags |= ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_ClipLabelForTrailingButton;
    bool open = ImGui::TreeNodeBehavior(id, flags, label);
    if (p_visible != NULL) {
      // Create a small overlapping close button
      // FIXME: We can evolve this into user accessible helpers to add extra buttons on title bars, headers, etc.
      // FIXME: CloseButton can overlap into text, need find a way to clip the text somehow.
      ImGuiContext& g = *GImGui;
      ImGuiLastItemData last_item_backup = g.LastItemData;
      float button_size = g.FontSize;
      float button_x = ImMax(g.LastItemData.Rect.Min.x, g.LastItemData.Rect.Max.x - g.Style.FramePadding.x * 2.0f - button_size);
      float button_y = g.LastItemData.Rect.Min.y;
      ImGuiID close_button_id = GetIDWithSeed("#CLOSE", NULL, id);
      if (CloseButton(close_button_id, ImVec2(button_x, button_y)))
        *p_visible = false;
      g.LastItemData = last_item_backup;
    }
    return open;
  }

}// namespace ImGui