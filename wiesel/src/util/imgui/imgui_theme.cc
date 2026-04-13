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

#include "util/w_logger.h"
#include "w_engine.h"

namespace ImGui {
namespace Moonlight {

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

  Wiesel::VfsFile file = Wiesel::Engine::vfs()->Open(kDefaultFontPath);
  if (!file) {
    LOG_ERROR("Failed to load font: {}", kDefaultFontPath);
    return;
  }

  // ImGui takes ownership of the buffer (calls IM_FREE), so allocate with IM_ALLOC
  size_t font_size = file.Size();
  void* font_data = IM_ALLOC(font_size);
  memcpy(font_data, file.Data(), font_size);

  // Exclude the editor icon PUA range so merged icon fonts take priority
  static const ImWchar exclude_ranges[] = {0xE000, 0xE0FF, 0};

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
    case Theme::OLED:
      return "OLED";
    default:
      return "Unknown";
  }
}

Theme GetCurrentTheme() {
  return current_theme_;
}

static void ApplySharedStyle() {
  ImGuiStyle& style = ImGui::GetStyle();

  style.WindowRounding = 8.0f;
  style.ChildRounding = 6.0f;
  style.PopupRounding = 8.0f;
  style.FrameRounding = 4.0f;
  style.GrabRounding = 4.0f;
  style.TabRounding = 6.0f;
  style.ScrollbarRounding = 6.0f;

  style.WindowPadding = {10.0f, 10.0f};
  style.FramePadding = {6.0f, 6.0f};
  style.CellPadding = {4.0f, 4.0f};
  style.ItemSpacing = {6.0f, 6.0f};
  style.ItemInnerSpacing = {6.0f, 4.0f};

  style.WindowBorderSize = 1.0f;
  style.FrameBorderSize = 1.0f;
  style.TabBorderSize = 0.0f;
  style.PopupBorderSize = 1.0f;
  style.ScrollbarSize = 11.0f;
  style.GrabMinSize = 9.0f;
  style.IndentSpacing = 18.0f;
  style.SeparatorTextBorderSize = 1.0f;
}

struct ThemeColors {
  ImVec4 accent;
  ImVec4 accent_hover;
  ImVec4 accent_active;
  float bg;
  float child;
  float popup;
  float frame;
  float frame_hover;
  float frame_active;
  float title;
  float menubar;
  float scrollbar_bg;
  float scrollbar_grab;
  float scrollbar_grab_hover;
  float btn;
  float btn_hover;
  float btn_active;
  float header;
  float header_hover;
  float border;
  float tab;
  float tab_hover;
  float tab_selected;
  float tab_dimmed;
  float table_header;
  float table_row;
  float table_row_alt;
};

static void ApplyColors(const ThemeColors& c) {
  ImVec4* colors = ImGui::GetStyle().Colors;
  auto g = [](float v) {
    return ImVec4(v, v, v, 1.0f);
  };
  auto ga = [](float v, float a) {
    return ImVec4(v, v, v, a);
  };

  colors[ImGuiCol_Text] = ImVec4(0.93f, 0.93f, 0.93f, 1.0f);
  colors[ImGuiCol_TextDisabled] = ImVec4(0.42f, 0.42f, 0.42f, 1.0f);
  colors[ImGuiCol_WindowBg] = g(c.bg);
  colors[ImGuiCol_ChildBg] = ga(c.child, 0.0f);
  colors[ImGuiCol_PopupBg] = ga(c.popup, 0.97f);
  colors[ImGuiCol_Border] = ga(c.border, 0.45f);
  colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
  colors[ImGuiCol_FrameBg] = g(c.frame);
  colors[ImGuiCol_FrameBgHovered] = g(c.frame_hover);
  colors[ImGuiCol_FrameBgActive] = g(c.frame_active);
  colors[ImGuiCol_TitleBg] = g(c.title);
  colors[ImGuiCol_TitleBgActive] = g(c.title + 0.02f);
  colors[ImGuiCol_TitleBgCollapsed] = g(c.title);
  colors[ImGuiCol_MenuBarBg] = g(c.menubar);
  colors[ImGuiCol_ScrollbarBg] = ga(c.scrollbar_bg, 0.5f);
  colors[ImGuiCol_ScrollbarGrab] = g(c.scrollbar_grab);
  colors[ImGuiCol_ScrollbarGrabHovered] = g(c.scrollbar_grab_hover);
  colors[ImGuiCol_ScrollbarGrabActive] = g(c.scrollbar_grab_hover + 0.06f);
  colors[ImGuiCol_CheckMark] = c.accent;
  colors[ImGuiCol_SliderGrab] = c.accent;
  colors[ImGuiCol_SliderGrabActive] = c.accent_hover;
  colors[ImGuiCol_Button] = g(c.btn);
  colors[ImGuiCol_ButtonHovered] = g(c.btn_hover);
  colors[ImGuiCol_ButtonActive] = g(c.btn_active);
  colors[ImGuiCol_Header] = ga(c.header, 0.8f);
  colors[ImGuiCol_HeaderHovered] = ga(c.header_hover, 0.9f);
  colors[ImGuiCol_HeaderActive] = g(c.btn_active);
  colors[ImGuiCol_Separator] = ga(c.border, 0.35f);
  colors[ImGuiCol_SeparatorHovered] = c.accent;
  colors[ImGuiCol_SeparatorActive] = c.accent_active;
  colors[ImGuiCol_ResizeGrip] = ga(c.btn, 0.4f);
  colors[ImGuiCol_ResizeGripHovered] = c.accent;
  colors[ImGuiCol_ResizeGripActive] = c.accent_active;
  colors[ImGuiCol_Tab] = g(c.tab);
  colors[ImGuiCol_TabHovered] = g(c.tab_hover);
  colors[ImGuiCol_TabSelected] = g(c.tab_selected);
  colors[ImGuiCol_TabSelectedOverline] = c.accent;
  colors[ImGuiCol_TabDimmed] = g(c.tab_dimmed);
  colors[ImGuiCol_TabDimmedSelected] = g(c.tab_selected - 0.02f);
  colors[ImGuiCol_PlotLines] = ImVec4(0.50f, 0.50f, 0.50f, 1.0f);
  colors[ImGuiCol_PlotLinesHovered] = c.accent_hover;
  colors[ImGuiCol_PlotHistogram] = c.accent;
  colors[ImGuiCol_PlotHistogramHovered] = c.accent_hover;
  colors[ImGuiCol_TableHeaderBg] = g(c.table_header);
  colors[ImGuiCol_TableBorderStrong] = ga(c.border, 0.5f);
  colors[ImGuiCol_TableBorderLight] = ga(c.border, 0.25f);
  colors[ImGuiCol_TableRowBg] = g(c.table_row);
  colors[ImGuiCol_TableRowBgAlt] = g(c.table_row_alt);
  colors[ImGuiCol_TextSelectedBg] =
      ImVec4(c.accent.x, c.accent.y, c.accent.z, 0.30f);
  colors[ImGuiCol_DragDropTarget] = c.accent;
  colors[ImGuiCol_NavHighlight] = c.accent;
  colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.0f, 1.0f, 1.0f, 0.70f);
  colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.55f);
  colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.55f);

  // Docking
  colors[ImGuiCol_DockingPreview] =
      ImVec4(c.accent.x, c.accent.y, c.accent.z, 0.25f);
  colors[ImGuiCol_DockingEmptyBg] = ga(c.bg - 0.02f, 1.0f);
}

