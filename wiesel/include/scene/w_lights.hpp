
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
      : Color({1.0f, 1.0f, 1.0f}),
        Ambient(0.20f),
        Diffuse(1.0f),
        Specular(0.85f),
        Density(1.0f) {}

  LightBase(glm::vec3 color, float density, float ambient)
      : Color(color), Density(density), Ambient(ambient) {}

  ~LightBase() = default;

  alignas(16) glm::vec3 Position;
  alignas(16) glm::vec3 Color;
  float Ambient;
  float Diffuse;
  float Specular;
  float Density;
};

struct alignas(16) LightDirect {
  LightDirect() : Base({}) {}

  LightDirect(LightBase base)
      : Base(base) {}

  ~LightDirect() = default;

  alignas(16) glm::vec3 Direction;
  LightBase Base;
};

struct alignas(16) LightPoint {
  LightPoint()
      : Base({}),
        Constant(1.0f),
        Linear(0.09f),
        Exp(0.032f) {}

  LightPoint(glm::vec3 position, LightBase base, float constant, float linear,
             float exp)
      : Base(base),
        Constant(constant),
        Linear(linear),
        Exp(exp) {}

  ~LightPoint() = default;

  LightBase Base;
  float Constant;
  float Linear;
  float Exp;
};

static const int MAX_LIGHTS = 16;

struct alignas(16) LightsUniformData {
  LightsUniformData() : DirectLightCount(0), PointLightCount(0){};

  uint32_t DirectLightCount;
  uint32_t PointLightCount;
  LightDirect DirectLights[MAX_LIGHTS];
  LightPoint PointLights[MAX_LIGHTS];
};

void UpdateLight(LightsUniformData& lights, const LightDirect& light,
                 const TransformComponent& transform);

void UpdateLight(LightsUniformData& lights, const LightPoint& light,
                 const TransformComponent& transform);

struct LightDirectComponent {
  LightDirectComponent() = default;
  LightDirectComponent(const LightDirectComponent&) = default;

  LightDirect LightData;
};

struct LightPointComponent {
  LightPointComponent() = default;
  LightPointComponent(const LightPointComponent&) = default;

  LightPoint LightData;
};

}  // namespace Wiesel