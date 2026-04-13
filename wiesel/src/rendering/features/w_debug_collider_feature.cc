
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/features/w_debug_collider_feature.h"
#include "asset/w_asset_manager.h"
#include "audio/w_audio.h"
#include "physics/w_collider.h"
#include "physics/w_rigidbody.h"
#include "rendering/w_camera.h"
#include "rendering/w_descriptor.h"
#include "rendering/w_descriptorlayout.h"
#include "rendering/w_mesh.h"
#include "rendering/w_mesh_renderer.h"
#include "rendering/w_pipeline.h"
#include "rendering/w_renderer.h"
#include "rendering/w_renderpass.h"
#include "scene/w_components.h"
#include "scene/w_scene.h"
#include "util/w_label_texture.h"
#include "w_engine.h"

namespace wiesel {

DebugColliderFeature::DebugColliderFeature(std::shared_ptr<Renderer> renderer)
    : renderer_(std::move(renderer)) {
  // Render pass: 1 color + depth (load existing depth for occlusion)
  SamplingMode msaa = renderer_->options().msaa_mode;

  render_pass_ = std::make_shared<RenderPass>(PassType::ForwardTransparency,
                                              "DebugCollider RenderPass");
  render_pass_->AttachOutput({.type = AttachmentTextureType::Offscreen,
                              .format = renderer_->GetSwapChainImageFormat(),
                              .msaa_mode = msaa});
  render_pass_->AttachOutput({.type = AttachmentTextureType::DepthStencil,
                              .format = renderer_->FindDepthFormat(),
                              .msaa_mode = msaa});
  if (msaa > SamplingMode::DISABLED) {
    render_pass_->AttachOutput({.type = AttachmentTextureType::Resolve,
                                .format = renderer_->GetSwapChainImageFormat(),
                                .msaa_mode = SamplingMode::DISABLED});
  }
  render_pass_->Bake();

  // Shaders
  auto vert = renderer_->CreateShader({ShaderTypeVertex, ShaderLangGLSL, "main",
                                       ShaderSourceSource,
                                       "engine://shaders/debug_collider.vert"});
  auto frag = renderer_->CreateShader({ShaderTypeFragment, ShaderLangGLSL,
                                       "main", ShaderSourceSource,
                                       "engine://shaders/debug_collider.frag"});

  // Wireframe pipeline: line-list, depth test (no write), no alpha blending
  {
    PipelineProperties wire_props{};
    wire_props.sampling_mode = msaa;
    wire_props.cull_mode = CullModeNone;
    wire_props.enable_wireframe = false;
    wire_props.enable_alpha_blending = false;
    wire_props.enable_depth_test = true;
    wire_props.enable_depth_write = false;
    wire_props.depth_compare_op = CompareOpLessOrEqual;
    wire_props.topology = PrimitiveTopology::LineList;
    wire_props.line_width = 1.0f;
    pipeline_ = std::make_shared<Pipeline>(wire_props);
  }

  // Vertex data: just vec3 position
  VkVertexInputBindingDescription binding{};
  binding.binding = 0;
  binding.stride = sizeof(glm::vec3);
  binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  std::vector<VkVertexInputAttributeDescription> attrs(1);
  attrs[0].binding = 0;
  attrs[0].location = 0;
  attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
  attrs[0].offset = 0;

  push_constant_ = std::make_shared<DebugColliderPushConstant>();

  pipeline_->SetVertexData(binding, attrs);
  pipeline_->SetRenderPass(render_pass_);
  pipeline_->AddPushConstant(push_constant_, VK_SHADER_STAGE_VERTEX_BIT |
                                                 VK_SHADER_STAGE_FRAGMENT_BIT);
  pipeline_->AddShader(vert);
  pipeline_->AddShader(frag);
  pipeline_->Bake();

  // No-depth wireframe pipeline for camera frustums (always visible)
  {
    PipelineProperties no_depth_props{};
    no_depth_props.sampling_mode = msaa;
    no_depth_props.cull_mode = CullModeNone;
    no_depth_props.enable_depth_test = false;
    no_depth_props.enable_depth_write = false;
    no_depth_props.topology = PrimitiveTopology::LineList;
    no_depth_props.line_width = 1.0f;
    no_depth_pipeline_ = std::make_shared<Pipeline>(no_depth_props);
  }
  no_depth_pipeline_->SetVertexData(binding, attrs);
  no_depth_pipeline_->SetRenderPass(render_pass_);
  no_depth_pipeline_->AddPushConstant(
      push_constant_,
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
  no_depth_pipeline_->AddShader(vert);
  no_depth_pipeline_->AddShader(frag);
  no_depth_pipeline_->Bake();

  // Textured filled pipeline: triangle-list, no depth write, alpha blend
  auto overlay_vert = renderer_->CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/debug_overlay.vert"});
  auto overlay_frag = renderer_->CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/debug_overlay.frag"});

  // Overlay vertex: vec3 position + vec2 uv
  VkVertexInputBindingDescription overlay_binding{};
  overlay_binding.binding = 0;
  overlay_binding.stride = sizeof(OverlayVertex);
  overlay_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  std::vector<VkVertexInputAttributeDescription> overlay_attrs(2);
  overlay_attrs[0].binding = 0;
  overlay_attrs[0].location = 0;
  overlay_attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
  overlay_attrs[0].offset = offsetof(OverlayVertex, position);
  overlay_attrs[1].binding = 0;
  overlay_attrs[1].location = 1;
  overlay_attrs[1].format = VK_FORMAT_R32G32_SFLOAT;
  overlay_attrs[1].offset = offsetof(OverlayVertex, uv);

  // Descriptor layout for label texture sampler
  overlay_desc_layout_ = std::make_shared<DescriptorSetLayout>();
  overlay_desc_layout_->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   VK_SHADER_STAGE_FRAGMENT_BIT);
  overlay_desc_layout_->Bake();

  {
    PipelineProperties filled_props{};
    filled_props.sampling_mode = msaa;
    filled_props.cull_mode = CullModeNone;
    filled_props.enable_alpha_blending = true;
    filled_props.enable_depth_test = true;
    filled_props.enable_depth_write = false;
    filled_props.depth_compare_op = CompareOpLessOrEqual;
    filled_pipeline_ = std::make_shared<Pipeline>(filled_props);
  }
  filled_pipeline_->SetVertexData(overlay_binding, overlay_attrs);
  filled_pipeline_->SetRenderPass(render_pass_);
  filled_pipeline_->AddPushConstant(
      push_constant_,
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
  filled_pipeline_->AddInputLayout(overlay_desc_layout_);
  filled_pipeline_->AddShader(overlay_vert);
  filled_pipeline_->AddShader(overlay_frag);
  filled_pipeline_->Bake();

  // Composite render pass: blend debug overlay onto PipelineOutput
  comp_render_pass_ = std::make_shared<RenderPass>(
      PassType::PostProcess, "DebugColliderComposite RenderPass");
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

  comp_pipeline_ = std::make_shared<Pipeline>(PipelineProperties{
      SamplingMode::DISABLED, CullModeBack, false, true, false, false});
  comp_pipeline_->SetRenderPass(comp_render_pass_);
  comp_pipeline_->AddInputLayout(renderer_->GetDescriptorLayout("Skybox"));
  comp_pipeline_->AddShader(fullscreen_vert);
  comp_pipeline_->AddShader(quad_frag);
  comp_pipeline_->Bake();

  // Generate wireframe geometry
  GenerateBoxGeometry();
  GenerateSphereGeometry();
  GenerateFilledBoxGeometry();
  GenerateFilledSphereGeometry();
}

void DebugColliderFeature::GenerateBoxGeometry() {
  // Unit box: 8 corners of [-0.5, 0.5]^3
  std::vector<glm::vec3> vertices = {
      {-0.5f, -0.5f, -0.5f},  // 0
      {0.5f, -0.5f, -0.5f},   // 1
      {0.5f, 0.5f, -0.5f},    // 2
      {-0.5f, 0.5f, -0.5f},   // 3
      {-0.5f, -0.5f, 0.5f},   // 4
      {0.5f, -0.5f, 0.5f},    // 5
      {0.5f, 0.5f, 0.5f},     // 6
      {-0.5f, 0.5f, 0.5f},    // 7
  };

  // 12 edges as line-list (24 indices)
  std::vector<Index> indices = {
      0, 1, 1, 2, 2, 3, 3, 0,  // front face
      4, 5, 5, 6, 6, 7, 7, 4,  // back face
      0, 4, 1, 5, 2, 6, 3, 7,  // connecting edges
  };

  box_vertex_buffer_ = renderer_->CreateVertexBuffer(
      "DebugColliderFeature::box_vertex_buffer_", vertices);
  box_index_buffer_ = renderer_->CreateIndexBuffer(
      "DebugColliderFeature::box_index_buffer_", indices);
  box_index_count_ = static_cast<uint32_t>(indices.size());
}

void DebugColliderFeature::GenerateSphereGeometry() {
  // 3 orthogonal circle rings
  constexpr int segments = 32;
  std::vector<glm::vec3> vertices;
  std::vector<Index> indices;

  auto add_ring = [&](int axis0, int axis1, int axis2) {
    uint32_t base = static_cast<uint32_t>(vertices.size());
    for (int i = 0; i < segments; i++) {
      float angle = glm::two_pi<float>() * static_cast<float>(i) /
                    static_cast<float>(segments);
      glm::vec3 v(0.0f);
      v[axis0] = cosf(angle) * 0.5f;
      v[axis1] = sinf(angle) * 0.5f;
      v[axis2] = 0.0f;
      vertices.push_back(v);
    }
    for (int i = 0; i < segments; i++) {
      indices.push_back(base + i);
      indices.push_back(base + (i + 1) % segments);
    }
  };

  add_ring(0, 1, 2);  // XY ring
  add_ring(0, 2, 1);  // XZ ring
  add_ring(1, 2, 0);  // YZ ring

  sphere_vertex_buffer_ = renderer_->CreateVertexBuffer(
      "DebugColliderFeature::sphere_vertex_buffer_", vertices);
  sphere_index_buffer_ = renderer_->CreateIndexBuffer(
      "DebugColliderFeature::sphere_index_buffer_", indices);
  sphere_index_count_ = static_cast<uint32_t>(indices.size());
}

void DebugColliderFeature::GenerateFilledBoxGeometry() {
  // Each face has its own 4 vertices with UVs so the texture maps per face
  std::vector<OverlayVertex> vertices = {
      // Front face (-Z)
      {{-0.5f, -0.5f, -0.5f}, {0, 1}},
      {{0.5f, -0.5f, -0.5f}, {1, 1}},
      {{0.5f, 0.5f, -0.5f}, {1, 0}},
      {{-0.5f, 0.5f, -0.5f}, {0, 0}},
      // Back face (+Z)
      {{0.5f, -0.5f, 0.5f}, {0, 1}},
      {{-0.5f, -0.5f, 0.5f}, {1, 1}},
      {{-0.5f, 0.5f, 0.5f}, {1, 0}},
      {{0.5f, 0.5f, 0.5f}, {0, 0}},
      // Left face (-X)
      {{-0.5f, -0.5f, 0.5f}, {0, 1}},
      {{-0.5f, -0.5f, -0.5f}, {1, 1}},
      {{-0.5f, 0.5f, -0.5f}, {1, 0}},
      {{-0.5f, 0.5f, 0.5f}, {0, 0}},
      // Right face (+X)
      {{0.5f, -0.5f, -0.5f}, {0, 1}},
      {{0.5f, -0.5f, 0.5f}, {1, 1}},
      {{0.5f, 0.5f, 0.5f}, {1, 0}},
      {{0.5f, 0.5f, -0.5f}, {0, 0}},
      // Top face (+Y)
      {{-0.5f, 0.5f, -0.5f}, {0, 1}},
      {{0.5f, 0.5f, -0.5f}, {1, 1}},
      {{0.5f, 0.5f, 0.5f}, {1, 0}},
      {{-0.5f, 0.5f, 0.5f}, {0, 0}},
      // Bottom face (-Y)
      {{-0.5f, -0.5f, 0.5f}, {0, 1}},
      {{0.5f, -0.5f, 0.5f}, {1, 1}},
      {{0.5f, -0.5f, -0.5f}, {1, 0}},
      {{-0.5f, -0.5f, -0.5f}, {0, 0}},
  };

  std::vector<Index> indices;
  for (uint32_t face = 0; face < 6; face++) {
    uint32_t base = face * 4;
    indices.push_back(base + 0);
    indices.push_back(base + 1);
    indices.push_back(base + 2);
    indices.push_back(base + 2);
    indices.push_back(base + 3);
    indices.push_back(base + 0);
  }

  filled_box_vb_ = renderer_->CreateVertexBuffer(
      "DebugColliderFeature::filled_box_vb_", vertices);
  filled_box_ib_ = renderer_->CreateIndexBuffer(
      "DebugColliderFeature::filled_box_ib_", indices);
  filled_box_ic_ = static_cast<uint32_t>(indices.size());
}

void DebugColliderFeature::GenerateFilledSphereGeometry() {
  constexpr int stacks = 12;
  constexpr int slices = 16;
  std::vector<OverlayVertex> vertices;
  std::vector<Index> indices;

  for (int i = 0; i <= stacks; i++) {
    float phi =
        glm::pi<float>() * static_cast<float>(i) / static_cast<float>(stacks);
    float v = static_cast<float>(i) / static_cast<float>(stacks);
    for (int j = 0; j <= slices; j++) {
      float theta = glm::two_pi<float>() * static_cast<float>(j) /
                    static_cast<float>(slices);
      float u = static_cast<float>(j) / static_cast<float>(slices);
      OverlayVertex vert;
      vert.position.x = sinf(phi) * cosf(theta) * 0.5f;
      vert.position.y = cosf(phi) * 0.5f;
      vert.position.z = sinf(phi) * sinf(theta) * 0.5f;
      vert.uv = {u, v};
      vertices.push_back(vert);
    }
  }

  for (int i = 0; i < stacks; i++) {
    for (int j = 0; j < slices; j++) {
      uint32_t a = i * (slices + 1) + j;
      uint32_t b = a + slices + 1;
      indices.push_back(a);
      indices.push_back(b);
      indices.push_back(a + 1);
      indices.push_back(a + 1);
      indices.push_back(b);
      indices.push_back(b + 1);
    }
  }

  filled_sphere_vb_ = renderer_->CreateVertexBuffer(
      "DebugColliderFeature::filled_sphere_vb_", vertices);
  filled_sphere_ib_ = renderer_->CreateIndexBuffer(
      "DebugColliderFeature::filled_sphere_ib_", indices);
  filled_sphere_ic_ = static_cast<uint32_t>(indices.size());
}

std::shared_ptr<DescriptorSet> DebugColliderFeature::GetOrCreateLabelDescriptor(
    const std::string& label, const glm::vec4& bg_color,
    const glm::vec4& text_color) {
  auto it = label_descriptors_.find(label);
  if (it != label_descriptors_.end()) {
    return it->second;
  }

  auto texture = GetOrCreateLabelTexture(label, label, bg_color, text_color);
  if (!texture) {
    return nullptr;
  }

  auto desc = std::make_shared<DescriptorSet>();
  desc->SetLayout(overlay_desc_layout_);
  desc->AddCombinedImageSampler(0, texture->image_view_,
                                renderer_->GetDefaultLinearSampler());
  desc->Bake();

  label_descriptors_[label] = desc;
  return desc;
}

void DebugColliderFeature::SetupResources(RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("DebugColliderFeature::SetupResources");
  auto& pool = ctx.resources;
  auto& renderer = *renderer_;
  uint32_t rw = static_cast<uint32_t>(ctx.viewport_size.x);
  uint32_t rh = static_cast<uint32_t>(ctx.viewport_size.y);

  // Wireframe offscreen texture
  SamplingMode msaa = renderer.options().msaa_mode;
  bool use_msaa = msaa > SamplingMode::DISABLED;

  pool.SetTexture("debug_collider.color",
                  renderer.CreateAttachmentTexture(
                      {rw, rh, AttachmentTextureType::Offscreen, 1,
                       renderer.GetSwapChainImageFormat(), msaa, true}));

  if (use_msaa) {
    pool.SetTexture("debug_collider.color_resolve",
                    renderer.CreateAttachmentTexture(
                        {rw, rh, AttachmentTextureType::Resolve, 1,
                         renderer.GetSwapChainImageFormat(),
                         SamplingMode::DISABLED, true}));
    std::array<AttachmentTexture*, 3> attachments{
        pool.GetTexture("debug_collider.color").get(),
        pool.GetTexture("geometry.depth_stencil").get(),
        pool.GetTexture("debug_collider.color_resolve").get()};
    pool.SetFramebuffer("debug_collider", render_pass_->CreateFramebuffer(
                                              0, attachments, {rw, rh}));
  } else {
    pool.SetTexture("debug_collider.color_resolve",
                    pool.GetTexture("debug_collider.color"));
    std::array<AttachmentTexture*, 2> attachments{
        pool.GetTexture("debug_collider.color").get(),
        pool.GetTexture("geometry.depth_stencil").get()};
    pool.SetFramebuffer("debug_collider", render_pass_->CreateFramebuffer(
                                              0, attachments, {rw, rh}));
  }

  // Descriptor to read wireframe output (use resolved texture)
  auto debug_output_desc = std::make_shared<DescriptorSet>();
  debug_output_desc->SetLayout(renderer.GetDescriptorLayout("Present"));
  debug_output_desc->AddCombinedImageSampler(
      0, pool.GetTexture("debug_collider.color_resolve")->image_views_[0],
      renderer.GetDefaultLinearSampler());
  debug_output_desc->Bake();
  pool.SetDescriptor("debug_collider.output", debug_output_desc);

  // Composite output texture
  pool.SetTexture(
      "debug_collider_comp.color",
      renderer.CreateAttachmentTexture(
          {rw, rh, AttachmentTextureType::Offscreen, 1,
           renderer.GetSwapChainImageFormat(), SamplingMode::DISABLED, true}));

  std::array<AttachmentTexture*, 1> comp_attachments{
      pool.GetTexture("debug_collider_comp.color").get()};
  pool.SetFramebuffer(
      "debug_collider_comp",
      comp_render_pass_->CreateFramebuffer(0, comp_attachments, {rw, rh}));

  // Descriptor to read previous PipelineOutput
  auto comp_input_desc = std::make_shared<DescriptorSet>();
  comp_input_desc->SetLayout(renderer.GetDescriptorLayout("Present"));
  comp_input_desc->AddCombinedImageSampler(
      0, pool.GetTexture("PipelineOutput")->image_views_[0],
      renderer.GetDefaultLinearSampler());
  comp_input_desc->Bake();
  pool.SetDescriptor("debug_collider_comp.input", comp_input_desc);

  // Update PipelineOutput for downstream features
  auto comp_output_desc = std::make_shared<DescriptorSet>();
  comp_output_desc->SetLayout(renderer.GetDescriptorLayout("Present"));
  comp_output_desc->AddCombinedImageSampler(
      0, pool.GetTexture("debug_collider_comp.color")->image_views_[0],
      renderer.GetDefaultLinearSampler());
  comp_output_desc->Bake();
  pool.SetTexture("PipelineOutput",
                  pool.GetTexture("debug_collider_comp.color"));
  pool.SetDescriptor("PipelineOutputDescriptor", comp_output_desc);
}

void DebugColliderFeature::AddPasses(RenderGraph& graph,
                                     RenderResourceRegistry& registry,
                                     RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("DebugColliderFeature::AddPasses");

  CameraResourcePool* pool = &ctx.resources;
  std::shared_ptr<Renderer> renderer = renderer_;
  MultiScene& scenes = ctx.scenes;
  auto pipeline = pipeline_;
  auto filled_pipe = filled_pipeline_;
  auto push_constant = push_constant_;
  auto box_vb = box_vertex_buffer_;
  auto box_ib = box_index_buffer_;
  auto box_ic = box_index_count_;
  auto sphere_vb = sphere_vertex_buffer_;
  auto sphere_ib = sphere_index_buffer_;
  auto sphere_ic = sphere_index_count_;
  auto fbox_vb = filled_box_vb_;
  auto fbox_ib = filled_box_ib_;
  auto fbox_ic = filled_box_ic_;
  auto fsphere_vb = filled_sphere_vb_;
  auto fsphere_ib = filled_sphere_ib_;
  auto fsphere_ic = filled_sphere_ic_;
  bool show_colliders = renderer_->options().show_colliders;
  bool show_triggers = renderer_->options().show_triggers;
  bool show_reverb = renderer_->options().show_reverb_zones;
  bool show_cameras = renderer_->options().show_cameras && ctx.is_external;
  bool show_bounds = renderer_->options().show_bounds;

  // Pre-create label descriptors
  auto trigger_desc = show_triggers
                          ? GetOrCreateLabelDescriptor(
                                "TRIGGER", {0.8f, 0.4f, 0, 0.4f}, {1, 1, 1, 1})
                          : nullptr;
  auto reverb_desc =
      show_reverb ? GetOrCreateLabelDescriptor(
                        "REVERB ZONE", {0.4f, 0.15f, 0.6f, 0.4f}, {1, 1, 1, 1})
                  : nullptr;

  // Compute VP matrix from camera
  glm::mat4 vp = ctx.camera.projection * ctx.camera.view_matrix;

  // Build heightfield debug geometry once (cached)
  if (show_colliders && !hf_cache_valid_) {
    hf_cache_.clear();
    scenes.ForEach<HeightfieldColliderComponent, TransformComponent>(
        [&](Scene& scene, entt::entity entity) {
          auto& hf = scene.GetComponent<HeightfieldColliderComponent>(entity);
          auto& tc = scene.GetComponent<TransformComponent>(entity);
          if (hf.width < 2 || hf.length < 2 || hf.height_data.empty()) {
            return;
          }

          std::vector<glm::vec3> vertices;
          std::vector<Index> indices;
          vertices.reserve(hf.width * hf.length);

          float half_w = (hf.width - 1) * 0.5f;
          float half_l = (hf.length - 1) * 0.5f;
          for (int row = 0; row < hf.length; row++) {
            for (int col = 0; col < hf.width; col++) {
              float x = (col - half_w) * hf.scale.x;
              float y = hf.height_data[row * hf.width + col] * hf.scale.y;
              float z = (row - half_l) * hf.scale.z;
              vertices.push_back({x, y, z});
            }
          }

          for (int row = 0; row < hf.length; row++) {
            for (int col = 0; col < hf.width - 1; col++) {
              indices.push_back(row * hf.width + col);
              indices.push_back(row * hf.width + col + 1);
            }
          }
          for (int row = 0; row < hf.length - 1; row++) {
            for (int col = 0; col < hf.width; col++) {
              indices.push_back(row * hf.width + col);
              indices.push_back((row + 1) * hf.width + col);
            }
          }

          CachedDebugData data;
          data.vb =
              renderer_->CreateVertexBuffer("CachedDebugData::vb", vertices);
          data.ib =
              renderer_->CreateIndexBuffer("CachedDebugData::ib", indices);
          data.index_count = static_cast<uint32_t>(indices.size());
          data.model = glm::translate(glm::mat4(1.0f), tc.GetPosition());
          hf_cache_.push_back(std::move(data));
        });
    hf_cache_valid_ = true;
  }
  if (!show_colliders) {
    hf_cache_.clear();
    hf_cache_valid_ = false;
  }
  std::vector<CachedDebugData>& hf_data = hf_cache_;

  // Build mesh collider debug geometry once (cached)
  if (show_colliders && !mesh_collider_cache_valid_) {
    mesh_collider_cache_.clear();
    scenes.ForEach<MeshColliderComponent, MeshRendererComponent,
                   TransformComponent>([&](Scene& scene, entt::entity entity) {
      auto& mr = scene.GetComponent<MeshRendererComponent>(entity);
      if (!mr.model_handle.IsValid()) {
        return;
      }
      const std::shared_ptr<Model>& model_data =
          Engine::asset_manager().GetOrLoad<Model>(mr.model_handle);
      if (!model_data) {
        return;
      }

      std::vector<glm::vec3> vertices;
      std::vector<Index> tri_indices;
      model_data->GetCollisionGeometry(vertices, tri_indices);

      if (tri_indices.empty()) {
        return;
      }

      // Convert triangle indices to line-list edges for wireframe
      std::vector<Index> line_indices;
      line_indices.reserve(tri_indices.size() * 2);
      for (size_t i = 0; i + 2 < tri_indices.size(); i += 3) {
        Index a = tri_indices[i];
        Index b = tri_indices[i + 1];
        Index c = tri_indices[i + 2];
        line_indices.push_back(a);
        line_indices.push_back(b);
        line_indices.push_back(b);
        line_indices.push_back(c);
        line_indices.push_back(c);
        line_indices.push_back(a);
      }

      auto& tc = scene.GetComponent<TransformComponent>(entity);
      glm::mat4 model = glm::translate(glm::mat4(1.0f), tc.GetWorldPosition()) *
                        glm::scale(glm::mat4(1.0f), tc.GetScale());

      CachedDebugData data;
      data.vb = renderer_->CreateVertexBuffer("CachedDebugData::vb", vertices);
      data.ib =
          renderer_->CreateIndexBuffer("CachedDebugData::ib", line_indices);
      data.index_count = static_cast<uint32_t>(line_indices.size());
      data.model = model;
      mesh_collider_cache_.push_back(std::move(data));
    });
    mesh_collider_cache_valid_ = true;
  }
  if (!show_colliders) {
    mesh_collider_cache_.clear();
    mesh_collider_cache_valid_ = false;
  }
  std::vector<CachedDebugData>& mesh_data = mesh_collider_cache_;

  // Import resources
  RGResource debug_out = graph.ImportTexture(
      "DebugCollidersOut", pool->GetTexture("debug_collider.color_resolve"));

  // Wireframe draw pass
  auto no_depth_pipe = no_depth_pipeline_;

  uint32_t draw_pass = graph.AddPass(
      "DebugColliders", render_pass_,
      [pipeline, filled_pipe, no_depth_pipe, push_constant, &scenes, renderer,
       vp, show_colliders, show_triggers, show_reverb, show_cameras,
       show_bounds, box_vb, box_ib, box_ic, sphere_vb, sphere_ib, sphere_ic,
       fbox_vb, fbox_ib, fbox_ic, fsphere_vb, fsphere_ib, fsphere_ic,
       trigger_desc, reverb_desc, &hf_data, &mesh_data](VkCommandBuffer cmd) {
        if (!show_colliders && !show_triggers && !show_reverb &&
            !show_cameras && !show_bounds) {
          return;
        }

        auto draw_wireframe = [&](std::shared_ptr<MemoryBuffer> vb,
                                  std::shared_ptr<IndexBuffer> ib,
                                  uint32_t index_count, const glm::mat4& model,
                                  const glm::vec4& color) {
          push_constant->mvp = vp * model;
          push_constant->model = model;
          push_constant->color = color;
          pipeline->PushConstants(cmd);

          VkBuffer buffers[] = {vb->buffer_handle_};
          VkDeviceSize offsets[] = {0};
          vkCmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);
          vkCmdBindIndexBuffer(cmd, ib->buffer_handle_, 0, ib->index_type_);
          vkCmdDrawIndexed(cmd, index_count, 1, 0, 0, 0);
        };

        auto draw_filled = [&](std::shared_ptr<MemoryBuffer> vb,
                               std::shared_ptr<IndexBuffer> ib,
                               uint32_t index_count, const glm::mat4& model,
                               const glm::vec4& color,
                               std::shared_ptr<DescriptorSet> label_desc) {
          filled_pipe->Bind(PipelineBindPointGraphics);
          push_constant->mvp = vp * model;
          push_constant->model = model;
          push_constant->color = color;
          filled_pipe->PushConstants(cmd);

          if (label_desc) {
            filled_pipe->BindDescriptorSets(cmd, {label_desc});
          }

          VkBuffer buffers[] = {vb->buffer_handle_};
          VkDeviceSize offsets[] = {0};
          vkCmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);
          vkCmdBindIndexBuffer(cmd, ib->buffer_handle_, 0, ib->index_type_);
          vkCmdDrawIndexed(cmd, index_count, 1, 0, 0, 0);
        };

        // Colliders and triggers share the same component types,
        // differentiated by is_trigger flag.
        if (show_colliders || show_triggers) {
          constexpr glm::vec4 static_wire(0.0f, 1.0f, 0.0f,
                                          1.0f);  // green: static collider
          constexpr glm::vec4 dynamic_wire(0.2f, 0.6f, 1.0f,
                                           1.0f);  // blue: has rigidbody
          constexpr glm::vec4 kinematic_wire(0.9f, 0.9f, 0.2f,
                                             1.0f);  // yellow: kinematic
          constexpr glm::vec4 trigger_fill(1.0f, 0.6f, 0.0f, 0.15f);
          constexpr glm::vec4 trigger_wire(1.0f, 0.6f, 0.0f, 1.0f);
          constexpr glm::vec4 terrain_wire(0.0f, 1.0f, 1.0f, 1.0f);

          // Helper: pick wireframe color based on rigidbody state
          auto get_collider_color = [&](Scene& scene,
                                        entt::entity entity) -> glm::vec4 {
            if (!scene.HasComponent<RigidBodyComponent>(entity)) {
              return static_wire;
            }
            auto& rb = scene.GetComponent<RigidBodyComponent>(entity);
            if (rb.type == RigidBodyType::Kinematic) {
              return kinematic_wire;
            }
            return dynamic_wire;
          };

          // Helper: draw a collider or trigger with the appropriate style
          auto draw_collider = [&](Scene& scene, entt::entity entity,
                                   bool is_trigger, const glm::mat4& model,
                                   std::shared_ptr<MemoryBuffer> wire_vb,
                                   std::shared_ptr<IndexBuffer> wire_ib,
                                   uint32_t wire_ic,
                                   std::shared_ptr<MemoryBuffer> fill_vb,
                                   std::shared_ptr<IndexBuffer> fill_ib,
                                   uint32_t fill_ic) {
            if (is_trigger && !show_triggers) {
              return;
            }
            if (!is_trigger && !show_colliders) {
              return;
            }

            if (is_trigger) {
              draw_filled(fill_vb, fill_ib, fill_ic, model, trigger_fill,
                          trigger_desc);
              pipeline->Bind(PipelineBindPointGraphics);
              draw_wireframe(wire_vb, wire_ib, wire_ic, model, trigger_wire);
            } else {
              pipeline->Bind(PipelineBindPointGraphics);
              draw_wireframe(wire_vb, wire_ib, wire_ic, model,
                             get_collider_color(scene, entity));
            }
          };

          // Box colliders/triggers
          scenes.ForEach<BoxColliderComponent, TransformComponent>(
              [&](Scene& scene, entt::entity entity) {
                auto& box = scene.GetComponent<BoxColliderComponent>(entity);
                auto& tc = scene.GetComponent<TransformComponent>(entity);
                glm::mat4 model =
                    glm::translate(glm::mat4(1.0f),
                                   tc.GetWorldPosition() + box.offset) *
                    glm::scale(glm::mat4(1.0f), box.half_extents * 2.0f);
                draw_collider(scene, entity, box.is_trigger, model, box_vb,
                              box_ib, box_ic, fbox_vb, fbox_ib, fbox_ic);
              });

          // Sphere colliders/triggers
          scenes.ForEach<SphereColliderComponent, TransformComponent>(
              [&](Scene& scene, entt::entity entity) {
                auto& sphere =
                    scene.GetComponent<SphereColliderComponent>(entity);
                auto& tc = scene.GetComponent<TransformComponent>(entity);
                glm::mat4 model =
                    glm::translate(glm::mat4(1.0f),
                                   tc.GetWorldPosition() + sphere.offset) *
                    glm::scale(glm::mat4(1.0f),
                               glm::vec3(sphere.radius * 2.0f));
                draw_collider(scene, entity, sphere.is_trigger, model,
                              sphere_vb, sphere_ib, sphere_ic, fsphere_vb,
                              fsphere_ib, fsphere_ic);
              });

          // Capsule colliders/triggers
          scenes.ForEach<CapsuleColliderComponent, TransformComponent>(
              [&](Scene& scene, entt::entity entity) {
                auto& cap =
                    scene.GetComponent<CapsuleColliderComponent>(entity);
                auto& tc = scene.GetComponent<TransformComponent>(entity);
                float total_h = cap.height + cap.radius * 2.0f;
                glm::vec3 scale_vec;
                switch (cap.axis) {
                  case CapsuleAxis::X:
                    scale_vec = {total_h, cap.radius * 2.0f, cap.radius * 2.0f};
                    break;
                  case CapsuleAxis::Y:
                    scale_vec = {cap.radius * 2.0f, total_h, cap.radius * 2.0f};
                    break;
                  case CapsuleAxis::Z:
                    scale_vec = {cap.radius * 2.0f, cap.radius * 2.0f, total_h};
                    break;
                }
                glm::mat4 model =
                    glm::translate(glm::mat4(1.0f),
                                   tc.GetWorldPosition() + cap.offset) *
                    glm::scale(glm::mat4(1.0f), scale_vec);
                draw_collider(scene, entity, cap.is_trigger, model, sphere_vb,
                              sphere_ib, sphere_ic, fsphere_vb, fsphere_ib,
                              fsphere_ic);
              });

          // Heightfield colliders: wireframe only
          if (show_colliders) {
            pipeline->Bind(PipelineBindPointGraphics);
            for (const CachedDebugData& hf : hf_data) {
              draw_wireframe(hf.vb, hf.ib, hf.index_count, hf.model,
                             terrain_wire);
            }
          }

          // Mesh colliders: wireframe only
          if (show_colliders) {
            pipeline->Bind(PipelineBindPointGraphics);
            for (const CachedDebugData& mc : mesh_data) {
              draw_wireframe(mc.vb, mc.ib, mc.index_count, mc.model,
                             static_wire);
            }
          }
        }

        // Reverb zones: filled + wireframe
        if (show_reverb) {
          const glm::vec4 reverb_fill(0.5f, 0.2f, 0.8f, 0.12f);
          const glm::vec4 reverb_wire(0.6f, 0.3f, 0.9f, 1.0f);
          const glm::vec4 reverb_active_fill(0.7f, 0.3f, 1.0f, 0.2f);
          const glm::vec4 reverb_active_wire(0.9f, 0.5f, 1.0f, 1.0f);

          scenes.ForEach<ReverbZoneComponent, TransformComponent>(
              [&](Scene& scene, entt::entity entity) {
                auto& zone = scene.GetComponent<ReverbZoneComponent>(entity);
                auto& tc = scene.GetComponent<TransformComponent>(entity);

                glm::mat4 model =
                    glm::translate(glm::mat4(1.0f), tc.GetPosition()) *
                    glm::scale(glm::mat4(1.0f), glm::vec3(zone.radius * 2.0f));

                draw_filled(fsphere_vb, fsphere_ib, fsphere_ic, model,
                            zone.active_ ? reverb_active_fill : reverb_fill,
                            reverb_desc);
                pipeline->Bind(PipelineBindPointGraphics);
                draw_wireframe(sphere_vb, sphere_ib, sphere_ic, model,
                               zone.active_ ? reverb_active_wire : reverb_wire);
              });
        }

        // Camera frustum wireframes (editor scene view only)
        if (show_cameras) {
          no_depth_pipe->Bind(PipelineBindPointGraphics);
          glm::vec4 cam_color = {0.7f, 0.7f, 0.7f, 0.6f};
          glm::vec4 cam_disabled_color = {0.5f, 0.5f, 0.5f, 0.3f};

          scenes.ForEach<CameraComponent, TransformComponent>(
              [&](Scene& scene, entt::entity cam_entity) {
                auto& cam = scene.GetComponent<CameraComponent>(cam_entity);
                auto& cam_transform =
                    scene.GetComponent<TransformComponent>(cam_entity);

                float vis_far = std::min(cam.far_plane, 50.0f);
                float aspect =
                    cam.viewport_size.x / std::max(1.0f, cam.viewport_size.y);

                // Compute frustum corners directly in view space (no matrix inversion)
                float nw, nh, fw, fh;
                if (cam.projection_mode == ProjectionMode::Perspective) {
                  float tan_half_fov =
                      tanf(glm::radians(cam.field_of_view * 0.5f));
                  nh = cam.near_plane * tan_half_fov;
                  nw = nh * aspect;
                  fh = vis_far * tan_half_fov;
                  fw = fh * aspect;
                } else {
                  float size = cam.ortho_size;
                  nw = fw = size * aspect;
                  nh = fh = size;
                }

                // 8 corners in view space (camera looks down +Z)
                float n = cam.near_plane;
                float f = vis_far;
                glm::vec3 view_corners[8] = {
                    {-nw, -nh, n}, {nw, -nh, n}, {nw, nh, n}, {-nw, nh, n},
                    {-fw, -fh, f}, {fw, -fh, f}, {fw, fh, f}, {-fw, fh, f},
                };

                glm::mat4 cam_world = cam.inv_view_matrix;

                // Build a model matrix that maps unit box corners to the frustum
                // corners. Since the frustum is a truncated pyramid, we can't do
                // this with one affine matrix. Instead, compute clip-space corners
                // and draw line segments directly.
                //
                // Map box corner index to frustum corner:
                // box[-0.5,-0.5,-0.5] = near-bottom-left  = view_corners[0]
                // box[ 0.5,-0.5,-0.5] = near-bottom-right = view_corners[1]
                // box[ 0.5, 0.5,-0.5] = near-top-right    = view_corners[2]
                // box[-0.5, 0.5,-0.5] = near-top-left     = view_corners[3]
                // box[-0.5,-0.5, 0.5] = far-bottom-left   = view_corners[4]
                // box[ 0.5,-0.5, 0.5] = far-bottom-right  = view_corners[5]
                // box[ 0.5, 0.5, 0.5] = far-top-right     = view_corners[6]
                // box[-0.5, 0.5, 0.5] = far-top-left      = view_corners[7]

                // We need to draw 12 edges. Since we can't use the unit box
                // approach (non-affine mapping), create a temporary vertex buffer.
                std::vector<glm::vec3> frustum_verts(8);
                for (int i = 0; i < 8; i++) {
                  frustum_verts[i] =
                      glm::vec3(cam_world * glm::vec4(view_corners[i], 1.0f));
                }

                auto frust_vb = renderer->CreateVertexBuffer(
                    "DebugColliderFeature::frustum_vb", frustum_verts);

                push_constant->mvp = vp;
                push_constant->model = glm::mat4(1.0f);
                push_constant->color =
                    cam.enabled ? cam_color : cam_disabled_color;
                no_depth_pipe->PushConstants(cmd);

                VkBuffer buffers[] = {frust_vb->buffer_handle_};
                VkDeviceSize offsets[] = {0};
                vkCmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);
                vkCmdBindIndexBuffer(cmd, box_ib->buffer_handle_, 0,
                                     box_ib->index_type_);
                vkCmdDrawIndexed(cmd, box_ic, 1, 0, 0, 0);
              });
        }

        // --- Culling bounds ---
        if (show_bounds) {
          pipeline->Bind(PipelineBindPointGraphics);
          auto& assets = Engine::asset_manager();
          glm::vec4 bounds_color(0.2f, 0.8f, 0.2f, 0.7f);
          glm::vec4 skel_bounds_color(0.8f, 0.6f, 0.2f, 0.7f);

          // MeshRendererComponent bounds
          scenes.ForEach<MeshRendererComponent, TransformComponent>(
              [&](Scene& scene, entt::entity entity) {
                auto& mr = scene.GetComponent<MeshRendererComponent>(entity);
                if (!mr.model_handle.IsValid() || mr.mesh_index < 0) {
                  return;
                }
                auto& transform =
                    scene.GetComponent<TransformComponent>(entity);
                const auto& md = assets.GetOrLoad<Model>(mr.model_handle);
                if (!md ||
                    mr.mesh_index >= static_cast<int32_t>(md->meshes.size())) {
                  return;
                }
                AABB wb = md->meshes[mr.mesh_index]->bounds.Transformed(
                    transform.GetTransformMatrix());
                glm::vec3 center = wb.Center();
                glm::vec3 extents = wb.Extents();
                glm::mat4 m = glm::translate(glm::mat4(1.0f), center) *
                              glm::scale(glm::mat4(1.0f), extents * 2.0f);
                draw_wireframe(box_vb, box_ib, box_ic, m, bounds_color);
              });

          // SkinnedMeshRendererComponent bounds
          scenes.ForEach<SkinnedMeshRendererComponent, TransformComponent>(
              [&](Scene& scene, entt::entity entity) {
                auto& mr =
                    scene.GetComponent<SkinnedMeshRendererComponent>(entity);
                if (!mr.model_handle.IsValid() || mr.mesh_index < 0) {
                  return;
                }
                const TransformComponent* dt =
                    &scene.GetComponent<TransformComponent>(entity);
                const SkeletalAnimRuntime* skel = nullptr;
                if (mr.skeleton_root != entt::null &&
                    scene.GetRegistry().valid(mr.skeleton_root)) {
                  if (scene.GetRegistry().all_of<TransformComponent>(
                          mr.skeleton_root)) {
                    dt = &scene.GetRegistry().get<TransformComponent>(
                        mr.skeleton_root);
                  }
                  if (scene.GetRegistry().all_of<SkeletalAnimRuntime>(
                          mr.skeleton_root)) {
                    skel = &scene.GetRegistry().get<SkeletalAnimRuntime>(
                        mr.skeleton_root);
                  }
                }
                if (skel && skel->rest_pose_bounds.Valid()) {
                  AABB wb = skel->rest_pose_bounds.Transformed(
                      dt->GetTransformMatrix());
                  if (skel->max_bone_reach > 0.0f) {
                    glm::vec3 expand(skel->max_bone_reach);
                    wb.min -= expand;
                    wb.max += expand;
                  }
                  glm::vec3 center = wb.Center();
                  glm::vec3 extents = wb.Extents();
                  glm::mat4 m = glm::translate(glm::mat4(1.0f), center) *
                                glm::scale(glm::mat4(1.0f), extents * 2.0f);
                  draw_wireframe(box_vb, box_ib, box_ic, m, skel_bounds_color);
                }
              });
        }
      });

  graph.PassWritesColor(draw_pass, debug_out);
  graph.SetPassFramebuffer(draw_pass, pool->GetFramebuffer("debug_collider"));
  graph.SetPassViewport(draw_pass, ctx.viewport_size);
  graph.SetPassClearColor(draw_pass, {0, 0, 0, 0});

  // Composite pass: blend debug overlay onto PipelineOutput
  RGResource pipeline_out = registry.Get("PipelineOutput");
  RGResource comp_out = graph.ImportTexture(
      "DebugCollidersCompOut", pool->GetTexture("debug_collider_comp.color"));

  std::shared_ptr<Pipeline> comp_pipeline = comp_pipeline_;
  uint32_t comp_pass = graph.AddPass(
      "DebugCollidersComposite", comp_render_pass_,
      [pool, renderer, comp_pipeline](VkCommandBuffer) {
        comp_pipeline->Bind(PipelineBindPointGraphics);
        // Draw previous PipelineOutput (background)
        renderer->DrawFullscreen(
            comp_pipeline, {pool->GetDescriptor("debug_collider_comp.input")});
        // Draw debug collider overlay
        renderer->DrawFullscreen(
            comp_pipeline, {pool->GetDescriptor("debug_collider.output")});
      });

  graph.PassReadsTexture(comp_pass, pipeline_out);
  graph.PassReadsTexture(comp_pass, debug_out);
  graph.PassWritesColor(comp_pass, comp_out);
  graph.SetPassFramebuffer(comp_pass,
                           pool->GetFramebuffer("debug_collider_comp"));
  graph.SetPassViewport(comp_pass, ctx.viewport_size);
  graph.SetPassClearColor(comp_pass, {0, 0, 0, 0});

  // Update PipelineOutput for downstream features
  registry.Register("PipelineOutput", comp_out);
}

}  // namespace wiesel
