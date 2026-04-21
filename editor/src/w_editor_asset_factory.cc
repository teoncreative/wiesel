//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "w_editor_asset_factory.h"

#include <imgui.h>
#include <map>
#include <utility>

namespace wiesel::editor {

namespace {
std::vector<AssetFactoryDesc>& Registry() {
  static std::vector<AssetFactoryDesc> v;
  return v;
}
}  // namespace

void AssetFactoryRegistry::Register(AssetFactoryDesc desc) {
  Registry().push_back(std::move(desc));
}

const std::vector<AssetFactoryDesc>& AssetFactoryRegistry::All() {
  return Registry();
}

void RenderAssetFactoryMenu() {
  const auto& factories = AssetFactoryRegistry::All();

  // Top-level (no group) entries first, in registration order.
  for (const auto& f : factories) {
    if (!f.group.empty()) {
      continue;
    }
    if (!f.icon.empty()) {
      ImGui::SetNextMenuItemIcon(f.icon.c_str());
    }
    if (ImGui::MenuItem(f.label.c_str()) && f.action) {
      f.action();
    }
  }

  // Group entries by `group`, preserving registration order both for groups
  // and items within them.
  std::vector<std::string> group_order;
  std::map<std::string, std::vector<const AssetFactoryDesc*>> groups;
  for (const auto& f : factories) {
    if (f.group.empty()) {
      continue;
    }
    auto it = groups.find(f.group);
    if (it == groups.end()) {
      group_order.push_back(f.group);
      groups[f.group] = {&f};
    } else {
      it->second.push_back(&f);
    }
  }
  for (const std::string& group : group_order) {
    if (ImGui::BeginMenu(group.c_str())) {
      for (const AssetFactoryDesc* f : groups[group]) {
        if (!f->icon.empty()) {
          ImGui::SetNextMenuItemIcon(f->icon.c_str());
        }
        if (ImGui::MenuItem(f->label.c_str()) && f->action) {
          f->action();
        }
      }
      ImGui::EndMenu();
    }
  }
}

}  // namespace wiesel::editor
