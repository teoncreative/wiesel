
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "w_command_palette.h"

#include <imgui_internal.h>

#include <algorithm>
#include <cctype>

#include "util/imgui/imgui_lucide.h"
#include "util/imgui/w_imguiutil.h"

namespace wiesel::editor {

namespace {

std::string ToLower(std::string_view s) {
  std::string out(s);
  for (auto& c : out) {
    c = static_cast<char>(
        std::tolower(static_cast<unsigned char>(c)));
  }
  return out;
}

std::string ToUpper(std::string_view s) {
  std::string out(s);
  for (auto& c : out) {
    c = static_cast<char>(
        std::toupper(static_cast<unsigned char>(c)));
  }
  return out;
}

bool Matches(const Command& cmd, const std::string& filter_lower) {
  if (filter_lower.empty()) {
    return true;
  }
  if (ToLower(cmd.label).find(filter_lower) != std::string::npos) {
    return true;
  }
  if (ToLower(cmd.category).find(filter_lower) != std::string::npos) {
    return true;
  }
  return false;
}

constexpr float kOuterPadX = 10.0f;  // margin between palette edge + rows
constexpr float kRowPadX = 12.0f;    // inner row content inset
constexpr float kRowPadY = 8.0f;     // inner row vertical inset
constexpr float kBadgeScale = 0.75f;
constexpr ImU32 kBadgeBg = IM_COL32(0x23, 0x21, 0x20, 0xff);
constexpr ImU32 kLegendBg = IM_COL32(0x18, 0x16, 0x15, 0xff);

void DrawBadge(ImDrawList* dl, ImFont* font, const char* text, float cy,
               float right_x, float host_font_size) {
  const float font_size = host_font_size * kBadgeScale;
  const ImVec2 text_sz =
      font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, text);
  const float badge_pad_x = 6.0f;
  const float badge_h = host_font_size;
  const float badge_w = text_sz.x + badge_pad_x * 2.0f;
  const ImVec2 badge_max(right_x, cy + badge_h * 0.5f);
  const ImVec2 badge_min(badge_max.x - badge_w, cy - badge_h * 0.5f);
  dl->AddRectFilled(badge_min, badge_max, kBadgeBg, 3.0f);
  dl->AddRect(badge_min, badge_max,
              ImGui::GetColorU32(ImGuiCol_Border), 3.0f, 0, 1.0f);
  dl->AddText(font, font_size,
              ImVec2(badge_min.x + badge_pad_x, cy - text_sz.y * 0.5f),
              ImGui::GetColorU32(ImGuiCol_TextDisabled), text);
}

float BadgeWidth(ImFont* font, const char* text, float host_font_size) {
  const float font_size = host_font_size * kBadgeScale;
  const float w = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, text).x;
  return w + 12.0f;  // matches DrawBadge's 2 * pad_x
}

}  // namespace

void CommandPalette::Register(Command cmd) {
  for (auto& existing : commands_) {
    if (existing.id == cmd.id) {
      existing = std::move(cmd);
      return;
    }
  }
  commands_.push_back(std::move(cmd));
}

void CommandPalette::Open() {
  open_ = true;
  just_opened_ = true;
  search_[0] = '\0';
  selected_index_ = 0;
}

bool CommandPalette::DispatchShortcuts() {
  bool fired = false;
  for (const auto& cmd : commands_) {
    if (cmd.shortcut == 0 || !cmd.action) {
      continue;
    }
    if (cmd.enabled && !cmd.enabled()) {
      continue;
    }
    if (ImGui::IsKeyChordPressed(cmd.shortcut)) {
      cmd.action();
      fired = true;
    }
  }
  return fired;
}

