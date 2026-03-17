
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/features/w_debug_collider_feature.hpp"
#include "physics/w_collider.hpp"
#include "rendering/w_pipeline.hpp"
#include "rendering/w_renderer.hpp"
#include "rendering/w_renderpass.hpp"
#include "scene/w_scene.hpp"
#include "w_engine.hpp"

namespace Wiesel {

DebugColliderFeature::DebugColliderFeature(std::shared_ptr<Renderer> renderer)
    : renderer_(std::move(renderer)) {
  // Wireframe render pass: 1 color attachment, no depth
  render_pass_ = std::make_shared<RenderPass>(PassType::PostProcess,
                                             "DebugCollider RenderPass");
  render_pass_->AttachOutput(
      {.type = AttachmentTextureType::Offscreen,
       .format = renderer_->GetSwapChainImageFormat(),
       .msaa_mode = SamplingMode::DISABLED});
  render_pass_->Bake();

  // Shaders
  auto vert = renderer_->CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "/engine/shaders/debug_collider.vert"});
  auto frag = renderer_->CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "/engine/shaders/debug_collider.frag"});

  // Wireframe pipeline: line-list, no depth, no alpha blending
  pipeline_ = std::make_shared<Pipeline>(PipelineProperties{
      SamplingMode::DISABLED, CullModeNone, false, false, false, false,
      PrimitiveTopology::LineList});

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

  pipeline_->SetVertexData(binding, attrs);
  pipeline_->SetRenderPass(render_pass_);

  push_constant_ = std::make_shared<DebugColliderPushConstant>();
  pipeline_->AddPushConstant(push_constant_,
                             VK_SHADER_STAGE_VERTEX_BIT |
                                 VK_SHADER_STAGE_FRAGMENT_BIT);

  pipeline_->AddShader(vert);
  pipeline_->AddShader(frag);
  pipeline_->Bake();

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
       "/engine/shaders/fullscreen_shader.vert"});
  auto quad_frag = renderer_->CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "/engine/shaders/quad_shader.frag"});

  comp_pipeline_ = std::make_shared<Pipeline>(PipelineProperties{
      SamplingMode::DISABLED, CullModeFront, false, true, false, false});
  comp_pipeline_->SetRenderPass(comp_render_pass_);
  comp_pipeline_->AddInputLayout(renderer_->GetSkyboxDescriptorLayout());
  comp_pipeline_->AddShader(fullscreen_vert);
  comp_pipeline_->AddShader(quad_frag);
  comp_pipeline_->Bake();

  // Generate wireframe geometry
  GenerateBoxGeometry();
  GenerateSphereGeometry();
}

void DebugColliderFeature::GenerateBoxGeometry() {
  // Unit box: 8 corners of [-0.5, 0.5]^3
  std::vector<glm::vec3> vertices = {
      {-0.5f, -0.5f, -0.5f},  // 0
      { 0.5f, -0.5f, -0.5f},  // 1
      { 0.5f,  0.5f, -0.5f},  // 2
      {-0.5f,  0.5f, -0.5f},  // 3
      {-0.5f, -0.5f,  0.5f},  // 4
      { 0.5f, -0.5f,  0.5f},  // 5
      { 0.5f,  0.5f,  0.5f},  // 6
      {-0.5f,  0.5f,  0.5f},  // 7
  };

  // 12 edges as line-list (24 indices)
  std::vector<Index> indices = {
      0, 1,  1, 2,  2, 3,  3, 0,  // front face
      4, 5,  5, 6,  6, 7,  7, 4,  // back face
      0, 4,  1, 5,  2, 6,  3, 7,  // connecting edges
  };

  box_vertex_buffer_ = renderer_->CreateVertexBuffer(vertices);
  box_index_buffer_ = renderer_->CreateIndexBuffer(indices);
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

  sphere_vertex_buffer_ = renderer_->CreateVertexBuffer(vertices);
  sphere_index_buffer_ = renderer_->CreateIndexBuffer(indices);
  sphere_index_count_ = static_cast<uint32_t>(indices.size());
}

