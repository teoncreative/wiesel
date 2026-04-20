//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/features/w_billboard_feature.h"
#include "asset/w_asset_manager.h"
#include "asset/w_asset_properties.h"
#include "rendering/w_billboard_renderer.h"
#include "rendering/w_billboard_text.h"
#include "rendering/w_camera.h"
#include "rendering/w_pipeline.h"
#include "rendering/w_renderer.h"
#include "rendering/w_renderpass.h"
#include "scene/w_components.h"
#include "scene/w_scene.h"
#include "ui/w_font.h"
#include "w_engine.h"

namespace wiesel {

// Billboard screen-size scaling factors
constexpr float kBillboardSizeOrtho = 0.05f;
constexpr float kBillboardSizePerspective = 0.1f;

BillboardFeature::BillboardFeature(std::shared_ptr<Renderer> renderer)
    : renderer_(std::move(renderer)) {
  SamplingMode msaa = renderer_->options().msaa_mode;

  // Render pass: color + depth + entity ID (load existing depth for occlusion)
  render_pass_ = std::make_shared<RenderPass>(PassType::ForwardTransparency,
                                              "Billboard RenderPass");
  render_pass_->AttachOutput({.type = AttachmentTextureType::Offscreen,
                              .format = renderer_->GetSwapChainImageFormat(),
                              .msaa_mode = msaa});
  render_pass_->AttachOutput({.type = AttachmentTextureType::DepthStencil,
                              .format = renderer_->FindDepthFormat(),
                              .msaa_mode = msaa});
  render_pass_->AttachOutput({.type = AttachmentTextureType::Offscreen,
                              .format = VK_FORMAT_R32_UINT,
                              .msaa_mode = msaa});
  if (msaa > SamplingMode::DISABLED) {
    render_pass_->AttachOutput({.type = AttachmentTextureType::Resolve,
                                .format = renderer_->GetSwapChainImageFormat(),
                                .msaa_mode = SamplingMode::DISABLED});
    render_pass_->AttachOutput({.type = AttachmentTextureType::Resolve,
                                .format = VK_FORMAT_R32_UINT,
                                .msaa_mode = SamplingMode::DISABLED});
  }
  render_pass_->Bake();

  // Billboard shaders
  auto vert = renderer_->CreateShader({ShaderTypeVertex, ShaderLangGLSL, "main",
                                       ShaderSourceSource,
                                       "engine://shaders/billboard.vert"});
  auto frag = renderer_->CreateShader({ShaderTypeFragment, ShaderLangGLSL,
                                       "main", ShaderSourceSource,
                                       "engine://shaders/billboard.frag"});

  VkVertexInputBindingDescription binding{};
  binding.binding = 0;
  binding.stride = sizeof(BillboardVertex);
  binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  std::vector<VkVertexInputAttributeDescription> attrs(2);
  attrs[0].binding = 0;
  attrs[0].location = 0;
  attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
  attrs[0].offset = offsetof(BillboardVertex, position);
  attrs[1].binding = 0;
  attrs[1].location = 1;
  attrs[1].format = VK_FORMAT_R32G32_SFLOAT;
  attrs[1].offset = offsetof(BillboardVertex, uv);

  // Descriptor layout for icon texture
  icon_desc_layout_ = std::make_shared<DescriptorSetLayout>();
  icon_desc_layout_->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                VK_SHADER_STAGE_FRAGMENT_BIT);
  icon_desc_layout_->Bake();

  push_constant_ = std::make_shared<BillboardPushConstant>();

  // Build the three image pipelines (standard / no-depth / occluded-only).
  auto make_image_pipeline = [&](bool depth_test, CompareOp compare) {
    PipelineProperties props{};
    props.sampling_mode = msaa;
    props.cull_mode = CullModeNone;
    props.enable_alpha_blending = true;
    props.enable_depth_test = depth_test;
    props.enable_depth_write = false;
    props.depth_compare_op = compare;
    auto p = std::make_shared<Pipeline>(props);
    p->SetVertexData(binding, attrs);
    p->SetRenderPass(render_pass_);
    p->AddPushConstant(push_constant_, VK_SHADER_STAGE_VERTEX_BIT |
                                           VK_SHADER_STAGE_FRAGMENT_BIT);
    p->AddInputLayout(icon_desc_layout_);
    p->AddShader(vert);
    p->AddShader(frag);
    p->Bake();
    return p;
  };

  pipeline_ = make_image_pipeline(true, CompareOpLessOrEqual);
  pipeline_no_depth_ = make_image_pipeline(false, CompareOpAlways);
  pipeline_occluded_ = make_image_pipeline(true, CompareOpGreater);

  // Text pipelines (same render pass, same quad geometry, different shader +
  // push constants that include a UV rect for glyph sub-sampling).
  auto text_vert = renderer_->CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/billboard_text.vert"});
  auto text_frag = renderer_->CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/billboard_text.frag"});

