
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include <entt/entt.hpp>
#include "scene/w_scene.hpp"
#include "ui/w_canvas.hpp"

namespace Wiesel {
class CanvasSystem {
 public:
  void Update(Scene& scene);
  void Render(Scene& scene);
  void OnEvent(Event& event);
};
}  // namespace Wiesel