void CommandPalette::RenderRow(const Command& cmd, bool selected,
                               bool /*in_category_view*/) {
  ImGuiWindow* window = ImGui::GetCurrentWindow();
  ImGuiContext& g = *GImGui;
  const float row_h = ImGui::GetFrameHeight() + kRowPadY;

  // Row is inset from both sides by kOuterPadX so the selection has a
  // margin + rounded corners, not a flush edge-to-edge stripe.
  const float avail_w = ImGui::GetContentRegionAvail().x;
  const float row_w = avail_w - kOuterPadX * 2.0f;
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + kOuterPadX);
  const ImVec2 pos = ImGui::GetCursorScreenPos();

  const bool disabled = cmd.enabled && !cmd.enabled();
  if (disabled) {
    ImGui::BeginDisabled();
  }

  ImGui::PushID(cmd.id.c_str());
  const bool clicked =
      ImGui::InvisibleButton("##row", ImVec2(row_w, row_h));
  const bool hovered = ImGui::IsItemHovered();
  ImGui::PopID();

  ImDrawList* dl = window->DrawList;
  const ImVec2 row_min = pos;
  const ImVec2 row_max(pos.x + row_w, pos.y + row_h);
  const float rounding = g.Style.FrameRounding;

  const bool highlighted = selected || hovered;
  if (highlighted) {
    const ImU32 bg_col = ImGui::GetColorU32(hovered ? ImGuiCol_HeaderHovered
                                                    : ImGuiCol_Header);
    dl->AddRectFilled(row_min, row_max, bg_col, rounding);
  }

  const float cy = pos.y + row_h * 0.5f;
  float x = pos.x + kRowPadX;

  // Icon: muted by default, accent when highlighted.
  if (!cmd.icon.empty()) {
    const ImVec2 sz = ImGui::CalcTextSize(cmd.icon.c_str());
    const ImU32 icon_col = highlighted
                               ? ImGui::GetColorU32(ImGuiCol_CheckMark)
                               : ImGui::GetColorU32(ImGuiCol_TextDisabled);
    dl->AddText(ImVec2(x, cy - sz.y * 0.5f), icon_col, cmd.icon.c_str());
    x += sz.x + g.Style.ItemInnerSpacing.x * 2.0f;
  }

  // Label (white).
  const ImVec2 label_sz = ImGui::CalcTextSize(cmd.label.c_str());
  dl->AddText(ImVec2(x, cy - label_sz.y * 0.5f),
              ImGui::GetColorU32(ImGuiCol_Text), cmd.label.c_str());

  float right_edge = row_max.x - kRowPadX;

  // Shortcut badge: right-aligned, monospace, smaller than label.
  if (!cmd.shortcut_text.empty()) {
    ImFont* font = mono_font_ ? mono_font_ : g.Font;
    DrawBadge(dl, font, cmd.shortcut_text.c_str(), cy, right_edge,
              ImGui::GetFontSize());
    right_edge -=
        BadgeWidth(font, cmd.shortcut_text.c_str(), ImGui::GetFontSize()) +
        g.Style.ItemInnerSpacing.x * 2.0f;
  }

  // Trailing label: muted, right-aligned, placed to the left of any
  // shortcut badge. Used to distinguish "Entity" vs "Asset: Texture"
  // style rows in the mixed search results.
  if (!cmd.trailing_label.empty()) {
    const ImVec2 sz = ImGui::CalcTextSize(cmd.trailing_label.c_str());
    dl->AddText(ImVec2(right_edge - sz.x, cy - sz.y * 0.5f),
                ImGui::GetColorU32(ImGuiCol_TextDisabled),
                cmd.trailing_label.c_str());
  }

  if (disabled) {
    ImGui::EndDisabled();
  }

  if (clicked && !disabled && cmd.action) {
    cmd.action();
    open_ = false;
  }
}