  text_desc_layout_ = std::make_shared<DescriptorSetLayout>();
  text_desc_layout_->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                VK_SHADER_STAGE_FRAGMENT_BIT);
  text_desc_layout_->Bake();

  text_push_constant_ = std::make_shared<BillboardTextPushConstant>();

  auto make_text_pipeline = [&](bool depth_test, CompareOp compare) {
    PipelineProperties props{};
    props.sampling_mode = msaa;
    props.cull_mode = CullModeNone;
    props.enable_alpha_blending = true;
    props.enable_depth_test = depth_test;
    props.enable_depth_write = false;
    props.depth_compare_op = compare;
    auto p = std::make_shared<Pipeline>(props);
    p->SetVertexData(binding, attrs);
    p->SetRenderPass(render_pass_);
    p->AddPushConstant(text_push_constant_, VK_SHADER_STAGE_VERTEX_BIT |
                                                VK_SHADER_STAGE_FRAGMENT_BIT);
    p->AddInputLayout(text_desc_layout_);
    p->AddShader(text_vert);
    p->AddShader(text_frag);
    p->Bake();
    return p;
  };

  text_pipeline_ = make_text_pipeline(true, CompareOpLessOrEqual);
  text_pipeline_no_depth_ = make_text_pipeline(false, CompareOpAlways);
  text_pipeline_occluded_ = make_text_pipeline(true, CompareOpGreater);

  // Composite render pass: blend billboard overlay onto PipelineOutput
  comp_render_pass_ = std::make_shared<RenderPass>(
      PassType::PostProcess, "BillboardComposite RenderPass");
  comp_render_pass_->AttachOutput(
      {.type = AttachmentTextureType::Offscreen,
       .format = renderer_->GetSwapChainImageFormat(),
       .msaa_mode = SamplingMode::DISABLED});
  comp_render_pass_->Bake();

  auto fullscreen_vert = renderer_->CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/fullscreen_shader.vert"});
  auto quad_frag = renderer_->CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/quad_shader.frag"});

  // Opaque pipeline for drawing the scene background
  {
    PipelineProperties props{};
    props.sampling_mode = SamplingMode::DISABLED;
    props.cull_mode = CullModeBack;
    props.enable_alpha_blending = false;
    props.enable_depth_test = false;
    props.enable_depth_write = false;
    comp_pipeline_ = std::make_shared<Pipeline>(props);
  }
  comp_pipeline_->SetRenderPass(comp_render_pass_);
  comp_pipeline_->AddInputLayout(renderer_->GetDescriptorLayout("Skybox"));
  comp_pipeline_->AddShader(fullscreen_vert);
  comp_pipeline_->AddShader(quad_frag);
  comp_pipeline_->Bake();

  // Alpha-blended pipeline for overlaying the billboard texture
  {
    PipelineProperties props{};
    props.sampling_mode = SamplingMode::DISABLED;
    props.cull_mode = CullModeBack;
    props.enable_alpha_blending = true;
    props.enable_depth_test = false;
    props.enable_depth_write = false;
    comp_blend_pipeline_ = std::make_shared<Pipeline>(props);
  }
  comp_blend_pipeline_->SetRenderPass(comp_render_pass_);
  comp_blend_pipeline_->AddInputLayout(
      renderer_->GetDescriptorLayout("Skybox"));
  comp_blend_pipeline_->AddShader(fullscreen_vert);
  comp_blend_pipeline_->AddShader(quad_frag);
  comp_blend_pipeline_->Bake();

  // Generate quad geometry
  std::vector<BillboardVertex> vertices = {
      {{-0.5f, -0.5f, 0.0f}, {0.0f, 1.0f}},
      {{0.5f, -0.5f, 0.0f}, {1.0f, 1.0f}},
      {{0.5f, 0.5f, 0.0f}, {1.0f, 0.0f}},
      {{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f}},
  };
  std::vector<Index> indices = {0, 1, 2, 2, 3, 0};
  quad_vb_ =
      renderer_->CreateVertexBuffer("BillboardFeature::quad_vb_", vertices);
  quad_ib_ =
      renderer_->CreateIndexBuffer("BillboardFeature::quad_ib_", indices);

  // Load icon textures
  LoadIconTexture("camera", "engine://textures/billboard/camera_icon.png");
  LoadIconTexture("point_light",
                  "engine://textures/billboard/point_light_icon.png");
}

void BillboardFeature::LoadIconTexture(const std::string& name,
                                       const std::string& vfs_path) {
  TextureProps tex_props{};
  tex_props.type = TextureTypeDiffuse;
  SamplerProps sampler_props{};
  sampler_props.min_filter = VK_FILTER_LINEAR;
  sampler_props.mag_filter = VK_FILTER_LINEAR;
  sampler_props.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  std::shared_ptr<Texture> tex =
      renderer_->CreateTexture(vfs_path, tex_props, sampler_props);
  if (!tex) {
    LOG_WARN("BillboardFeature: failed to load icon '{}'", vfs_path);
    return;
  }
  icon_textures_[name] = tex;

  auto desc = std::make_shared<DescriptorSet>();
  desc->SetLayout(icon_desc_layout_);
  desc->AddCombinedImageSampler(0, tex->image_view_, tex->sampler_);
  desc->Bake();
  icon_descriptors_[name] = desc;
}

