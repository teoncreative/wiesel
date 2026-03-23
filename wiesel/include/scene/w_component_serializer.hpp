//
// Component serializer registry - single source of truth for component
// serialization used by both scene files and prefabs.
//

#pragma once

#include <functional>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace Wiesel {

class Entity;
class Scene;

// --- Shared serialization helpers ---

namespace SerializeUtil {

nlohmann::json Vec2(const glm::vec2& v);
nlohmann::json Vec3(const glm::vec3& v);
nlohmann::json Vec4(const glm::vec4& v);

glm::vec2 Vec2(const nlohmann::json& v, glm::vec2 fallback = {0, 0});
glm::vec3 Vec3(const nlohmann::json& v, glm::vec3 fallback = {0, 0, 0});
glm::vec4 Vec4(const nlohmann::json& v, glm::vec4 fallback = {0, 0, 0, 0});

}  // namespace SerializeUtil

// --- Registry ---

struct ComponentSerializerDesc {
  std::string json_key;
  std::function<bool(Entity&)> Has;
  std::function<nlohmann::json(Entity&)> Serialize;
  // Scene* is non-null for scene loading (RequestAsset), null for prefab.
  std::function<void(Entity&, const nlohmann::json&, Scene*)> Deserialize;
};

class ComponentSerializerRegistry {
 public:
  static void Register(ComponentSerializerDesc desc);
  static void SerializeAll(Entity& entity, nlohmann::json& out);
  static void DeserializeAll(Entity& entity, const nlohmann::json& in,
                             Scene* scene);

 private:
  static std::vector<ComponentSerializerDesc>& Registry();
};

// Call once from Engine::InitEngine() to register all component serializers.
void InitializeComponentSerializers();

}  // namespace Wiesel
