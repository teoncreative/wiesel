
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "systems/w_replication_sync_system.h"

#include "networking/w_network.h"
#include "networking/w_network_component_serializer.h"
#include "networking/w_replication_types.h"
#include "scene/w_entity.h"
#include "scene/w_scene.h"
#include "w_engine.h"

namespace wiesel {

void ReplicationSyncSystem::Update(Scene& scene, float delta_time) {
  auto& network = Engine::network();
  if (network.role() == NetworkRole::kNone) {
    return;
  }

  auto& registry = scene.GetRegistry();
  auto view = registry.view<NetworkIdentityComponent>();

  for (auto ent : view) {
    auto& net_id = view.get<NetworkIdentityComponent>(ent);
    if (net_id.pending_spawn || net_id.net_id == 0 || net_id.is_remote) {
      continue;
    }

    Entity entity(ent, &scene);
    NetworkComponentSerializerRegistry::UpdateDirtyFlags(entity, net_id);
  }
}

}  // namespace wiesel
