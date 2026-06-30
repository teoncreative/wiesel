//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "ui/w_ui_field.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_vulkan.h>

#include "rendering/w_texture.h"
#include "ui/w_ui_draw.h"

namespace wiesel::editor::ui::field {

std::string PrefixLabel(const char* label) {
  const float width = ImGui::CalcItemWidth();
  const float x = ImGui::GetCursorPosX();
  ImGui::AlignTextToFramePadding();
  ImGui::Text("%s", label);
  ImGui::SameLine();
  ImGui::SetCursorPosX(x + width * 0.5f +
                       ImGui::GetStyle().ItemInnerSpacing.x);
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
  const ImVec2 thumb_size(16, 16);
  ImGui::Image(reinterpret_cast<ImTextureID>(desc), thumb_size);
  if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    const float max_preview = 256.0f;
    const float aspect =
        (tex->width_ > 0 && tex->height_ > 0)
            ? static_cast<float>(tex->width_) / static_cast<float>(tex->height_)
            : 1.0f;
    const ImVec2 preview_size = (aspect >= 1.0f)
                                    ? ImVec2(max_preview, max_preview / aspect)
                                    : ImVec2(max_preview * aspect, max_preview);
    ImGui::Image(reinterpret_cast<ImTextureID>(desc), preview_size);
    ImGui::EndTooltip();
  }
}

void TypeIconBadge(const char* icon) {
  const ImGuiStyle& s = ImGui::GetStyle();
  const float sz = ImGui::GetFrameHeight();
  const ImVec2 pos = ImGui::GetCursorScreenPos();
  // Reserve the layout space - the badge isn't interactive but must occupy
  // its slot so adjacent widgets advance past it.
  ImGui::Dummy(ImVec2(sz, sz));

  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 p_max(pos.x + sz, pos.y + sz);
  dl->AddRectFilled(pos, p_max, ImGui::GetColorU32(ImGuiCol_FrameBg),
                    s.FrameRounding);
  if (s.FrameBorderSize > 0.0f) {
    dl->AddRect(pos, p_max, ImGui::GetColorU32(ImGuiCol_Border),
                s.FrameRounding, 0, s.FrameBorderSize);
  }
  if (icon && icon[0]) {
    const ImVec2 glyph_pos = draw::GetCenteredTextPos(pos, p_max, icon);
    dl->AddText(glyph_pos, ImGui::GetColorU32(ImGuiCol_TextDisabled), icon);
  }
}

}  // namespace wiesel::editor::ui::field
