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

  template<>
  void UpdateLight(LightsUniformBufferObject& lights, LightDirect light, Entity entity) {
    auto& transform = entity.GetComponent<TransformComponent>();
    lights.DirectLights[lights.DirectLightCount].Direction = glm::normalize(transform.Position);
    lights.DirectLights[lights.DirectLightCount].Base.Color = light.Base.Color;
    lights.DirectLights[lights.DirectLightCount].Base.Ambient = light.Base.Ambient;
    lights.DirectLights[lights.DirectLightCount].Base.Diffuse = light.Base.Diffuse;
    lights.DirectLights[lights.DirectLightCount].Base.Specular = light.Base.Specular;
    lights.DirectLights[lights.DirectLightCount].Base.Density = light.Base.Density;
    lights.DirectLightCount++;
  }

  template<>
  void UpdateLight(LightsUniformBufferObject& lights, LightPoint light, Entity entity) {
    auto& transform = entity.GetComponent<TransformComponent>();
    lights.PointLights[lights.PointLightCount].Position = transform.Position;
    lights.PointLights[lights.PointLightCount].Base.Color = light.Base.Color;
    lights.PointLights[lights.PointLightCount].Base.Ambient = light.Base.Ambient;
    lights.PointLights[lights.PointLightCount].Base.Diffuse = light.Base.Diffuse;
    lights.PointLights[lights.PointLightCount].Base.Specular = light.Base.Specular;
    lights.PointLights[lights.PointLightCount].Base.Density = light.Base.Density;
    lights.PointLights[lights.PointLightCount].Constant = light.Constant;
    lights.PointLights[lights.PointLightCount].Linear = light.Linear;
    lights.PointLights[lights.PointLightCount].Exp = light.Exp;
    lights.PointLightCount++;
  }

}// namespace Wiesel