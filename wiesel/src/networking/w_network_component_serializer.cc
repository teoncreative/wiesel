
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "networking/w_network_component_serializer.h"

#include "physics/w_rigidbody.h"
#include "scene/w_components.h"
#include "util/w_logger.h"
#include "znet/buffer.h"

namespace wiesel {

static std::vector<NetworkComponentSerializerDesc>& GetMutableRegistry() {
  static std::vector<NetworkComponentSerializerDesc> registry;
  return registry;
}

NetworkComponentTypeId NetworkComponentSerializerRegistry::Register(
    NetworkComponentSerializerDesc desc) {
  auto& reg = GetMutableRegistry();
  desc.type_id = static_cast<NetworkComponentTypeId>(reg.size());
  reg.push_back(std::move(desc));
  return desc.type_id;
}

const std::vector<NetworkComponentSerializerDesc>&
NetworkComponentSerializerRegistry::Registry() {
  return GetMutableRegistry();
}

void NetworkComponentSerializerRegistry::SerializeDirty(
    Entity& entity, NetworkIdentityComponent& net_id,
    znet::Buffer& buffer) {
  const auto& reg = GetMutableRegistry();

  uint8_t count = 0;
  for (size_t i = 0; i < reg.size(); i++) {
    if (net_id.dirty_components.test(i) && reg[i].Has(entity)) {
      count++;
    }
  }

  buffer.WriteInt<uint8_t>(count);

  for (size_t i = 0; i < reg.size(); i++) {
    if (!net_id.dirty_components.test(i) || !reg[i].Has(entity)) {
      continue;
    }

    buffer.WriteInt<uint16_t>(reg[i].type_id);

    size_t len_pos = buffer.write_cursor();
    buffer.WriteInt<uint16_t>(0);
    size_t data_start = buffer.write_cursor();

    reg[i].Serialize(entity, buffer);

    size_t data_end = buffer.write_cursor();
    uint16_t data_len = static_cast<uint16_t>(data_end - data_start);

    buffer.set_write_cursor(len_pos);
    buffer.WriteInt<uint16_t>(data_len);
    buffer.set_write_cursor(data_end);
  }
}

void NetworkComponentSerializerRegistry::SerializeAll(
    Entity& entity, znet::Buffer& buffer) {
  const auto& reg = GetMutableRegistry();

  uint8_t count = 0;
  for (const auto& desc : reg) {
    if (desc.Has(entity)) {
      count++;
    }
  }

  buffer.WriteInt<uint8_t>(count);

  for (const auto& desc : reg) {
    if (!desc.Has(entity)) {
      continue;
    }

    buffer.WriteInt<uint16_t>(desc.type_id);

    size_t len_pos = buffer.write_cursor();
    buffer.WriteInt<uint16_t>(0);
    size_t data_start = buffer.write_cursor();

    desc.Serialize(entity, buffer);

    size_t data_end = buffer.write_cursor();
    uint16_t data_len = static_cast<uint16_t>(data_end - data_start);

    buffer.set_write_cursor(len_pos);
    buffer.WriteInt<uint16_t>(data_len);
    buffer.set_write_cursor(data_end);
  }
}

void NetworkComponentSerializerRegistry::DeserializeAll(
    Entity& entity, znet::Buffer& buffer) {
  const auto& reg = GetMutableRegistry();

  uint8_t count = buffer.ReadInt<uint8_t>();

  for (uint8_t i = 0; i < count; i++) {
    uint16_t type_id = buffer.ReadInt<uint16_t>();
    uint16_t data_len = buffer.ReadInt<uint16_t>();

    if (type_id < reg.size()) {
      reg[type_id].Deserialize(entity, buffer);
    } else {
      LOG_WARN("Unknown network component type_id {}, skipping {} bytes",
               type_id, data_len);
      buffer.SkipRead(data_len);
    }
  }
}

void NetworkComponentSerializerRegistry::UpdateDirtyFlags(
    Entity& entity, NetworkIdentityComponent& net_id) {
  const auto& reg = GetMutableRegistry();

  for (size_t i = 0; i < reg.size(); i++) {
    if (reg[i].Has(entity) && reg[i].HasChanged(entity)) {
      net_id.dirty_components.set(i);
    }
  }
}

static void WriteVec3(znet::Buffer& buffer, const glm::vec3& v) {
  buffer.WriteFloat(v.x);
  buffer.WriteFloat(v.y);
  buffer.WriteFloat(v.z);
}

static glm::vec3 ReadVec3(znet::Buffer& buffer) {
  float x = buffer.ReadFloat();
  float y = buffer.ReadFloat();
  float z = buffer.ReadFloat();
  return {x, y, z};
}

void InitializeNetworkComponentSerializers() {
  // TransformComponent (type_id 0)
  NetworkComponentSerializerRegistry::Register({
      .type_id = 0,
      .debug_name = "Transform",
      .Has = [](Entity& e) { return e.HasComponent<TransformComponent>(); },
      .Serialize =
          [](Entity& e, znet::Buffer& buf) {
            auto& tc = e.GetComponent<TransformComponent>();
            WriteVec3(buf, tc.GetPosition());
            WriteVec3(buf, tc.GetRotation());
            WriteVec3(buf, tc.GetScale());
          },
      .Deserialize =
          [](Entity& e, znet::Buffer& buf) {
            auto& tc = e.GetComponent<TransformComponent>();
            tc.SetPosition(ReadVec3(buf));
            tc.SetRotation(ReadVec3(buf));
            tc.SetScale(ReadVec3(buf));
          },
      .HasChanged =
          [](Entity& e) {
            return e.GetComponent<TransformComponent>().IsChanged();
          },
  });

  // RigidBodyComponent (type_id 1)
  NetworkComponentSerializerRegistry::Register({
      .type_id = 0,
      .debug_name = "RigidBody",
      .Has = [](Entity& e) { return e.HasComponent<RigidBodyComponent>(); },
      .Serialize =
          [](Entity& e, znet::Buffer& buf) {
            auto& rb = e.GetComponent<RigidBodyComponent>();
            buf.WriteInt<uint8_t>(static_cast<uint8_t>(rb.type));
            buf.WriteFloat(rb.mass);
            buf.WriteFloat(rb.friction);
            buf.WriteFloat(rb.restitution);
            buf.WriteFloat(rb.linear_damping);
            buf.WriteFloat(rb.angular_damping);

            std::bitset<8> flags;
            flags.set(0, rb.lock_position_x);
            flags.set(1, rb.lock_position_y);
            flags.set(2, rb.lock_position_z);
            flags.set(3, rb.lock_rotation_x);
            flags.set(4, rb.lock_rotation_y);
            flags.set(5, rb.lock_rotation_z);
            flags.set(6, rb.HasBody());
            buf.WriteBitset(flags);

            if (rb.HasBody()) {
              WriteVec3(buf, rb.GetLinearVelocity());
              WriteVec3(buf, rb.GetAngularVelocity());
            }
          },
      .Deserialize =
          [](Entity& e, znet::Buffer& buf) {
            auto& rb = e.GetScene()->GetRegistry()
                           .get_or_emplace<RigidBodyComponent>(e.handle());
            rb.type = static_cast<RigidBodyType>(buf.ReadInt<uint8_t>());
            rb.mass = buf.ReadFloat();
            rb.friction = buf.ReadFloat();
            rb.restitution = buf.ReadFloat();
            rb.linear_damping = buf.ReadFloat();
            rb.angular_damping = buf.ReadFloat();

            auto flags = buf.ReadBitset<8>();
            rb.lock_position_x = flags.test(0);
            rb.lock_position_y = flags.test(1);
            rb.lock_position_z = flags.test(2);
            rb.lock_rotation_x = flags.test(3);
            rb.lock_rotation_y = flags.test(4);
            rb.lock_rotation_z = flags.test(5);

            if (flags.test(6)) {
              glm::vec3 linear_vel = ReadVec3(buf);
              glm::vec3 angular_vel = ReadVec3(buf);
              if (rb.HasBody()) {
                rb.SetLinearVelocity(linear_vel);
                rb.SetAngularVelocity(angular_vel);
              }
            }
            rb.needs_recreate = true;
          },
      .HasChanged =
          [](Entity& e) {
            auto& rb = e.GetComponent<RigidBodyComponent>();
            return rb.type == RigidBodyType::Dynamic && rb.HasBody();
          },
  });

  LOG_INFO("Registered {} network component serializers",
           NetworkComponentSerializerRegistry::Registry().size());
}

}  // namespace wiesel
