//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "ui/w_ui_section.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <memory>
#include <vector>

#include "ui/w_ui_draw.h"
#include "ui/w_ui_layout.h"
#include "ui/w_ui_style.h"
#include "util/imgui/imgui_lucide.h"

namespace wiesel::editor::ui::section {

// --- Thread-local state for ClosableTreeNode -------------------------------

namespace {

thread_local const char* t_next_header_icon = nullptr;
thread_local HeaderState t_header_state{};

}  // namespace

void SetNextHeaderIcon(const char* icon) {
  t_next_header_icon = (icon && icon[0]) ? icon : nullptr;
}

void ResetHeaderState() {
  t_header_state = HeaderState{};
}

const HeaderState& GetHeaderState() {
  return t_header_state;
}

bool ClosableTreeNode(const char* label, bool* p_visible, int flags) {
  ImGuiContext& g = *GImGui;
  ImGuiWindow* window = ImGui::GetCurrentWindow();
  if (window->SkipItems) {
    return false;
  }

  const bool no_collapse = (flags & HeaderFlags_NoCollapse) != 0;

  ImGui::PushID(label);

  const char* icon_glyph = t_next_header_icon;
  t_next_header_icon = nullptr;

  const ImGuiID id = window->GetID(label);
  ImGuiStorage* storage = window->DC.StateStorage;
  // NoCollapse sections are always "open" and never toggle.
  bool open = no_collapse ? true : storage->GetBool(id, false);

  // Row height matches the docked tab bar so headers read as the same bar.
  const float h = ImGui::GetFrameHeight() + g.Style.DockingTabBarExtraHeight;
  const ImVec2 cursor_start = ImGui::GetCursorScreenPos();
  const float row_w = ImGui::GetContentRegionAvail().x;
  const float close_w = (p_visible != nullptr) ? h : 0.0f;
  const float main_w = row_w - close_w;

  if (no_collapse) {
    // Still reserve the row height; the header isn't clickable.
    ImGui::Dummy(ImVec2(main_w, h));
  } else {
    ImGui::InvisibleButton("##header", ImVec2(main_w, h));
    if (ImGui::IsItemClicked()) {
      open = !open;
      storage->SetBool(id, open);
    }
  }

  ImDrawList* dl = window->DrawList;
  const float cy = cursor_start.y + h * 0.5f;
  const float full_left = window->Pos.x;
  const float left_pad = g.Style.FramePadding.x;
  float x_cursor = full_left + left_pad;

  // Chevron (collapsible variants only).
  if (!no_collapse) {
    const char* chev = open ? ICON_LC_CHEVRON_DOWN : ICON_LC_CHEVRON_RIGHT;
    const ImVec2 chev_sz = ImGui::CalcTextSize(chev);
    const ImVec2 chev_pos(x_cursor, cy - chev_sz.y * 0.5f);
    dl->AddText(chev_pos, ImGui::GetColorU32(ImGuiCol_TextDisabled), chev);
    x_cursor = chev_pos.x + chev_sz.x + g.Style.ItemInnerSpacing.x;
  }

  // Icon + label via the shared inline helper.
  draw::IconLabelInline(
      dl, x_cursor, cy, icon_glyph, label,
      ImGui::GetColorU32(ImGuiCol_CheckMark),
      ImGui::GetColorU32(ImGuiCol_Text));

  // Close button (right-aligned), mirroring the chevron's left inset.
  if (p_visible != nullptr) {
    ImGui::SameLine(0.0f, 0.0f);
    ImGui::InvisibleButton("##close", ImVec2(close_w, h));
    const bool close_hovered = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) {
      *p_visible = false;
    }
    const float full_right = window->Pos.x + window->Size.x;
    const ImVec2 x_sz = ImGui::CalcTextSize(ICON_LC_X);
    dl->AddText(ImVec2(full_right - left_pad - x_sz.x, cy - x_sz.y * 0.5f),
                ImGui::GetColorU32(close_hovered ? ImGuiCol_Text
                                                 : ImGuiCol_TextDisabled),
                ICON_LC_X);
  }

  t_header_state.header_rendered = true;
  t_header_state.open = open;
  t_header_state.header_bottom_y = cursor_start.y + h;

