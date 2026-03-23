
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

#include "scene/w_components.hpp"

namespace Wiesel {

// Marker component that enables pointer event detection on a canvas entity.
// The entity must also have a RectangleTransformComponent for hit testing.
struct InteractableComponent : public IComponent {
  bool enabled = true;
  bool blocks_raycast =
      true;  // if true, consumes the event and stops propagation

  // Runtime state (not serialized)
  bool hovered_ = false;
  bool pressed_ = false;
};

}  // namespace Wiesel