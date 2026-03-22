//
// Created by Metehan Gezer on 05.03.2026.
//

#pragma once

#include <nlohmann/json.hpp>

#include "scene/w_scene.hpp"
#include "w_pch.hpp"

namespace Wiesel {

class SceneSerializer {
 public:
  explicit SceneSerializer(std::shared_ptr<Scene> scene);

  bool Serialize(const std::filesystem::path& path) const;
  bool Deserialize(const std::filesystem::path& path);

  std::string SerializeToString() const;
  bool DeserializeFromString(const std::string& json_str);

 private:
  nlohmann::json SerializeEntity(Entity entity) const;
  void DeserializeEntity(const nlohmann::json& entity_json);

  std::shared_ptr<Scene> scene_;
};

}  // namespace Wiesel
