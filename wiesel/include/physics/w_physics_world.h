
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

#include <BulletCollision/CollisionDispatch/btGhostObject.h>
#include <btBulletDynamicsCommon.h>
#include <entt/entt.hpp>
#include <memory>
#include <set>
#include <unordered_map>
#include "w_pch.h"

namespace Wiesel {

class Scene;
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

  // Simulation
  void StepSimulation(float delta_time);
  void SyncTransformsFromECS();  // ECS -> Bullet (kinematic + ghost)
  void SyncTransformsToECS();    // Bullet -> ECS (dynamic)

  // Contact detection - walks manifolds/ghosts, dispatches callbacks
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

 private:
  static btVector3 ToBt(const glm::vec3& v);
  static glm::vec3 ToGlm(const btVector3& v);
  btTransform MakeBtTransform(entt::entity entity, const glm::vec3& offset);
  void WriteBtTransform(const btTransform& bt_tf, entt::entity entity,
                        const glm::vec3& offset, bool skip_rotation = false);
  btCollisionShape* CreateShapeForEntity(entt::entity entity);

  Scene* scene_;

  // Bullet core (reverse-declaration order = destruction order)
  std::unique_ptr<btGhostPairCallback> ghost_pair_callback_;
  std::unique_ptr<btDiscreteDynamicsWorld> dynamics_world_;
  std::unique_ptr<btSequentialImpulseConstraintSolver> solver_;
  std::unique_ptr<btDbvtBroadphase> broadphase_;
  std::unique_ptr<btCollisionDispatcher> dispatcher_;
  std::unique_ptr<btDefaultCollisionConfiguration> collision_config_;

  // Entity → Bullet data
  struct BodyData {
    btCollisionObject* bt_object = nullptr;
    btCollisionShape* bt_shape = nullptr;
    btDefaultMotionState* motion_state = nullptr;
    bool is_ghost = false;
  };

  std::unordered_map<entt::entity, BodyData> bodies_;

  // Contact tracking
  std::set<ContactPair> prev_solid_contacts_;
  std::set<ContactPair> prev_trigger_contacts_;
};

}  // namespace Wiesel
