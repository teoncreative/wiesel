//
// Created by Metehan Gezer on 05.03.2026.
//

#include "scene/w_prefab.hpp"

#include "scene/w_component_serializer.hpp"
#include "util/w_logger.hpp"
#include "w_engine.hpp"

namespace Wiesel {

static nlohmann::json SerializeSingleEntity(Entity entity) {
  nlohmann::json j;

  j["name"] = entity.GetName();

  // All components via registry
  ComponentSerializerRegistry::SerializeAll(entity, j);

  // Children
  if (entity.child_handles() && !entity.child_handles()->empty()) {
    nlohmann::json children = nlohmann::json::array();
    for (auto child_id : *entity.child_handles()) {
      Entity child{child_id, entity.GetScene()};
      children.push_back(SerializeSingleEntity(child));
    }
    j["children"] = children;
  }

  return j;
}

static Entity DeserializeSingleEntity(std::shared_ptr<Scene> scene,
                                      const nlohmann::json& j,
                                      entt::entity parent = entt::null) {
  std::string name = j.value("name", "Entity");
  Entity entity = scene->CreateEntity(name);

  if (parent != entt::null) {
    scene->LinkEntities(parent, entity);
  }

  // All components via registry (Scene* = nullptr for prefab)
  ComponentSerializerRegistry::DeserializeAll(entity, j, nullptr);

  // Recurse into children
  if (j.contains("children") && j["children"].is_array()) {
    for (const auto& child_json : j["children"]) {
      DeserializeSingleEntity(scene, child_json, entity.handle());
    }
  }

  return entity;
}

// --- Public API ---

nlohmann::json Prefab::SerializeEntityTree(Entity entity) {
  nlohmann::json root;
  root["prefab_version"] = 1;
  root["root"] = SerializeSingleEntity(entity);
  return root;
}

bool Prefab::SaveToFile(Entity entity, const std::filesystem::path& path) {
  nlohmann::json j = SerializeEntityTree(entity);

  std::filesystem::create_directories(path.parent_path());

  std::ofstream file(path);
  if (!file.is_open()) {
    LOG_ERROR("Failed to save prefab to: {}", path.string());
    return false;
  }

  file << j.dump(2);
  LOG_INFO("Prefab saved to: {}", path.string());
  return true;
}

Entity Prefab::DeserializeEntityTree(std::shared_ptr<Scene> scene,
                                     const nlohmann::json& json) {
  if (!json.contains("root")) {
    LOG_ERROR("Prefab JSON missing 'root'");
    return Entity{entt::null, scene.get()};
  }

  return DeserializeSingleEntity(scene, json["root"]);
}

Entity Prefab::InstantiateFromFile(std::shared_ptr<Scene> scene,
                                   const std::filesystem::path& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    LOG_ERROR("Failed to open prefab file: {}", path.string());
    return Entity{entt::null, scene.get()};
  }

  nlohmann::json j;
  try {
    file >> j;
  } catch (const nlohmann::json::parse_error& e) {
    LOG_ERROR("Failed to parse prefab: {}", e.what());
    return Entity{entt::null, scene.get()};
  }

  return DeserializeEntityTree(scene, j);
}

}  // namespace Wiesel
