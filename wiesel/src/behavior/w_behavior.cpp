
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "behavior/w_behavior.hpp"
#include <filesystem>


namespace Wiesel {

void IBehavior::OnUpdate(float_t delta_time) {}

void IBehavior::OnEvent(Event& event) {}

void IBehavior::SetEnabled(bool enabled) {
  if (unset_)
    return;
  enabled_ = enabled;
}

void BehaviorsComponent::OnEvent(Wiesel::Event& event) {
  for (const auto& entry : behaviors_) {
    entry.second->OnEvent(event);
  }
}

}  // namespace Wiesel