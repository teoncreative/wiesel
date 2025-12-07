
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

#include "scene/w_entity.hpp"
#include "util/w_color.hpp"
#include "w_pch.hpp"

namespace Wiesel {

struct alignas(16) LightBase {
  LightBase()
      : color({1.0f, 1.0f, 1.0f}),
        ambient(0.20f),
        diffuse(1.0f),
        specular(0.85f),
        density(1.0f) {}

  LightBase(glm::vec3 color, float density, float ambient)
      : color(color), density(density), ambient(ambient) {}

  ~LightBase() = default;

  alignas(16) glm::vec3 position;
  alignas(16) glm::vec3 color;
  float ambient;
  float diffuse;
  float specular;
  float density;
};

struct alignas(16) LightDirect {
  LightDirect() : base({}) {}

  LightDirect(LightBase base)
      : base(base) {}

  ~LightDirect() = default;

  alignas(16) glm::vec3 direction;
  LightBase base;
};

struct alignas(16) LightPoint {
  LightPoint()
      : base({}),
        constant(1.0f),
        linear(0.09f),
        exp(0.032f) {}

  LightPoint(glm::vec3 position, LightBase base, float constant, float linear,
             float exp)
      : base(base),
        constant(constant),
        linear(linear),
        exp(exp) {}

  ~LightPoint() = default;

  LightBase base;
  float constant;
  float linear;
  float exp;
};

static const int MAX_LIGHTS = 16;

struct alignas(16) LightsUniformData {
  LightsUniformData() : direct_light_count(0), point_light_count(0){};

  uint32_t direct_light_count;
  uint32_t point_light_count;
  LightDirect direct_lights[MAX_LIGHTS];
  LightPoint point_lights[MAX_LIGHTS];
};

void UpdateLight(LightsUniformData& lights, const LightDirect& light,
                 const TransformComponent& transform);

void UpdateLight(LightsUniformData& lights, const LightPoint& light,
                 const TransformComponent& transform);

struct LightDirectComponent {
  LightDirectComponent() = default;
  LightDirectComponent(const LightDirectComponent&) = default;

  LightDirect light_data;
};

struct LightPointComponent {
  LightPointComponent() = default;
  LightPointComponent(const LightPointComponent&) = default;

  LightPoint light_data;
};

}  // namespace Wiesel