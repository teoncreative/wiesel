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

namespace ImGui {
namespace Moonlight {

enum class Theme {
  DarkGray,
  Count,
};

static constexpr const char* kDefaultFontPath = "engine://fonts/Inter.ttf";
// Named instance index for FreeType variable font selection.
// Inter: 0=default, 1=Thin, 2=ExtraLight, 3=Light, 4=Regular, 5=Medium, ...
static constexpr int kDefaultFontInstance = 4;
static constexpr float kDefaultRegularFontSize = 13.0f;
static constexpr float kDefaultMonospacedFontSize = 13.0f;
static constexpr float kDefaultIconFontSize = 13.0f;
static constexpr float kDefaultFontOffsetY = 1.0f;

const char* GetThemeName(Theme theme);

void LoadFont();

// Apply a theme by enum
void ApplyTheme(Theme theme = Theme::DarkGray);

// Get the currently active theme
Theme GetCurrentTheme();

}  // namespace Moonlight
}  // namespace ImGui