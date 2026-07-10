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

#include <functional>
#include <string>
#include <vector>

namespace wiesel::editor {

// Describes one entry in the asset browser's "Create" submenu. The action
// can do anything the editor wants - open a wizard popup, create a file
// directly, etc. The registry exists purely to consolidate the previous
// hardcoded list and let new asset types register themselves with one call.
struct AssetFactoryDesc {
  std::string label;   // e.g. "Cursor Set"
  std::string group;   // optional submenu name; "" = top level
  std::string icon;    // ICON_LC_* glyph; may be empty
  std::function<void()> action;
};

class AssetFactoryRegistry {
 public:
  static void Register(AssetFactoryDesc desc);
  static const std::vector<AssetFactoryDesc>& All();
};

// Render the contents of the asset browser's "Create" submenu by iterating
// the registry. Top-level factories appear directly; grouped ones nest in
// BeginMenu submenus with the group name as the label.
void RenderAssetFactoryMenu();

}  // namespace wiesel::editor
