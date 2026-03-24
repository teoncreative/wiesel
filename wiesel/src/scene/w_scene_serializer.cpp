//
//   Copyright 2025 Metehan Gezer
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

#include "scene/w_scene_serializer.hpp"

#include "asset/w_asset_manager.hpp"
#include "scene/w_component_serializer.hpp"
#include "util/w_logger.hpp"
#include "w_engine.hpp"

namespace Wiesel {

SceneSerializer::SceneSerializer(std::shared_ptr<Scene> scene)
    : scene_(std::move(scene)) {}

// --- Entity serialization ---

nlohmann::json SceneSerializer::SerializeEntity(Entity entity) const {
  nlohmann::json j;

  j["uuid"] = entity.GetUUID().ToString();
  j["name"] = entity.GetName();

  // Game tags
  auto& tag_comp = entity.GetComponent<TagComponent>();
  if (!tag_comp.tags.empty()) {
    j["tags"] = tag_comp.tags;
  }

  // Parent reference
  auto parent = entity.GetParent();
  if (parent) {
    j["parent"] = parent.GetUUID().ToString();
  }

  // All components via registry
  ComponentSerializerRegistry::SerializeAll(entity, j);

  return j;
}

void SceneSerializer::DeserializeEntity(const nlohmann::json& entity_json) {
  std::string uuid_str = entity_json.value("uuid", "");
  std::string name = entity_json.value("name", "Entity");

  UUID uuid = UUID::FromString(uuid_str);
  Entity entity = scene_->CreateEntityWithUUID(uuid, name);

  // Game tags
  if (entity_json.contains("tags") && entity_json["tags"].is_array()) {
    auto& tag_comp = entity.GetComponent<TagComponent>();
    for (auto& t : entity_json["tags"]) {
      tag_comp.AddTag(t.get<std::string>());
    }
  }

  // All components via registry
  ComponentSerializerRegistry::DeserializeAll(entity, entity_json,
                                              scene_.get());
}

// --- Full scene serialization ---

std::string SceneSerializer::SerializeToString() const {
  nlohmann::json root;

  // Serialize entities in hierarchy order
  nlohmann::json entities = nlohmann::json::array();
  for (auto entity_id : scene_->GetSceneHierarchy()) {
    Entity entity{entity_id, scene_.get()};

    // Skip children - they're serialized recursively from their parent
    if (entity.GetParent()) {
      continue;
    }

    entities.push_back(SerializeEntity(entity));

    // Also serialize children recursively
    if (entity.child_handles()) {
      std::function<void(const std::vector<entt::entity>&)> serialize_children;
      serialize_children = [&](const std::vector<entt::entity>& children) {
        for (auto child_id : children) {
          Entity child{child_id, scene_.get()};
          entities.push_back(SerializeEntity(child));
          if (child.child_handles() && !child.child_handles()->empty()) {
            serialize_children(*child.child_handles());
          }
        }
      };
      if (!entity.child_handles()->empty()) {
        serialize_children(*entity.child_handles());
      }
    }
  }

  root["entities"] = entities;

  // Scene-level properties
  if (scene_->GetSkyboxAsset().IsValid()) {
    root["skybox"] = scene_->GetSkyboxAsset().ToString();
  }
  if (scene_->GetKeepAssetsLoaded()) {
    root["keep_assets_loaded"] = true;
  }
  if (scene_->GetPreloadAssets()) {
    root["preload_assets"] = true;
  }

  return root.dump(2);
}

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
    std::string skybox_str = root["skybox"].get<std::string>();
    if (!skybox_str.empty()) {
      auto skybox_handle = AssetHandle::FromString(skybox_str);
      scene_->RequestAsset(skybox_handle);
      scene_->SetSkyboxAsset(skybox_handle);
    }
  }
  scene_->SetKeepAssetsLoaded(root.value("keep_assets_loaded", false));
  scene_->SetPreloadAssets(root.value("preload_assets", false));

  // First pass: create all entities
  for (const auto& entity_json : root["entities"]) {
    DeserializeEntity(entity_json);
  }

  // Second pass: restore parent-child relationships
  for (const auto& entity_json : root["entities"]) {
    if (entity_json.contains("parent") && entity_json["parent"].is_string()) {
      std::string parent_uuid_str = entity_json["parent"].get<std::string>();
      std::string child_uuid_str = entity_json["uuid"].get<std::string>();

      UUID parent_uuid = UUID::FromString(parent_uuid_str);
      UUID child_uuid = UUID::FromString(child_uuid_str);

      entt::entity parent_entity = entt::null;
      entt::entity child_entity = entt::null;

      auto view = scene_->GetAllEntitiesWith<IdComponent>();
      for (auto e : view) {
        auto& id = scene_->GetComponent<IdComponent>(e);
        if (id.Id == parent_uuid) {
          parent_entity = e;
        }
        if (id.Id == child_uuid) {
          child_entity = e;
        }
      }

      if (parent_entity != entt::null && child_entity != entt::null) {
        scene_->LinkEntities(parent_entity, child_entity);
      }
    }
  }

  LOG_INFO("Scene deserialized: {} entities", root["entities"].size());
  return true;
}

}  // namespace Wiesel
