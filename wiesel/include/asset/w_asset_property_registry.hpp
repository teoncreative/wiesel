
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include <functional>
#include <memory>
#include <unordered_map>
#include <nlohmann/json_fwd.hpp>
#include "asset/w_asset_handle.hpp"

namespace Wiesel {

struct AssetPropertyDesc {
  // Create default properties for this asset type
  std::function<std::shared_ptr<void>()> Create;
  // Serialize properties to JSON
  std::function<nlohmann::json(const void*)> Serialize;
  // Deserialize properties from JSON
  std::function<std::shared_ptr<void>(const nlohmann::json&)> Deserialize;
  // Render ImGui editor for properties. Returns true if changed.
  std::function<bool(void*)> RenderImGui;
};

class AssetPropertyRegistry {
 public:
  static void Register(AssetType type, AssetPropertyDesc desc);
  static const AssetPropertyDesc* Get(AssetType type);
  static bool HasProperties(AssetType type);

 private:
  static std::unordered_map<AssetType, AssetPropertyDesc>& Registry();
};

void InitializeAssetProperties();

}  // namespace Wiesel