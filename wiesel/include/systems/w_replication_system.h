
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
#include "systems/w_system.h"

namespace wiesel {

struct ReplicationCommand {
  enum Type { kSpawn, kDestroy, kState, kOwnership, kRpc, kSyncVar, kSceneLoad };
  Type type;
  std::shared_ptr<znet::Packet> packet;
};

class ReplicationSystem : public ISystem {
 public:
  void Update(Scene& scene, float delta_time) override;

  const char* GetName() const override { return "Replication"; }

  int GetPriority() const override { return 790; }

  bool RunOnFirstUpdate() const override { return false; }

  void PushCommand(ReplicationCommand command);

  void RegisterPacketHandlers();

  void QueueFullSyncForSession(uint64_t session_id);

 private:
  // Server: scan ALL loaded scenes for networked entities
  void ServerUpdate(float delta_time);

  // Client: process commands, spawn into correct scenes, interpolate
  void ClientUpdate(float delta_time);

  // Send all networked entities across all scenes to a session
  void SendFullStateToSession(uint64_t session_id);

  // Spawn an entity from a spawn packet into the correct scene
  void SpawnEntityFromPacket(std::shared_ptr<EntitySpawnPacket> packet);

  uint32_t AllocateNetId();

  // Maps net_id -> Entity (carries both handle and scene)
  std::unordered_map<uint32_t, Entity> net_id_to_entity_;
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