std::shared_ptr<DescriptorSet> BillboardFeature::GetOrCreateTextureDescriptor(
    std::shared_ptr<Texture> texture) {
  if (!texture) return nullptr;
  auto it = texture_descriptors_.find(texture.get());
  if (it != texture_descriptors_.end()) {
    return it->second;
  }
  auto desc = std::make_shared<DescriptorSet>();
  desc->SetLayout(icon_desc_layout_);
  desc->AddCombinedImageSampler(0, texture->image_view_, texture->sampler_);
  desc->Bake();
  texture_descriptors_[texture.get()] = desc;
  return desc;
}

std::shared_ptr<DescriptorSet>
BillboardFeature::GetOrCreateTextAtlasDescriptor(
    std::shared_ptr<ImageView> atlas_view) {
  if (!atlas_view) return nullptr;
  auto it = text_atlas_descriptors_.find(atlas_view.get());
  if (it != text_atlas_descriptors_.end()) {
    return it->second;
  }
  auto desc = std::make_shared<DescriptorSet>();
  desc->SetLayout(text_desc_layout_);
  desc->AddCombinedImageSampler(0, atlas_view, renderer_->GetDefaultLinearSampler());
  desc->Bake();
  text_atlas_descriptors_[atlas_view.get()] = desc;
  return desc;
}

bool BillboardFeature::IsEnabled(const RenderContext& ctx) const {
  return true;
}

void BillboardFeature::SetupResources(RenderContext& /*ctx*/) {}

