//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "ui/w_ui_popup.h"

#include <imgui_internal.h>

#include "ui/w_ui_draw.h"
#include "ui/w_ui_style.h"
#include "util/imgui/imgui_lucide.h"

namespace wiesel::editor::ui::popup {

const ImGuiWindowFlags kModalFlags =
    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
    ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoDocking |
    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav;

namespace {

// Esc and outside-click dismiss. Called after BeginPopupModal returned true.
// Flips *p_open to false and closes the popup when either fires.
//
// The hover check uses ImGuiHoveredFlags_AllowWhenBlockedByPopup so a
// combo/tooltip/etc. popup opened inside our content doesn't make
// IsWindowHovered return false. Without that flag, a click that closes a
// nested combo would also close us (the blocking-popup state causes
// IsWindowHovered to return false even for clicks geometrically inside
// our window).
void HandleCancel(bool* p_open) {
  if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
    *p_open = false;
    ImGui::CloseCurrentPopup();
    return;
  }
  const ImGuiHoveredFlags hovered_flags =
      ImGuiHoveredFlags_RootAndChildWindows |
      ImGuiHoveredFlags_AllowWhenBlockedByPopup;
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
      !ImGui::IsWindowHovered(hovered_flags)) {
    *p_open = false;
    ImGui::CloseCurrentPopup();
  }
}

}  // namespace

bool CloseIconButton(const char* id) {
  const ImGuiStyle& s = ImGui::GetStyle();
  ImGuiContext& g = *GImGui;

  const float btn_size = ImGui::GetFrameHeight();
  const ImVec2 btn_pos = ImGui::GetCursorScreenPos();
  const bool clicked = ImGui::InvisibleButton(id, ImVec2(btn_size, btn_size));
  const bool hovered = ImGui::IsItemHovered();
  const bool held = ImGui::IsItemActive();

  ImDrawList* dl = ImGui::GetWindowDrawList();
  if (hovered || held) {
    dl->AddRectFilled(
        btn_pos, ImVec2(btn_pos.x + btn_size, btn_pos.y + btn_size),
        ImGui::GetColorU32(held ? ImGuiCol_HeaderActive
                                : ImGuiCol_HeaderHovered),
        s.FrameRounding);
  }

  // SetWindowFontScale may be in effect when we're called from Begin() - use
  // GetFontSize so the glyph matches the surrounding scaled text.
  const float font_size = ImGui::GetFontSize();
  const ImVec2 x_sz = ImGui::CalcTextSize(ICON_LC_X);
  dl->AddText(g.Font, font_size,
              ImVec2(btn_pos.x + (btn_size - x_sz.x) * 0.5f,
                     btn_pos.y + (btn_size - x_sz.y) * 0.5f),
              ImGui::GetColorU32(ImGuiCol_Text), ICON_LC_X);
  return clicked;
}

bool Begin(const char* id, const char* icon, const char* title, bool* p_open,
           const ImVec2& size) {
  if (!p_open) {
    static bool dummy = true;
    p_open = &dummy;
  }

  ImGuiViewport* vp = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Always,
                          ImVec2(0.5f, 0.5f));
  if (size.x > 0.0f || size.y > 0.0f) {
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
  }

  // Outer window padding 0 - the header strip and body manage their own pads
  // so we get full-width separators / hover regions.
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  const bool visible = ImGui::BeginPopupModal(id, nullptr, kModalFlags);
  ImGui::PopStyleVar();
  if (!visible) {
    return false;
  }

  HandleCancel(p_open);

  const ImGuiStyle& s = ImGui::GetStyle();

  // Header: kSeparatorPadY above, title icon + label at the scaled header
  // font, square close button right-aligned with the same WindowPadding.x
  // inset as the title text on the left.
  ImGui::Dummy(ImVec2(0.0f, style::kSeparatorPadY));

  ImGui::Indent(s.WindowPadding.x);
  ImGui::SetWindowFontScale(style::kHeaderFontScale);

  // Center the title text vertically relative to the close button (a
  // frame-sized widget). Without this the text sits FramePadding.y above the
  // button's vertical center.
  ImGui::AlignTextToFramePadding();

  // Slight extra inset so the title doesn't sit flush against the popup's
  // left edge - mirrors the visual breathing the close button gets from its
  // FramePadding on the right side.
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + s.FramePadding.x);

  ImGui::TextDisabled("%s", (icon && icon[0]) ? icon : "");
  if (icon && icon[0]) {
    ImGui::SameLine();
  }
  ImGui::TextUnformatted(title ? title : "");

  // Close button, right-anchored at WindowPadding.x from the right edge.
  ImGui::SameLine();
  const float btn_size = ImGui::GetFrameHeight();
  const float content_w = ImGui::GetContentRegionAvail().x;
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + content_w - btn_size -
                       s.WindowPadding.x);
  if (CloseIconButton("##ui_popup_close")) {
    *p_open = false;
    ImGui::CloseCurrentPopup();
  }

  ImGui::SetWindowFontScale(1.0f);
  ImGui::Unindent(s.WindowPadding.x);

  // No separator after the header - body content (sections, fields) provides
  // its own visual division. The natural WindowPadding gap below is enough.
  ImGui::Dummy(ImVec2(0.0f, style::kSeparatorPadY));

  // Body padding via Indent so callers render normally without extra
  // SetCursorPos calls.
  ImGui::Indent(s.WindowPadding.x);

  return true;
}

void End() {
  const ImGuiStyle& s = ImGui::GetStyle();
  ImGui::Unindent(s.WindowPadding.x);
  // ImGui's CursorMaxPos drops the trailing ItemSpacing.y after the final
  // item, so a Dummy(kSeparatorPadY) on its own lands shorter than the
  // matching top margin. Add the missing ItemSpacing back so the popup
  // reads vertically symmetric.
  ImGui::Dummy(ImVec2(0.0f, style::kSeparatorPadY + s.ItemSpacing.y));
  ImGui::EndPopup();
}

bool BeginCentered(const char* id, const ImVec2& size, bool auto_resize,
                   bool* p_open) {
  if (!p_open) {
    static bool dummy = true;
    p_open = &dummy;
  }

  ImGuiViewport* vp = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Always,
                          ImVec2(0.5f, 0.5f));
  if (size.x > 0.0f || size.y > 0.0f) {
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
  }

  const ImGuiWindowFlags flags =
      kModalFlags | (auto_resize ? ImGuiWindowFlags_AlwaysAutoResize : 0);

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  const bool visible = ImGui::BeginPopupModal(id, nullptr, flags);
  ImGui::PopStyleVar();
  if (!visible) {
    return false;
  }

  HandleCancel(p_open);
  return true;
}

void EndCentered() {
  ImGui::EndPopup();
}

}  // namespace wiesel::editor::ui::popup
