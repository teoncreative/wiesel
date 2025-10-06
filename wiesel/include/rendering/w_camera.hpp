
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

#include <scene/w_components.hpp>
#include "events/w_appevents.hpp"
#include "util/w_uuid.hpp"
#include "w_framebuffer.hpp"
#include "w_pch.hpp"
#include "w_texture.hpp"

namespace Wiesel {

struct FrustumPlanes {
  glm::vec4 Left, Right, Bottom, Top, Near, Far;
};
struct Cascade {
  float SplitDepth;
  glm::mat4 ViewProjMatrix;
};

struct CameraComponent {
  CameraComponent() = default;
  CameraComponent(const CameraComponent&) = default;
  ~CameraComponent() = default;

  float field_of_view = 60;
  float near_plane = 0.01f;
  float far_plane = 400.0f;
  float aspect_ratio = 1.0;

  glm::mat4 view_matrix;
  glm::mat4 projection;
  glm::mat4 inv_projection;
  glm::vec2 viewport_size;
  glm::mat4 inv_view_matrix;

#ifdef ID_BUFFER_PASS
  Ref<AttachmentTexture> id_image;
  Ref<AttachmentTexture> id_depth_stencil;
#endif

  Ref<AttachmentTexture> geometry_normal_image;
  Ref<AttachmentTexture> geometry_normal_resolve_image;
  Ref<AttachmentTexture> GeometryDepthImage;
  Ref<AttachmentTexture> geometry_depth_resolve_image;
  Ref<AttachmentTexture> geometry_albedo_image;
  Ref<AttachmentTexture> geometry_albedo_resolve_image;
  Ref<AttachmentTexture> geometry_view_pos_image;
  Ref<AttachmentTexture> geometry_view_pos_resolve_image;
  Ref<AttachmentTexture> geometry_world_pos_image;
  Ref<AttachmentTexture> geometry_world_pos_resolve_image;
  Ref<AttachmentTexture> geometry_material_image;
  Ref<AttachmentTexture> geometry_material_resolve_image;
  Ref<AttachmentTexture> geometry_depth_stencil;

  Ref<AttachmentTexture> ssao_color_image;
  Ref<AttachmentTexture> ssao_blur_horz_color_image;
  Ref<AttachmentTexture> ssao_blur_vert_color_image;

  Ref<AttachmentTexture> lighting_color_image;
  Ref<AttachmentTexture> lighting_color_resolve_image;

  Ref<AttachmentTexture> sprite_color_image;

  Ref<AttachmentTexture> composite_color_image;
  Ref<AttachmentTexture> composite_color_resolve_image;

#ifdef ID_BUFFER_PASS
  Ref<Framebuffer> id_framebuffer;
#endif
  Ref<Framebuffer> geometry_framebuffer;
  Ref<Framebuffer> ssao_gen_framebuffer;
  Ref<Framebuffer> ssao_blur_horz_framebuffer;
  Ref<Framebuffer> ssao_blur_vert_framebuffer;
  Ref<Framebuffer> lighting_framebuffer;
  Ref<Framebuffer> sprite_framebuffer;
  Ref<Framebuffer> composite_framebuffer;
  Ref<DescriptorSet> global_descriptor;
  Ref<DescriptorSet> shadow_descriptor;

  Ref<DescriptorSet> geometry_output_descriptor;
  Ref<DescriptorSet> ssao_output_descriptor;
  Ref<DescriptorSet> ssao_blur_horz_output_descriptor;
  Ref<DescriptorSet> ssao_blur_vert_output_descriptor;
  Ref<DescriptorSet> lighting_output_descriptor;
  Ref<DescriptorSet> sprite_output_descriptor;
  Ref<DescriptorSet> composite_output_descriptor;
  Ref<DescriptorSet> ssao_gen_descriptor;
  FrustumPlanes planes;

