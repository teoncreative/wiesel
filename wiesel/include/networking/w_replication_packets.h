
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

#include "w_pch.h"
#include "znet/buffer.h"
#include "znet/packet.h"
#include "znet/packet_serializer.h"

namespace wiesel {

// Packet IDs 100-199 reserved for engine replication.
// Game packets should use 0-99 or 200+.
enum ReplicationPacketId : znet::PacketId {
  kEntitySpawnPacket = 100,
  kEntityDestroyPacket = 101,
  kEntityStatePacket = 102,
  kEntityOwnershipPacket = 103,
  kRpcPacket = 104,
  kSyncVarUpdatePacket = 105,
  kSceneLoadPacket = 106,
};

class EntitySpawnPacket : public znet::Packet {
 public:
  EntitySpawnPacket() : Packet(kEntitySpawnPacket) {}

  uint32_t net_id = 0;
  uint64_t uuid_hi = 0;
  uint64_t uuid_lo = 0;
  std::string entity_name;
  uint8_t authority = 0;
  uint64_t owner_session_id = 0;
  std::string scene_name;
  std::shared_ptr<znet::Buffer> component_data;
};

class EntityDestroyPacket : public znet::Packet {
 public:
  EntityDestroyPacket() : Packet(kEntityDestroyPacket) {}

  uint32_t net_id = 0;
};

class EntityStatePacket : public znet::Packet {
 public:
  EntityStatePacket() : Packet(kEntityStatePacket) {}

  struct EntityUpdate {
    uint32_t net_id = 0;
    std::shared_ptr<znet::Buffer> component_data;
  };

  std::vector<EntityUpdate> updates;
};

class EntityOwnershipPacket : public znet::Packet {
 public:
  EntityOwnershipPacket() : Packet(kEntityOwnershipPacket) {}

  uint32_t net_id = 0;
  uint8_t authority = 0;
  uint64_t owner_session_id = 0;
};

// Serializers

class EntitySpawnPacketSerializer
    : public znet::PacketSerializer<EntitySpawnPacket> {
 public:
  std::shared_ptr<znet::Buffer> SerializeTyped(
      std::shared_ptr<EntitySpawnPacket> packet,
      std::shared_ptr<znet::Buffer> buffer) override;

  std::shared_ptr<EntitySpawnPacket> DeserializeTyped(
      std::shared_ptr<znet::Buffer> buffer) override;
};

class EntityDestroyPacketSerializer
    : public znet::PacketSerializer<EntityDestroyPacket> {
 public:
  std::shared_ptr<znet::Buffer> SerializeTyped(
      std::shared_ptr<EntityDestroyPacket> packet,
      std::shared_ptr<znet::Buffer> buffer) override;

  std::shared_ptr<EntityDestroyPacket> DeserializeTyped(
      std::shared_ptr<znet::Buffer> buffer) override;
};

class EntityStatePacketSerializer
    : public znet::PacketSerializer<EntityStatePacket> {
 public:
  std::shared_ptr<znet::Buffer> SerializeTyped(
      std::shared_ptr<EntityStatePacket> packet,
      std::shared_ptr<znet::Buffer> buffer) override;

  std::shared_ptr<EntityStatePacket> DeserializeTyped(
      std::shared_ptr<znet::Buffer> buffer) override;
};

class EntityOwnershipPacketSerializer
    : public znet::PacketSerializer<EntityOwnershipPacket> {
 public:
  std::shared_ptr<znet::Buffer> SerializeTyped(
      std::shared_ptr<EntityOwnershipPacket> packet,
      std::shared_ptr<znet::Buffer> buffer) override;

  std::shared_ptr<EntityOwnershipPacket> DeserializeTyped(
      std::shared_ptr<znet::Buffer> buffer) override;
};

// RPC direction
enum class RpcDirection : uint8_t {
  kToServer = 0,
  kToClients = 1,
};

class RpcPacket : public znet::Packet {
 public:
  RpcPacket() : Packet(kRpcPacket) {}

  uint32_t net_id = 0;
  RpcDirection direction = RpcDirection::kToServer;
  std::string rpc_name;
  std::shared_ptr<znet::Buffer> args_data;  // serialized arguments
};

class RpcPacketSerializer : public znet::PacketSerializer<RpcPacket> {
 public:
  std::shared_ptr<znet::Buffer> SerializeTyped(
      std::shared_ptr<RpcPacket> packet,
      std::shared_ptr<znet::Buffer> buffer) override;

  std::shared_ptr<RpcPacket> DeserializeTyped(
      std::shared_ptr<znet::Buffer> buffer) override;
};

class SyncVarUpdatePacket : public znet::Packet {
 public:
  SyncVarUpdatePacket() : Packet(kSyncVarUpdatePacket) {}

  uint32_t net_id = 0;

  struct VarEntry {
    std::string name;
    uint8_t type = 0;
    int int_val = 0;
    float float_val = 0.0f;
    bool bool_val = false;
    std::string string_val;
    glm::vec3 vec3_val{0.0f};
  };

  std::vector<VarEntry> vars;
};

class SyncVarUpdatePacketSerializer
    : public znet::PacketSerializer<SyncVarUpdatePacket> {
 public:
  std::shared_ptr<znet::Buffer> SerializeTyped(
      std::shared_ptr<SyncVarUpdatePacket> packet,
      std::shared_ptr<znet::Buffer> buffer) override;

  std::shared_ptr<SyncVarUpdatePacket> DeserializeTyped(
      std::shared_ptr<znet::Buffer> buffer) override;
};

class SceneLoadPacket : public znet::Packet {
 public:
  SceneLoadPacket() : Packet(kSceneLoadPacket) {}

  std::string scene_name;
  uint8_t load_mode = 0;      // 0=Single, 1=Additive, 2=WithLoading
  std::string loading_scene;  // only used when load_mode=2
};

class SceneLoadPacketSerializer
    : public znet::PacketSerializer<SceneLoadPacket> {
 public:
  std::shared_ptr<znet::Buffer> SerializeTyped(
      std::shared_ptr<SceneLoadPacket> packet,
      std::shared_ptr<znet::Buffer> buffer) override;

  std::shared_ptr<SceneLoadPacket> DeserializeTyped(
      std::shared_ptr<znet::Buffer> buffer) override;
};

class NetworkManager;
void RegisterReplicationPackets(NetworkManager& network);

}  // namespace wiesel
