
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

#include <entt/entt.hpp>
#include "scene/w_scene.h"
#include "ui/w_canvas.h"

namespace wiesel {
class CanvasSystem {
 public:
  void Update(Scene& scene, glm::vec2 screen_size);
  void OnEvent(Event& event);

 private:
  static glm::vec2 ComputeAnchorOrigin(AnchorPreset anchor,
                                       glm::vec2 parent_size);
  void LayoutChildren(Scene& scene, entt::entity parent, glm::vec2 parent_pos,
                      glm::vec2 parent_size, const CanvasComponent* canvas,
                      int32_t& draw_order);
};
}  // namespace wiesel
