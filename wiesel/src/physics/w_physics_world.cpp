
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "physics/w_physics_world.hpp"
#include "physics/w_collider.hpp"
#include "physics/w_rigidbody.hpp"
#include "scene/w_components.hpp"
#include "scene/w_scene.hpp"
#include "behavior/w_behavior.hpp"
#include "script/mono/w_monobehavior.hpp"
#include "script/w_scriptmanager.hpp"

namespace Wiesel {

// Store entity handle in Bullet's user pointer
static void* EntityToPtr(entt::entity e) {
  return reinterpret_cast<void*>(static_cast<uintptr_t>(
      static_cast<uint32_t>(e)));
}

static entt::entity PtrToEntity(void* ptr) {
  return static_cast<entt::entity>(
      static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ptr)));
}

PhysicsWorld::PhysicsWorld(Scene* scene) : scene_(scene) {
  collision_config_ = new btDefaultCollisionConfiguration();
  dispatcher_ = new btCollisionDispatcher(collision_config_);
  broadphase_ = new btDbvtBroadphase();
  solver_ = new btSequentialImpulseConstraintSolver();
  dynamics_world_ = new btDiscreteDynamicsWorld(
      dispatcher_, broadphase_, solver_, collision_config_);
  dynamics_world_->setGravity(btVector3(0, -20.0f, 0));

  // Required for btGhostObject overlap detection
  ghost_pair_callback_ = new btGhostPairCallback();
  broadphase_->getOverlappingPairCache()->setInternalGhostPairCallback(
      ghost_pair_callback_);
}

PhysicsWorld::~PhysicsWorld() {
  // Remove all bodies
  for (auto& [entity, data] : bodies_) {
    if (data.is_ghost) {
      dynamics_world_->removeCollisionObject(data.bt_object);
    } else {
      dynamics_world_->removeRigidBody(
          static_cast<btRigidBody*>(data.bt_object));
    }
    delete data.bt_object;
    delete data.bt_shape;
    delete data.motion_state;
  }
  bodies_.clear();

  delete dynamics_world_;
  delete solver_;
  delete broadphase_;
  delete dispatcher_;
  delete collision_config_;
  delete ghost_pair_callback_;
}

btVector3 PhysicsWorld::ToBt(const glm::vec3& v) {
  return btVector3(v.x, v.y, v.z);
}

glm::vec3 PhysicsWorld::ToGlm(const btVector3& v) {
  return glm::vec3(v.x(), v.y(), v.z());
}

btTransform PhysicsWorld::MakeBtTransform(entt::entity entity,
                                          const glm::vec3& offset) {
  auto& tc = scene_->GetComponent<TransformComponent>(entity);
  btTransform tf;
  tf.setIdentity();
  tf.setOrigin(ToBt(tc.position + offset));
  glm::quat q = glm::quat(glm::radians(tc.rotation));
  tf.setRotation(btQuaternion(q.x, q.y, q.z, q.w));
  return tf;
}

void PhysicsWorld::WriteBtTransform(const btTransform& bt_tf,
                                    entt::entity entity,
                                    const glm::vec3& offset,
                                    bool skip_rotation) {
  auto& tc = scene_->GetComponent<TransformComponent>(entity);
  btVector3 pos = bt_tf.getOrigin();
  tc.position = ToGlm(pos) - offset;

  if (!skip_rotation) {
    btQuaternion rot = bt_tf.getRotation();
    glm::quat q(rot.w(), rot.x(), rot.y(), rot.z());
    tc.rotation = glm::degrees(glm::eulerAngles(q));
  }
  tc.is_changed = true;
}

btCollisionShape* PhysicsWorld::CreateShapeForEntity(entt::entity entity) {
  auto& registry = scene_->GetRegistry();
  if (registry.any_of<BoxColliderComponent>(entity)) {
    auto& box = registry.get<BoxColliderComponent>(entity);
    return new btBoxShape(ToBt(box.half_extents));
  }
  if (registry.any_of<SphereColliderComponent>(entity)) {
    auto& sphere = registry.get<SphereColliderComponent>(entity);
    return new btSphereShape(sphere.radius);
  }
  return nullptr;
}

