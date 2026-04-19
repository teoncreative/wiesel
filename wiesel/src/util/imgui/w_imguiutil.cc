
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
#include "util/imgui/imgui_lucide.h"

namespace wiesel {

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

}  // namespace wiesel

namespace ImGui {

static thread_local const char* g_next_tree_node_icon = nullptr;
static thread_local ComponentHeaderState g_header_state{};

void SetNextTreeNodeIcon(const char* icon) {
  g_next_tree_node_icon = (icon && icon[0]) ? icon : nullptr;
}

void ResetComponentHeaderState() {
  g_header_state = ComponentHeaderState{};
}

const ComponentHeaderState& GetComponentHeaderState() {
  return g_header_state;
}

bool ClosableTreeNode(const char* label, bool* p_visible) {
  ImGuiContext& g = *GImGui;
  ImGuiWindow* window = ImGui::GetCurrentWindow();
  if (window->SkipItems) {
    return false;
  }

  ImGui::PushID(label);

  const char* icon_glyph = g_next_tree_node_icon;
  g_next_tree_node_icon = nullptr;

  const ImGuiID id = window->GetID(label);
  ImGuiStorage* storage = window->DC.StateStorage;
  bool open = storage->GetBool(id, false);

  // Height matches the docked tab bar so headers read as the same bar size.
  const float h = ImGui::GetFrameHeight() + g.Style.DockingTabBarExtraHeight;
  const ImVec2 cursor_start = ImGui::GetCursorScreenPos();
  // Clickable region spans the content area; the background/border/icon
  // visuals are drawn out to the window's edges to ignore WindowPadding.
  const float row_w = ImGui::GetContentRegionAvail().x;
  const float close_w = (p_visible != nullptr) ? h : 0.0f;
  const float main_w = row_w - close_w;

  ImGui::InvisibleButton("##header", ImVec2(main_w, h));
  if (ImGui::IsItemClicked()) {
    open = !open;
    storage->SetBool(id, open);
  }

  ImDrawList* dl = window->DrawList;
  const float cy = cursor_start.y + h * 0.5f;
  const float full_left = window->Pos.x;

  const char* chev =
      open ? ICON_LC_CHEVRON_DOWN : ICON_LC_CHEVRON_RIGHT;
  const ImVec2 chev_sz = ImGui::CalcTextSize(chev);
  const float left_pad = g.Style.FramePadding.x;
  const ImVec2 chev_pos(full_left + left_pad, cy - chev_sz.y * 0.5f);
  dl->AddText(chev_pos, ImGui::GetColorU32(ImGuiCol_TextDisabled), chev);
  float x_cursor = chev_pos.x + chev_sz.x + g.Style.ItemInnerSpacing.x;

  if (icon_glyph) {
    const ImVec2 icon_sz = ImGui::CalcTextSize(icon_glyph);
    const ImVec2 icon_pos(x_cursor, cy - icon_sz.y * 0.5f);
    dl->AddText(icon_pos, ImGui::GetColorU32(ImGuiCol_CheckMark), icon_glyph);
    x_cursor = icon_pos.x + icon_sz.x + g.Style.ItemInnerSpacing.x;
  }

  const ImVec2 name_sz = ImGui::CalcTextSize(label);
  const ImVec2 name_pos(x_cursor, cy - name_sz.y * 0.5f);
  dl->AddText(name_pos, ImGui::GetColorU32(ImGuiCol_Text), label);

  if (p_visible != nullptr) {
    ImGui::SameLine(0.0f, 0.0f);
    ImGui::InvisibleButton("##close", ImVec2(close_w, h));
    const bool close_hovered = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) {
      *p_visible = false;
    }
    // Mirror the chevron's left inset on the right side.
    const float full_right = window->Pos.x + window->Size.x;
    const ImVec2 x_sz = ImGui::CalcTextSize(ICON_LC_X);
    const ImVec2 x_pos(full_right - left_pad - x_sz.x,
                       cy - x_sz.y * 0.5f);
    dl->AddText(x_pos,
                ImGui::GetColorU32(close_hovered ? ImGuiCol_Text
                                                 : ImGuiCol_TextDisabled),
                ICON_LC_X);
  }

  g_header_state.header_rendered = true;
  g_header_state.open = open;
  g_header_state.header_bottom_y = cursor_start.y + h;

  ImGui::PopID();
  // TreeNodeBehavior contract: open => push so callers' TreePop balances.
  if (open) {
    ImGui::TreePushOverrideID(id);
  }
  return open;
}

struct ToolbarGroupState {
  ImDrawListSplitter splitter;
  ImDrawList* draw_list;
  float pad;
  bool active;
};

static ToolbarGroupState& GetToolbarGroupState() {
  static ToolbarGroupState s{};
  return s;
}

void BeginToolbarGroup(const char* id) {
  ToolbarGroupState& st = GetToolbarGroupState();
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
  ToolbarGroupState& st = GetToolbarGroupState();
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
  if (active || held)
    bg_col = ImGui::GetColorU32(ImGuiCol_ButtonActive);
  else if (hovered)
    bg_col = ImGui::GetColorU32(ImGuiCol_ButtonHovered);

  ImDrawList* dl = ImGui::GetWindowDrawList();
  if (bg_col != 0) {
    dl->AddRectFilled(pos, ImVec2(pos.x + sz, pos.y + sz), bg_col,
                      g.Style.FrameRounding);
  }

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

void DashedRectRounded(ImDrawList* dl, const ImVec2& p_min,
                       const ImVec2& p_max, ImU32 col, float rounding,
                       float thickness, float dash_len, float gap_len) {
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

void FullWidthSeparator() {
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

bool HierarchyTreeNodeEx(const char* label, ImGuiTreeNodeFlags flags,
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
  const float win_left = window->Pos.x;
  const float win_right = window->Pos.x + window->Size.x;
  ImDrawList* dl = window->DrawList;

  auto fill_gutters = [&](ImU32 col) {
    if (item_min.x > win_left) {
      dl->AddRectFilled(ImVec2(win_left, item_min.y),
                        ImVec2(item_min.x, item_max.y), col);
    }
    if (win_right > item_max.x) {
      dl->AddRectFilled(ImVec2(item_max.x, item_min.y),
                        ImVec2(win_right, item_max.y), col);
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
    if (held && hovered)
      bg_col = ImGui::GetColorU32(ImGuiCol_HeaderActive);
    else if (hovered)
      bg_col = ImGui::GetColorU32(ImGuiCol_HeaderHovered);
    else
      bg_col = ImGui::GetColorU32(ImGuiCol_Header);
    fill_gutters(bg_col);
  }
  if (is_selected) {
    // 2px accent line on the far left.
    dl->AddRectFilled(ImVec2(win_left, item_min.y),
                      ImVec2(win_left + 2.0f, item_max.y),
                      ImGui::GetColorU32(ImGuiCol_CheckMark));
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