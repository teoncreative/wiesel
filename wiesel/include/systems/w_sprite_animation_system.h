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

class SpriteAnimationSystem : public ISystem {
 public:
  void Update(Scene& scene, float delta_time) override;

  const char* GetName() const override { return "SpriteAnimation"; }

  int GetPriority() const override { return 1000; }

  bool RunOnFirstUpdate() const override { return true; }
};

}  // namespace Wiesel
