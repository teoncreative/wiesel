
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

#include <bitset>
#include <nlohmann/json.hpp>

#include "scene/w_components.h"

namespace wiesel {

enum class NetworkAuthority : uint8_t {
  kNone = 0,
  kServer = 1,
  kClient = 2,
};

using NetworkComponentTypeId = uint16_t;

struct TransformSnapshot {
  glm::vec3 position{0.0f};
  glm::vec3 rotation{0.0f};
  glm::vec3 scale{1.0f};
};

struct NetworkIdentityComponent : public IComponent {
  uint32_t net_id = 0;
  NetworkAuthority authority = NetworkAuthority::kServer;
  uint64_t owner_session_id = 0;
  std::bitset<64> dirty_components{};
  bool pending_spawn = true;
  bool is_remote = false;

  // Snapshot interpolation (for remote entities)
  TransformSnapshot from_snapshot;
  TransformSnapshot to_snapshot;
  float interp_elapsed = 0.0f;
  bool has_snapshots = false;

  // Synced variables (stored as JSON for type-agnostic serialization)
  std::unordered_map<std::string, nlohmann::json> sync_vars;
  std::unordered_map<std::string, nlohmann::json> sync_vars_last_sent;
};

}  // namespace wiesel
