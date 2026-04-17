
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

#include "behavior/w_behavior.h"
#include "script/mono/w_monobehavior.h"
#include "physics/w_collider.h"
#include "physics/w_rigidbody.h"
#include "rendering/w_mesh_renderer.h"
#include "rendering/w_sprite.h"
#include "scene/w_components.h"
#include "scene/w_lights.h"
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
            // Don't set needs_recreate here - the PhysicsBodySystem will
            // naturally detect and create the body once all components
            // (including colliders) are deserialized.
          },
      .HasChanged =
          [](Entity& e) {
            auto& rb = e.GetComponent<RigidBodyComponent>();
            return rb.type == RigidBodyType::Dynamic && rb.HasBody();
          },
  });

  // MeshRendererComponent (type_id 2) - spawn only, no delta
  NetworkComponentSerializerRegistry::Register({
      .type_id = 0,
      .debug_name = "MeshRenderer",
      .Has = [](Entity& e) { return e.HasComponent<MeshRendererComponent>(); },
      .Serialize =
          [](Entity& e, znet::Buffer& buf) {
            auto& mr = e.GetComponent<MeshRendererComponent>();
            buf.WriteString(mr.model_handle.ToString());
            buf.WriteInt<int32_t>(mr.mesh_index);
            buf.WriteBool(mr.enable_rendering);
            buf.WriteBool(mr.receive_shadows);
            buf.WriteString(mr.material_handle.ToString());
          },
      .Deserialize =
          [](Entity& e, znet::Buffer& buf) {
            auto& reg = e.GetScene()->GetRegistry();
            auto& mr = reg.get_or_emplace<MeshRendererComponent>(e.handle());
            mr.model_handle = AssetHandle::FromString(buf.ReadString());
            mr.mesh_index = buf.ReadInt<int32_t>();
            mr.enable_rendering = buf.ReadBool();
            mr.receive_shadows = buf.ReadBool();
            mr.material_handle = AssetHandle::FromString(buf.ReadString());
          },
      .HasChanged = [](Entity&) { return false; },
  });

  // BoxColliderComponent (type_id 3) - spawn only
  NetworkComponentSerializerRegistry::Register({
      .type_id = 0,
      .debug_name = "BoxCollider",
      .Has = [](Entity& e) { return e.HasComponent<BoxColliderComponent>(); },
      .Serialize =
          [](Entity& e, znet::Buffer& buf) {
            auto& bc = e.GetComponent<BoxColliderComponent>();
            WriteVec3(buf, bc.offset);
            WriteVec3(buf, bc.half_extents);
            buf.WriteBool(bc.is_trigger);
            buf.WriteBool(bc.is_one_way);
            buf.WriteInt<uint16_t>(bc.collision_group);
          },
      .Deserialize =
          [](Entity& e, znet::Buffer& buf) {
            auto& reg = e.GetScene()->GetRegistry();
            auto& bc = reg.get_or_emplace<BoxColliderComponent>(e.handle());
            bc.offset = ReadVec3(buf);
            bc.half_extents = ReadVec3(buf);
            bc.is_trigger = buf.ReadBool();
            bc.is_one_way = buf.ReadBool();
            bc.collision_group = buf.ReadInt<uint16_t>();
          },
      .HasChanged = [](Entity&) { return false; },
  });

  // SphereColliderComponent (type_id 4) - spawn only
  NetworkComponentSerializerRegistry::Register({
      .type_id = 0,
      .debug_name = "SphereCollider",
      .Has = [](Entity& e) { return e.HasComponent<SphereColliderComponent>(); },
      .Serialize =
          [](Entity& e, znet::Buffer& buf) {
            auto& sc = e.GetComponent<SphereColliderComponent>();
            WriteVec3(buf, sc.offset);
            buf.WriteFloat(sc.radius);
            buf.WriteBool(sc.is_trigger);
            buf.WriteBool(sc.is_one_way);
            buf.WriteInt<uint16_t>(sc.collision_group);
          },
      .Deserialize =
          [](Entity& e, znet::Buffer& buf) {
            auto& reg = e.GetScene()->GetRegistry();
            auto& sc = reg.get_or_emplace<SphereColliderComponent>(e.handle());
            sc.offset = ReadVec3(buf);
            sc.radius = buf.ReadFloat();
            sc.is_trigger = buf.ReadBool();
            sc.is_one_way = buf.ReadBool();
            sc.collision_group = buf.ReadInt<uint16_t>();
          },
      .HasChanged = [](Entity&) { return false; },
  });

  // CapsuleColliderComponent (type_id 5) - spawn only
  NetworkComponentSerializerRegistry::Register({
      .type_id = 0,
      .debug_name = "CapsuleCollider",
      .Has = [](Entity& e) { return e.HasComponent<CapsuleColliderComponent>(); },
      .Serialize =
          [](Entity& e, znet::Buffer& buf) {
            auto& cc = e.GetComponent<CapsuleColliderComponent>();
            WriteVec3(buf, cc.offset);
            buf.WriteFloat(cc.radius);
            buf.WriteFloat(cc.height);
            buf.WriteInt<uint8_t>(static_cast<uint8_t>(cc.axis));
            buf.WriteBool(cc.is_trigger);
            buf.WriteBool(cc.is_one_way);
            buf.WriteInt<uint16_t>(cc.collision_group);
          },
      .Deserialize =
          [](Entity& e, znet::Buffer& buf) {
            auto& reg = e.GetScene()->GetRegistry();
            auto& cc = reg.get_or_emplace<CapsuleColliderComponent>(e.handle());
            cc.offset = ReadVec3(buf);
            cc.radius = buf.ReadFloat();
            cc.height = buf.ReadFloat();
            cc.axis = static_cast<CapsuleAxis>(buf.ReadInt<uint8_t>());
            cc.is_trigger = buf.ReadBool();
            cc.is_one_way = buf.ReadBool();
            cc.collision_group = buf.ReadInt<uint16_t>();
          },
      .HasChanged = [](Entity&) { return false; },
  });

  // LightDirectComponent (type_id 6) - spawn only
  NetworkComponentSerializerRegistry::Register({
      .type_id = 0,
      .debug_name = "LightDirect",
      .Has = [](Entity& e) { return e.HasComponent<LightDirectComponent>(); },
      .Serialize =
          [](Entity& e, znet::Buffer& buf) {
            auto& lc = e.GetComponent<LightDirectComponent>();
            WriteVec3(buf, lc.light_data.base.color);
            buf.WriteFloat(lc.light_data.base.ambient);
            buf.WriteFloat(lc.light_data.base.diffuse);
            buf.WriteFloat(lc.light_data.base.specular);
            buf.WriteFloat(lc.light_data.base.density);
            WriteVec3(buf, lc.light_data.direction);
          },
      .Deserialize =
          [](Entity& e, znet::Buffer& buf) {
            auto& reg = e.GetScene()->GetRegistry();
            auto& lc = reg.get_or_emplace<LightDirectComponent>(e.handle());
            lc.light_data.base.color = ReadVec3(buf);
            lc.light_data.base.ambient = buf.ReadFloat();
            lc.light_data.base.diffuse = buf.ReadFloat();
            lc.light_data.base.specular = buf.ReadFloat();
            lc.light_data.base.density = buf.ReadFloat();
            lc.light_data.direction = ReadVec3(buf);
          },
      .HasChanged = [](Entity&) { return false; },
  });

  // LightPointComponent (type_id 7) - spawn only
  NetworkComponentSerializerRegistry::Register({
      .type_id = 0,
      .debug_name = "LightPoint",
      .Has = [](Entity& e) { return e.HasComponent<LightPointComponent>(); },
      .Serialize =
          [](Entity& e, znet::Buffer& buf) {
            auto& lc = e.GetComponent<LightPointComponent>();
            WriteVec3(buf, lc.light_data.base.color);
            buf.WriteFloat(lc.light_data.base.ambient);
            buf.WriteFloat(lc.light_data.base.diffuse);
            buf.WriteFloat(lc.light_data.base.specular);
            buf.WriteFloat(lc.light_data.base.density);
            buf.WriteFloat(lc.light_data.constant);
            buf.WriteFloat(lc.light_data.linear);
            buf.WriteFloat(lc.light_data.exp);
          },
      .Deserialize =
          [](Entity& e, znet::Buffer& buf) {
            auto& reg = e.GetScene()->GetRegistry();
            auto& lc = reg.get_or_emplace<LightPointComponent>(e.handle());
            lc.light_data.base.color = ReadVec3(buf);
            lc.light_data.base.ambient = buf.ReadFloat();
            lc.light_data.base.diffuse = buf.ReadFloat();
            lc.light_data.base.specular = buf.ReadFloat();
            lc.light_data.base.density = buf.ReadFloat();
            lc.light_data.constant = buf.ReadFloat();
            lc.light_data.linear = buf.ReadFloat();
            lc.light_data.exp = buf.ReadFloat();
          },
      .HasChanged = [](Entity&) { return false; },
  });

  // AnimatorComponent (type_id 8) - spawn only
  NetworkComponentSerializerRegistry::Register({
      .type_id = 0,
      .debug_name = "Animator",
      .Has = [](Entity& e) { return e.HasComponent<AnimatorComponent>(); },
      .Serialize =
          [](Entity& e, znet::Buffer& buf) {
            auto& ac = e.GetComponent<AnimatorComponent>();
            buf.WriteString(ac.controller_handle.ToString());
            buf.WriteFloat(ac.playback_speed);
            buf.WriteBool(ac.playing);
          },
      .Deserialize =
          [](Entity& e, znet::Buffer& buf) {
            auto& reg = e.GetScene()->GetRegistry();
            auto& ac = reg.get_or_emplace<AnimatorComponent>(e.handle());
            ac.controller_handle = AssetHandle::FromString(buf.ReadString());
            ac.playback_speed = buf.ReadFloat();
            ac.playing = buf.ReadBool();
          },
      .HasChanged = [](Entity&) { return false; },
  });

  // SpriteRendererComponent (type_id 9) - spawn only
  NetworkComponentSerializerRegistry::Register({
      .type_id = 0,
      .debug_name = "SpriteRenderer",
      .Has = [](Entity& e) { return e.HasComponent<SpriteRendererComponent>(); },
      .Serialize =
          [](Entity& e, znet::Buffer& buf) {
            auto& sr = e.GetComponent<SpriteRendererComponent>();
            buf.WriteString(sr.sprite_handle_.ToString());
            buf.WriteFloat(sr.pivot_.x);
            buf.WriteFloat(sr.pivot_.y);
            buf.WriteFloat(sr.tint_.x);
            buf.WriteFloat(sr.tint_.y);
            buf.WriteFloat(sr.tint_.z);
            buf.WriteFloat(sr.tint_.w);
            buf.WriteBool(sr.flip_x_);
            buf.WriteBool(sr.flip_y_);
            buf.WriteInt<uint8_t>(sr.sort_layer_);
          },
      .Deserialize =
          [](Entity& e, znet::Buffer& buf) {
            auto& reg = e.GetScene()->GetRegistry();
            auto& sr = reg.get_or_emplace<SpriteRendererComponent>(e.handle());
            sr.sprite_handle_ = AssetHandle::FromString(buf.ReadString());
            sr.pivot_.x = buf.ReadFloat();
            sr.pivot_.y = buf.ReadFloat();
            sr.tint_.x = buf.ReadFloat();
            sr.tint_.y = buf.ReadFloat();
            sr.tint_.z = buf.ReadFloat();
            sr.tint_.w = buf.ReadFloat();
            sr.flip_x_ = buf.ReadBool();
            sr.flip_y_ = buf.ReadBool();
            sr.sort_layer_ = buf.ReadInt<uint8_t>();
          },
      .HasChanged = [](Entity&) { return false; },
  });

  // BehaviorsComponent (type_id 10) - spawn only
  NetworkComponentSerializerRegistry::Register({
      .type_id = 0,
      .debug_name = "Behaviors",
      .Has =
          [](Entity& e) { return e.HasComponent<BehaviorsComponent>(); },
      .Serialize =
          [](Entity& e, znet::Buffer& buf) {
            auto& bc = e.GetComponent<BehaviorsComponent>();
            buf.WriteInt<uint16_t>(
                static_cast<uint16_t>(bc.behaviors_.size()));
            for (auto& [name, behavior] : bc.behaviors_) {
              buf.WriteString(name);
            }
          },
      .Deserialize =
          [](Entity& e, znet::Buffer& buf) {
            uint16_t count = buf.ReadInt<uint16_t>();
            if (!e.HasComponent<BehaviorsComponent>()) {
              e.AddComponent<BehaviorsComponent>();
            }
            auto& bc = e.GetComponent<BehaviorsComponent>();
            for (uint16_t i = 0; i < count; i++) {
              std::string name = buf.ReadString();
              if (bc.behaviors_.find(name) == bc.behaviors_.end()) {
                auto* mono = new MonoBehavior(e, name);
                bc.behaviors_.insert(std::pair(name, mono));
              }
            }
          },
      .HasChanged = [](Entity&) { return false; },
  });

  LOG_INFO("Registered {} network component serializers",
           NetworkComponentSerializerRegistry::Registry().size());
}

}  // namespace wiesel
