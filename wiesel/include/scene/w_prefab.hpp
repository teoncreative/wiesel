//
// Created by Claude on 05.03.2026.
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
  static Entity InstantiateFromFile(Ref<Scene> scene,
                                    const std::filesystem::path& path);

  // Serialize an entity subtree to JSON (used internally and by the editor)
  static nlohmann::json SerializeEntityTree(Entity entity);

  // Deserialize an entity subtree from JSON into a scene
  // Generates new UUIDs so each instance is unique
  static Entity DeserializeEntityTree(Ref<Scene> scene,
                                      const nlohmann::json& json);
};

}  // namespace Wiesel