#pragma once

#include <entt/entt.hpp>
#include <variant>
#include "scene/w_components.hpp"
#include "w_pch.hpp"

namespace Wiesel {

struct AgentMemory : public IComponent {
  void Set(const std::string& key, float value);
  void Set(const std::string& key, int value);
  void Set(const std::string& key, bool value);
  void Set(const std::string& key, const glm::vec3& value);
  void Set(const std::string& key, entt::entity value);
  void Set(const std::string& key, const std::string& value);

  float GetFloat(const std::string& key, float fallback = 0.0f) const;
  int GetInt(const std::string& key, int fallback = 0) const;
  bool GetBool(const std::string& key, bool fallback = false) const;
  glm::vec3 GetVec3(const std::string& key,
                     glm::vec3 fallback = glm::vec3(0.0f)) const;
  entt::entity GetEntity(const std::string& key) const;
  std::string GetString(const std::string& key,
                        const std::string& fallback = "") const;
  bool Has(const std::string& key) const;

 private:
  using Value =
      std::variant<float, int, bool, glm::vec3, entt::entity, std::string>;
  std::unordered_map<std::string, Value> values_;
};

}  // namespace Wiesel