  // Shadow stuff
  bool does_shadow_pass = false;
  std::array<Cascade, WIESEL_SHADOW_CASCADE_COUNT> shadow_map_cascades;
  Ref<AttachmentTexture> shadow_depth_stencil;
  std::array<Ref<ImageView>, WIESEL_SHADOW_CASCADE_COUNT> shadow_depth_views;
  Ref<ImageView> shadow_depth_view_array;
  std::array<Ref<Framebuffer>, WIESEL_SHADOW_CASCADE_COUNT> shadow_framebuffers;

  glm::vec3 previous_light_dir;
  bool force_light_reset = false;
  bool pos_changed = true;
  bool view_changed = true;
  bool any_changed = true;
  bool enabled = true;

  void UpdateProjection();
  void UpdateView(const glm::mat4& worldTransform);
  void UpdateAll();

  void ComputeCascades(const glm::vec3& lightDir);
  void ExtractFrustumPlanes();

};

struct CameraData {
  CameraData() = default;
  CameraData(glm::vec3 position, glm::mat4 view_matrix, glm::mat4 projection)
      : position(position), view_matrix(view_matrix), projection(projection){}
  CameraData(const CameraData&) = default;
  ~CameraData() = default;

  glm::vec3 position;
  glm::mat4 view_matrix;
  glm::mat4 projection;
  glm::mat4 inv_projection;
  glm::vec2 viewport_size;
  float near_plane = 0.01f;
  float far_plane = 1000.0f;

  Ref<AttachmentTexture> geometry_normal_image;
  Ref<AttachmentTexture> geometry_normal_resolve_image;
  Ref<AttachmentTexture> geometry_albedo_image;
  Ref<AttachmentTexture> geometry_albedo_resolve_image;
  Ref<AttachmentTexture> geometry_view_pos_image;
  Ref<AttachmentTexture> geometry_view_pos_resolve_image;
  Ref<AttachmentTexture> geometry_world_pos_image;
  Ref<AttachmentTexture> geometry_world_pos_resolve_image;
  Ref<AttachmentTexture> geometry_depth_image;
  Ref<AttachmentTexture> geometry_depth_resolve_image;
  Ref<AttachmentTexture> geometry_depth_stencil;
  Ref<AttachmentTexture> geometry_material_image;
  Ref<AttachmentTexture> geometry_material_resolve_image;

  Ref<AttachmentTexture> ssao_color_image;
  Ref<AttachmentTexture> ssao_blur_horz_color_image;
  Ref<AttachmentTexture> ssao_blur_vert_color_image;

  Ref<AttachmentTexture> lighting_color_image;
  Ref<AttachmentTexture> lighting_color_resolve_image;

  Ref<AttachmentTexture> sprite_color_image;

  Ref<AttachmentTexture> composite_color_image;
  Ref<AttachmentTexture> composite_color_resolve_image;

  Ref<Framebuffer> geometry_framebuffer;
  Ref<Framebuffer> ssao_gen_framebuffer;
  Ref<Framebuffer> ssao_blur_horz_framebuffer;
  Ref<Framebuffer> ssao_blur_vert_framebuffer;
  Ref<Framebuffer> lighting_framebuffer;
  Ref<Framebuffer> sprite_framebuffer;
  Ref<Framebuffer> composite_framebuffer;
  Ref<DescriptorSet> global_descriptor; // to draw geometry
  Ref<DescriptorSet> shadow_descriptor; // to draw geometry to shadow pass
  Ref<DescriptorSet> geometry_output_descriptor; // to draw geometry pass output
  Ref<DescriptorSet> ssao_output_descriptor; // to draw ssao pass output
  Ref<DescriptorSet> ssao_blur_horz_output_descriptor; // to draw ssao blur horz pass output
  Ref<DescriptorSet> ssao_blur_vert_output_descriptor; // to draw ssao blur vert pass output
  Ref<DescriptorSet> lighting_output_descriptor; // to draw lighting pass output
  Ref<DescriptorSet> sprite_output_descriptor; // to draw sprite pass output
  Ref<DescriptorSet> composite_output_descriptor; // to draw composite pass output
  Ref<DescriptorSet> ssao_gen_descriptor; // used to render geometry pass output to ssao pass
  FrustumPlanes planes;