void PhysicsWorld::CreateBody(entt::entity entity) {
  if (bodies_.count(entity)) return;

  auto& registry = scene_->GetRegistry();
  btCollisionShape* shape = CreateShapeForEntity(entity);
  if (!shape) return;

  // Determine collider offset
  glm::vec3 offset(0.0f);
  if (registry.any_of<BoxColliderComponent>(entity))
    offset = registry.get<BoxColliderComponent>(entity).offset;
  else if (registry.any_of<SphereColliderComponent>(entity))
    offset = registry.get<SphereColliderComponent>(entity).offset;

  // Check if this is a trigger (ghost object)
  bool is_trigger = false;
  if (registry.any_of<BoxColliderComponent>(entity))
    is_trigger = registry.get<BoxColliderComponent>(entity).is_trigger;
  else if (registry.any_of<SphereColliderComponent>(entity))
    is_trigger = registry.get<SphereColliderComponent>(entity).is_trigger;

  btTransform start_tf = MakeBtTransform(entity, offset);

  if (is_trigger) {
    auto* ghost = new btGhostObject();
    ghost->setCollisionShape(shape);
    ghost->setWorldTransform(start_tf);
    ghost->setCollisionFlags(ghost->getCollisionFlags() |
                             btCollisionObject::CF_NO_CONTACT_RESPONSE);
    ghost->setUserPointer(EntityToPtr(entity));
    dynamics_world_->addCollisionObject(
        ghost, btBroadphaseProxy::SensorTrigger,
        btBroadphaseProxy::AllFilter);
    bodies_[entity] = {ghost, shape, nullptr, true};
    return;
  }

  // Solid body
  RigidBodyType type = RigidBodyType::Static;
  float mass = 0.0f;
  float friction = 0.5f;
  float restitution = 0.0f;
  float linear_damping = 0.0f;
  float angular_damping = 0.05f;

  if (registry.any_of<RigidBodyComponent>(entity)) {
    auto& rb = registry.get<RigidBodyComponent>(entity);
    type = rb.type;
    mass = (type == RigidBodyType::Dynamic) ? rb.mass : 0.0f;
    friction = rb.friction;
    restitution = rb.restitution;
    linear_damping = rb.linear_damping;
    angular_damping = rb.angular_damping;
  }

  btVector3 inertia(0, 0, 0);
  if (mass > 0.0f) {
    shape->calculateLocalInertia(mass, inertia);
  }

  auto* motion_state = new btDefaultMotionState(start_tf);
  btRigidBody::btRigidBodyConstructionInfo info(mass, motion_state, shape,
                                                inertia);
  info.m_friction = friction;
  info.m_restitution = restitution;
  info.m_linearDamping = linear_damping;
  info.m_angularDamping = angular_damping;

  auto* body = new btRigidBody(info);
  body->setUserPointer(EntityToPtr(entity));

  if (type == RigidBodyType::Kinematic) {
    body->setCollisionFlags(body->getCollisionFlags() |
                            btCollisionObject::CF_KINEMATIC_OBJECT);
    body->setActivationState(DISABLE_DEACTIVATION);
  } else if (type == RigidBodyType::Static) {
    body->setCollisionFlags(body->getCollisionFlags() |
                            btCollisionObject::CF_STATIC_OBJECT);
  } else {
    // Dynamic - prevent deactivation so scripts can always read velocity
    body->setActivationState(DISABLE_DEACTIVATION);
  }

  // Rotation locks
  if (registry.any_of<RigidBodyComponent>(entity)) {
    auto& rb = registry.get<RigidBodyComponent>(entity);
    btVector3 angular_factor(
        rb.lock_rotation_x ? 0.0f : 1.0f,
        rb.lock_rotation_y ? 0.0f : 1.0f,
        rb.lock_rotation_z ? 0.0f : 1.0f);
    body->setAngularFactor(angular_factor);
    rb.bt_body_ptr = body;
    rb.is_dirty = false;
  }

  dynamics_world_->addRigidBody(body);
  bodies_[entity] = {body, shape, motion_state, false};
}