void BillboardFeature::AddPasses(RenderGraph& graph,
                                 RenderResourceRegistry& registry,
                                 RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("BillboardFeature::AddPasses");

  CameraResourcePool* pool = &ctx.resources;
  MultiScene& scenes = ctx.scenes;
  auto renderer = renderer_;
  auto pipeline = pipeline_;
  auto push_constant = push_constant_;
  auto quad_vb = quad_vb_;
  auto quad_ib = quad_ib_;
  BillboardFeature* self = this;
  bool show_editor_icons =
      ctx.is_external && renderer_->options().show_billboards;

  // Get icon descriptors (captured by lambda)
  auto camera_icon_it = icon_descriptors_.find("camera");
  std::shared_ptr<DescriptorSet> camera_icon_desc =
      camera_icon_it != icon_descriptors_.end() ? camera_icon_it->second
                                                : nullptr;

  auto point_light_icon_it = icon_descriptors_.find("point_light");
  std::shared_ptr<DescriptorSet> point_light_icon_desc =
      point_light_icon_it != icon_descriptors_.end()
          ? point_light_icon_it->second
          : nullptr;

  glm::mat4 vp = ctx.camera.projection * ctx.camera.view_matrix;
  glm::vec3 cam_pos = ctx.camera.inv_view_matrix[3];
  float fov = ctx.camera.field_of_view;
  bool is_ortho = ctx.camera.projection_mode == ProjectionMode::Orthographic;
  float ortho_size = ctx.camera.ortho_size;
  glm::vec2 viewport = ctx.viewport_size;

  // Extract camera axes from view matrix for billboarding
  glm::mat4 view = ctx.camera.view_matrix;
  glm::vec3 cam_right = glm::vec3(view[0][0], view[1][0], view[2][0]);
  glm::vec3 cam_up = glm::vec3(view[0][1], view[1][1], view[2][1]);

  // Transient draw targets. With MSAA we declare both the MSAA color/entity
  // and the resolved single-sample versions; without MSAA, resolves alias
  // the MSAA textures.
  SamplingMode msaa = renderer_->options().msaa_mode;
  bool use_msaa = msaa > SamplingMode::DISABLED;
  uint32_t rw = static_cast<uint32_t>(ctx.viewport_size.x);
  uint32_t rh = static_cast<uint32_t>(ctx.viewport_size.y);

  RGResource billboard_color = graph.DeclareTransient(RGTextureDesc{
      .name = "billboard.color",
      .width = rw,
      .height = rh,
      .format = renderer_->GetSwapChainImageFormat(),
      .samples = msaa,
      .type = AttachmentTextureType::Offscreen,
      .layer_count = 1,
      .sampled = !use_msaa});
  RGResource billboard_entity = graph.DeclareTransient(RGTextureDesc{
      .name = "billboard.entity_id",
      .width = rw,
      .height = rh,
      .format = VK_FORMAT_R32_UINT,
      .samples = msaa,
      .type = AttachmentTextureType::Offscreen,
      .layer_count = 1,
      .sampled = !use_msaa});
  RGResource billboard_out =
      use_msaa ? graph.DeclareTransient(RGTextureDesc{
                     .name = "billboard.color_resolve",
                     .width = rw,
                     .height = rh,
                     .format = renderer_->GetSwapChainImageFormat(),
                     .samples = SamplingMode::DISABLED,
                     .type = AttachmentTextureType::Resolve,
                     .layer_count = 1,
                     .sampled = true})
               : billboard_color;
  RGResource billboard_entity_id =
      use_msaa ? graph.DeclareTransient(RGTextureDesc{
                     .name = "billboard.entity_id_resolve",
                     .width = rw,
                     .height = rh,
                     .format = VK_FORMAT_R32_UINT,
                     .samples = SamplingMode::DISABLED,
                     .type = AttachmentTextureType::Resolve,
                     .layer_count = 1,
                     .sampled = true})
               : billboard_entity;

  // Draw pass
  uint32_t draw_pass = graph.AddPass(
      "Billboards", render_pass_,
      [pipeline, pipeline_no_depth = pipeline_no_depth_,
       pipeline_occluded = pipeline_occluded_, push_constant,
       text_pipeline = text_pipeline_,
       text_pipeline_no_depth = text_pipeline_no_depth_,
       text_pipeline_occluded = text_pipeline_occluded_,
       text_push_constant = text_push_constant_, &scenes, renderer, self, vp,
       cam_pos, cam_right, cam_up, fov, is_ortho, ortho_size, viewport, quad_vb,
       quad_ib, camera_icon_desc, point_light_icon_desc,
       show_editor_icons](VkCommandBuffer cmd) {
        VkBuffer vb_handle = quad_vb->buffer_handle_;
        VkDeviceSize vb_offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vb_handle, &vb_offset);
        vkCmdBindIndexBuffer(cmd, quad_ib->buffer_handle_, 0,
                             VK_INDEX_TYPE_UINT32);

        // Screen-size heuristic used for editor icons (distance-fade).
        auto icon_screen_scale = [&](const glm::vec3& world_pos) {
          float distance = glm::length(world_pos - cam_pos);
          float scale;
          if (is_ortho) {
            scale = ortho_size * kBillboardSizeOrtho;
          } else {
            scale = distance * tanf(glm::radians(fov * 0.5f)) *
                    kBillboardSizePerspective;
          }
          return glm::clamp(scale, 0.2f, 20.0f);
        };

        // Compute world-space size of a billboard. `size` is in pixels at
        // 1m reference depth; perspective naturally scales it with distance.
        // `min_mul` / `max_mul` clamp the distance-based scaling factor.
        auto pixels_to_world_scale = [&](const glm::vec3& world_pos,
                                         glm::vec2 size_px, float min_mul,
                                         float max_mul) -> glm::vec2 {
          if (is_ortho) {
            // Orthographic: no perspective falloff, size is constant on screen.
            float px_per_world =
                viewport.y / (2.0f * std::max(ortho_size, 0.0001f));
            return size_px / std::max(px_per_world, 0.0001f);
          }
          glm::vec3 fwd = glm::normalize(glm::cross(cam_up, cam_right));
          float depth = std::abs(glm::dot(world_pos - cam_pos, fwd));
          depth = std::max(depth, 0.0001f);
          constexpr float kRefDistance = 1.0f;
          float distance_factor = glm::clamp(kRefDistance / depth, min_mul,
                                             max_mul);
          float view_height_at_depth =
              2.0f * depth * tanf(glm::radians(fov * 0.5f));
          float world_per_px =
              view_height_at_depth / std::max(viewport.y, 1.0f);
          return size_px * distance_factor * world_per_px;
        };

        // Pipeline currently bound for image draws (tracked to avoid redundant
        // binds when iterating mixed occlusion modes).
        Pipeline* active_image_pipeline = nullptr;

        // Core billboard draw. `world_scale` is the final world-space size of
        // the quad (width, height). `pivot_offset` shifts the quad relative to
        // the world position (in world units).
        auto draw_billboard = [&](const glm::vec3& world_pos,
                                  glm::vec2 world_scale, glm::vec2 pivot_offset,
                                  const glm::vec4& tint,
                                  const std::shared_ptr<DescriptorSet>& tex_desc,
                                  uint32_t encoded_entity_id) {
          if (!tex_desc || !active_image_pipeline) return;

          glm::vec3 origin = world_pos + cam_right * pivot_offset.x +
                             cam_up * pivot_offset.y;
          glm::mat4 model = glm::mat4(1.0f);
          model[0] = glm::vec4(cam_right * world_scale.x, 0.0f);
          model[1] = glm::vec4(cam_up * world_scale.y, 0.0f);
          model[2] =
              glm::vec4(glm::cross(cam_right, cam_up), 0.0f);
          model[3] = glm::vec4(origin, 1.0f);

          push_constant->mvp = vp * model;
          push_constant->color = tint;
          push_constant->entity_id = encoded_entity_id;
          active_image_pipeline->PushConstants(cmd);
          active_image_pipeline->BindDescriptorSets(cmd, {tex_desc});
          vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
        };

        auto encode_id = [&renderer](entt::entity entity) -> uint32_t {
          return (static_cast<uint32_t>(renderer->GetCurrentSceneIndex())
                  << 24) |
                 (static_cast<uint32_t>(entity) + 1);
        };

        auto& assets = Engine::asset_manager();

        // Collect billboard entries once, sorted by layer. Each pass below
        // iterates this list filtered by occlusion mode.
        struct BillboardEntry {
          Scene* scene;
          entt::entity entity;
          int32_t layer;
        };
        std::vector<BillboardEntry> entries;
        scenes.ForEach<BillboardRendererComponent, TransformComponent>(
            [&](Scene& scene, entt::entity entity) {
              auto& b = scene.GetComponent<BillboardRendererComponent>(entity);
              entries.push_back({&scene, entity, b.sort_layer});
            });
        std::sort(entries.begin(), entries.end(),
                  [](const BillboardEntry& a, const BillboardEntry& b) {
                    return a.layer < b.layer;
                  });

        // Draws one billboard entry (handles nine-slice internally). Used by
        // each occlusion-mode pass below.
        auto render_billboard_entry = [&](const BillboardEntry& e,
                                          const glm::vec4& tint) {
          auto& b =
              e.scene->GetComponent<BillboardRendererComponent>(e.entity);
          auto& tc = e.scene->GetComponent<TransformComponent>(e.entity);

          if (!b.texture_handle.IsValid()) return;

          if (b.bound_handle != b.texture_handle || !b.cached_texture) {
            b.cached_texture = assets.Get<Texture>(b.texture_handle);
            b.cached_descriptor = nullptr;
            b.bound_handle = b.texture_handle;
          }
          if (!b.cached_texture || !b.cached_texture->is_allocated_) return;
          if (!b.cached_descriptor) {
            b.cached_descriptor =
                self->GetOrCreateTextureDescriptor(b.cached_texture);
          }
          if (!b.cached_descriptor) return;

          glm::vec3 world_pos = tc.GetWorldPosition();
          glm::vec3 world_scale = tc.GetWorldScale();
          glm::vec2 px_size = b.size * glm::vec2(world_scale.x, world_scale.y);
          glm::vec2 total = pixels_to_world_scale(
              world_pos, px_size, b.min_size, b.max_size);

          glm::vec4 slice{0};
          auto handle = assets.FindBySourcePath(b.cached_texture->path_);
          if (handle.IsValid()) {
            const auto* meta = assets.GetMetadata(handle);
            if (meta) {
              const auto* props = meta->GetProperties<TextureAssetProperties>();
              if (props) slice = props->slice_border;
            }
          }
          bool sliced =
              slice.x > 0 || slice.y > 0 || slice.z > 0 || slice.w > 0;

          glm::vec2 pivot_offset = -(b.pivot - glm::vec2(0.5f)) * total;
          uint32_t eid = encode_id(e.entity);

          if (!sliced) {
            draw_billboard(world_pos, total, pivot_offset, tint,
                           b.cached_descriptor, eid);
            return;
          }

          float tw = std::max(
              static_cast<float>(b.cached_texture->width_), 1.0f);
          float th = std::max(
              static_cast<float>(b.cached_texture->height_), 1.0f);
          glm::vec2 world_per_tex_px{total.x / tw, total.y / th};
          float bL_w = slice.x * world_per_tex_px.x;
          float bT_w = slice.y * world_per_tex_px.y;
          float bR_w = slice.z * world_per_tex_px.x;
          float bB_w = slice.w * world_per_tex_px.y;

          float x0 = -total.x * 0.5f;
          float x3 = total.x * 0.5f;
          float x1 = x0 + bL_w;
          float x2 = x3 - bR_w;
          float y0 = -total.y * 0.5f;
          float y3 = total.y * 0.5f;
          float y1 = y0 + bT_w;
          float y2 = y3 - bB_w;
          float xs[3][2] = {{x0, x1}, {x1, x2}, {x2, x3}};
          float ys[3][2] = {{y0, y1}, {y1, y2}, {y2, y3}};

          for (int row = 0; row < 3; row++) {
            for (int col = 0; col < 3; col++) {
              float cw = xs[col][1] - xs[col][0];
              float ch = ys[row][1] - ys[row][0];
              if (cw <= 0 || ch <= 0) continue;
              glm::vec2 cell_size{cw, ch};
              glm::vec2 cell_center{(xs[col][0] + xs[col][1]) * 0.5f,
                                    (ys[row][0] + ys[row][1]) * 0.5f};
              glm::vec2 cell_offset = pivot_offset + cell_center;
              draw_billboard(world_pos, cell_size, cell_offset, tint,
                             b.cached_descriptor, eid);
            }
          }
        };

        auto bind_image_pipeline = [&](Pipeline* p) {
          if (active_image_pipeline != p) {
            p->Bind(PipelineBindPointGraphics);
            active_image_pipeline = p;
          }
        };

        // Pass 1: standard depth-test pipeline.
        // Renders editor icons + billboards with Disabled or Faded occlusion.
        bind_image_pipeline(pipeline.get());
        if (show_editor_icons) {
          auto draw_icon = [&](const glm::vec3& world_pos,
                               const std::shared_ptr<DescriptorSet>& icon,
                               uint32_t eid) {
            if (!icon) return;
            float s = icon_screen_scale(world_pos);
            draw_billboard(world_pos, {s, s}, {0, 0}, glm::vec4(1.0f), icon,
                           eid);
          };

          scenes.ForEach<CameraComponent, TransformComponent>(
              [&](Scene& scene, entt::entity entity) {
                draw_icon(scene.GetComponent<TransformComponent>(entity)
                              .GetWorldPosition(),
                          camera_icon_desc, encode_id(entity));
              });
          scenes.ForEach<LightDirectComponent, TransformComponent>(
              [&](Scene& scene, entt::entity entity) {
                draw_icon(scene.GetComponent<TransformComponent>(entity)
                              .GetWorldPosition(),
                          point_light_icon_desc, encode_id(entity));
              });
          scenes.ForEach<LightPointComponent, TransformComponent>(
              [&](Scene& scene, entt::entity entity) {
                draw_icon(scene.GetComponent<TransformComponent>(entity)
                              .GetWorldPosition(),
                          point_light_icon_desc, encode_id(entity));
              });
        }
        for (auto& e : entries) {
          auto& b =
              e.scene->GetComponent<BillboardRendererComponent>(e.entity);
          if (b.occlusion == BillboardOcclusionMode::Disabled ||
              b.occlusion == BillboardOcclusionMode::Faded) {
            render_billboard_entry(e, b.tint);
          }
        }

        // Pass 2: no-depth pipeline. AlwaysVisible billboards.
        bool need_no_depth = std::any_of(
            entries.begin(), entries.end(), [&](const BillboardEntry& e) {
              return e.scene->GetComponent<BillboardRendererComponent>(e.entity)
                         .occlusion == BillboardOcclusionMode::AlwaysVisible;
            });
        if (need_no_depth) {
          bind_image_pipeline(pipeline_no_depth.get());
          for (auto& e : entries) {
            auto& b =
                e.scene->GetComponent<BillboardRendererComponent>(e.entity);
            if (b.occlusion == BillboardOcclusionMode::AlwaysVisible) {
              render_billboard_entry(e, b.tint);
            }
          }
        }

        // Pass 3: occluded pipeline (depth > current). Faded billboards are
        // redrawn with reduced alpha to show through geometry.
        bool need_occluded = std::any_of(
            entries.begin(), entries.end(), [&](const BillboardEntry& e) {
              return e.scene->GetComponent<BillboardRendererComponent>(e.entity)
                         .occlusion == BillboardOcclusionMode::Faded;
            });
        if (need_occluded) {
          bind_image_pipeline(pipeline_occluded.get());
          for (auto& e : entries) {
            auto& b =
                e.scene->GetComponent<BillboardRendererComponent>(e.entity);
            if (b.occlusion == BillboardOcclusionMode::Faded) {
              glm::vec4 faded_tint = b.tint;
              faded_tint.a *= b.occluded_alpha;
              render_billboard_entry(e, faded_tint);
            }
          }
        }

        // --- Text billboards ---
        struct TextEntry {
          Scene* scene;
          entt::entity entity;
          int32_t layer;
        };
        std::vector<TextEntry> text_entries;
        scenes.ForEach<BillboardTextComponent, TransformComponent>(
            [&](Scene& scene, entt::entity entity) {
              auto& t = scene.GetComponent<BillboardTextComponent>(entity);
              text_entries.push_back({&scene, entity, t.sort_layer});
            });
        if (!text_entries.empty()) {
          std::sort(text_entries.begin(), text_entries.end(),
                    [](const TextEntry& a, const TextEntry& b) {
                      return a.layer < b.layer;
                    });

          Pipeline* active_text_pipeline = nullptr;
          auto bind_text_pipeline = [&](Pipeline* p) {
            if (active_text_pipeline != p) {
              p->Bind(PipelineBindPointGraphics);
              vkCmdBindVertexBuffers(cmd, 0, 1, &vb_handle, &vb_offset);
              vkCmdBindIndexBuffer(cmd, quad_ib->buffer_handle_, 0,
                                   VK_INDEX_TYPE_UINT32);
              active_text_pipeline = p;
            }
          };

          auto render_text_entry = [&](const TextEntry& e,
                                       const glm::vec4& color) {
            auto& t =
                e.scene->GetComponent<BillboardTextComponent>(e.entity);
            auto& tc = e.scene->GetComponent<TransformComponent>(e.entity);
            if (t.text.empty() || !t.font_handle.IsValid()) return;

            auto font = FontCache::Get(t.font_handle, t.font_size);
            if (!font || !font->IsLoaded()) return;
            font->FlushAtlas();

            auto atlas_view = font->GetAtlasImageView();
            if (!atlas_view) return;

            if (t.cached_atlas_ptr != atlas_view.get() ||
                !t.cached_descriptor) {
              t.cached_descriptor =
                  self->GetOrCreateTextAtlasDescriptor(atlas_view);
              t.cached_atlas_ptr = atlas_view.get();
            }
            if (!t.cached_descriptor) return;

            active_text_pipeline->BindDescriptorSets(cmd,
                                                    {t.cached_descriptor});

            glm::vec3 world_pos = tc.GetWorldPosition();
            glm::vec3 world_scale = tc.GetWorldScale();

            float world_per_px = 1.0f;
            if (is_ortho) {
              float px_per_world =
                  viewport.y / (2.0f * std::max(ortho_size, 0.0001f));
              world_per_px = 1.0f / std::max(px_per_world, 0.0001f);
            } else {
              glm::vec3 fwd = glm::normalize(glm::cross(cam_up, cam_right));
              float depth =
                  std::abs(glm::dot(world_pos - cam_pos, fwd));
              depth = std::max(depth, 0.0001f);
              constexpr float kRefDistance = 1.0f;
              float distance_factor =
                  glm::clamp(kRefDistance / depth, t.min_size, t.max_size);
              float view_height_at_depth =
                  2.0f * depth * tanf(glm::radians(fov * 0.5f));
              world_per_px =
                  view_height_at_depth / std::max(viewport.y, 1.0f) *
                  distance_factor;
            }

            glm::vec2 text_px = font->MeasureText(t.text, t.font_size);
            float align_offset_x = 0.0f;
            if (t.alignment == TextAlignment::Center) {
              align_offset_x = -text_px.x * 0.5f;
            } else if (t.alignment == TextAlignment::Right) {
              align_offset_x = -text_px.x;
            }
            float baseline_y = -font->GetAscent() * 0.5f;

            uint32_t eid = encode_id(e.entity);

            float pen_x = align_offset_x;
            float pen_y = baseline_y;
            float scale_x = world_scale.x;
            float scale_y = world_scale.y;

            for (size_t i = 0; i < t.text.size();) {
              uint32_t cp = Font::DecodeUTF8(t.text, i);
              if (cp == '\n') {
                pen_x = align_offset_x;
                pen_y -= font->GetLineHeight();
                continue;
              }
              const GlyphInfo* g = font->GetGlyph(cp);
              if (!g) continue;
              if (g->size.x > 0 && g->size.y > 0) {
                float gx = pen_x + g->bearing.x;
                float gy = pen_y + g->bearing.y;
                float gw = static_cast<float>(g->size.x);
                float gh = static_cast<float>(g->size.y);

                glm::vec2 center_px{gx + gw * 0.5f, gy - gh * 0.5f};
                glm::vec2 size_px{gw, gh};

                glm::vec2 world_size =
                    size_px * world_per_px * glm::vec2(scale_x, scale_y);
                glm::vec2 world_offset =
                    center_px * world_per_px * glm::vec2(scale_x, scale_y);

                glm::vec3 origin = world_pos +
                                   cam_right * world_offset.x +
                                   cam_up * world_offset.y;

                glm::mat4 model = glm::mat4(1.0f);
                model[0] = glm::vec4(cam_right * world_size.x, 0.0f);
                model[1] = glm::vec4(cam_up * world_size.y, 0.0f);
                model[2] = glm::vec4(glm::cross(cam_right, cam_up), 0.0f);
                model[3] = glm::vec4(origin, 1.0f);

                text_push_constant->mvp = vp * model;
                text_push_constant->color = color;
                text_push_constant->uv_rect = {g->uv_min.x, g->uv_min.y,
                                               g->uv_max.x, g->uv_max.y};
                text_push_constant->entity_id = eid;
                active_text_pipeline->PushConstants(cmd);
                vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
              }
              pen_x += static_cast<float>(g->advance >> 6);
            }
          };

          // Pass 1: depth-test (Disabled + Faded normal)
          bool has_depth_test = std::any_of(
              text_entries.begin(), text_entries.end(),
              [&](const TextEntry& e) {
                auto mode = e.scene->GetComponent<BillboardTextComponent>(
                                 e.entity).occlusion;
                return mode == BillboardOcclusionMode::Disabled ||
                       mode == BillboardOcclusionMode::Faded;
              });
          if (has_depth_test) {
            bind_text_pipeline(text_pipeline.get());
            for (auto& e : text_entries) {
              auto& t =
                  e.scene->GetComponent<BillboardTextComponent>(e.entity);
              if (t.occlusion == BillboardOcclusionMode::Disabled ||
                  t.occlusion == BillboardOcclusionMode::Faded) {
                render_text_entry(e, t.color);
              }
            }
          }

          // Pass 2: no-depth (AlwaysVisible)
          bool has_always = std::any_of(
              text_entries.begin(), text_entries.end(),
              [&](const TextEntry& e) {
                return e.scene->GetComponent<BillboardTextComponent>(e.entity)
                           .occlusion ==
                       BillboardOcclusionMode::AlwaysVisible;
              });
          if (has_always) {
            bind_text_pipeline(text_pipeline_no_depth.get());
            for (auto& e : text_entries) {
              auto& t =
                  e.scene->GetComponent<BillboardTextComponent>(e.entity);
              if (t.occlusion == BillboardOcclusionMode::AlwaysVisible) {
                render_text_entry(e, t.color);
              }
            }
          }

          // Pass 3: occluded (Faded, reduced alpha)
          bool has_faded = std::any_of(
              text_entries.begin(), text_entries.end(),
              [&](const TextEntry& e) {
                return e.scene->GetComponent<BillboardTextComponent>(e.entity)
                           .occlusion == BillboardOcclusionMode::Faded;
              });
          if (has_faded) {
            bind_text_pipeline(text_pipeline_occluded.get());
            for (auto& e : text_entries) {
              auto& t =
                  e.scene->GetComponent<BillboardTextComponent>(e.entity);
              if (t.occlusion == BillboardOcclusionMode::Faded) {
                glm::vec4 faded = t.color;
                faded.a *= t.occluded_alpha;
                render_text_entry(e, faded);
              }
            }
          }
        }
      });

  graph.PassWritesColor(draw_pass, billboard_color);
  graph.PassWritesColor(draw_pass, billboard_entity);
  if (use_msaa) {
    graph.PassWritesColor(draw_pass, billboard_out);
    graph.PassWritesColor(draw_pass, billboard_entity_id);
  }
  graph.SetPassViewport(draw_pass, ctx.viewport_size);
  graph.SetPassClearColor(draw_pass, {0, 0, 0, 0});
  graph.SetPassResolveFn(
      draw_pass,
      [this, pool, draw_pass, billboard_color, billboard_entity, billboard_out,
       billboard_entity_id, use_msaa, rw, rh](RenderGraph& g) {
        auto color = g.GetTexture(billboard_color);
        auto entity = g.GetTexture(billboard_entity);
        auto color_resolve = g.GetTexture(billboard_out);
        auto entity_resolve = g.GetTexture(billboard_entity_id);
        auto depth = pool->GetTexture("geometry.depth_stencil");

        if (draw_color_key_ != color.get() ||
            draw_color_resolve_key_ != color_resolve.get() ||
            draw_entity_id_key_ != entity.get() ||
            draw_entity_id_resolve_key_ != entity_resolve.get() ||
            draw_depth_key_ != depth.get()) {
          if (use_msaa) {
            std::array<AttachmentTexture*, 5> atts{color.get(), depth.get(),
                                                   entity.get(),
                                                   color_resolve.get(),
                                                   entity_resolve.get()};
            draw_framebuffer_ =
                render_pass_->CreateFramebuffer(0, atts, {rw, rh});
          } else {
            std::array<AttachmentTexture*, 3> atts{color.get(), depth.get(),
                                                   entity.get()};
            draw_framebuffer_ =
                render_pass_->CreateFramebuffer(0, atts, {rw, rh});
          }
          auto desc = std::make_shared<DescriptorSet>();
          desc->SetLayout(renderer_->GetDescriptorLayout("Present"));
          desc->AddCombinedImageSampler(0, color_resolve->image_views_[0],
                                        renderer_->GetDefaultLinearSampler());
          desc->Bake();
          draw_output_desc_ = desc;
          draw_color_key_ = color.get();
          draw_color_resolve_key_ = color_resolve.get();
          draw_entity_id_key_ = entity.get();
          draw_entity_id_resolve_key_ = entity_resolve.get();
          draw_depth_key_ = depth.get();
        }
        g.SetPassFramebuffer(draw_pass, draw_framebuffer_);
        // Publish the resolved entity-id texture so the viewport's
        // click-to-select path can read it via CameraResourcePool.
        pool->SetTexture("billboard.entity_id_resolve", entity_resolve);
      });

  // Composite pass: draw previous PipelineOutput then billboard overlay
  RGResource pipeline_out = registry.Get("PipelineOutput");
  RGResource comp_out = graph.DeclareTransient(RGTextureDesc{
      .name = "billboard_comp.color",
      .width = rw,
      .height = rh,
      .format = renderer_->GetSwapChainImageFormat(),
      .samples = SamplingMode::DISABLED,
      .type = AttachmentTextureType::Offscreen,
      .layer_count = 1,
      .sampled = true});

  std::shared_ptr<Pipeline> comp_pipeline = comp_pipeline_;
  std::shared_ptr<Pipeline> comp_blend = comp_blend_pipeline_;
  uint32_t comp_pass = graph.AddPass(
      "BillboardComposite", comp_render_pass_,
      [this, renderer, comp_pipeline, comp_blend](VkCommandBuffer) {
        comp_pipeline->Bind(PipelineBindPointGraphics);
        renderer->DrawFullscreen(comp_pipeline, {comp_input_desc_});
        comp_blend->Bind(PipelineBindPointGraphics);
        renderer->DrawFullscreen(comp_blend, {draw_output_desc_});
      });

  if (pipeline_out.IsValid()) {
    graph.PassReadsTexture(comp_pass, pipeline_out);
  }
  graph.PassReadsTexture(comp_pass, billboard_out);
  graph.PassWritesColor(comp_pass, comp_out);
  graph.SetPassViewport(comp_pass, ctx.viewport_size);
  graph.SetPassClearColor(comp_pass, {0, 0, 0, 1});
  graph.SetPassResolveFn(
      comp_pass,
      [this, pool, comp_pass, comp_out, pipeline_out, rw, rh](RenderGraph& g) {
        auto output = g.GetTexture(comp_out);
        auto input = pipeline_out.IsValid()
                         ? g.GetTexture(pipeline_out)
                         : std::shared_ptr<AttachmentTexture>{};
        auto linear = renderer_->GetDefaultLinearSampler();
        auto present_layout = renderer_->GetDescriptorLayout("Present");

        if (comp_output_key_ != output.get()) {
          comp_framebuffer_ = comp_render_pass_->CreateFramebuffer(
              0, {output.get()}, {rw, rh});
          auto desc = std::make_shared<DescriptorSet>();
          desc->SetLayout(present_layout);
          desc->AddCombinedImageSampler(0, output->image_views_[0], linear);
          desc->Bake();
          comp_output_desc_ = desc;
          comp_output_key_ = output.get();
        }
        g.SetPassFramebuffer(comp_pass, comp_framebuffer_);

        AttachmentTexture* input_key = input ? input.get() : nullptr;
        if (comp_input_key_ != input_key) {
          auto desc = std::make_shared<DescriptorSet>();
          desc->SetLayout(present_layout);
          if (input) {
            desc->AddCombinedImageSampler(0, input->image_views_[0], linear);
          }
          desc->Bake();
          comp_input_desc_ = desc;
          comp_input_key_ = input_key;
        }

        pool->SetTexture("PipelineOutput", output);
        pool->SetDescriptor("PipelineOutputDescriptor", comp_output_desc_);
      });

  registry.Register("PipelineOutput", comp_out);
}

}  // namespace wiesel