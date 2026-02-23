
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "physics/w_rigidbody.hpp"

#include <btBulletDynamicsCommon.h>

namespace Wiesel {

static btRigidBody* GetBody(void* ptr) {
  return static_cast<btRigidBody*>(ptr);
}

bool RigidBodyComponent::HasBody() const {
  return bt_body_ptr != nullptr;
}

glm::vec3 RigidBodyComponent::GetLinearVelocity() const {
  auto* body = GetBody(bt_body_ptr);
  if (!body) return glm::vec3(0.0f);
  const btVector3& v = body->getLinearVelocity();
  return {v.x(), v.y(), v.z()};
}

void RigidBodyComponent::SetLinearVelocity(const glm::vec3& v) {
  auto* body = GetBody(bt_body_ptr);
  if (!body) return;
  body->setLinearVelocity(btVector3(v.x, v.y, v.z));
  body->activate(true);
}

glm::vec3 RigidBodyComponent::GetAngularVelocity() const {
  auto* body = GetBody(bt_body_ptr);
  if (!body) return glm::vec3(0.0f);
  const btVector3& v = body->getAngularVelocity();
  return {v.x(), v.y(), v.z()};
}

void RigidBodyComponent::SetAngularVelocity(const glm::vec3& v) {
  auto* body = GetBody(bt_body_ptr);
  if (!body) return;
  body->setAngularVelocity(btVector3(v.x, v.y, v.z));
  body->activate(true);
}

void RigidBodyComponent::AddForce(const glm::vec3& force) {
  auto* body = GetBody(bt_body_ptr);
  if (!body) return;
  body->activate(true);
  body->applyCentralForce(btVector3(force.x, force.y, force.z));
}

void RigidBodyComponent::AddImpulse(const glm::vec3& impulse) {
  auto* body = GetBody(bt_body_ptr);
  if (!body) return;
  body->activate(true);
  body->applyCentralImpulse(btVector3(impulse.x, impulse.y, impulse.z));
}

void RigidBodyComponent::AddForceAtPosition(const glm::vec3& force,
                                             const glm::vec3& position) {
  auto* body = GetBody(bt_body_ptr);
  if (!body) return;
  body->activate(true);
  body->applyForce(btVector3(force.x, force.y, force.z),
                   btVector3(position.x, position.y, position.z));
}

void RigidBodyComponent::AddImpulseAtPosition(const glm::vec3& impulse,
                                               const glm::vec3& position) {
  auto* body = GetBody(bt_body_ptr);
  if (!body) return;
  body->activate(true);
  body->applyImpulse(btVector3(impulse.x, impulse.y, impulse.z),
                     btVector3(position.x, position.y, position.z));
}

void RigidBodyComponent::AddTorque(const glm::vec3& torque) {
  auto* body = GetBody(bt_body_ptr);
  if (!body) return;
  body->activate(true);
  body->applyTorque(btVector3(torque.x, torque.y, torque.z));
}

void RigidBodyComponent::SetFrictionRuntime(float v) {
  friction = v;
  auto* body = GetBody(bt_body_ptr);
  if (body) body->setFriction(v);
}

void RigidBodyComponent::SetRestitutionRuntime(float v) {
  restitution = v;
  auto* body = GetBody(bt_body_ptr);
  if (body) body->setRestitution(v);
}

void RigidBodyComponent::SetLinearDampingRuntime(float v) {
  linear_damping = v;
  auto* body = GetBody(bt_body_ptr);
  if (body) body->setDamping(v, angular_damping);
}

void RigidBodyComponent::SetAngularDampingRuntime(float v) {
  angular_damping = v;
  auto* body = GetBody(bt_body_ptr);
  if (body) body->setDamping(linear_damping, v);
}

void RigidBodyComponent::SetMassRuntime(float v) {
  mass = v;
  is_dirty = true;
  auto* body = GetBody(bt_body_ptr);
  if (!body) return;
  btVector3 inertia(0, 0, 0);
  if (v > 0.0f) {
    body->getCollisionShape()->calculateLocalInertia(v, inertia);
  }
  body->setMassProps(v, inertia);
  body->updateInertiaTensor();
}

}  // namespace Wiesel
