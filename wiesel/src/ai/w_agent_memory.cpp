#include "ai/w_agent_memory.hpp"

namespace Wiesel {

void AgentMemory::Set(const std::string& key, float value) {
  values_[key] = value;
}

void AgentMemory::Set(const std::string& key, int value) {
  values_[key] = value;
}

void AgentMemory::Set(const std::string& key, bool value) {
  values_[key] = value;
}

void AgentMemory::Set(const std::string& key, const glm::vec3& value) {
  values_[key] = value;
}

void AgentMemory::Set(const std::string& key, entt::entity value) {
  values_[key] = value;
}

void AgentMemory::Set(const std::string& key, const std::string& value) {
  values_[key] = value;
}

float AgentMemory::GetFloat(const std::string& key, float fallback) const {
  auto it = values_.find(key);
  if (it == values_.end()) return fallback;
  if (auto* v = std::get_if<float>(&it->second)) return *v;
  return fallback;
}

int AgentMemory::GetInt(const std::string& key, int fallback) const {
  auto it = values_.find(key);
  if (it == values_.end()) return fallback;
  if (auto* v = std::get_if<int>(&it->second)) return *v;
  return fallback;
}

bool AgentMemory::GetBool(const std::string& key, bool fallback) const {
  auto it = values_.find(key);
  if (it == values_.end()) return fallback;
  if (auto* v = std::get_if<bool>(&it->second)) return *v;
  return fallback;
}

glm::vec3 AgentMemory::GetVec3(const std::string& key,
                                    glm::vec3 fallback) const {
  auto it = values_.find(key);
  if (it == values_.end()) return fallback;
  if (auto* v = std::get_if<glm::vec3>(&it->second)) return *v;
  return fallback;
}

entt::entity AgentMemory::GetEntity(const std::string& key) const {
  auto it = values_.find(key);
  if (it == values_.end()) return entt::null;
  if (auto* v = std::get_if<entt::entity>(&it->second)) return *v;
  return entt::null;
}

std::string AgentMemory::GetString(const std::string& key,
                                       const std::string& fallback) const {
  auto it = values_.find(key);
  if (it == values_.end()) return fallback;
  if (auto* v = std::get_if<std::string>(&it->second)) return *v;
  return fallback;
}

bool AgentMemory::Has(const std::string& key) const {
  return values_.find(key) != values_.end();
}

}  // namespace Wiesel