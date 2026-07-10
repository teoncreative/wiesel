
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

namespace wiesel {

// Per-scene system that detects dirty networked entities and marks them
// for the ReplicationManager. Runs at priority 790: after
// PhysicsSimulation (750) but before TransformSystem (800), so
// IsChanged() is still set when we read it.
class ReplicationSyncSystem : public ISystem {
 public:
  void Update(Scene& scene, float delta_time) override;

  const char* GetName() const override { return "ReplicationSync"; }

  int GetPriority() const override { return 790; }

  bool RunOnFirstUpdate() const override { return false; }
};

}  // namespace wiesel
