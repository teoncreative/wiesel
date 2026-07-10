//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "util/imgui/imgui_theme.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>

#include "util/w_logger.h"
#include "w_engine.h"

namespace ImGui {
namespace Moonlight {

// OKLCH -> sRGB. Standard Oklab coefficients (Björn Ottosson) + linear->sRGB
// gamma encoding. H is degrees.
static ImVec4 Oklch(float L, float C, float H, float alpha = 1.0f) {
  constexpr float kPi = 3.14159265358979323846f;
  const float h_rad = H * kPi / 180.0f;
  const float a = C * std::cos(h_rad);
  const float b = C * std::sin(h_rad);

  const float l_ = L + 0.3963377774f * a + 0.2158037573f * b;
  const float m_ = L - 0.1055613458f * a - 0.0638541728f * b;
  const float s_ = L - 0.0894841775f * a - 1.2914855480f * b;

  const float lc = l_ * l_ * l_;
  const float mc = m_ * m_ * m_;
  const float sc = s_ * s_ * s_;

  float rlin =  4.0767416621f * lc - 3.3077115913f * mc + 0.2309699292f * sc;
  float glin = -1.2684380046f * lc + 2.6097574011f * mc - 0.3413193965f * sc;
  float blin = -0.0041960863f * lc - 0.7034186147f * mc + 1.7076147010f * sc;

  auto lin_to_srgb = [](float x) {
    if (x <= 0.0f) return 0.0f;
    if (x <= 0.0031308f) return 12.92f * x;
    return 1.055f * std::pow(x, 1.0f / 2.4f) - 0.055f;
  };

  return ImVec4(std::clamp(lin_to_srgb(rlin), 0.0f, 1.0f),
                std::clamp(lin_to_srgb(glin), 0.0f, 1.0f),
                std::clamp(lin_to_srgb(blin), 0.0f, 1.0f), alpha);
}

// Hand calibrated, might be off in some cases
static constexpr float kDisplayLShift = 0.030f;

static ImVec4 N(float L, float C = 0.004f) {
  return Oklch(L + kDisplayLShift, C, 60.0f);
}

// Convenience mixers for pre-composited accent tints (alpha over Bg1).
static ImVec4 WithAlpha(const ImVec4& c, float a) {
  return ImVec4(c.x, c.y, c.z, a);
}

void LoadFont() {
  static const ImWchar ranges[] = {
      0x0020, 0x00FF,  // Basic Latin + Latin Supplement
      0x00c7, 0x00c7,  // Ç
      0x00e7, 0x00e7,  // ç
      0x011e, 0x011e,  // Ğ
      0x011f, 0x011f,  // ğ
      0x0130, 0x0130,  // İ
      0x0131, 0x0131,  // ı
      0x00d6, 0x00d6,  // Ö
      0x00f6, 0x00f6,  // ö
      0x015e, 0x015e,  // Ş
      0x015f, 0x015f,  // ş
      0x00dc, 0x00dc,  // Ü
      0x00fc, 0x00fc,  // ü
      0};

  wiesel::VfsFile file = wiesel::Engine::vfs()->Open(kDefaultFontPath);
  if (!file) {
    LOG_ERROR("Failed to load font: {}", kDefaultFontPath);
    return;
  }

  // ImGui takes ownership of the buffer (calls IM_FREE), so allocate with IM_ALLOC
  size_t font_size = file.Size();
  void* font_data = IM_ALLOC(font_size);
  memcpy(font_data, file.Data(), font_size);

  // Exclude Inter's PUA glyphs (stylistic alternates) across lucide's range
  // so the merged lucide icon font wins for every icon codepoint.
  static const ImWchar exclude_ranges[] = {0xE000, 0xE6DE, 0};

  ImFontConfig config;
  config.GlyphRanges = ranges;
  config.GlyphExcludeRanges = exclude_ranges;
  config.GlyphOffset = ImVec2(0.0f, kDefaultFontOffsetY);
  config.OversampleH = 2;
  config.OversampleV = 2;
  // Select named instance in variable font: (instance_index << 16) | face_index
  config.FontNo = (kDefaultFontInstance << 16);

  ImGuiIO& io = ImGui::GetIO();
  ImFont* font = io.Fonts->AddFontFromMemoryTTF(
      font_data, static_cast<int>(font_size), kDefaultRegularFontSize, &config);
  if (!font) {
    LOG_ERROR("Failed to create ImGui font");
    return;
  }
  io.FontDefault = font;
}

static Theme current_theme_ = Theme::DarkGray;

const char* GetThemeName(Theme theme) {
  switch (theme) {
    case Theme::DarkGray:
      return "Dark Gray";
    default:
      return "Unknown";
  }
}

Theme GetCurrentTheme() {
  return current_theme_;
}

static void ApplySharedStyle() {
  ImGuiStyle& style = ImGui::GetStyle();

  style.WindowRounding    = 10.0f;
  style.ChildRounding     = 10.0f;
  style.PopupRounding     = 10.0f;
  style.FrameRounding     = 6.0f;
  style.GrabRounding      = 4.0f;
  style.TabRounding       = 0.0f;  // square tabs
  style.ScrollbarRounding = 10.0f;

  // Matched WindowPadding.y and ItemSpacing.y keep first-row spacing
  // equal to inter-row spacing.
  style.WindowPadding    = ImVec2(10.0f, 10.0f);

  style.FramePadding     = ImVec2(8.0f, 6.0f);
  style.CellPadding      = ImVec2(4.0f, 4.0f);
  style.ItemSpacing      = ImVec2(10.0f, 10.0f);
  style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
  style.IndentSpacing    = 14.0f;
  style.ScrollbarSize    = 10.0f;
  style.GrabMinSize      = 12.0f;

  style.WindowBorderSize = 1.0f;
  style.FrameBorderSize  = 1.0f;
  style.TabBorderSize    = 1.0f;
  style.TabBarOverlineSize = 2.0f;
  // -1 = always show the close button (not only on hover).
  style.TabCloseButtonMinWidthSelected   = -1.0f;
  style.TabCloseButtonMinWidthUnselected = -1.0f;
  style.PopupBorderSize  = 1.0f;
  style.SeparatorTextBorderSize = 1.0f;

  // Docked panel isolation — inset every docked window by this padding so
  // adjacent panels have a visible gap (fork addition).
  style.DockingSeparatorSize = 0.0f;
  style.DockingWindowPadding = ImVec2(3.0f, 3.0f);
  style.DockingTabBarExtraHeight = 12.0f;

  style.WindowMenuButtonPosition = ImGuiDir_None;
  // BeginMainMenuBar uses DisplaySafeAreaPadding as the menu's left inset.
  style.DisplaySafeAreaPadding = ImVec2(10.0f, 10.0f);
}

static void ApplyDarkGray() {
  // OKLCH tokens (C=0.004 H=60 unless noted), L shift applied inside N().
  const ImVec4 bg_0      = N(0.170f);
  const ImVec4 bg_1      = N(0.195f);
  const ImVec4 bg_2      = N(0.225f);
  const ImVec4 bg_3      = N(0.260f);
  const ImVec4 bg_hover  = N(0.235f);

  const ImVec4 border_soft    = N(0.235f);
  const ImVec4 border         = N(0.280f);
  const ImVec4 border_strong  = N(0.340f);

  const ImVec4 fg       = Oklch(0.94f, 0.004f, 60.0f);
  const ImVec4 fg_muted = Oklch(0.72f, 0.004f, 60.0f);

  // Accent kept as direct sRGB so it's unaffected by the neutral L shift.
  const ImVec4 accent        = ImVec4(0.9647f, 0.4235f, 0.4000f, 1.0f);  // #f66c66
  const ImVec4 accent_hover  = ImVec4(0.980f,  0.540f,  0.500f,  1.0f);
  const ImVec4 accent_dim    = WithAlpha(accent, 0.18f);

  ImVec4* col = ImGui::GetStyle().Colors;

  col[ImGuiCol_WindowBg]    = bg_1;
  col[ImGuiCol_ChildBg]     = bg_1;
  col[ImGuiCol_PopupBg]     = bg_1;
  col[ImGuiCol_MenuBarBg]   = bg_0;

  col[ImGuiCol_Border]       = border_soft;
  col[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

  col[ImGuiCol_Text]         = fg;
  // Unified muted tone (#a5a2a1). Used by disabled text, tree arrows,
  // breadcrumbs, toolbar button labels, etc.
  col[ImGuiCol_TextDisabled] = ImVec4(0.6471f, 0.6353f, 0.6314f, 1.0f);

  col[ImGuiCol_FrameBg]        = bg_0;
  col[ImGuiCol_FrameBgHovered] = bg_2;
  col[ImGuiCol_FrameBgActive]  = accent_dim;

  // Same bg across states so focus/collapse don't flip the title color.
  col[ImGuiCol_TitleBg]          = bg_1;
  col[ImGuiCol_TitleBgActive]    = bg_1;
  col[ImGuiCol_TitleBgCollapsed] = bg_1;

  col[ImGuiCol_Button]        = bg_2;
  col[ImGuiCol_ButtonHovered] = bg_3;
  col[ImGuiCol_ButtonActive]  = accent_dim;

  col[ImGuiCol_Header]        = accent_dim;
  col[ImGuiCol_HeaderHovered] = bg_hover;
  col[ImGuiCol_HeaderActive]  = accent_dim;

  // Transparent tab fills — selection/focus are shown via the overline and
  // the icon/text colors emitted by TabItemLabelAndCloseButton (fork patch).
  const ImVec4 transparent = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
  col[ImGuiCol_Tab]                       = transparent;
  col[ImGuiCol_TabHovered]                = bg_2;
  col[ImGuiCol_TabSelected]               = transparent;
  col[ImGuiCol_TabSelectedOverline]       = accent;
  col[ImGuiCol_TabDimmedSelectedOverline] = accent;
  col[ImGuiCol_TabDimmed]                 = transparent;
  col[ImGuiCol_TabDimmedSelected]         = transparent;

  col[ImGuiCol_CheckMark]         = accent;
  col[ImGuiCol_SliderGrab]        = accent;
  col[ImGuiCol_SliderGrabActive]  = accent_hover;
  col[ImGuiCol_ResizeGrip]        = border;
  col[ImGuiCol_ResizeGripHovered] = accent;
  col[ImGuiCol_ResizeGripActive]  = accent;

  col[ImGuiCol_Separator]        = border_soft;
  col[ImGuiCol_SeparatorHovered] = accent;
  col[ImGuiCol_SeparatorActive]  = accent;

  col[ImGuiCol_ScrollbarBg]          = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
  col[ImGuiCol_ScrollbarGrab]        = bg_3;
  col[ImGuiCol_ScrollbarGrabHovered] = border;
  col[ImGuiCol_ScrollbarGrabActive]  = border_strong;

  col[ImGuiCol_PlotLines]            = fg_muted;
  col[ImGuiCol_PlotLinesHovered]     = accent_hover;
  col[ImGuiCol_PlotHistogram]        = accent;
  col[ImGuiCol_PlotHistogramHovered] = accent_hover;

  col[ImGuiCol_TableHeaderBg]     = bg_1;
  col[ImGuiCol_TableBorderStrong] = border;
  col[ImGuiCol_TableBorderLight]  = border_soft;
  col[ImGuiCol_TableRowBg]        = bg_1;
  col[ImGuiCol_TableRowBgAlt]     = bg_2;

  col[ImGuiCol_TextSelectedBg]        = WithAlpha(accent, 0.30f);
  col[ImGuiCol_DragDropTarget]        = accent;
  col[ImGuiCol_NavHighlight]          = accent;
  col[ImGuiCol_NavWindowingHighlight] = ImVec4(1.0f, 1.0f, 1.0f, 0.70f);
  col[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.0f, 0.0f, 0.0f, 0.55f);
  col[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.0f, 0.0f, 0.0f, 0.55f);

  col[ImGuiCol_DockingPreview] = accent_dim;
  col[ImGuiCol_DockingEmptyBg] = bg_0;
}

void ApplyTheme(Theme theme) {
  ApplySharedStyle();
  ApplyDarkGray();
  current_theme_ = theme;
}

}  // namespace Moonlight
}  // namespace ImGui