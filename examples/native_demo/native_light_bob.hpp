
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

#include "behavior/w_native_behavior.hpp"
#include "scene/w_components.hpp"

namespace NativeDemo {

class NativeLightBob : public Wiesel::NativeBehavior {
 public:
  NativeLightBob(Wiesel::Entity entity, const std::string& name)
      : NativeBehavior(entity, name) {}

  void OnStart() override {
    auto& t = GetComponent<Wiesel::TransformComponent>();
    start_x_ = t.position.x;
    start_y_ = t.position.y;
    start_z_ = t.position.z;
    phase_offset_ = start_x_ * 0.9f;
  }

  void OnTick(float_t delta_time) override {
    time_ += delta_time;
    float t = time_ * speed_ + phase_offset_;

    float x = 5.0f + std::sin(t * 0.7f) * 4.0f;
    float y = start_y_ + 0.5f + std::sin(t * 0.5f + 1.2f) * 0.5f;
    float z = start_z_ + std::sin(t * 0.4f + 2.5f) * 1.5f;

    auto& transform = GetComponent<Wiesel::TransformComponent>();
    transform.position = {x, y, z};
    transform.is_changed = true;
  }

 private:
  float start_x_ = 0.0f;
  float start_y_ = 0.0f;
  float start_z_ = 0.0f;
  float time_ = 0.0f;
  float speed_ = 0.6f;
  float phase_offset_ = 0.0f;
};

}  // namespace LeapLand