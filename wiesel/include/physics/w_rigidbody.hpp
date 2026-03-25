
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

#include "scene/w_components.hpp"

namespace Wiesel {

enum class RigidBodyType : uint8_t { Static = 0, Kinematic = 1, Dynamic = 2 };

struct RigidBodyComponent : public IComponent {
  RigidBodyComponent() = default;
  RigidBodyComponent(const RigidBodyComponent&) = default;

  RigidBodyType type = RigidBodyType::Static;
  float mass = 1.0f;
  float friction = 0.5f;
  float restitution = 0.0f;
  float linear_damping = 0.0f;
  float angular_damping = 0.05f;
  bool lock_position_x = false;
  bool lock_position_y = false;
  bool lock_position_z = false;
  bool lock_rotation_x = false;
  bool lock_rotation_y = false;
  bool lock_rotation_z = false;
  bool is_dirty = true;

  // Runtime only - set by PhysicsWorld after body creation
  void* bt_body_ptr = nullptr;

  // Runtime API - reads/writes Bullet state through bt_body_ptr.
  // Returns zero vectors when no body exists yet.
  bool HasBody() const;

  glm::vec3 GetLinearVelocity() const;
  void SetLinearVelocity(const glm::vec3& v);

  glm::vec3 GetAngularVelocity() const;
  void SetAngularVelocity(const glm::vec3& v);

  void AddForce(const glm::vec3& force);
  void AddImpulse(const glm::vec3& impulse);
  void AddForceAtPosition(const glm::vec3& force, const glm::vec3& position);
  void AddImpulseAtPosition(const glm::vec3& impulse,
                            const glm::vec3& position);
  void AddTorque(const glm::vec3& torque);

  // Setters that update both the component field AND the live Bullet body
  void SetFrictionRuntime(float v);
  void SetRestitutionRuntime(float v);
  void SetLinearDampingRuntime(float v);
  void SetAngularDampingRuntime(float v);
  void SetMassRuntime(float v);
};

}  // namespace Wiesel