void PhysicsWorld::DestroyBody(entt::entity entity) {
  auto it = bodies_.find(entity);
  if (it == bodies_.end()) return;

  auto& data = it->second;
  if (data.is_ghost) {
    dynamics_world_->removeCollisionObject(data.bt_object);
  } else {
    dynamics_world_->removeRigidBody(
        static_cast<btRigidBody*>(data.bt_object));
  }
  delete data.bt_object;
  delete data.bt_shape;
  delete data.motion_state;
  bodies_.erase(it);

  // Clean up contact tracking
  std::erase_if(prev_solid_contacts_, [entity](const ContactPair& p) {
    return p.a == entity || p.b == entity;
  });
  std::erase_if(prev_trigger_contacts_, [entity](const ContactPair& p) {
    return p.a == entity || p.b == entity;
  });
}

void PhysicsWorld::EnsureBodiesExist() {
  auto& registry = scene_->GetRegistry();

  // Entities with box collider
  for (auto handle :
       registry.view<TransformComponent, BoxColliderComponent>()) {
    if (!bodies_.count(handle)) {
      CreateBody(handle);
    }
  }
  // Entities with sphere collider
  for (auto handle :
       registry.view<TransformComponent, SphereColliderComponent>()) {
    if (!bodies_.count(handle)) {
      CreateBody(handle);
    }
  }
}

void PhysicsWorld::StepSimulation(float delta_time) {
  dynamics_world_->stepSimulation(delta_time, 4, 1.0f / 60.0f);
}

void PhysicsWorld::SyncTransformsFromECS() {
  auto& registry = scene_->GetRegistry();
  for (auto& [entity, data] : bodies_) {
    if (!registry.valid(entity)) continue;

    glm::vec3 offset(0.0f);
    if (registry.any_of<BoxColliderComponent>(entity))
      offset = registry.get<BoxColliderComponent>(entity).offset;
    else if (registry.any_of<SphereColliderComponent>(entity))
      offset = registry.get<SphereColliderComponent>(entity).offset;

    btTransform tf = MakeBtTransform(entity, offset);

    if (data.is_ghost) {
      data.bt_object->setWorldTransform(tf);
      continue;
    }

    auto* body = static_cast<btRigidBody*>(data.bt_object);
    if (data.motion_state)
      body->getMotionState()->setWorldTransform(tf);
    body->setWorldTransform(tf);
    body->activate(true);
  }
}

void PhysicsWorld::SyncTransformsToECS() {
  auto& registry = scene_->GetRegistry();
  for (auto& [entity, data] : bodies_) {
    if (data.is_ghost) continue;
    if (!registry.valid(entity)) continue;
    if (!registry.any_of<RigidBodyComponent>(entity)) continue;

    auto& rb = registry.get<RigidBodyComponent>(entity);
    if (rb.type != RigidBodyType::Dynamic) continue;

    glm::vec3 offset(0.0f);
    if (registry.any_of<BoxColliderComponent>(entity))
      offset = registry.get<BoxColliderComponent>(entity).offset;
    else if (registry.any_of<SphereColliderComponent>(entity))
      offset = registry.get<SphereColliderComponent>(entity).offset;

    btTransform tf;
    static_cast<btRigidBody*>(data.bt_object)
        ->getMotionState()
        ->getWorldTransform(tf);

    bool skip_rotation =
        rb.lock_rotation_x && rb.lock_rotation_y && rb.lock_rotation_z;
    WriteBtTransform(tf, entity, offset, skip_rotation);
  }
}

