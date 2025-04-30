//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "scene/w_lights.hpp"

namespace Wiesel {

glm::vec3 EulerToDirection(glm::vec3 euler) {
  float pitch = glm::radians(euler.x);
  float yaw = glm::radians(euler.y);

  return glm::normalize(
      glm::vec3(cos(pitch) * sin(yaw), sin(pitch), cos(pitch) * cos(yaw)));
}

void UpdateLight(LightsUniformData& lights, const LightDirect& light,
                 const TransformComponent& transform) {
  // world position is the 4th column
  glm::vec3 worldPos = glm::vec3(transform.TransformMatrix[3]);

  glm::vec3 worldDir = glm::normalize(
      glm::vec3(transform.TransformMatrix * glm::vec4(0,0,-1,0)));

  LightDirect& dst = lights.DirectLights[lights.DirectLightCount++];
  dst.Direction                    = worldDir;
  dst.Base.Position                = worldPos;
  dst.Base.Color                   = light.Base.Color;
  dst.Base.Ambient                 = light.Base.Ambient;
  dst.Base.Diffuse                 = light.Base.Diffuse;
  dst.Base.Specular                = light.Base.Specular;
  dst.Base.Density                 = light.Base.Density;
}

void UpdateLight(LightsUniformData& lights, const LightPoint& light,
                 const TransformComponent& transform) {
  glm::vec3 worldPos = glm::vec3(transform.TransformMatrix[3]);

  LightPoint& dst = lights.PointLights[lights.PointLightCount++];
  dst.Base.Position = worldPos;
  dst.Base.Color    = light.Base.Color;
  dst.Base.Ambient  = light.Base.Ambient;
  dst.Base.Diffuse  = light.Base.Diffuse;
  dst.Base.Specular = light.Base.Specular;
  dst.Base.Density  = light.Base.Density;
  dst.Constant      = light.Constant;
  dst.Linear        = light.Linear;
  dst.Exp           = light.Exp;
}

}  // namespace Wiesel