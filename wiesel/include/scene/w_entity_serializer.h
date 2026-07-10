//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include <nlohmann/json.hpp>

#include "scene/w_scene.h"

namespace wiesel {

// Shared entity tree serialization used by scenes, prefabs, and undo/redo.
// Single source of truth for serializing/deserializing entity hierarchies.
namespace entity_serializer {

// Serialize an entity and all its children into a nested JSON tree.
nlohmann::json Serialize(Entity entity);

// Deserialize a nested JSON tree into entities.
// Returns the root entity.
Entity Deserialize(Scene& scene, const nlohmann::json& json);

}  // namespace EntitySerializer

}  // namespace wiesel
