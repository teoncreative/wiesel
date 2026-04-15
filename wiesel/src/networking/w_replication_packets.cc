
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "networking/w_replication_packets.h"

#include "networking/w_network.h"
#include "networking/w_replication_types.h"

namespace wiesel {

// EntitySpawnPacketSerializer

std::shared_ptr<znet::Buffer> EntitySpawnPacketSerializer::SerializeTyped(
    std::shared_ptr<EntitySpawnPacket> packet,
    std::shared_ptr<znet::Buffer> buffer) {
  buffer->WriteInt<uint32_t>(packet->net_id);
  buffer->WriteInt<uint64_t>(packet->uuid_hi);
  buffer->WriteInt<uint64_t>(packet->uuid_lo);
  buffer->WriteString(packet->entity_name);
  buffer->WriteInt<uint8_t>(packet->authority);
  buffer->WriteInt<uint64_t>(packet->owner_session_id);
  buffer->WriteString(packet->scene_name);

  if (packet->component_data && packet->component_data->size() > 0) {
    uint16_t data_size =
        static_cast<uint16_t>(packet->component_data->size());
    buffer->WriteInt<uint16_t>(data_size);
    buffer->Write(packet->component_data->data(),
                  packet->component_data->size());
  } else {
    buffer->WriteInt<uint16_t>(0);
  }

  return buffer;
}

std::shared_ptr<EntitySpawnPacket>
EntitySpawnPacketSerializer::DeserializeTyped(
    std::shared_ptr<znet::Buffer> buffer) {
  auto packet = std::make_shared<EntitySpawnPacket>();
  packet->net_id = buffer->ReadInt<uint32_t>();
  packet->uuid_hi = buffer->ReadInt<uint64_t>();
  packet->uuid_lo = buffer->ReadInt<uint64_t>();
  packet->entity_name = buffer->ReadString();
  packet->authority = buffer->ReadInt<uint8_t>();
  packet->owner_session_id = buffer->ReadInt<uint64_t>();
  packet->scene_name = buffer->ReadString();

  uint16_t data_size = buffer->ReadInt<uint16_t>();
  if (data_size > 0) {
    packet->component_data = std::make_shared<znet::Buffer>(
        buffer->read_cursor_data(), data_size);
    buffer->SkipRead(data_size);
  }

  return packet;
}

// EntityDestroyPacketSerializer

std::shared_ptr<znet::Buffer> EntityDestroyPacketSerializer::SerializeTyped(
    std::shared_ptr<EntityDestroyPacket> packet,
    std::shared_ptr<znet::Buffer> buffer) {
  buffer->WriteInt<uint32_t>(packet->net_id);
  return buffer;
}

std::shared_ptr<EntityDestroyPacket>
EntityDestroyPacketSerializer::DeserializeTyped(
    std::shared_ptr<znet::Buffer> buffer) {
  auto packet = std::make_shared<EntityDestroyPacket>();
  packet->net_id = buffer->ReadInt<uint32_t>();
  return packet;
}

// EntityStatePacketSerializer

std::shared_ptr<znet::Buffer> EntityStatePacketSerializer::SerializeTyped(
    std::shared_ptr<EntityStatePacket> packet,
    std::shared_ptr<znet::Buffer> buffer) {
  uint16_t count = static_cast<uint16_t>(packet->updates.size());
  buffer->WriteInt<uint16_t>(count);

  for (const auto& update : packet->updates) {
    buffer->WriteInt<uint32_t>(update.net_id);

    if (update.component_data && update.component_data->size() > 0) {
      uint16_t data_size =
          static_cast<uint16_t>(update.component_data->size());
      buffer->WriteInt<uint16_t>(data_size);
      buffer->Write(update.component_data->data(),
                    update.component_data->size());
    } else {
      buffer->WriteInt<uint16_t>(0);
    }
  }

  return buffer;
}

std::shared_ptr<EntityStatePacket>
EntityStatePacketSerializer::DeserializeTyped(
    std::shared_ptr<znet::Buffer> buffer) {
  auto packet = std::make_shared<EntityStatePacket>();
  uint16_t count = buffer->ReadInt<uint16_t>();

  packet->updates.reserve(count);
  for (uint16_t i = 0; i < count; i++) {
    EntityStatePacket::EntityUpdate update;
    update.net_id = buffer->ReadInt<uint32_t>();

    uint16_t data_size = buffer->ReadInt<uint16_t>();
    if (data_size > 0) {
      update.component_data = std::make_shared<znet::Buffer>(
          buffer->read_cursor_data(), data_size);
      buffer->SkipRead(data_size);
    }

    packet->updates.push_back(std::move(update));
  }

  return packet;
}

// EntityOwnershipPacketSerializer

std::shared_ptr<znet::Buffer> EntityOwnershipPacketSerializer::SerializeTyped(
    std::shared_ptr<EntityOwnershipPacket> packet,
    std::shared_ptr<znet::Buffer> buffer) {
  buffer->WriteInt<uint32_t>(packet->net_id);
  buffer->WriteInt<uint8_t>(packet->authority);
  buffer->WriteInt<uint64_t>(packet->owner_session_id);
  return buffer;
}

std::shared_ptr<EntityOwnershipPacket>
EntityOwnershipPacketSerializer::DeserializeTyped(
    std::shared_ptr<znet::Buffer> buffer) {
  auto packet = std::make_shared<EntityOwnershipPacket>();
  packet->net_id = buffer->ReadInt<uint32_t>();
  packet->authority = buffer->ReadInt<uint8_t>();
  packet->owner_session_id = buffer->ReadInt<uint64_t>();
  return packet;
}

// RpcPacketSerializer

std::shared_ptr<znet::Buffer> RpcPacketSerializer::SerializeTyped(
    std::shared_ptr<RpcPacket> packet,
    std::shared_ptr<znet::Buffer> buffer) {
  buffer->WriteInt<uint32_t>(packet->net_id);
  buffer->WriteInt<uint8_t>(static_cast<uint8_t>(packet->direction));
  buffer->WriteString(packet->rpc_name);

  if (packet->args_data && packet->args_data->size() > 0) {
    uint16_t args_size = static_cast<uint16_t>(packet->args_data->size());
    buffer->WriteInt<uint16_t>(args_size);
    buffer->Write(packet->args_data->data(), packet->args_data->size());
  } else {
    buffer->WriteInt<uint16_t>(0);
  }
  return buffer;
}

std::shared_ptr<RpcPacket> RpcPacketSerializer::DeserializeTyped(
    std::shared_ptr<znet::Buffer> buffer) {
  auto packet = std::make_shared<RpcPacket>();
  packet->net_id = buffer->ReadInt<uint32_t>();
  packet->direction = static_cast<RpcDirection>(buffer->ReadInt<uint8_t>());
  packet->rpc_name = buffer->ReadString();

  uint16_t args_size = buffer->ReadInt<uint16_t>();
  if (args_size > 0) {
    packet->args_data = std::make_shared<znet::Buffer>(
        buffer->read_cursor_data(), args_size);
    buffer->SkipRead(args_size);
  }
  return packet;
}

// SyncVarUpdatePacketSerializer

static void WriteSyncVarEntry(znet::Buffer& buf,
                              const SyncVarUpdatePacket::VarEntry& entry) {
  buf.WriteString(entry.name);
  buf.WriteInt<uint8_t>(entry.type);
  switch (static_cast<SyncVarType>(entry.type)) {
    case SyncVarType::kInt:
      buf.WriteInt<int32_t>(entry.int_val);
      break;
    case SyncVarType::kFloat:
      buf.WriteFloat(entry.float_val);
      break;
    case SyncVarType::kBool:
      buf.WriteBool(entry.bool_val);
      break;
    case SyncVarType::kString:
      buf.WriteString(entry.string_val);
      break;
    case SyncVarType::kVec3:
      buf.WriteFloat(entry.vec3_val.x);
      buf.WriteFloat(entry.vec3_val.y);
      buf.WriteFloat(entry.vec3_val.z);
      break;
  }
}

static SyncVarUpdatePacket::VarEntry ReadSyncVarEntry(znet::Buffer& buf) {
  SyncVarUpdatePacket::VarEntry entry;
  entry.name = buf.ReadString();
  entry.type = buf.ReadInt<uint8_t>();
  switch (static_cast<SyncVarType>(entry.type)) {
    case SyncVarType::kInt:
      entry.int_val = buf.ReadInt<int32_t>();
      break;
    case SyncVarType::kFloat:
      entry.float_val = buf.ReadFloat();
      break;
    case SyncVarType::kBool:
      entry.bool_val = buf.ReadBool();
      break;
    case SyncVarType::kString:
      entry.string_val = buf.ReadString();
      break;
    case SyncVarType::kVec3:
      entry.vec3_val.x = buf.ReadFloat();
      entry.vec3_val.y = buf.ReadFloat();
      entry.vec3_val.z = buf.ReadFloat();
      break;
  }
  return entry;
}

std::shared_ptr<znet::Buffer> SyncVarUpdatePacketSerializer::SerializeTyped(
    std::shared_ptr<SyncVarUpdatePacket> packet,
    std::shared_ptr<znet::Buffer> buffer) {
  buffer->WriteInt<uint32_t>(packet->net_id);
  uint16_t count = static_cast<uint16_t>(packet->vars.size());
  buffer->WriteInt<uint16_t>(count);
  for (const auto& var : packet->vars) {
    WriteSyncVarEntry(*buffer, var);
  }
  return buffer;
}

std::shared_ptr<SyncVarUpdatePacket>
SyncVarUpdatePacketSerializer::DeserializeTyped(
    std::shared_ptr<znet::Buffer> buffer) {
  auto packet = std::make_shared<SyncVarUpdatePacket>();
  packet->net_id = buffer->ReadInt<uint32_t>();
  uint16_t count = buffer->ReadInt<uint16_t>();
  packet->vars.reserve(count);
  for (uint16_t i = 0; i < count; i++) {
    packet->vars.push_back(ReadSyncVarEntry(*buffer));
  }
  return packet;
}

// SceneLoadPacketSerializer

std::shared_ptr<znet::Buffer> SceneLoadPacketSerializer::SerializeTyped(
    std::shared_ptr<SceneLoadPacket> packet,
    std::shared_ptr<znet::Buffer> buffer) {
  buffer->WriteString(packet->scene_name);
  buffer->WriteInt<uint8_t>(packet->load_mode);
  buffer->WriteString(packet->loading_scene);
  return buffer;
}

std::shared_ptr<SceneLoadPacket> SceneLoadPacketSerializer::DeserializeTyped(
    std::shared_ptr<znet::Buffer> buffer) {
  auto packet = std::make_shared<SceneLoadPacket>();
  packet->scene_name = buffer->ReadString();
  packet->load_mode = buffer->ReadInt<uint8_t>();
  packet->loading_scene = buffer->ReadString();
  return packet;
}

// Registration

void RegisterReplicationPackets(NetworkManager& network) {
  network.RegisterPacket(kEntitySpawnPacket,
                         std::make_unique<EntitySpawnPacketSerializer>());
  network.RegisterPacket(kEntityDestroyPacket,
                         std::make_unique<EntityDestroyPacketSerializer>());
  network.RegisterPacket(kEntityStatePacket,
                         std::make_unique<EntityStatePacketSerializer>());
  network.RegisterPacket(kEntityOwnershipPacket,
                         std::make_unique<EntityOwnershipPacketSerializer>());
  network.RegisterPacket(kRpcPacket,
                         std::make_unique<RpcPacketSerializer>());
  network.RegisterPacket(kSyncVarUpdatePacket,
                         std::make_unique<SyncVarUpdatePacketSerializer>());
  network.RegisterPacket(kSceneLoadPacket,
                         std::make_unique<SceneLoadPacketSerializer>());
}

}  // namespace wiesel
