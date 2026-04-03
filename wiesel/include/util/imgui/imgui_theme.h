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
  OLED,
  Count,
};

const char* GetThemeName(Theme theme);

// Load SourceSansProRegular and sets it as a default font.
// You may want to call ImGui::GetIO().Fonts->Clear() before this
void LoadFont(float size = 19.0f);

// Apply a theme by enum
void ApplyTheme(Theme theme = Theme::DarkGray);

// Get the currently active theme
Theme GetCurrentTheme();

}  // namespace Moonlight
}  // namespace ImGui