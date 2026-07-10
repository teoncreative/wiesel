//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

//
// Created by Metehan Gezer on 05.03.2026.
//

#pragma once

#include <nlohmann/json.hpp>

#include "asset/w_asset_handle.h"
#include "scene/w_scene.h"
#include "w_pch.h"

namespace wiesel {

class Prefab {
 public:
  Prefab() = default;

  // Create a prefab from an existing entity (and its children)
  static bool SaveToFile(Entity entity, const std::filesystem::path& path);

  // Instantiate a prefab into a scene, returns the root entity
  static Entity Instantiate(Scene& target_scene, AssetHandle handle);

  // Serialize an entity subtree to JSON (used internally and by the editor)
  static nlohmann::json SerializeEntityTree(Entity entity);

  // Deserialize an entity subtree from JSON into a scene
  // Generates new UUIDs so each instance is unique
  static Entity DeserializeEntityTree(Scene& target_scene,
                                      const nlohmann::json& json);
};

}  // namespace wiesel