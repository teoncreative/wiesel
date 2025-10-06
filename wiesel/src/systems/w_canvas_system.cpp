//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "systems/w_canvas_system.hpp"
#include "scene/w_entity.hpp"

namespace Wiesel {
void CanvasSystem::Update(Scene& scene) {}

void CanvasSystem::Render(Scene& scene) {
  for (const auto& handle : scene.GetAllEntitiesWith<TextComponent>()) {
    Entity entity{handle, &scene};
    Entity parent = entity.GetParent();
    CanvasComponent* canvas;
    // Loop through parents
    while (true) {
      if (parent.HasComponent<CanvasComponent>()) {
        canvas = &parent.GetComponent<CanvasComponent>();
      }
      if (Entity parent_parent = parent.GetParent()) {
        parent = parent_parent;
      } else {
        break;
      }
    }
    if (!canvas) {
      continue;  // No rendering for this component :(
    }
    // Unlike entity transforms, canvas objects should have BoxTransformComponent and objects are offseted by their parent locations.
    if (!entity.HasComponent<RectangleTransformComponent>()) {
      continue;
    }
    RectangleTransformComponent transform =
        entity.GetComponent<RectangleTransformComponent>();
    // todo
  }
}

void CanvasSystem::OnEvent(Wiesel::Event& event) {}
}  // namespace Wiesel