void PhysicsWorld::DetectContacts() {
  auto& registry = scene_->GetRegistry();

  // Helper to invoke callbacks on all MonoBehaviors of an entity
  auto invoke = [&](entt::entity entity, entt::entity other,
                    void (ScriptInstance::*method)(entt::entity)) {
    if (!registry.valid(entity)) return;
    if (!registry.any_of<BehaviorsComponent>(entity)) return;
    auto& behaviors = registry.get<BehaviorsComponent>(entity);
    for (auto& [name, behavior] : behaviors.behaviors_) {
      auto* mono = dynamic_cast<MonoBehavior*>(behavior);
      if (!mono || !mono->script_instance()) continue;
      (mono->script_instance()->*method)(other);
    }
  };

  // Solid contacts from collision manifolds
  std::set<ContactPair> current_solid;
  int num_manifolds = dispatcher_->getNumManifolds();
  for (int i = 0; i < num_manifolds; i++) {
    btPersistentManifold* manifold = dispatcher_->getManifoldByIndexInternal(i);
    if (manifold->getNumContacts() == 0) continue;

    const btCollisionObject* objA = manifold->getBody0();
    const btCollisionObject* objB = manifold->getBody1();
    // Skip if either is a ghost (handled separately as trigger)
    if (objA->getInternalType() == btCollisionObject::CO_GHOST_OBJECT ||
        objB->getInternalType() == btCollisionObject::CO_GHOST_OBJECT)
      continue;

    entt::entity eA = PtrToEntity(objA->getUserPointer());
    entt::entity eB = PtrToEntity(objB->getUserPointer());
    current_solid.insert(ContactPair(eA, eB));
  }

  // Diff: solid contacts Enter/Stay/Exit
  for (auto& pair : current_solid) {
    if (!prev_solid_contacts_.contains(pair)) {
      invoke(pair.a, pair.b, &ScriptInstance::OnCollisionEnter);
      invoke(pair.b, pair.a, &ScriptInstance::OnCollisionEnter);
    }
  }
  for (auto& pair : prev_solid_contacts_) {
    if (!current_solid.contains(pair)) {
      invoke(pair.a, pair.b, &ScriptInstance::OnCollisionExit);
      invoke(pair.b, pair.a, &ScriptInstance::OnCollisionExit);
    }
  }
  prev_solid_contacts_ = std::move(current_solid);

  // Trigger contacts from ghost objects
  std::set<ContactPair> current_trigger;
  for (auto& [entity, data] : bodies_) {
    if (!data.is_ghost) continue;
    auto* ghost = static_cast<btGhostObject*>(data.bt_object);
    for (int i = 0; i < ghost->getNumOverlappingObjects(); i++) {
      btCollisionObject* other_obj = ghost->getOverlappingObject(i);
      entt::entity other_entity = PtrToEntity(other_obj->getUserPointer());
      if (entity == other_entity) continue;
      current_trigger.insert(ContactPair(entity, other_entity));
    }
  }

  // Diff: trigger contacts Enter/Stay/Exit
  for (auto& pair : current_trigger) {
    if (!prev_trigger_contacts_.contains(pair)) {
      invoke(pair.a, pair.b, &ScriptInstance::OnTriggerEnter);
      invoke(pair.b, pair.a, &ScriptInstance::OnTriggerEnter);
    }
  }
  for (auto& pair : prev_trigger_contacts_) {
    if (!current_trigger.contains(pair)) {
      invoke(pair.a, pair.b, &ScriptInstance::OnTriggerExit);
      invoke(pair.b, pair.a, &ScriptInstance::OnTriggerExit);
    }
  }
  prev_trigger_contacts_ = std::move(current_trigger);
}

