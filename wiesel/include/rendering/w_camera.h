
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include <scene/w_components.h>
#include "events/w_appevents.h"
#include "rendering/w_render_feature.h"
#include "util/w_uuid.h"
#include "w_framebuffer.h"
#include "w_pch.h"
#include "w_texture.h"

namespace wiesel {

class RenderGraph;

struct FrustumPlanes {
  glm::vec4 Left, Right, Bottom, Top, Near, Far;

  // Test if an AABB (already in world space) is outside the frustum.
  // Returns true if the box is completely outside any plane (should be culled).
  bool IsBoxOutside(const glm::vec3& aabb_min,
                    const glm::vec3& aabb_max) const {
    const glm::vec4* planes_arr = &Left;
    for (int i = 0; i < 6; i++) {
      glm::vec3 n(planes_arr[i]);
      float d = planes_arr[i].w;
      // Find the AABB corner most in the direction of the plane normal
      glm::vec3 p_vertex(n.x >= 0 ? aabb_max.x : aabb_min.x,
                         n.y >= 0 ? aabb_max.y : aabb_min.y,
                         n.z >= 0 ? aabb_max.z : aabb_min.z);
      if (glm::dot(n, p_vertex) + d < 0.0f) {
        return true;
      }
    }
    return false;
  }
};

struct Cascade {
  float SplitDepth;
  glm::mat4 ViewProjMatrix;
};

enum class ProjectionMode : int {
  Perspective = 0,
  Orthographic = 1,
};

WCLASS()

struct CameraComponent {
  CameraComponent() = default;
  CameraComponent(const CameraComponent&) = default;
  ~CameraComponent() = default;

  // Camera parameters
  WPROPERTY(Serializable)
  ProjectionMode projection_mode = ProjectionMode::Perspective;
  WPROPERTY(Serializable, Animatable)
  float field_of_view = 60;  // perspective only
  WPROPERTY(Serializable, Animatable)
  float ortho_size = 5.0f;  // orthographic only: half-height in world units
  WPROPERTY(Serializable)
  float near_plane = 0.3f;
  WPROPERTY(Serializable)
  float far_plane = 1000.0f;
  float aspect_ratio = 0.0;
  WPROPERTY(Serializable)
  glm::vec4 background_color = {0.0f, 0.0f, 0.0f,
                                1.0f};  // used in ortho mode instead of skybox

  glm::mat4 view_matrix;
  glm::mat4 projection;
  glm::mat4 inv_projection;
  glm::vec2 viewport_size{1280.0f, 720.0f};

  // Resource versioning: tracks which pipeline version and viewport size
  // this camera's resources were built for. Automatically triggers rebuild
  // when the pipeline is recreated or viewport changes.
  uint32_t resource_pipeline_version = 0;
  glm::vec2 resource_viewport_size = {0.0f, 0.0f};
  glm::mat4 inv_view_matrix;
  bool enabled = true;

  FrustumPlanes planes;

  // Dynamic resource storage
  CameraResourcePool resource_pool;

  // Per-camera render graph (rebuilt each frame)
  std::shared_ptr<RenderGraph> render_graph;

  // Per-camera pipeline override (nullptr = use scene default)
  std::shared_ptr<RenderPipeline> render_pipeline;

  // Shadow cascade data (structural, kept separate from pool)
  bool does_shadow_pass = false;
  std::array<Cascade, WIESEL_SHADOW_CASCADE_COUNT> shadow_map_cascades;

  // Frame-to-frame state
  glm::vec3 previous_light_dir;
  glm::mat4 prev_view_projection{1.0f};
  uint32_t taa_frame_count = 0;
  bool force_light_reset = false;
  bool pos_changed = true;
  bool view_changed = true;
  bool any_changed = true;

  void UpdateProjection();
  void UpdateView(const glm::mat4& worldTransform);
  void UpdateAll();

  void ComputeCascades(const glm::vec3& lightDir);
  void ExtractFrustumPlanes();
};

struct CameraData {
  CameraData() = default;

  CameraData(glm::vec3 position, glm::mat4 view_matrix, glm::mat4 projection)
      : position(position), view_matrix(view_matrix), projection(projection) {}

  CameraData(const CameraData&) = default;
  ~CameraData() = default;

  glm::vec3 position;
  glm::mat4 view_matrix;
  glm::mat4 projection;
  glm::mat4 inv_projection;
  glm::vec2 viewport_size;
  float near_plane = 0.01f;
  float far_plane = 1000.0f;

  FrustumPlanes planes;

  // Pointer to camera's resource pool (not owned)
  CameraResourcePool* resource_pool = nullptr;

  // Shadow cascade data
  bool does_shadow_pass = false;
  std::array<Cascade, WIESEL_SHADOW_CASCADE_COUNT> shadow_map_cascades;

  glm::mat4 prev_view_projection{1.0f};

  void TransferFrom(CameraComponent& camera, TransformComponent& transform) {
    position = transform.GetWorldPosition();
    view_matrix = camera.view_matrix;
    projection = camera.projection;
    inv_projection = camera.inv_projection;
    viewport_size = camera.viewport_size;
    near_plane = camera.near_plane;
    far_plane = camera.far_plane;
    planes = camera.planes;

    resource_pool = &camera.resource_pool;

    does_shadow_pass = camera.does_shadow_pass;
    shadow_map_cascades = camera.shadow_map_cascades;
    prev_view_projection = camera.prev_view_projection;
  }
};

}  // namespace wiesel