  ImGui::PopID();
  // TreeNodeBehavior contract: open => push so callers' TreePop balances.
  // NoCollapse skips the push so callers don't need a matching TreePop.
  if (open && !no_collapse) {
    ImGui::TreePushOverrideID(id);
  }
  return open;
}

// --- DrawerFrame stack -----------------------------------------------------

namespace {

struct DrawerFrame {
  ImDrawListSplitter splitter;
  float top_y = 0.0f;
};

// Frames live behind unique_ptrs so the vector can grow without copying the
// non-copy-safe ImDrawListSplitter (which shallow-copies its channel buffer
// pointers via memcpy and double-frees on destruction).
std::vector<std::unique_ptr<DrawerFrame>>& DrawerStack() {
  static std::vector<std::unique_ptr<DrawerFrame>> v;
  return v;
}

}  // namespace

void BeginDrawerFrame() {
  DrawerStack().push_back(std::make_unique<DrawerFrame>());
  DrawerFrame& f = *DrawerStack().back();

  ImDrawList* dl = ImGui::GetWindowDrawList();
  f.splitter.Split(dl, 2);
  f.splitter.SetCurrentChannel(dl, 1);
  f.top_y = ImGui::GetCursorScreenPos().y;
}

void EndDrawerFrame(float header_bottom_y, bool fill) {
  if (DrawerStack().empty()) {
    return;
  }
  DrawerFrame& f = *DrawerStack().back();

  const ImGuiStyle& s = ImGui::GetStyle();

  // ImGui adds ItemSpacing.y after the last body item; strip it so the box
  // hugs the actual content. Add WindowPadding.y of breathing room above the
  // bottom border when there's a drawer body to fill.
  const float content_end_y =
      ImGui::GetCursorScreenPos().y - s.ItemSpacing.y;
  const float bottom_y = content_end_y + (fill ? s.WindowPadding.y : 0.0f);

  ImDrawList* dl = ImGui::GetWindowDrawList();
  f.splitter.SetCurrentChannel(dl, 0);

  const layout::WindowEdges edges = layout::GetWindowEdges();
  const ImU32 border_col = ImGui::GetColorU32(ImGuiCol_Border);

  if (fill) {
    dl->AddRectFilled(ImVec2(edges.left, header_bottom_y),
                      ImVec2(edges.right, bottom_y), style::kDrawerBg);
  }

  dl->AddLine(ImVec2(edges.left, f.top_y), ImVec2(edges.right, f.top_y),
              border_col, 1.0f);
  dl->AddLine(ImVec2(edges.left, f.top_y), ImVec2(edges.left, bottom_y),
              border_col, 1.0f);
  dl->AddLine(ImVec2(edges.right, f.top_y), ImVec2(edges.right, bottom_y),
              border_col, 1.0f);
  dl->AddLine(ImVec2(edges.left, bottom_y), ImVec2(edges.right, bottom_y),
              border_col, 1.0f);

  f.splitter.Merge(dl);

  // Park the cursor at the bottom border so the next BeginDrawerFrame starts
  // flush - adjacent drawers share a single 1px divider instead of a gap.
  const ImVec2 cur = ImGui::GetCursorScreenPos();
  ImGui::SetCursorScreenPos(ImVec2(cur.x, bottom_y));

  DrawerStack().pop_back();
}

// --- Section (DrawerFrame + non-collapsible header) ------------------------

namespace {

// Per BeginSection frame state. Stacked alongside DrawerFrame so nested
// sections work if anyone needs them.
struct SectionFrame {
  float header_bottom_y = 0.0f;
  bool fill = false;
};
std::vector<SectionFrame>& SectionStack() {
  static std::vector<SectionFrame> v;
  return v;
}

}  // namespace

void BeginSection(const char* label, const char* icon, bool fill) {
  BeginDrawerFrame();
  SetNextHeaderIcon((icon && icon[0]) ? icon : nullptr);
  ClosableTreeNode(label ? label : "", nullptr, HeaderFlags_NoCollapse);
  SectionStack().push_back({GetHeaderState().header_bottom_y, fill});
}

void EndSection() {
  if (SectionStack().empty()) {
    return;
  }
  const SectionFrame f = SectionStack().back();
  SectionStack().pop_back();
  EndDrawerFrame(f.header_bottom_y, f.fill);
}

}  // namespace wiesel::editor::ui::section
