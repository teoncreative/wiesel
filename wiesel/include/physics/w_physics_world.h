
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

#include <entt/entt.hpp>
#include <memory>
#include <set>
#include <unordered_map>
#include "w_pch.h"

namespace JPH {
class PhysicsSystem;
class TempAllocator;
class JobSystem;
class Shape;
}  // namespace JPH

namespace wiesel {

class Scene;
class BroadPhaseLayerInterfaceImpl;
class ObjectVsBroadPhaseLayerFilterImpl;
class ObjectLayerPairFilterImpl;
class WieselContactListener;
struct TransformComponent;

struct RaycastHit {
  entt::entity entity = entt::null;
  glm::vec3 point{0.0f};
  glm::vec3 normal{0.0f};
  float distance = 0.0f;
};

// Ordered contact pair for Enter/Stay/Exit tracking
struct ContactPair {
  entt::entity a;
  entt::entity b;

  ContactPair(entt::entity x, entt::entity y) {
    if (x < y) {
      a = x;
      b = y;
    } else {
      a = y;
      b = x;
    }
  }

  bool operator<(const ContactPair& o) const {
    if (a != o.a) {
      return a < o.a;
    }
    return b < o.b;
  }

  bool operator==(const ContactPair& o) const { return a == o.a && b == o.b; }
};

class PhysicsWorld {
 public:
  explicit PhysicsWorld(Scene* scene);
  ~PhysicsWorld();

  PhysicsWorld(const PhysicsWorld&) = delete;
  PhysicsWorld& operator=(const PhysicsWorld&) = delete;

  // Body lifecycle
  void CreateBody(entt::entity entity);
  void DestroyBody(entt::entity entity);

  // Create bodies for any new entities that have collider + optional rigidbody
  void EnsureBodiesExist();

  // Rebuild a body whose properties changed (type, mass, etc.)
  void RecreateBodyIfNeeded(entt::entity entity);

  // Simulation
  void StepSimulation(float delta_time);
  void SyncTransformsFromECS();  // ECS -> Jolt (kinematic + sensor)
  void SyncTransformsToECS();    // Jolt -> ECS (dynamic)

  // Contact detection - processes buffered contacts, dispatches callbacks
  void DetectContacts();

  // Queries
  bool Raycast(const glm::vec3& from, const glm::vec3& to, RaycastHit& hit,
               entt::entity ignore = entt::null) const;
  bool Raycast(const glm::vec3& from, const glm::vec3& to, RaycastHit& hit,
               entt::entity ignore, uint16_t collision_mask) const;
  std::vector<entt::entity> OverlapBox(const glm::vec3& center,
                                       const glm::vec3& half_extents) const;
  std::vector<entt::entity> OverlapSphere(const glm::vec3& center,
                                          float radius) const;

  void SetGravity(const glm::vec3& gravity);
  glm::vec3 GetGravity() const;

  JPH::PhysicsSystem* GetPhysicsSystem() const { return physics_system_; }

 private:
  glm::vec3 GetColliderOffset(entt::entity entity) const;
  JPH::Shape* CreateShapeForEntity(entt::entity entity) const;

  Scene* scene_;

  // Jolt core
  JPH::PhysicsSystem* physics_system_ = nullptr;
  JPH::TempAllocator* temp_allocator_ = nullptr;
  JPH::JobSystem* job_system_ = nullptr;
  std::unique_ptr<BroadPhaseLayerInterfaceImpl> bp_layer_interface_;
  std::unique_ptr<ObjectVsBroadPhaseLayerFilterImpl> obj_vs_bp_filter_;
  std::unique_ptr<ObjectLayerPairFilterImpl> obj_layer_pair_filter_;
  std::unique_ptr<WieselContactListener> contact_listener_;

  // Entity -> Jolt body data
  struct BodyData {
    uint32_t body_id_raw = ~0u;
    bool is_sensor = false;
  };

  std::unordered_map<entt::entity, BodyData> bodies_;

  // Contact tracking
  std::set<ContactPair> prev_solid_contacts_;
  std::set<ContactPair> prev_trigger_contacts_;
};

}  // namespace wiesel
