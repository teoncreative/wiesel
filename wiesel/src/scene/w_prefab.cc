//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "scene/w_prefab.h"

#include "asset/w_asset_manager.h"
#include "scene/w_entity_serializer.h"
#include "util/w_logger.h"
#include "util/w_vfs.h"
#include "w_engine.h"

namespace Wiesel {

nlohmann::json Prefab::SerializeEntityTree(Entity entity) {
  nlohmann::json root;
  root["prefab_version"] = 1;
  root["root"] = EntitySerializer::Serialize(entity);
  return root;
}

Entity Prefab::DeserializeEntityTree(std::shared_ptr<Scene> scene,
                                     const nlohmann::json& json) {
  if (json.contains("root")) {
    return EntitySerializer::Deserialize(scene, json["root"]);
  }
  // If no wrapper, treat the json itself as the entity tree
  return EntitySerializer::Deserialize(scene, json);
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