bool PhysicsWorld::Raycast(const glm::vec3& from, const glm::vec3& to,
                           RaycastHit& hit, entt::entity ignore) const {
  btVector3 bt_from = ToBt(from);
  btVector3 bt_to = ToBt(to);

  // Custom callback that skips a specific collision object
  struct ClosestNotSelf : public btCollisionWorld::ClosestRayResultCallback {
    const btCollisionObject* ignore_obj;
    ClosestNotSelf(const btVector3& f, const btVector3& t,
                   const btCollisionObject* ig)
        : ClosestRayResultCallback(f, t), ignore_obj(ig) {}
    btScalar addSingleResult(btCollisionWorld::LocalRayResult& result,
                             bool normalInWorldSpace) override {
      if (result.m_collisionObject == ignore_obj) return 1.0f;
      return ClosestRayResultCallback::addSingleResult(result,
                                                       normalInWorldSpace);
    }
  };

  const btCollisionObject* ignore_obj = nullptr;
  if (ignore != entt::null) {
    auto it = bodies_.find(ignore);
    if (it != bodies_.end()) ignore_obj = it->second.bt_object;
  }

  ClosestNotSelf callback(bt_from, bt_to, ignore_obj);
  dynamics_world_->rayTest(bt_from, bt_to, callback);

  if (!callback.hasHit()) return false;

  hit.entity = PtrToEntity(
      callback.m_collisionObject->getUserPointer());
  hit.point = ToGlm(callback.m_hitPointWorld);
  hit.normal = ToGlm(callback.m_hitNormalWorld);
  hit.distance = (callback.m_hitPointWorld - bt_from).length();
  return true;
}

std::vector<entt::entity> PhysicsWorld::OverlapBox(
    const glm::vec3& center, const glm::vec3& half_extents) const {
  btBoxShape query_shape(ToBt(half_extents));
  btTransform query_tf;
  query_tf.setIdentity();
  query_tf.setOrigin(ToBt(center));

  btGhostObject query_ghost;
  query_ghost.setCollisionShape(&query_shape);
  query_ghost.setWorldTransform(query_tf);
  query_ghost.setCollisionFlags(btCollisionObject::CF_NO_CONTACT_RESPONSE);

  // Use contactTest to find all overlapping objects
  struct ResultCallback : public btCollisionWorld::ContactResultCallback {
    std::vector<entt::entity> results;
    btScalar addSingleResult(btManifoldPoint&,
                             const btCollisionObjectWrapper* colObj0Wrap, int,
                             int,
                             const btCollisionObjectWrapper* colObj1Wrap, int,
                             int) override {
      const btCollisionObject* other = colObj1Wrap->getCollisionObject();
      entt::entity e = PtrToEntity(other->getUserPointer());
      results.push_back(e);
      return 0;
    }
  } callback;

  dynamics_world_->contactTest(&query_ghost, callback);
  return callback.results;
}

std::vector<entt::entity> PhysicsWorld::OverlapSphere(
    const glm::vec3& center, float radius) const {
  btSphereShape query_shape(radius);
  btTransform query_tf;
  query_tf.setIdentity();
  query_tf.setOrigin(ToBt(center));

  btGhostObject query_ghost;
  query_ghost.setCollisionShape(&query_shape);
  query_ghost.setWorldTransform(query_tf);
  query_ghost.setCollisionFlags(btCollisionObject::CF_NO_CONTACT_RESPONSE);

  struct ResultCallback : public btCollisionWorld::ContactResultCallback {
    std::vector<entt::entity> results;
    btScalar addSingleResult(btManifoldPoint&,
                             const btCollisionObjectWrapper* colObj0Wrap, int,
                             int,
                             const btCollisionObjectWrapper* colObj1Wrap, int,
                             int) override {
      const btCollisionObject* other = colObj1Wrap->getCollisionObject();
      entt::entity e = PtrToEntity(other->getUserPointer());
      results.push_back(e);
      return 0;
    }
  } callback;

  dynamics_world_->contactTest(&query_ghost, callback);
  return callback.results;
}

void PhysicsWorld::SetGravity(const glm::vec3& gravity) {
  dynamics_world_->setGravity(ToBt(gravity));
}

glm::vec3 PhysicsWorld::GetGravity() const {
  return ToGlm(dynamics_world_->getGravity());
}

}  // namespace Wiesel
