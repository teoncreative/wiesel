//
// Created by Claude on 05.03.2026.
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

  static nlohmann::json SerializeVec3(const glm::vec3& v);
  static nlohmann::json SerializeVec4(const glm::vec4& v);
  static nlohmann::json SerializeVec2(const glm::vec2& v);
  static glm::vec3 DeserializeVec3(const nlohmann::json& v, glm::vec3 fallback = {0, 0, 0});
  static glm::vec4 DeserializeVec4(const nlohmann::json& v, glm::vec4 fallback = {0, 0, 0, 0});
  static glm::vec2 DeserializeVec2(const nlohmann::json& v, glm::vec2 fallback = {0, 0});

  std::shared_ptr<Scene> scene_;
};

}  // namespace Wiesel
