//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "scene/w_entity_serializer.h"

#include "scene/w_component_serializer.h"
#include "scene/w_entity.h"
#include "util/w_logger.h"

namespace Wiesel::EntitySerializer {

// --- Serialize ---

nlohmann::json Serialize(Entity entity) {
  nlohmann::json j;

  j["uuid"] = entity.GetUUID().ToString();
  j["name"] = entity.GetName();

  // Game tags
  auto& tag_comp = entity.GetComponent<TagComponent>();
  if (!tag_comp.tags.empty()) {
    j["tags"] = tag_comp.tags;
  }

  // All components via registry
  ComponentSerializerRegistry::SerializeAll(entity, j);

  // Children (nested)
  if (entity.child_handles() && !entity.child_handles()->empty()) {
    nlohmann::json children = nlohmann::json::array();
    for (auto child_id : *entity.child_handles()) {
      Entity child{child_id, entity.GetScene()};
      children.push_back(Serialize(child));
    }
    j["children"] = children;
  }

  return j;
}

// --- Deserialize (two-phase) ---

struct EntityEntry {
  const nlohmann::json* json;
  int parent_index;
  entt::entity handle = entt::null;
};

static void Collect(const nlohmann::json& j, int parent_idx,
                    std::vector<EntityEntry>& entries) {
  int my_idx = static_cast<int>(entries.size());
  entries.push_back({&j, parent_idx, entt::null});
  if (j.contains("children") && j["children"].is_array()) {
    for (const auto& child : j["children"]) {
      Collect(child, my_idx, entries);
    }
  }
}

Entity Deserialize(std::shared_ptr<Scene> scene, const nlohmann::json& json) {
  std::vector<EntityEntry> entries;
  Collect(json, -1, entries);

  if (entries.empty()) {
    return Entity{entt::null, scene.get()};
  }

  // Phase 1: create all entities with UUIDs and names
  for (auto& entry : entries) {
    std::string uuid_str = entry.json->value("uuid", "");
    std::string name = entry.json->value("name", "Entity");
    UUID uuid =
        uuid_str.empty() ? UUID::GenerateV4() : UUID::FromString(uuid_str);
    Entity entity = scene->CreateEntityWithUUID(uuid, name);
    entry.handle = entity.handle();
  }

  // Phase 2: link parent-child relationships
  for (const auto& entry : entries) {
    if (entry.parent_index >= 0) {
      scene->LinkEntities(entries[entry.parent_index].handle, entry.handle,
                          false);
    }
  }

  // Phase 3: deserialize all components (all entities + hierarchy exist)
  for (const auto& entry : entries) {
    Entity entity{entry.handle, scene.get()};

    if (entry.json->contains("tags") && (*entry.json)["tags"].is_array()) {
      auto& tag_comp = entity.GetComponent<TagComponent>();
      for (const auto& t : (*entry.json)["tags"]) {
        tag_comp.AddTag(t.get<std::string>());
      }
    }

    ComponentSerializerRegistry::DeserializeAll(entity, *entry.json,
                                                scene.get());
  }

  return Entity{entries[0].handle, scene.get()};
}

}  // namespace Wiesel::EntitySerializer
