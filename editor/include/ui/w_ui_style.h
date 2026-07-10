//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include <imgui.h>

// Editor UI style constants. Every hardcoded spacing / color that shows up in
// the editor should live here so theme tweaks propagate through one edit.
// Prefer ImGui::GetStyle() fields (WindowPadding, ItemSpacing, FramePadding,
// IndentSpacing) when one exists - only add constants here for values that
// don't map to an ImGui style field.

namespace wiesel::editor::ui::style {

// --- Popups / section headers ------------------------------------------------

// Font scale applied to the title row of editor popups and dialogs
// (Welcome, About, Project Settings, etc.).
constexpr float kHeaderFontScale = 1.25f;

// Vertical padding placed above AND below each layout::Separator inside editor
// popups and the startup dialog. Combined with ImGuiStyle::ItemSpacing.y this
// lands at ~8 px on each side, which is what makes content read as visually
// centered between section dividers.
constexpr float kSeparatorPadY = 4.0f;

// --- Backgrounds -------------------------------------------------------------

// Darker "drawer" bg used for section fills, component drawers, and the body
// side of settings-style popups. Slightly darker than ImGuiCol_WindowBg so
// these regions read as recessed.
constexpr ImU32 kDrawerBg = IM_COL32(0x1a, 0x18, 0x17, 0xff);

// Darker-still background used behind the command palette / name prompt
// legend strip at the bottom of the popup.
constexpr ImU32 kLegendBg = IM_COL32(0x18, 0x16, 0x15, 0xff);

// Badge background - used by keyboard shortcut pills in command palette
// rows and name prompt hints. Slightly lighter than kDrawerBg so badges
// stand out on the darker strips.
constexpr ImU32 kBadgeBg = IM_COL32(0x23, 0x21, 0x20, 0xff);

// --- Command palette / name prompt row metrics ------------------------------

// Horizontal margin between the popup's outer edge and each row. Rows get a
// rounded highlight, so we leave this gap rather than running flush.
constexpr float kRowOuterPadX = 10.0f;

// Inner horizontal inset for a row's content (icon, label, right-aligned
// badges). Bigger than kRowOuterPadX so there's breathing room inside the
// highlight rect.
constexpr float kRowInnerPadX = 12.0f;

// Inner vertical padding added on top of ImGuiStyle::FramePadding.y so rows
// have a more comfortable click target than a bare button.
constexpr float kRowInnerPadY = 8.0f;

// Badge font scale relative to the host row's font size. 0.75 reads as a
// secondary/meta element next to the full-size label.
constexpr float kBadgeScale = 0.75f;

// Inner horizontal padding added on each side of the badge label.
constexpr float kBadgePadX = 6.0f;

// Corner radius used for badge rects.
constexpr float kBadgeRounding = 3.0f;

// --- Console log colors -----------------------------------------------------

constexpr ImU32 kLogWarnColor = IM_COL32(0xff, 0xa5, 0x2a, 0xff);
constexpr ImU32 kLogErrorColor = IM_COL32(0xe6, 0x3f, 0x3f, 0xff);

}  // namespace wiesel::editor::ui::style
