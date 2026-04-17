
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

#include <unordered_set>

#include "networking/w_replication_packets.h"
#include "networking/w_replication_types.h"
#include "scene/w_entity.h"

namespace wiesel {

struct ReplicationCommand {
  enum Type { kSpawn, kDestroy, kState, kOwnership, kRpc, kSyncVar, kSceneLoad };
  Type type;
  std::shared_ptr<znet::Packet> packet;
};

class ReplicationManager {
 public:
  void Update(float delta_time);

  void PushCommand(ReplicationCommand command);

  void RegisterPacketHandlers();

  void QueueFullSyncForSession(uint64_t session_id);

 private:
  // Process all incoming commands from the network
  void ProcessIncomingCommands();

  // Server-only: handle spawns, destroys, late joiner sync
  void ServerHandleSpawnsAndDestroys();

  // Both sides: send dirty state for entities we own
  void SendOwnedEntityState(float delta_time);

  // Both sides: interpolate transforms for remote entities
  void InterpolateRemoteEntities(float delta_time);

  // Send all networked entities to a late joiner
  void SendFullStateToSession(uint64_t session_id);

  // Spawn an entity from a spawn packet
  void SpawnEntityFromPacket(std::shared_ptr<EntitySpawnPacket> packet);

  // Build a spawn packet from an entity's current state
  std::shared_ptr<EntitySpawnPacket> BuildSpawnPacket(
      Entity& entity, const NetworkIdentityComponent& net_id);

  uint32_t AllocateNetId();

  // Maps net_id -> EntityRef
  std::unordered_map<uint32_t, EntityRef> net_id_to_entity_;
  uint32_t next_net_id_ = 1;

  float send_accumulator_ = 0.0f;

  std::mutex command_mutex_;
  std::vector<ReplicationCommand> pending_commands_;

  std::mutex sync_mutex_;
  std::vector<uint64_t> pending_full_sync_;

  std::mutex destroy_mutex_;
  std::vector<uint32_t> pending_destroys_;

  // Deferred spawns for scenes not yet loaded (client-side)
  std::unordered_map<std::string, std::vector<ReplicationCommand>>
      deferred_spawns_;
  std::unordered_set<std::string> loading_scenes_;

  bool handlers_registered_ = false;
};

}  // namespace wiesel