void CommandPalette::Render() {
  // Open the popup on the transition to open_.
  static const char* kPopupId = "##CommandPalette";
  if (open_ && !ImGui::IsPopupOpen(kPopupId)) {
    ImGui::OpenPopup(kPopupId);
  }
  if (!open_) {
    return;
  }

  ImGuiViewport* vp = ImGui::GetMainViewport();
  const ImVec2 size(640.0f, 420.0f);
  ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Always,
                          ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(size, ImGuiCond_Always);

  // ModalWindowDimBg dims the rest of the viewport automatically.
  const ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoDocking |
      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  bool visible = ImGui::BeginPopupModal(kPopupId, nullptr, flags);
  ImGui::PopStyleVar();
  if (!visible) {
    open_ = false;
    return;
  }

  // Close on Esc or click outside (BeginPopupModal blocks background
  // interaction, so a click outside the window's rect is our cue).
  if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
    open_ = false;
    ImGui::CloseCurrentPopup();
  }
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
      !ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows)) {
    open_ = false;
    ImGui::CloseCurrentPopup();
  }

  ImGuiContext& g = *GImGui;
  ImFont* mono = mono_font_ ? mono_font_ : g.Font;

  // --- Search input: bigger font, transparent bg, fixed total row
  //     height. Instead of relying on InputText's symmetric FramePadding
  //     (which reads top-biased with large fonts) we wrap it in a
  //     Dummy(top) + Input(smaller frame) + Dummy(bottom) sandwich so we
  //     control the visible top vs bottom spacing independently.
  const float kSearchFontScale = 1.2f;
  ImGui::SetWindowFontScale(kSearchFontScale);
  const float search_font_size = ImGui::GetFontSize();
  ImGui::SetWindowFontScale(1.0f);

  const float search_row_h = std::floor(search_font_size * 2.6f);
  const float input_inner_pad = 2.0f;
  const float input_frame_h = search_font_size + input_inner_pad * 2.0f;
  const float remaining = search_row_h - input_frame_h;
  // Push the visible text slightly below the row midline to counter the
  // font's baseline-biased appearance.
  const float visual_bias = 2.0f;
  const float top_gap = remaining * 0.5f + visual_bias;
  const float bottom_gap = remaining - top_gap;

  const ImVec2 icon_sz_before_scale = ImGui::CalcTextSize(ICON_LC_COMMAND);
  const float scaled_icon_w = icon_sz_before_scale.x * kSearchFontScale;
  const float scaled_icon_h = icon_sz_before_scale.y * kSearchFontScale;
  const float icon_gap = g.Style.ItemInnerSpacing.x * 2.0f;
  const float left_inset = kRowPadX + kOuterPadX;
  const float input_pad_x = left_inset + scaled_icon_w + icon_gap;

  const ImVec2 input_start = ImGui::GetCursorScreenPos();

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
  ImGui::Dummy(ImVec2(0.0f, top_gap));
  ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                      ImVec2(input_pad_x, input_inner_pad));
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
  ImGui::SetWindowFontScale(kSearchFontScale);
  if (just_opened_) {
    ImGui::SetKeyboardFocusHere();
    just_opened_ = false;
  }
  ImGui::InputTextWithHint("##CmdSearch",
                           "Run a command, jump to an asset or entity",
                           search_, sizeof(search_));
  ImGui::SetWindowFontScale(1.0f);
  ImGui::PopStyleVar(3);
  ImGui::PopStyleColor();
  ImGui::Dummy(ImVec2(0.0f, bottom_gap));
  ImGui::PopStyleVar();  // ItemSpacing

  // Draw the ⌘ icon on the left at the text's baseline y so it tracks
  // whatever bias we applied above.
  {
    ImDrawList* top_dl = ImGui::GetWindowDrawList();
    const float text_top = input_start.y + top_gap + input_inner_pad;
    const float cy = text_top + search_font_size * 0.5f;
    top_dl->AddText(
        g.Font, search_font_size,
        ImVec2(input_start.x + left_inset, cy - scaled_icon_h * 0.5f),
        ImGui::GetColorU32(ImGuiCol_TextDisabled), ICON_LC_COMMAND);
  }

  ImGui::FullWidthSeparator();

  // --- Filter -----------------------------------------------------------
  const std::string filter_lower = ToLower(search_);
  const bool has_filter = !filter_lower.empty();

  // Dynamic entries from providers (entities, assets, ...) live in this
  // frame-scoped vector so the const Command* pointers below stay valid
  // while the palette renders.
  std::vector<Command> dynamic_entries;
  if (has_filter) {
    for (auto& provider : providers_) {
      provider(filter_lower, dynamic_entries);
    }
  }

  std::vector<const Command*> filtered;
  filtered.reserve(commands_.size() + dynamic_entries.size());
  for (const auto& cmd : commands_) {
    if (!cmd.action) {
      continue;
    }
    if (Matches(cmd, filter_lower)) {
      filtered.push_back(&cmd);
    }
  }
  for (const auto& cmd : dynamic_entries) {
    filtered.push_back(&cmd);
  }
  if (filtered.empty()) {
    selected_index_ = 0;
  } else {
    selected_index_ = std::clamp(selected_index_, 0,
                                 static_cast<int>(filtered.size()) - 1);
  }

  if (!filtered.empty()) {
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true)) {
      selected_index_ =
          (selected_index_ + 1) % static_cast<int>(filtered.size());
    } else if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true)) {
      selected_index_ =
          (selected_index_ - 1 + static_cast<int>(filtered.size())) %
          static_cast<int>(filtered.size());
    }
  }

  if (!filtered.empty() && ImGui::IsKeyPressed(ImGuiKey_Enter, false)) {
    const Command* cmd = filtered[selected_index_];
    const bool disabled = cmd->enabled && !cmd->enabled();
    if (!disabled && cmd->action) {
      cmd->action();
      open_ = false;
    }
  }

  // --- List (scrolls) then legend (fixed bg strip at the bottom) -------
  const float legend_h = ImGui::GetFrameHeight() + kRowPadY * 1.5f;
  const float list_h =
      ImGui::GetContentRegionAvail().y - legend_h;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                      ImVec2(0.0f, kRowPadY * 0.5f));
  ImGui::BeginChild("##CmdList", ImVec2(0.0f, list_h),
                    ImGuiChildFlags_None, ImGuiWindowFlags_None);
  ImGui::PopStyleVar();

  // Rows stack flush (only categories introduce vertical gaps).
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                      ImVec2(g.Style.ItemSpacing.x, 0.0f));

  if (has_filter) {
    for (size_t i = 0; i < filtered.size(); i++) {
      RenderRow(*filtered[i], static_cast<int>(i) == selected_index_,
                false);
    }
  } else {
    std::vector<std::string> categories;
    for (const auto* cmd : filtered) {
      const std::string& c =
          cmd->category.empty() ? std::string("General") : cmd->category;
      if (std::find(categories.begin(), categories.end(), c) ==
          categories.end()) {
        categories.push_back(c);
      }
    }
    int row_idx = 0;
    for (size_t ci = 0; ci < categories.size(); ci++) {
      const std::string& cat = categories[ci];
      // Consistent gap before every category — including the first — so
      // the list doesn't start flush against the separator.
      ImGui::Dummy(ImVec2(0.0f, kRowPadY));
      ImGui::Indent(kRowPadX + kOuterPadX);
      ImGui::SetWindowFontScale(0.7f);
      ImGui::PushStyleColor(
          ImGuiCol_Text,
          ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
      ImGui::TextUnformatted(ToUpper(cat).c_str());
      ImGui::PopStyleColor();
      ImGui::SetWindowFontScale(1.0f);
      ImGui::Unindent(kRowPadX + kOuterPadX);
      ImGui::Dummy(ImVec2(0.0f, kRowPadY * 0.25f));

      for (const auto* cmd : filtered) {
        const std::string& c =
            cmd->category.empty() ? std::string("General") : cmd->category;
        if (c != cat) {
          continue;
        }
        RenderRow(*cmd, row_idx == selected_index_, true);
        row_idx++;
      }
    }
  }
  ImGui::PopStyleVar();  // ItemSpacing
  ImGui::EndChild();

  // --- Legend strip: dark bg (#181615), key badges + result count -----
  ImGuiWindow* palette_win = ImGui::GetCurrentWindow();
  const ImVec2 legend_pos = ImGui::GetCursorScreenPos();
  const float legend_right = palette_win->Pos.x + palette_win->Size.x;
  const float legend_bottom = palette_win->Pos.y + palette_win->Size.y;
  const float legend_cy = (legend_pos.y + legend_bottom) * 0.5f;
  ImDrawList* dl = ImGui::GetWindowDrawList();
  // Bottom corners rounded to follow the window radius; the top edge
  // stays square so it butts against the separator above. The rect
  // extends all the way to the window's physical bottom so the legend
  // can't overflow past the rounded corners.
  // Popups get drawn with PopupRounding, not WindowRounding — use the
  // same value the window's top corners are using so all four corners
  // line up to the same radius.
  const float corner_r =
      g.Style.PopupRounding > 0.0f ? g.Style.PopupRounding
                                   : g.Style.WindowRounding;
  dl->AddRectFilled(legend_pos, ImVec2(legend_right, legend_bottom),
                    kLegendBg, corner_r, ImDrawFlags_RoundCornersBottom);

  const ImU32 muted = ImGui::GetColorU32(ImGuiCol_TextDisabled);
  const float host_fs = ImGui::GetFontSize();

  struct Hint {
    std::vector<const char*> keys;
    const char* text;
  };
  const Hint hints[] = {
      {{"Up", "Down"}, " Navigate"},
      {{"Enter"},      " Run"},
      {{"Esc"},        " Close"},
  };

  // Legend text + count render in the same monospace font used by the
  // shortcut badges, at the same reduced size.
  const float legend_font_size = host_fs * kBadgeScale;

  float x = legend_pos.x + kRowPadX;
  for (const auto& h : hints) {
    for (const char* key : h.keys) {
      const float w = BadgeWidth(mono, key, host_fs);
      DrawBadge(dl, mono, key, legend_cy, x + w, host_fs);
      x += w + 4.0f;
    }
    const ImVec2 text_sz =
        mono->CalcTextSizeA(legend_font_size, FLT_MAX, 0.0f, h.text);
    dl->AddText(mono, legend_font_size,
                ImVec2(x, legend_cy - text_sz.y * 0.5f), muted, h.text);
    x += text_sz.x + 12.0f;
  }

  std::string count = std::to_string(filtered.size()) +
                      (filtered.size() == 1 ? " result" : " results");
  const ImVec2 count_sz =
      mono->CalcTextSizeA(legend_font_size, FLT_MAX, 0.0f, count.c_str());
  dl->AddText(mono, legend_font_size,
              ImVec2(legend_right - kRowPadX - count_sz.x,
                     legend_cy - count_sz.y * 0.5f),
              muted, count.c_str());
  ImGui::Dummy(ImVec2(0.0f, legend_h));

  ImGui::EndPopup();
}

}  // namespace wiesel::editor
