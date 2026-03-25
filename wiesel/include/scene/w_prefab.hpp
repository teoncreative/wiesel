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

#include "scene/w_scene.hpp"
#include "w_pch.hpp"

namespace Wiesel {

class Prefab {
 public:
  Prefab() = default;

  // Create a prefab from an existing entity (and its children)
  static bool SaveToFile(Entity entity, const std::filesystem::path& path);

  // Instantiate a prefab into a scene, returns the root entity
  static Entity InstantiateFromFile(std::shared_ptr<Scene> scene,
                                    const std::filesystem::path& path);

  // Serialize an entity subtree to JSON (used internally and by the editor)
  static nlohmann::json SerializeEntityTree(Entity entity);

  // Deserialize an entity subtree from JSON into a scene
  // Generates new UUIDs so each instance is unique
  static Entity DeserializeEntityTree(std::shared_ptr<Scene> scene,
                                      const nlohmann::json& json);
};

}  // namespace Wiesel