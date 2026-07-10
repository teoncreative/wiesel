//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "systems/w_audio_source_system.h"

#include "audio/w_audio.h"
#include "scene/w_scene.h"
#include "w_engine.h"

namespace wiesel {

void AudioSourceSystem::Update(Scene& scene, float delta_time) {
  PROFILE_ZONE_SCOPED_N("AudioSourceSystem::Update");
  entt::registry& registry = scene.GetRegistry();
  auto& audio = Engine::audio();
  for (auto entity :
       registry.view<AudioSourceComponent, TransformComponent>()) {
    auto& src = registry.get<AudioSourceComponent>(entity);
    auto& transform = registry.get<TransformComponent>(entity);

    if (src.mute) {
      if (src.playing_handle_.IsValid()) {
        audio.Stop(src.playing_handle_);
        src.playing_handle_ = {};
      }
      continue;
    }

    // Play on start (first frame only)
    if (src.play_on_start && !src.started_ && src.clip.IsValid()) {
      src.playing_handle_ =
          audio.Play(src.clip, src.MakeParams(transform.GetPosition()));
      src.started_ = true;
    }

    // Update position for spatial sounds
    if (src.playing_handle_.IsValid() && src.spatial_blend > 0.0f) {
      audio.SetSoundPosition(src.playing_handle_, transform.GetPosition());
    }
  }
}

}  // namespace wiesel