  // Shadow stuff
  bool does_shadow_pass = false;
  std::array<Cascade, WIESEL_SHADOW_CASCADE_COUNT> shadow_map_cascades;
  Ref<AttachmentTexture> shadow_depth_stencil;
  std::array<Ref<Framebuffer>, WIESEL_SHADOW_CASCADE_COUNT> shadow_framebuffers;

  void TransferFrom(CameraComponent& camera, TransformComponent& transform) {
    // Perhaps we could do this differently?
    // At first, we had 3 variables to update, but now we have a lot more...

    position = transform.position;
    view_matrix = camera.view_matrix;
    projection = camera.projection;
    inv_projection = camera.inv_projection;
    viewport_size = camera.viewport_size;
    near_plane = camera.near_plane;
    far_plane = camera.far_plane;

    geometry_normal_image = camera.geometry_normal_image;
    geometry_normal_resolve_image = camera.geometry_normal_resolve_image;
    geometry_albedo_image = camera.geometry_albedo_image;
    geometry_albedo_resolve_image = camera.geometry_albedo_resolve_image;
    geometry_view_pos_image = camera.geometry_view_pos_image;
    geometry_view_pos_resolve_image = camera.geometry_view_pos_resolve_image;
    geometry_world_pos_image = camera.geometry_world_pos_image;
    geometry_world_pos_resolve_image = camera.geometry_world_pos_resolve_image;
    geometry_depth_image = camera.GeometryDepthImage;
    geometry_depth_resolve_image = camera.geometry_depth_resolve_image;
    geometry_depth_stencil = camera.geometry_depth_stencil;
    geometry_material_image = camera.geometry_material_image;
    geometry_material_resolve_image = camera.geometry_material_resolve_image;

    ssao_color_image = camera.ssao_color_image;
    ssao_blur_horz_color_image = camera.ssao_blur_horz_color_image;
    ssao_blur_vert_color_image = camera.ssao_blur_vert_color_image;

    lighting_color_image = camera.lighting_color_image;
    lighting_color_resolve_image = camera.lighting_color_resolve_image;

    sprite_color_image = camera.sprite_color_image;

    composite_color_image = camera.composite_color_image;
    composite_color_resolve_image = camera.composite_color_resolve_image;

    geometry_framebuffer = camera.geometry_framebuffer;
    ssao_gen_framebuffer = camera.ssao_gen_framebuffer;
    ssao_blur_horz_framebuffer = camera.ssao_blur_horz_framebuffer;
    ssao_blur_vert_framebuffer = camera.ssao_blur_vert_framebuffer;
    lighting_framebuffer = camera.lighting_framebuffer;
    sprite_framebuffer = camera.sprite_framebuffer;
    composite_framebuffer = camera.composite_framebuffer;
    global_descriptor = camera.global_descriptor;
    shadow_descriptor = camera.shadow_descriptor;
    geometry_output_descriptor = camera.geometry_output_descriptor;
    ssao_output_descriptor = camera.ssao_output_descriptor;
    ssao_blur_horz_output_descriptor = camera.ssao_blur_horz_output_descriptor;
    ssao_blur_vert_output_descriptor = camera.ssao_blur_vert_output_descriptor;
    lighting_output_descriptor = camera.lighting_output_descriptor;
    sprite_output_descriptor = camera.sprite_output_descriptor;
    composite_output_descriptor = camera.composite_output_descriptor;
    ssao_gen_descriptor = camera.ssao_gen_descriptor;
    planes = camera.planes;

    does_shadow_pass = camera.does_shadow_pass;
    shadow_map_cascades = camera.shadow_map_cascades;
    shadow_depth_stencil = camera.shadow_depth_stencil;
    shadow_framebuffers = camera.shadow_framebuffers;
  }

};

}  // namespace Wiesel
