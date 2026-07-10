//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "ui/w_ui_layout.h"

#include <imgui_internal.h>

#include "ui/w_ui_style.h"

namespace wiesel::editor::ui::layout {

namespace {

// Amount of indent that popup::Begin applies to the body, so SplitBody can
// undo and restore it. Matches ImGuiStyle::WindowPadding.x.
float PopupBodyIndent() {
  return ImGui::GetStyle().WindowPadding.x;
}

}  // namespace

WindowEdges GetWindowEdges() {
  ImGuiWindow* window = ImGui::GetCurrentWindow();
  return WindowEdges{window->Pos.x, window->Pos.x + window->Size.x};
}

void Separator() {
  ImGuiWindow* window = ImGui::GetCurrentWindow();
  if (window->SkipItems) {
    return;
  }
  const float thick = ImMax(1.0f, ImGui::GetStyle().WindowBorderSize);
  const float y = window->DC.CursorPos.y + thick * 0.5f;
  window->DrawList->AddLine(ImVec2(window->Pos.x, y),
                            ImVec2(window->Pos.x + window->Size.x, y),
                            ImGui::GetColorU32(ImGuiCol_Separator), thick);
  // Advance cursor by the line thickness only, bypassing the usual ItemSize
  // + ItemSpacing so callers can place the next item flush against the line.
  window->DC.CursorPos.y += thick;
  window->DC.CursorMaxPos.y =
      ImMax(window->DC.CursorMaxPos.y, window->DC.CursorPos.y);
  window->DC.CursorPosPrevLine.y = window->DC.CursorPos.y;
  window->DC.PrevLineSize.y = 0.0f;
}

void VerticalDivider() {
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 cur = ImGui::GetCursorScreenPos();
  const float h = ImGui::GetContentRegionAvail().y;
  dl->AddLine(cur, ImVec2(cur.x, cur.y + h),
              ImGui::GetColorU32(ImGuiCol_Separator), 1.0f);
  // Reserve the 1px width and leave the cursor flush to the line's right edge
  // so the next widget sits touching it.
  ImGui::Dummy(ImVec2(1.0f, 0.0f));
  ImGui::SameLine(0.0f, 0.0f);
}

void BeginSidebarBody(float sidebar_width) {
  // Undo the popup helper's WindowPadding indent so the sidebar reaches the
  // popup's left edge. Restored in EndSidebarBody so popup::End stays
  // balanced.
  ImGui::Unindent(PopupBodyIndent());

  // Horizontal separator between the popup title and the sidebar/body split.
  Separator();

  // Sidebar child: rows flush against each other (ItemSpacing.y = 0), no
  // inner padding, no rounded corners on the bg.
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
  ImGui::BeginChild("##ui_layout_sidebar", ImVec2(sidebar_width, 0));
}

void BeginBody() {
  ImGui::EndChild();            // end sidebar
  ImGui::PopStyleVar(3);        // WindowPadding + ItemSpacing + ChildRounding

  ImGui::SameLine(0.0f, 0.0f);
  VerticalDivider();

  // Body child: dark drawer bg, no rounding, and crucially
  // ImGuiChildFlags_AlwaysUseWindowPadding so a non-bordered child honors
  // the current WindowPadding (ImGui skips it by default for bordered=0).
  ImGui::PushStyleColor(ImGuiCol_ChildBg, style::kDrawerBg);
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
  ImGui::BeginChild("##ui_layout_body", ImVec2(0, 0),
                    ImGuiChildFlags_AlwaysUseWindowPadding);
  ImGui::PopStyleVar();
  ImGui::PopStyleColor();
}

void EndSidebarBody() {
  ImGui::EndChild();
  ImGui::Indent(PopupBodyIndent());
}

void CenterCursorX(float item_w) {
  const float win_w = ImGui::GetWindowWidth();
  ImGui::SetCursorPosX((win_w - item_w) * 0.5f);
}

void RightAlignCursorX(float item_w, float right_pad) {
  if (right_pad < 0.0f) {
    right_pad = ImGui::GetStyle().WindowPadding.x;
  }
  const float win_w = ImGui::GetWindowWidth();
  ImGui::SetCursorPosX(win_w - item_w - right_pad);
}

}  // namespace wiesel::editor::ui::layout
