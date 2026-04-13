
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "util/imgui/w_imguiutil.h"

#include <imgui_internal.h>
#include "rendering/w_texture.h"

namespace Wiesel {

std::string PrefixLabel(const char* label) {
  float width = ImGui::CalcItemWidth();

  float x = ImGui::GetCursorPosX();
  ImGui::AlignTextToFramePadding();
  ImGui::Text("%s", label);
  ImGui::SameLine();
  ImGui::SetCursorPosX(x + width * 0.5f + ImGui::GetStyle().ItemInnerSpacing.x);
  ImGui::SetNextItemWidth(-1);

  std::string labelID = "##";
  labelID += label;

  return labelID;
}

void RenderTexturePreview(const char* label, Texture* tex) {
  if (!tex) {
    ImGui::TextDisabled("  %s: No", label);
    return;
  }
  VkDescriptorSet desc = tex->GetImGuiDescriptor();
  if (!desc) {
    ImGui::TextDisabled("  %s: (loading)", label);
    return;
  }
  ImGui::Text("  %s:", label);
  ImGui::SameLine();
  ImVec2 thumb_size(16, 16);
  ImGui::Image(reinterpret_cast<ImTextureID>(desc), thumb_size);
  if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    float max_preview = 256.0f;
    float aspect =
        (tex->width_ > 0 && tex->height_ > 0)
            ? static_cast<float>(tex->width_) / static_cast<float>(tex->height_)
            : 1.0f;
    ImVec2 preview_size = (aspect >= 1.0f)
                              ? ImVec2(max_preview, max_preview / aspect)
                              : ImVec2(max_preview * aspect, max_preview);
    ImGui::Image(reinterpret_cast<ImTextureID>(desc), preview_size);
    ImGui::EndTooltip();
  }
}

}  // namespace Wiesel

namespace ImGui {

bool ClosableTreeNode(const char* label, bool* p_visible) {
  unsigned int id = ImGui::GetID(label);
  ImGuiTreeNodeFlags flags = 0;
  flags |= ImGuiTreeNodeFlags_Framed;
  if (p_visible) {
    flags |= ImGuiTreeNodeFlags_AllowOverlap |
             ImGuiTreeNodeFlags_ClipLabelForTrailingButton;
  }
  bool open = ImGui::TreeNodeBehavior(id, flags, label);
  if (p_visible != NULL) {
    // Create a small overlapping close button
    // FIXME: We can evolve this into user accessible helpers to add extra buttons on title bars, headers, etc.
    // FIXME: CloseButton can overlap into text, need find a way to clip the text somehow.
    ImGuiContext& g = *GImGui;
    ImGuiLastItemData last_item_backup = g.LastItemData;
    float button_size = g.FontSize;
    float button_x = ImMax(g.LastItemData.Rect.Min.x,
                           g.LastItemData.Rect.Max.x -
                               g.Style.FramePadding.x * 2.0f - button_size);
    float frame_h = g.LastItemData.Rect.Max.y - g.LastItemData.Rect.Min.y;
    float button_y = g.LastItemData.Rect.Min.y + (frame_h - button_size) * 0.5f;
    ImGuiID close_button_id = GetIDWithSeed("#CLOSE", NULL, id);
    if (CloseButton(close_button_id, ImVec2(button_x, button_y))) {
      *p_visible = false;
    }
    g.LastItemData = last_item_backup;
  }
  return open;
}

bool PaddedTreeNodeEx(const char* label, ImGuiTreeNodeFlags flags,
                      float padding_y, float rounding) {
  float pad_x = ImGui::GetStyle().FramePadding.x;
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(pad_x, padding_y));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, rounding);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

  // Framed tree nodes always draw ImGuiCol_Header as background.
  // When not selected, blend with window background so it looks invisible.
  // When selected, the caller sets ImGuiTreeNodeFlags_Selected which still
  // uses ImGuiCol_Header - so we swap the color based on selection state.
  bool is_selected = (flags & ImGuiTreeNodeFlags_Selected) != 0;
  if (!is_selected) {
    ImGui::PushStyleColor(ImGuiCol_Header,
                          ImGui::GetStyleColorVec4(ImGuiCol_WindowBg));
  }

  flags |= ImGuiTreeNodeFlags_Framed;
  bool open = ImGui::TreeNodeEx(label, flags);

  if (!is_selected) {
    ImGui::PopStyleColor();
  }
  ImGui::PopStyleVar(4);
  return open;
}

}  // namespace ImGui