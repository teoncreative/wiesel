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

// Ensures SkeletalAnimRuntime exists and is initialized for all entities
// that have SkinnedMeshRendererComponent pointing to a skeleton root.
// Runs before rendering so render features never need to create components.
class SkinnedMeshSystem : public ISystem {
 public:
  void Update(Scene& scene, float delta_time) override;

  const char* GetName() const override { return "SkinnedMesh"; }

  int GetPriority() const override { return 1040; }  // before Animation (1050)

  bool RunOnFirstUpdate() const override { return true; }

  bool RunInEditor() const override { return true; }
};

}  // namespace wiesel
