
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "physics/w_rigidbody.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/PhysicsSystem.h>

JPH_SUPPRESS_WARNINGS

namespace Wiesel {

static JPH::BodyInterface& GetBodyInterface(void* sys) {
  return static_cast<JPH::PhysicsSystem*>(sys)->GetBodyInterface();
}

static JPH::BodyID GetBodyID(uint32_t raw) {
  return JPH::BodyID(raw);
}

static JPH::Vec3 ToJolt(const glm::vec3& v) {
  return JPH::Vec3(v.x, v.y, v.z);
}

static glm::vec3 ToGlm(JPH::Vec3 v) {
  return glm::vec3(v.GetX(), v.GetY(), v.GetZ());
}

bool RigidBodyComponent::HasBody() const {
  return physics_system_ptr != nullptr && body_id_raw != ~0u;
}

glm::vec3 RigidBodyComponent::GetLinearVelocity() const {
  if (!HasBody()) {
    return glm::vec3(0.0f);
  }
  return ToGlm(GetBodyInterface(physics_system_ptr)
                   .GetLinearVelocity(GetBodyID(body_id_raw)));
}

void RigidBodyComponent::SetLinearVelocity(const glm::vec3& v) {
  if (!HasBody()) {
    return;
  }
  GetBodyInterface(physics_system_ptr)
      .SetLinearVelocity(GetBodyID(body_id_raw), ToJolt(v));
}

glm::vec3 RigidBodyComponent::GetAngularVelocity() const {
  if (!HasBody()) {
    return glm::vec3(0.0f);
  }
  return ToGlm(GetBodyInterface(physics_system_ptr)
                   .GetAngularVelocity(GetBodyID(body_id_raw)));
}

void RigidBodyComponent::SetAngularVelocity(const glm::vec3& v) {
  if (!HasBody()) {
    return;
  }
  GetBodyInterface(physics_system_ptr)
      .SetAngularVelocity(GetBodyID(body_id_raw), ToJolt(v));
}

void RigidBodyComponent::AddForce(const glm::vec3& force) {
  if (!HasBody()) {
    return;
  }
  auto& bi = GetBodyInterface(physics_system_ptr);
  bi.ActivateBody(GetBodyID(body_id_raw));
  bi.AddForce(GetBodyID(body_id_raw), ToJolt(force));
}

void RigidBodyComponent::AddImpulse(const glm::vec3& impulse) {
  if (!HasBody()) {
    return;
  }
  auto& bi = GetBodyInterface(physics_system_ptr);
  bi.ActivateBody(GetBodyID(body_id_raw));
  bi.AddImpulse(GetBodyID(body_id_raw), ToJolt(impulse));
}

void RigidBodyComponent::AddForceAtPosition(const glm::vec3& force,
                                            const glm::vec3& position) {
  if (!HasBody()) {
    return;
  }
  auto& bi = GetBodyInterface(physics_system_ptr);
  bi.ActivateBody(GetBodyID(body_id_raw));
  bi.AddForce(GetBodyID(body_id_raw), ToJolt(force),
              JPH::RVec3(position.x, position.y, position.z));
}

void RigidBodyComponent::AddImpulseAtPosition(const glm::vec3& impulse,
                                              const glm::vec3& position) {
  if (!HasBody()) {
    return;
  }
  auto& bi = GetBodyInterface(physics_system_ptr);
  bi.ActivateBody(GetBodyID(body_id_raw));
  bi.AddImpulse(GetBodyID(body_id_raw), ToJolt(impulse),
                JPH::RVec3(position.x, position.y, position.z));
}

void RigidBodyComponent::AddTorque(const glm::vec3& torque) {
  if (!HasBody()) {
    return;
  }
  auto& bi = GetBodyInterface(physics_system_ptr);
  bi.ActivateBody(GetBodyID(body_id_raw));
  bi.AddTorque(GetBodyID(body_id_raw), ToJolt(torque));
}

void RigidBodyComponent::SetFrictionRuntime(float v) {
  friction = v;
  if (HasBody()) {
    GetBodyInterface(physics_system_ptr).SetFriction(GetBodyID(body_id_raw), v);
  }
}

void RigidBodyComponent::SetRestitutionRuntime(float v) {
  restitution = v;
  if (HasBody()) {
    GetBodyInterface(physics_system_ptr)
        .SetRestitution(GetBodyID(body_id_raw), v);
  }
}

void RigidBodyComponent::SetLinearDampingRuntime(float v) {
  linear_damping = v;
  if (HasBody()) {
    auto* system = static_cast<JPH::PhysicsSystem*>(physics_system_ptr);
    JPH::BodyLockWrite lock(system->GetBodyLockInterface(),
                            GetBodyID(body_id_raw));
    if (lock.Succeeded()) {
      lock.GetBody().GetMotionProperties()->SetLinearDamping(v);
    }
  }
}

void RigidBodyComponent::SetAngularDampingRuntime(float v) {
  angular_damping = v;
  if (HasBody()) {
    auto* system = static_cast<JPH::PhysicsSystem*>(physics_system_ptr);
    JPH::BodyLockWrite lock(system->GetBodyLockInterface(),
                            GetBodyID(body_id_raw));
    if (lock.Succeeded()) {
      lock.GetBody().GetMotionProperties()->SetAngularDamping(v);
    }
  }
}

void RigidBodyComponent::SetMassRuntime(float v) {
  mass = v;
  if (HasBody() && v > 0.0f) {
    auto* system = static_cast<JPH::PhysicsSystem*>(physics_system_ptr);
    JPH::BodyLockWrite lock(system->GetBodyLockInterface(),
                            GetBodyID(body_id_raw));
    if (lock.Succeeded() && lock.GetBody().GetMotionProperties()) {
      lock.GetBody().GetMotionProperties()->SetInverseMass(1.0f / v);
    }
  }
}

}  // namespace Wiesel