void DebugColliderFeature::SetupResources(RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("DebugColliderFeature::SetupResources");
  auto& pool = ctx.resources;
  auto& renderer = *renderer_;
  uint32_t rw = static_cast<uint32_t>(ctx.viewport_size.x);
  uint32_t rh = static_cast<uint32_t>(ctx.viewport_size.y);

  // Wireframe offscreen texture
  pool.SetTexture("debug_collider.color",
                  renderer.CreateAttachmentTexture(
                      {rw, rh, AttachmentTextureType::Offscreen, 1,
                       renderer.GetSwapChainImageFormat(),
                       SamplingMode::DISABLED, true}));

  std::array<AttachmentTexture*, 1> attachments{
      pool.GetTexture("debug_collider.color").get()};
  pool.SetFramebuffer("debug_collider",
      render_pass_->CreateFramebuffer(0, attachments, {rw, rh}));

  // Descriptor to read wireframe output
  auto debug_output_desc = std::make_shared<DescriptorSet>();
  debug_output_desc->SetLayout(renderer.GetPresentDescriptorLayout());
  debug_output_desc->AddCombinedImageSampler(
      0, pool.GetTexture("debug_collider.color")->image_views_[0],
      renderer.GetDefaultLinearSampler());
  debug_output_desc->Bake();
  pool.SetDescriptor("debug_collider.output", debug_output_desc);

  // Composite output texture
  pool.SetTexture("debug_collider_comp.color",
                  renderer.CreateAttachmentTexture(
                      {rw, rh, AttachmentTextureType::Offscreen, 1,
                       renderer.GetSwapChainImageFormat(),
                       SamplingMode::DISABLED, true}));

  std::array<AttachmentTexture*, 1> comp_attachments{
      pool.GetTexture("debug_collider_comp.color").get()};
  pool.SetFramebuffer("debug_collider_comp",
      comp_render_pass_->CreateFramebuffer(0, comp_attachments, {rw, rh}));

  // Descriptor to read previous PipelineOutput
  auto comp_input_desc = std::make_shared<DescriptorSet>();
  comp_input_desc->SetLayout(renderer.GetPresentDescriptorLayout());
  comp_input_desc->AddCombinedImageSampler(
      0, pool.GetTexture("PipelineOutput")->image_views_[0],
      renderer.GetDefaultLinearSampler());
  comp_input_desc->Bake();
  pool.SetDescriptor("debug_collider_comp.input", comp_input_desc);

  // Update PipelineOutput for downstream features
  auto comp_output_desc = std::make_shared<DescriptorSet>();
  comp_output_desc->SetLayout(renderer.GetPresentDescriptorLayout());
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

  auto* pool = &ctx.resources;
  auto renderer = renderer_;
  auto* scene = &ctx.scene;
  auto pipeline = pipeline_;
  auto push_constant = push_constant_;
  auto box_vb = box_vertex_buffer_;
  auto box_ib = box_index_buffer_;
  auto box_ic = box_index_count_;
  auto sphere_vb = sphere_vertex_buffer_;
  auto sphere_ib = sphere_index_buffer_;
  auto sphere_ic = sphere_index_count_;
  bool enabled = renderer_->options().debug_colliders;

  // Compute VP matrix from camera
  glm::mat4 vp = ctx.camera.projection * ctx.camera.view_matrix;

  // Build heightfield debug geometry once (cached)
  if (enabled && !hf_cache_valid_) {
    hf_cache_.clear();
    for (const auto& entity :
         scene->GetAllEntitiesWith<HeightfieldColliderComponent,
                                     TransformComponent>()) {
      auto& hf = scene->GetComponent<HeightfieldColliderComponent>(entity);
      auto& tc = scene->GetComponent<TransformComponent>(entity);
      if (hf.width < 2 || hf.length < 2 || hf.height_data.empty()) continue;

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

      HeightfieldDebugData data;
      data.vb = renderer_->CreateVertexBuffer(vertices);
      data.ib = renderer_->CreateIndexBuffer(indices);
      data.index_count = static_cast<uint32_t>(indices.size());
      data.model = glm::translate(glm::mat4(1.0f), tc.position);
      hf_cache_.push_back(std::move(data));
    }
    hf_cache_valid_ = true;
  }
  if (!enabled) {
    hf_cache_.clear();
    hf_cache_valid_ = false;
  }
  auto& hf_data = hf_cache_;

  // Import resources
  RGResource debug_out =
      graph.ImportTexture("DebugCollidersOut",
                          pool->GetTexture("debug_collider.color"));

  // Wireframe draw pass
  uint32_t draw_pass = graph.AddPass(
      "DebugColliders", render_pass_,
      [pipeline, push_constant, scene, renderer, vp, enabled,
       box_vb, box_ib, box_ic, sphere_vb, sphere_ib, sphere_ic,
       &hf_data](
          VkCommandBuffer cmd) {
        if (!enabled) return;
        pipeline->Bind(PipelineBindPointGraphics);

        auto draw_wireframe = [&](std::shared_ptr<MemoryBuffer> vb, std::shared_ptr<IndexBuffer> ib,
                                  uint32_t index_count, const glm::mat4& model,
                                  const glm::vec4& color) {
          push_constant->mvp = vp * model;
          push_constant->color = color;
          vkCmdPushConstants(cmd, pipeline->layout_,
                             VK_SHADER_STAGE_VERTEX_BIT |
                                 VK_SHADER_STAGE_FRAGMENT_BIT,
                             0, sizeof(DebugColliderPushConstant),
                             push_constant.get());

          VkBuffer buffers[] = {vb->buffer_handle_};
          VkDeviceSize offsets[] = {0};
          vkCmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);
          vkCmdBindIndexBuffer(cmd, ib->buffer_handle_, 0,
                               ib->index_type_);
          vkCmdDrawIndexed(cmd, index_count, 1, 0, 0, 0);
        };

        const glm::vec4 solid_color(0.0f, 1.0f, 0.0f, 1.0f);
        const glm::vec4 trigger_color(1.0f, 1.0f, 0.0f, 1.0f);
        const glm::vec4 terrain_color(0.0f, 1.0f, 1.0f, 1.0f);

        // Draw box colliders
        for (const auto& entity :
             scene->GetAllEntitiesWith<BoxColliderComponent,
                                        TransformComponent>()) {
          auto& box = scene->GetComponent<BoxColliderComponent>(entity);
          auto& tc = scene->GetComponent<TransformComponent>(entity);

          // Model = translate(position + offset) * scale(half_extents * 2)
          glm::mat4 model = glm::translate(glm::mat4(1.0f),
                                            tc.position + box.offset) *
                            glm::scale(glm::mat4(1.0f),
                                       box.half_extents * 2.0f);

          draw_wireframe(box_vb, box_ib, box_ic, model,
                         box.is_trigger ? trigger_color : solid_color);
        }

        // Draw sphere colliders
        for (const auto& entity :
             scene->GetAllEntitiesWith<SphereColliderComponent,
                                        TransformComponent>()) {
          auto& sphere =
              scene->GetComponent<SphereColliderComponent>(entity);
          auto& tc = scene->GetComponent<TransformComponent>(entity);

          glm::mat4 model =
              glm::translate(glm::mat4(1.0f), tc.position + sphere.offset) *
              glm::scale(glm::mat4(1.0f),
                         glm::vec3(sphere.radius * 2.0f));

          draw_wireframe(sphere_vb, sphere_ib, sphere_ic, model,
                         sphere.is_trigger ? trigger_color : solid_color);
        }

        // Draw heightfield colliders
        for (const auto& hf : hf_data) {
          draw_wireframe(hf.vb, hf.ib, hf.index_count, hf.model,
                         terrain_color);
        }
      });

  graph.PassWritesColor(draw_pass, debug_out);
  graph.SetPassFramebuffer(draw_pass, pool->GetFramebuffer("debug_collider"));
  graph.SetPassViewport(draw_pass, ctx.viewport_size);
  graph.SetPassClearColor(draw_pass, {0, 0, 0, 0});

  // Composite pass: blend debug overlay onto PipelineOutput
  RGResource pipeline_out = registry.Get("PipelineOutput");
  RGResource comp_out =
      graph.ImportTexture("DebugCollidersCompOut",
                          pool->GetTexture("debug_collider_comp.color"));

  auto comp_pipeline = comp_pipeline_;
  uint32_t comp_pass = graph.AddPass(
      "DebugCollidersComposite", comp_render_pass_,
      [pool, renderer, comp_pipeline](VkCommandBuffer) {
        comp_pipeline->Bind(PipelineBindPointGraphics);
        // Draw previous PipelineOutput (background)
        renderer->DrawFullscreen(comp_pipeline,
            {pool->GetDescriptor("debug_collider_comp.input")});
        // Draw debug collider overlay
        renderer->DrawFullscreen(comp_pipeline,
            {pool->GetDescriptor("debug_collider.output")});
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

}  // namespace Wiesel
