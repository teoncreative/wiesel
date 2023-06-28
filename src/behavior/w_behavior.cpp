
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

namespace Wiesel {

  void IBehavior::OnUpdate(float_t deltaTime) {
  }

  void IBehavior::OnEvent(Event& event) {
  }

  void IBehavior::SetEnabled(bool enabled) {
    if (m_Unset) return;
    m_Enabled = enabled;
  }

  void BehaviorsComponent::OnEvent(Wiesel::Event& event) {
    for (const auto& entry : m_Behaviors) {
      if (!entry.second->IsEnabled()) {
        continue;
      }
      entry.second->OnEvent(event);
    }
  }

}// namespace Wiesel