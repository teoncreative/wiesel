
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
#include <variant>

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

using SyncVarValue = std::variant<int, float, bool, std::string, glm::vec3>;

enum class SyncVarType : uint8_t {
  kInt = 0,
  kFloat = 1,
  kBool = 2,
  kString = 3,
  kVec3 = 4,
};

struct NetworkIdentityComponent : public IComponent {
  uint32_t net_id = 0;
  NetworkAuthority authority = NetworkAuthority::kServer;
  uint64_t owner_session_id = 0;
  std::bitset<64> dirty_components{};
  bool pending_spawn = true;

  // Snapshot interpolation (client-side, for remote entities)
  TransformSnapshot from_snapshot;
  TransformSnapshot to_snapshot;
  float interp_elapsed = 0.0f;
  bool has_snapshots = false;

  // Synced variables
  std::unordered_map<std::string, SyncVarValue> sync_vars;
  std::unordered_map<std::string, SyncVarValue> sync_vars_last_sent;
};

}  // namespace wiesel
