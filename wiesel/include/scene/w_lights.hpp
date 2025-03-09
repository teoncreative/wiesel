
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

  alignas(16) glm::vec3 Color;
  alignas(4) float Ambient;
  alignas(4) float Diffuse;
  alignas(4) float Specular;
  alignas(4) float Density;
};

struct alignas(16) LightDirect {
  LightDirect() : Direction({1.0f, 1.0f, 1.0f}) {}

  LightDirect(glm::vec3 direction, LightBase base)
      : Direction(direction), Base(base) {}

  ~LightDirect() = default;

  alignas(16) glm::vec3 Direction;
  LightBase Base;
};

struct alignas(16) LightPoint {
  LightPoint()
      : Position({0.0f, 1.0f, 0.0f}),
        Constant(1.0f),
        Linear(0.09f),
        Exp(0.032f) {}

  LightPoint(glm::vec3 position, LightBase base, float constant, float linear,
             float exp)
      : Position(position),
        Base(base),
        Constant(constant),
        Linear(linear),
        Exp(exp) {}

  ~LightPoint() = default;

  alignas(16) glm::vec3 Position;
  LightBase Base;
  alignas(4) float Constant;
  alignas(4) float Linear;
  alignas(4) float Exp;
};

static const int MAX_LIGHTS = 16;

struct alignas(16) LightsUniformData {
  LightsUniformData() : DirectLightCount(0), PointLightCount(0){};

  uint32_t DirectLightCount;
  uint32_t PointLightCount;
  LightDirect DirectLights[MAX_LIGHTS];
  LightPoint PointLights[MAX_LIGHTS];
};


void UpdateLight(LightsUniformData& lights, LightDirect light,
                 TransformComponent& transform);

void UpdateLight(LightsUniformData& lights, LightPoint light,
                 TransformComponent& transform);

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