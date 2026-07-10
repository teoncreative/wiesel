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

#include "w_pch.h"

namespace wiesel {

class Scene;

// Base interface for all ECS systems.
// Systems are registered on Scene and executed in priority order each frame.
class ISystem {
 public:
  virtual ~ISystem() = default;

  // Called every frame. Implement game logic here.
  virtual void Update(Scene& scene, float delta_time) = 0;

  // Human-readable name for profiling and debugging.
  virtual const char* GetName() const = 0;

  // Execution order. Lower priority runs first.
  virtual int GetPriority() const { return 0; }

  // Whether this system should run on the first frame.
  // Most gameplay systems skip the first frame to avoid initialization artifacts.
  virtual bool RunOnFirstUpdate() const { return true; }

  // Whether this system runs in editor mode (outside Play).
  // Default false - only infrastructure systems (transforms, skinned mesh,
  // camera, lights) should return true.
  virtual bool RunInEditor() const { return false; }
};

}  // namespace wiesel
