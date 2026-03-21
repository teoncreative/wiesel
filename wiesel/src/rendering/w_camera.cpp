
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
  if (aspect_ratio <= 0.0f) {
    aspect_ratio = 16.0f / 9.0f;
  }
  if (projection_mode == ProjectionMode::Orthographic) {
    float half_h = ortho_size;
    float half_w = half_h * aspect_ratio;
    projection = glm::ortho(-half_w, half_w, -half_h, half_h,
                             near_plane, far_plane);
  } else {
    projection = glm::perspective(glm::radians(field_of_view), aspect_ratio,
                                  near_plane, far_plane);
  }
  // GLM is designed for OpenGL where Y is flipped compared to Vulkan
  projection[1][1] *= -1;
  inv_projection = glm::inverse(projection);
  any_changed = true;
}

void CameraComponent::UpdateView(const glm::mat4& worldTransform) {
  inv_view_matrix = worldTransform;
  view_matrix    = glm::inverse(worldTransform);
  any_changed  = true;
}

void CameraComponent::UpdateAll() {
  ExtractFrustumPlanes();
  force_light_reset = true;
}

void CameraComponent::ComputeCascades(const glm::vec3& lightDir) {
  /*if (!ForceLightReset && PreviousLightDir == lightDir) {
    return;
  }*/

  float cascadeSplitLambda = 0.95f;
  float cascadeSplits[WIESEL_SHADOW_CASCADE_COUNT];

  // This ensures shadow quality is consistent regardless of camera settings.
  float effectiveNear = 0.5f;
  float effectiveFar  = 1000.0f;

  float clipRange = effectiveFar - effectiveNear;
  float minZ = effectiveNear;
  float maxZ = effectiveFar;

  float range = maxZ - minZ;
  float ratio = maxZ / minZ;

  // Calculate split depths as normalized [0,1] fractions of the effective range
  for (uint32_t i = 0; i < WIESEL_SHADOW_CASCADE_COUNT; ++i) {
    float p = (i + 1.0f) / static_cast<float>(WIESEL_SHADOW_CASCADE_COUNT);
    float log = minZ * std::pow(ratio, p);
    float uniform = minZ + range * p;
    float d = cascadeSplitLambda * (log - uniform) + uniform;
    cascadeSplits[i] = (d - effectiveNear) / clipRange;
  }

  // Build a shadow-specific projection using the clamped near/far.
  // This ensures frustum corners (and thus cascade ortho volumes) are
  // sized by the effective range, not the real camera range.
  glm::mat4 shadowProjection = glm::perspective(
      glm::radians(field_of_view), aspect_ratio, effectiveNear, effectiveFar);
  shadowProjection[1][1] *= -1;

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
    // Unproject using the shadow-specific projection (clamped near/far)
    glm::mat4 invCam = glm::inverse(shadowProjection * view_matrix);
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
      float dist = glm::length(frustumCorners[j] - frustumCenter);
      radius = glm::max(radius, dist);
    }
    radius = std::ceil(radius * 16.0f) / 16.0f;

    glm::vec3 maxExtents = glm::vec3(radius);
    glm::vec3 minExtents = -maxExtents;

    // Shadow camera position: offset from frustum center along light direction
    glm::vec3 shadowCamPos = frustumCenter + lightDir * radius;

    glm::mat4 lightViewMatrix = glm::lookAt(shadowCamPos, frustumCenter, glm::vec3(0.0f, 1.0f, 0.0f));

    // Compute tight Z bounds in light-view space
    float lightZMin = std::numeric_limits<float>::max();
    float lightZMax = std::numeric_limits<float>::lowest();
    for (uint32_t j = 0; j < 8; j++) {
      glm::vec4 vCorner = lightViewMatrix * glm::vec4(frustumCorners[j], 1.0f);
      lightZMin = glm::min(lightZMin, vCorner.z);
      lightZMax = glm::max(lightZMax, vCorner.z);
    }

    // Extend behind camera to catch shadow casters outside the view frustum
    float zMargin = radius;
    glm::mat4 lightOrthoMatrix = glm::ortho(
        minExtents.x, maxExtents.x,
        minExtents.y, maxExtents.y,
        -lightZMax - zMargin, -lightZMin + zMargin
    );
    lightOrthoMatrix[1][1] *= -1;

    // Texel-space snapping: prevents shadow edge swimming when camera moves
    // We snap the projection's XY translation to texel boundaries rather than
    // snapping the camera position in world space (which would shift the view direction)
    float halfDim = static_cast<float>(WIESEL_SHADOWMAP_DIM) / 2.0f;
    glm::vec4 shadowOrigin = (lightOrthoMatrix * lightViewMatrix) * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    shadowOrigin.x *= halfDim;
    shadowOrigin.y *= halfDim;
    glm::vec4 roundedOrigin = glm::round(shadowOrigin);
    lightOrthoMatrix[3][0] += (roundedOrigin.x - shadowOrigin.x) / halfDim;
    lightOrthoMatrix[3][1] += (roundedOrigin.y - shadowOrigin.y) / halfDim;

    // Store split distance and matrix in cascade
    shadow_map_cascades[i].SplitDepth = (effectiveNear + splitDist * clipRange) * -1.0f;
    shadow_map_cascades[i].ViewProjMatrix = lightOrthoMatrix * lightViewMatrix;
    lastSplitDist = cascadeSplits[i];
  }

  previous_light_dir = lightDir;
  does_shadow_pass = true;
  force_light_reset = false;
}

void CameraComponent::ExtractFrustumPlanes() {
  glm::mat4 m = projection * view_matrix;
  // Each plane is in the form (a,b,c,d), representing ax + by + cz + d = 0
  planes.Left = glm::normalize(glm::vec4(m[0][3] + m[0][0], m[1][3] + m[1][0],
                                         m[2][3] + m[2][0], m[3][3] + m[3][0]));
  planes.Right =
      glm::normalize(glm::vec4(m[0][3] - m[0][0], m[1][3] - m[1][0],
                               m[2][3] - m[2][0], m[3][3] - m[3][0]));
  planes.Bottom =
      glm::normalize(glm::vec4(m[0][3] + m[0][1], m[1][3] + m[1][1],
                               m[2][3] + m[2][1], m[3][3] + m[3][1]));
  planes.Top = glm::normalize(glm::vec4(m[0][3] - m[0][1], m[1][3] - m[1][1],
                                        m[2][3] - m[2][1], m[3][3] - m[3][1]));
  planes.Near = glm::normalize(glm::vec4(m[0][3] + m[0][2], m[1][3] + m[1][2],
                                         m[2][3] + m[2][2], m[3][3] + m[3][2]));
  planes.Far = glm::normalize(glm::vec4(m[0][3] - m[0][2], m[1][3] - m[1][2],
                                        m[2][3] - m[2][2], m[3][3] - m[3][2]));
}

}  // namespace Wiesel
