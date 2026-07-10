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

#include "w_icon_source.h"

struct ImFontLoader;

namespace wiesel::editor {

// Bridges IIconSource to ImGui's ImFontLoader system.
// Call SetSource() before registering the font with ImGui.
class IconFontLoader {
 public:
  // Set the icon source (must outlive the ImGui font atlas).
  static void SetSource(IIconSource* source);

  // Get the ImFontLoader struct for use in ImFontConfig::FontLoader.
  static const ImFontLoader* GetLoader();

  // Accessible by the static ImFontLoader callbacks
  static IIconSource* source_;
};

}  // namespace wiesel::editor
