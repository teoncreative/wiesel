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

#include "scene/w_prefab.h"

#include "asset/w_asset_manager.h"
#include "scene/w_component_serializer.h"
#include "util/w_logger.h"
#include "w_engine.h"

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

Entity Prefab::Instantiate(std::shared_ptr<Scene> scene, AssetHandle handle) {
  if (!handle.IsValid()) {
    LOG_ERROR("Prefab::Instantiate: invalid asset handle");
    return Entity{entt::null, scene.get()};
  }

  const auto* meta = Engine::asset_manager().GetMetadata(handle);
  if (!meta) {
    LOG_ERROR("Prefab::Instantiate: asset not found: {}", handle.ToString());
    return Entity{entt::null, scene.get()};
  }

  VfsFile file = Engine::vfs()->Open(meta->virtual_source_path);
  if (!file) {
    LOG_ERROR("Failed to open prefab file: {}", meta->virtual_source_path);
    return Entity{entt::null, scene.get()};
  }

  nlohmann::json j;
  try {
    std::string content((std::istreambuf_iterator<char>(file.Stream())),
                        std::istreambuf_iterator<char>());
    j = nlohmann::json::parse(content);
  } catch (const nlohmann::json::parse_error& e) {
    LOG_ERROR("Failed to parse prefab: {}", e.what());
    return Entity{entt::null, scene.get()};
  }

  return DeserializeEntityTree(scene, j);
}

}  // namespace Wiesel
