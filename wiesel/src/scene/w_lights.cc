//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "scene/w_lights.h"

namespace wiesel {

glm::vec3 EulerToDirection(glm::vec3 euler) {
  float pitch = glm::radians(euler.x);
  float yaw = glm::radians(euler.y);

  return glm::normalize(
      glm::vec3(cos(pitch) * sin(yaw), sin(pitch), cos(pitch) * cos(yaw)));
}

void UpdateLight(LightsUniformData& lights, const LightDirect& light,
                 const TransformComponent& transform) {
  // world position is the 4th column
  glm::vec3 worldPos = transform.GetWorldPosition();

  glm::vec3 worldDir = transform.LocalToWorldDirection({0, 0, -1});

  LightDirect& dst = lights.direct_lights[lights.direct_light_count++];
  dst.direction = worldDir;
  dst.base.position = worldPos;
  dst.base.color = light.base.color;
  dst.base.ambient = light.base.ambient;
  dst.base.diffuse = light.base.diffuse;
  dst.base.specular = light.base.specular;
  dst.base.density = light.base.density;
}

void UpdateLight(LightsUniformData& lights, const LightPoint& light,
                 const TransformComponent& transform) {
  glm::vec3 worldPos = transform.GetWorldPosition();

  LightPoint& dst = lights.point_lights[lights.point_light_count++];
  dst.base.position = worldPos;
  dst.base.color = light.base.color;
  dst.base.ambient = light.base.ambient;
  dst.base.diffuse = light.base.diffuse;
  dst.base.specular = light.base.specular;
  dst.base.density = light.base.density;
  dst.constant = light.constant;
  dst.linear = light.linear;
  dst.exp = light.exp;
}

}  // namespace wiesel