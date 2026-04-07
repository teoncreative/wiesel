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

#include "systems/w_system.h"

namespace Wiesel {

// Creates physics bodies for new entities. Runs before behaviors
// so scripts can interact with newly added colliders.
class PhysicsBodySystem : public ISystem {
 public:
  void Update(Scene& scene, float delta_time) override;

  const char* GetName() const override { return "PhysicsBody"; }

  int GetPriority() const override { return 200; }

  bool RunOnFirstUpdate() const override { return false; }
};

// Runs the physics simulation. Must run AFTER behaviors so that
// scripts can apply forces/velocities before the step.
class PhysicsSimulationSystem : public ISystem {
 public:
  void Update(Scene& scene, float delta_time) override;

  const char* GetName() const override { return "PhysicsSimulation"; }

  int GetPriority() const override { return 750; }

  bool RunOnFirstUpdate() const override { return false; }
};

}  // namespace Wiesel
