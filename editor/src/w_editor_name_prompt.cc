//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "w_editor_name_prompt.h"

#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>
#include <vector>

#include "ui/w_ui_draw.h"
#include "ui/w_ui_layout.h"
#include "ui/w_ui_popup.h"
#include "ui/w_ui_style.h"
#include "util/imgui/imgui_lucide.h"
#include "util/w_vfs.h"
#include "w_engine.h"

namespace wiesel::editor {

namespace style = ui::style;
namespace draw = ui::draw;

void NamePromptPopup::Open(NamePromptRequest req) {
  req_ = std::move(req);
  std::snprintf(name_buf_, sizeof(name_buf_), "%s",
                req_.default_name.c_str());
  open_ = true;
  just_opened_ = true;
}

void NamePromptPopup::Render() {
  static const char* kPopupId = "##NamePrompt";
  if (open_ && !ImGui::IsPopupOpen(kPopupId)) {
    ImGui::OpenPopup(kPopupId);
  }
  if (!open_) {
    return;
  }

  if (!ui::popup::BeginCentered(kPopupId, ImVec2(520.0f, 0.0f),
                                /*auto_resize=*/true, &open_)) {
    return;
  }
  if (!open_) {
    ui::popup::EndCentered();
    return;
  }

  ImGuiContext& g = *GImGui;
  ImFont* mono = mono_font_ ? mono_font_ : g.Font;

  // Input row (mirrors command palette layout)
  const float kInputFontScale = 1.2f;
  ImGui::SetWindowFontScale(kInputFontScale);
  const float input_font_size = ImGui::GetFontSize();
  ImGui::SetWindowFontScale(1.0f);

  const float input_row_h = std::floor(input_font_size * 2.6f);
  const float input_inner_pad = 2.0f;
  const float input_frame_h = input_font_size + input_inner_pad * 2.0f;
  const float remaining = input_row_h - input_frame_h;
  const float visual_bias = 2.0f;
  const float top_gap = remaining * 0.5f + visual_bias;
  const float bottom_gap = remaining - top_gap;

  const char* icon_str =
      req_.icon.empty() ? ICON_LC_FILE_PLUS : req_.icon.c_str();
  const ImVec2 icon_sz_before_scale = ImGui::CalcTextSize(icon_str);
  const float scaled_icon_w = icon_sz_before_scale.x * kInputFontScale;
  const float scaled_icon_h = icon_sz_before_scale.y * kInputFontScale;
  const float icon_gap = g.Style.ItemInnerSpacing.x * 2.0f;
  const float left_inset = style::kRowInnerPadX + style::kRowOuterPadX;
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
  ImGui::SetWindowFontScale(kInputFontScale);
  if (just_opened_) {
    ImGui::SetKeyboardFocusHere();
    just_opened_ = false;
  }
  bool submitted = ImGui::InputTextWithHint(
      "##NamePromptInput",
      req_.hint.empty() ? "Name..." : req_.hint.c_str(), name_buf_,
      sizeof(name_buf_), ImGuiInputTextFlags_EnterReturnsTrue);
  ImGui::SetWindowFontScale(1.0f);
  ImGui::PopStyleVar(3);
  ImGui::PopStyleColor();
  ImGui::Dummy(ImVec2(0.0f, bottom_gap));
  ImGui::PopStyleVar();  // ItemSpacing

  // Draw the icon at the input baseline.
  {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float text_top = input_start.y + top_gap + input_inner_pad;
    const float cy = text_top + input_font_size * 0.5f;
    dl->AddText(g.Font, input_font_size,
                ImVec2(input_start.x + left_inset, cy - scaled_icon_h * 0.5f),
                ImGui::GetColorU32(ImGuiCol_TextDisabled), icon_str);
  }

  ui::layout::Separator();

  // Body: title + path preview
  ImGui::Dummy(ImVec2(0.0f, style::kRowInnerPadY));

  ImGui::Indent(style::kRowInnerPadX + style::kRowOuterPadX);
  if (!req_.title.empty()) {
    ImGui::SetWindowFontScale(0.7f);
    ImGui::PushStyleColor(ImGuiCol_Text,
                          ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    std::string upper;
    upper.reserve(req_.title.size());
    for (char c : req_.title) {
      upper.push_back(
          static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }
    ImGui::TextUnformatted(upper.c_str());
    ImGui::PopStyleColor();
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Dummy(ImVec2(0.0f, 2.0f));
  }

  // Preview line: full vfs path with extension. If a file/dir already lives
  // there we paint the path red and show a warning beneath it; the Enter
  // shortcut is blocked further down so the user can't accidentally clobber
  // an existing asset.
  std::string preview = req_.base_dir;
  if (!preview.empty() && preview.back() != '/') {
    preview += '/';
  }
  preview += name_buf_[0] ? name_buf_ : "<name>";
  preview += req_.extension;

  bool collision = false;
  if (name_buf_[0] != '\0') {
    VirtualFileSystem* vfs = Engine::vfs().get();
    if (vfs) {
      collision = req_.extension.empty() ? vfs->DirectoryExists(preview)
                                         : vfs->FileExists(preview);
    }
  }

  const ImVec4 muted_text = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
  // Warm-leaning red, plays nicer with the dark theme than ImGui's default.
  const ImVec4 warn_text(0.95f, 0.45f, 0.45f, 1.0f);
  ImGui::PushStyleColor(ImGuiCol_Text, collision ? warn_text : muted_text);
  ImGui::PushFont(mono);
  ImGui::TextUnformatted(preview.c_str());
  ImGui::PopFont();
  ImGui::PopStyleColor();

  if (collision) {
    ImGui::PushStyleColor(ImGuiCol_Text, warn_text);
    ImGui::TextUnformatted(req_.extension.empty()
                               ? "A folder with this name already exists."
                               : "A file with this name already exists.");
    ImGui::PopStyleColor();
  }

  ImGui::Unindent(style::kRowInnerPadX + style::kRowOuterPadX);
  ImGui::Dummy(ImVec2(0.0f, style::kRowInnerPadY));

  // Legend strip
  const float legend_h = ImGui::GetFrameHeight() + style::kRowInnerPadY * 1.5f;
  ImGuiWindow* win = ImGui::GetCurrentWindow();
  const ImVec2 legend_pos = ImGui::GetCursorScreenPos();
  const float legend_right = win->Pos.x + win->Size.x;
  const float legend_bottom = win->Pos.y + win->Size.y;
  const float legend_cy = (legend_pos.y + legend_bottom) * 0.5f;
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const float corner_r =
      g.Style.PopupRounding > 0.0f ? g.Style.PopupRounding
                                   : g.Style.WindowRounding;
  dl->AddRectFilled(legend_pos, ImVec2(legend_right, legend_bottom),
                    style::kLegendBg, corner_r, ImDrawFlags_RoundCornersBottom);

  const ImU32 muted = ImGui::GetColorU32(ImGuiCol_TextDisabled);
  const float host_fs = ImGui::GetFontSize();
  const float legend_font_size = host_fs * style::kBadgeScale;

  struct Hint {
    std::vector<const char*> keys;
    const char* text;
  };
  const Hint hints[] = {
      {{"Enter"}, " Create"},
      {{"Esc"}, " Cancel"},
  };
  float x = legend_pos.x + style::kRowInnerPadX;
  for (const auto& h : hints) {
    for (const char* key : h.keys) {
      const float w = draw::BadgeWidth(mono, key, host_fs);
      draw::DrawBadge(dl, mono, key, legend_cy, x + w, host_fs);
      x += w + 4.0f;
    }
    const ImVec2 text_sz =
        mono->CalcTextSizeA(legend_font_size, FLT_MAX, 0.0f, h.text);
    dl->AddText(mono, legend_font_size,
                ImVec2(x, legend_cy - text_sz.y * 0.5f), muted, h.text);
    x += text_sz.x + 12.0f;
  }
  ImGui::Dummy(ImVec2(0.0f, legend_h));

  // Confirmation
  // Block creation when a file/dir already lives at the computed path so the
  // user has to disambiguate before clobbering anything.
  if (submitted && name_buf_[0] != '\0' && !collision) {
    if (req_.on_confirm) {
      req_.on_confirm(name_buf_, preview);
    }
    open_ = false;
    ImGui::CloseCurrentPopup();
  }

  ui::popup::EndCentered();
}

}  // namespace wiesel::editor
