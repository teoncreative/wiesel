
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/w_camera.hpp"

namespace Wiesel {

void CameraComponent::UpdateProjection() {
  Projection = glm::perspective(glm::radians(FieldOfView), AspectRatio,
                                NearPlane, FarPlane);
  Projection[1][1] *=
      -1;  // glm is originally designed for OpenGL, which Y coords where flipped
  InvProjection = glm::inverse(Projection);
  IsAnyChanged = true;
}

void CameraComponent::UpdateView(glm::vec3& position, glm::vec3& rotation) {
  glm::vec3 rotationDegrees =
      glm::radians(rotation);  // Convert degrees to radians
  glm::mat4 rotationMatrix = glm::toMat4(glm::quat(rotationDegrees));
  InvViewMatrix = glm::translate(glm::mat4(1.0f), position) * rotationMatrix;
  ViewMatrix = glm::inverse(InvViewMatrix);
  IsAnyChanged = true;
}

void CameraComponent::UpdateAll() {
  ExtractFrustumPlanes();
  ForceLightReset = true;
}

void CameraComponent::ComputeCascades(const glm::vec3& lightDir) {
  /*if (!ForceLightReset && PreviousLightDir == lightDir) {
    return;
  }*/

  float cascadeSplitLambda = 0.95f;
  float cascadeSplits[WIESEL_SHADOW_CASCADE_COUNT];

  float clipRange = FarPlane - NearPlane;
  float minZ = NearPlane;
  float maxZ = NearPlane + clipRange;

  float range = maxZ - minZ;
  float ratio = maxZ / minZ;

  // Calculate split depths
  for (uint32_t i = 0; i < WIESEL_SHADOW_CASCADE_COUNT; ++i) {
    float p = (i + 1.0f) / static_cast<float>(WIESEL_SHADOW_CASCADE_COUNT);
    float log = minZ * std::pow(ratio, p);
    float uniform = minZ + range * p;
    float d = cascadeSplitLambda * (log - uniform) + uniform;
    cascadeSplits[i] = (d - NearPlane) / clipRange;
  }

  // Calculate orthographic projection matrix for each cascade
  float lastSplitDist = 0.0;
  for (uint32_t i = 0; i < WIESEL_SHADOW_CASCADE_COUNT; i++) {
    float splitDist = cascadeSplits[i];

    glm::vec3 frustumCorners[8] = {
        glm::vec3(-1.0f, 1.0f, 0.0f), glm::vec3(1.0f, 1.0f, 0.0f),
        glm::vec3(1.0f, -1.0f, 0.0f), glm::vec3(-1.0f, -1.0f, 0.0f),
        glm::vec3(-1.0f, 1.0f, 1.0f), glm::vec3(1.0f, 1.0f, 1.0f),
        glm::vec3(1.0f, -1.0f, 1.0f), glm::vec3(-1.0f, -1.0f, 1.0f),
    };
    // Project frustum corners into world space
    glm::mat4 invCam = glm::inverse(Projection * ViewMatrix);
    for (uint32_t j = 0; j < 8; j++) {
      glm::vec4 invCorner = invCam * glm::vec4(frustumCorners[j], 1.0f);
      frustumCorners[j] = invCorner / invCorner.w;
    }

    for (uint32_t j = 0; j < 4; j++) {
      glm::vec3 dist = frustumCorners[j + 4] - frustumCorners[j];
      frustumCorners[j + 4] = frustumCorners[j] + (dist * splitDist);
      frustumCorners[j] = frustumCorners[j] + (dist * lastSplitDist);
    }

    // Get frustum center
    glm::vec3 frustumCenter = glm::vec3(0.0f);
    for (uint32_t j = 0; j < 8; j++) {
      frustumCenter += frustumCorners[j];
    }
    frustumCenter /= 8.0f;

    float radius = 0.0f;
    for (uint32_t j = 0; j < 8; j++) {
      glm::vec3 cornerToCenter = frustumCorners[j] - frustumCenter;
      float projX = glm::dot(cornerToCenter, glm::normalize(glm::vec3(1,0,0)));
      float projY = glm::dot(cornerToCenter, glm::normalize(glm::vec3(0,1,0)));
      float projZ = glm::dot(cornerToCenter, glm::normalize(lightDir)); // directional depth
      radius = glm::max(radius, glm::length(glm::vec3(projX, projY, projZ)));
    }
    radius = std::ceil(radius * 16.0f) / 16.0f;

    glm::vec3 maxExtents = glm::vec3(radius);
    glm::vec3 minExtents = -maxExtents;

    float texelSize = (radius * 2.0f) / WIESEL_SHADOWMAP_DIM;
    glm::vec3 shadowCamPos = frustumCenter + lightDir * -minExtents.z;
    shadowCamPos /= texelSize;
    shadowCamPos = glm::floor(shadowCamPos);
    shadowCamPos *= texelSize;

    glm::mat4 lightViewMatrix = glm::lookAt(shadowCamPos, frustumCenter, glm::vec3(0.0f, 1.0f, 0.0f));

    glm::mat4 lightOrthoMatrix = glm::ortho(
        minExtents.x, maxExtents.x,
        minExtents.y, maxExtents.y,
        minExtents.z - 10.0f, maxExtents.z + 10.0f
    );
    lightOrthoMatrix[1][1] *= -1;
    // Store split distance and matrix in cascade
    ShadowMapCascades[i].SplitDepth = (NearPlane + splitDist * clipRange) * -1.0f;
    ShadowMapCascades[i].ViewProjMatrix = lightOrthoMatrix * lightViewMatrix;
    lastSplitDist = cascadeSplits[i];
  }

  PreviousLightDir = lightDir;
  DoesShadowPass = true;
  ForceLightReset = false;
}

void CameraComponent::ExtractFrustumPlanes() {
  glm::mat4 m = Projection * ViewMatrix;
  // Each plane is in the form (a,b,c,d), representing ax + by + cz + d = 0
  Planes.Left = glm::normalize(glm::vec4(m[0][3] + m[0][0], m[1][3] + m[1][0],
                                         m[2][3] + m[2][0], m[3][3] + m[3][0]));
  Planes.Right =
      glm::normalize(glm::vec4(m[0][3] - m[0][0], m[1][3] - m[1][0],
                               m[2][3] - m[2][0], m[3][3] - m[3][0]));
  Planes.Bottom =
      glm::normalize(glm::vec4(m[0][3] + m[0][1], m[1][3] + m[1][1],
                               m[2][3] + m[2][1], m[3][3] + m[3][1]));
  Planes.Top = glm::normalize(glm::vec4(m[0][3] - m[0][1], m[1][3] - m[1][1],
                                        m[2][3] - m[2][1], m[3][3] - m[3][1]));
  Planes.Near = glm::normalize(glm::vec4(m[0][3] + m[0][2], m[1][3] + m[1][2],
                                         m[2][3] + m[2][2], m[3][3] + m[3][2]));
  Planes.Far = glm::normalize(glm::vec4(m[0][3] - m[0][2], m[1][3] - m[1][2],
                                        m[2][3] - m[2][2], m[3][3] - m[3][2]));
}

}  // namespace Wiesel
