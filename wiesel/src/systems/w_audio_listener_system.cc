//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "systems/w_audio_listener_system.h"

#include "audio/w_audio.h"
#include "scene/w_scene.h"
#include "w_engine.h"

namespace wiesel {

void AudioListenerSystem::Update(Scene& scene, float delta_time) {
  PROFILE_ZONE_SCOPED_N("AudioListenerSystem::Update");
  entt::registry& registry = scene.GetRegistry();
  auto& audio = Engine::audio();
  for (auto entity : registry.view<CameraComponent, TransformComponent>()) {
    auto& cam = registry.get<CameraComponent>(entity);
    if (!cam.enabled) {
      continue;
    }
    auto& transform = registry.get<TransformComponent>(entity);
    audio.SetListenerPosition(transform.GetPosition());
    glm::vec3 forward = glm::vec3(cam.inv_view_matrix[2]);
    glm::vec3 up = glm::vec3(cam.inv_view_matrix[1]);
    audio.SetListenerDirection(glm::normalize(forward), glm::normalize(up));

    // Check reverb zones against listener position
    bool in_reverb = false;
    for (auto zone_entity :
         registry.view<ReverbZoneComponent, TransformComponent>()) {
      auto& zone = registry.get<ReverbZoneComponent>(zone_entity);
      auto& zone_transform = registry.get<TransformComponent>(zone_entity);
      float dist =
          glm::distance(transform.GetPosition(), zone_transform.GetPosition());
      if (dist < zone.radius) {
        // Blend wet amount based on how deep inside the zone we are
        float blend = 1.0f - dist / zone.radius;
        audio.SetReverb(zone.delay_ms, zone.decay, zone.wet * blend);
        zone.active_ = true;
        in_reverb = true;
        break;  // use the closest/first zone
      } else {
        zone.active_ = false;
      }
    }
    if (!in_reverb) {
      audio.ClearReverb();
    }

    break;
  }
}

}  // namespace wiesel
