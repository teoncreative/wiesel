//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "scene/w_scene_serializer.h"

#include "scene/w_component_serializer.h"
#include "scene/w_entity.h"
#include "scene/w_entity_serializer.h"
#include "util/w_logger.h"

namespace Wiesel {

static constexpr int kCurrentSceneVersion = 2;

SceneSerializer::SceneSerializer(std::shared_ptr<Scene> scene)
    : scene_(std::move(scene)) {}

// --- Serialize (V2: nested format via EntitySerializer) ---

std::string SceneSerializer::SerializeToString() const {
  nlohmann::json root;
  root["version"] = kCurrentSceneVersion;

  nlohmann::json entities = nlohmann::json::array();
  for (auto entity_id : scene_->GetSceneHierarchy()) {
    Entity entity{entity_id, scene_.get()};

    // Only serialize root entities - children are nested inside
    if (entity.GetParent()) {
      continue;
    }

    entities.push_back(EntitySerializer::Serialize(entity));
  }
  root["entities"] = entities;

  // Scene-level properties
  if (scene_->GetSkyboxAsset().IsValid()) {
    root["skybox"] = scene_->GetSkyboxAsset().ToString();
  }
  if (scene_->GetCursorSetAsset().IsValid()) {
    root["cursor_set"] = scene_->GetCursorSetAsset().ToString();
  }
  if (scene_->GetKeepAssetsLoaded()) {
    root["keep_assets_loaded"] = true;
  }
  if (scene_->GetPreloadAssets()) {
    root["preload_assets"] = true;
  }

  return root.dump(2);
}

// --- V1 deserialization (flat format with parent references) ---

static bool DeserializeV1(std::shared_ptr<Scene> scene,
                          const nlohmann::json& root) {
  // First pass: create all entities
  std::unordered_map<UUID, entt::entity> uuid_map;
  for (const nlohmann::json& ej : root["entities"]) {
    std::string uuid_str = ej.value("uuid", "");
    std::string name = ej.value("name", "Entity");
    UUID uuid = UUID::FromString(uuid_str);
    Entity entity = scene->CreateEntityWithUUID(uuid, name);
    uuid_map[uuid] = entity.handle();
  }

  // Second pass: deserialize components
  for (const nlohmann::json& ej : root["entities"]) {
    UUID uuid = UUID::FromString(ej.value("uuid", ""));
    auto it = uuid_map.find(uuid);
    if (it == uuid_map.end()) {
      continue;
    }
    Entity entity{it->second, scene.get()};
    if (ej.contains("tags") && ej["tags"].is_array()) {
      auto& tc = entity.GetComponent<TagComponent>();
      for (const auto& t : ej["tags"]) {
        tc.AddTag(t.get<std::string>());
      }
    }
    ComponentSerializerRegistry::DeserializeAll(entity, ej, scene.get());
  }

  // Third pass: link hierarchy
  for (const nlohmann::json& ej : root["entities"]) {
    if (ej.contains("parent") && ej["parent"].is_string()) {
      UUID parent_uuid = UUID::FromString(ej["parent"].get<std::string>());
      UUID child_uuid = UUID::FromString(ej["uuid"].get<std::string>());
      auto pi = uuid_map.find(parent_uuid);
      auto ci = uuid_map.find(child_uuid);
      if (pi != uuid_map.end() && ci != uuid_map.end()) {
        scene->LinkEntities(pi->second, ci->second, false);
      }
    }
  }
  return true;
}

// --- V2 deserialization (nested format via EntitySerializer) ---

static bool DeserializeV2(std::shared_ptr<Scene> scene,
                          const nlohmann::json& root) {
  for (const nlohmann::json& entity_tree : root["entities"]) {
    EntitySerializer::Deserialize(scene, entity_tree);
  }
  return true;
}

// --- Entry point ---

bool SceneSerializer::DeserializeFromString(const std::string& json_str) {
  nlohmann::json root;
  try {
    root = nlohmann::json::parse(json_str);
  } catch (const nlohmann::json::parse_error& e) {
    LOG_ERROR("Failed to parse scene JSON: {}", e.what());
    return false;
  }

  if (!root.contains("entities") || !root["entities"].is_array()) {
    LOG_ERROR("Scene file missing 'entities' array");
    return false;
  }

  // Scene-level properties
  if (root.contains("skybox") && root["skybox"].is_string()) {
    std::string s = root["skybox"].get<std::string>();
    if (!s.empty()) {
      auto h = AssetHandle::FromString(s);
      scene_->RequestAsset(h);
      scene_->SetSkyboxAsset(h);
    }
  }
  if (root.contains("cursor_set") && root["cursor_set"].is_string()) {
    std::string s = root["cursor_set"].get<std::string>();
    if (!s.empty()) {
      auto h = AssetHandle::FromString(s);
      scene_->RequestAsset(h);
      scene_->SetCursorSetAsset(h);
    }
  }
  scene_->SetKeepAssetsLoaded(root.value("keep_assets_loaded", false));
  scene_->SetPreloadAssets(root.value("preload_assets", false));

  int version = root.value("version", 1);
  bool ok = (version >= 2) ? DeserializeV2(scene_, root)
                           : DeserializeV1(scene_, root);

  if (ok) {
    LOG_INFO("Scene deserialized (v{}): {} entities", version,
             root["entities"].size());
  }
  return ok;
}

}  // namespace Wiesel