static void ApplyDarkGray() {
  // Accent: #dc4141 (engine red)
  ApplyColors({
      .accent = {0.863f, 0.255f, 0.255f, 1.0f},
      .accent_hover = {0.922f, 0.380f, 0.380f, 1.0f},
      .accent_active = {0.745f, 0.200f, 0.200f, 1.0f},
      .bg = 0.11f,
      .child = 0.11f,
      .popup = 0.09f,
      .frame = 0.06f,
      .frame_hover = 0.09f,
      .frame_active = 0.11f,
      .title = 0.07f,
      .menubar = 0.09f,
      .scrollbar_bg = 0.08f,
      .scrollbar_grab = 0.20f,
      .scrollbar_grab_hover = 0.26f,
      .btn = 0.16f,
      .btn_hover = 0.20f,
      .btn_active = 0.12f,
      .header = 0.15f,
      .header_hover = 0.19f,
      .border = 0.20f,
      .tab = 0.08f,
      .tab_hover = 0.15f,
      .tab_selected = 0.13f,
      .tab_dimmed = 0.07f,
      .table_header = 0.09f,
      .table_row = 0.11f,
      .table_row_alt = 0.12f,
  });
}

static void ApplyOLED() {
  // Accent: #dc4141 (engine red)
  ApplyColors({
      .accent = {0.863f, 0.255f, 0.255f, 1.0f},
      .accent_hover = {0.922f, 0.380f, 0.380f, 1.0f},
      .accent_active = {0.745f, 0.200f, 0.200f, 1.0f},
      .bg = 0.02f,
      .child = 0.02f,
      .popup = 0.03f,
      .frame = 0.06f,
      .frame_hover = 0.09f,
      .frame_active = 0.11f,
      .title = 0.02f,
      .menubar = 0.03f,
      .scrollbar_bg = 0.02f,
      .scrollbar_grab = 0.14f,
      .scrollbar_grab_hover = 0.20f,
      .btn = 0.10f,
      .btn_hover = 0.14f,
      .btn_active = 0.06f,
      .header = 0.08f,
      .header_hover = 0.11f,
      .border = 0.12f,
      .tab = 0.03f,
      .tab_hover = 0.08f,
      .tab_selected = 0.06f,
      .tab_dimmed = 0.02f,
      .table_header = 0.03f,
      .table_row = 0.02f,
      .table_row_alt = 0.04f,
  });
}

void ApplyTheme(Theme theme) {
  ApplySharedStyle();
  switch (theme) {
    case Theme::DarkGray:
      ApplyDarkGray();
      break;
    case Theme::OLED:
      ApplyOLED();
      break;
    default:
      ApplyDarkGray();
      break;
  }
  current_theme_ = theme;
}

}  // namespace Moonlight
}  // namespace ImGui