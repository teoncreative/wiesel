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
#include "rendering/w_camera.h"
#include "rendering/w_pipeline.h"
#include "rendering/w_renderer.h"
#include "rendering/w_renderpass.h"
#include "scene/w_components.h"
#include "scene/w_scene.h"
#include "w_engine.h"

namespace Wiesel {

BillboardFeature::BillboardFeature(std::shared_ptr<Renderer> renderer)
    : renderer_(std::move(renderer)) {
  SamplingMode msaa = renderer_->options().msaa_mode;

  // Render pass: color + depth (load existing depth for occlusion)
  render_pass_ = std::make_shared<RenderPass>(PassType::ForwardTransparency,
                                              "Billboard RenderPass");
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

  // Billboard shaders
  auto vert = renderer_->CreateShader({ShaderTypeVertex, ShaderLangGLSL, "main",
                                       ShaderSourceSource,
                                       "engine://shaders/billboard.vert"});
  auto frag = renderer_->CreateShader({ShaderTypeFragment, ShaderLangGLSL,
                                       "main", ShaderSourceSource,
                                       "engine://shaders/billboard.frag"});

  // Pipeline: alpha blend, depth test, no depth write, triangle list
  {
    PipelineProperties props{};
    props.sampling_mode = msaa;
    props.cull_mode = CullModeNone;
    props.enable_alpha_blending = true;
    props.enable_depth_test = true;
    props.enable_depth_write = false;
    props.depth_compare_op = CompareOpLessOrEqual;
    pipeline_ = std::make_shared<Pipeline>(props);
  }

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

  pipeline_->SetVertexData(binding, attrs);
  pipeline_->SetRenderPass(render_pass_);

  // Descriptor layout for icon texture
  icon_desc_layout_ = std::make_shared<DescriptorSetLayout>();
  icon_desc_layout_->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                VK_SHADER_STAGE_FRAGMENT_BIT);
  icon_desc_layout_->Bake();

  push_constant_ = std::make_shared<BillboardPushConstant>();
  pipeline_->AddPushConstant(push_constant_, VK_SHADER_STAGE_VERTEX_BIT |
                                                 VK_SHADER_STAGE_FRAGMENT_BIT);
  pipeline_->AddInputLayout(icon_desc_layout_);
  pipeline_->AddShader(vert);
  pipeline_->AddShader(frag);
  pipeline_->Bake();

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

  comp_pipeline_ = std::make_shared<Pipeline>(PipelineProperties{
      SamplingMode::DISABLED, CullModeFront, false, true, false, false});
  comp_pipeline_->SetRenderPass(comp_render_pass_);
  comp_pipeline_->AddInputLayout(renderer_->GetDescriptorLayout("Skybox"));
  comp_pipeline_->AddShader(fullscreen_vert);
  comp_pipeline_->AddShader(quad_frag);
  comp_pipeline_->Bake();

  // Generate quad geometry
  std::vector<BillboardVertex> vertices = {
      {{-0.5f, -0.5f, 0.0f}, {0.0f, 1.0f}},
      {{0.5f, -0.5f, 0.0f}, {1.0f, 1.0f}},
      {{0.5f, 0.5f, 0.0f}, {1.0f, 0.0f}},
      {{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f}},
  };
  std::vector<Index> indices = {0, 1, 2, 2, 3, 0};
  quad_vb_ = renderer_->CreateVertexBuffer(vertices);
  quad_ib_ = renderer_->CreateIndexBuffer(indices);

  // Load icon textures
  LoadIconTexture("camera", "engine://textures/icons/camera_icon.png");
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

bool BillboardFeature::IsEnabled(const RenderContext& ctx) const {
  return ctx.is_external && renderer_->options().show_billboards;
}

void BillboardFeature::SetupResources(RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("BillboardFeature::SetupResources");
  CameraResourcePool& pool = ctx.resources;
  auto& renderer = *renderer_;
  uint32_t rw = static_cast<uint32_t>(ctx.viewport_size.x);
  uint32_t rh = static_cast<uint32_t>(ctx.viewport_size.y);
  SamplingMode msaa = renderer.options().msaa_mode;
  bool use_msaa = msaa > SamplingMode::DISABLED;

  pool.SetTexture("billboard.color",
                  renderer.CreateAttachmentTexture(
                      {rw, rh, AttachmentTextureType::Offscreen, 1,
                       renderer.GetSwapChainImageFormat(), msaa, true}));

  if (use_msaa) {
    pool.SetTexture("billboard.color_resolve",
                    renderer.CreateAttachmentTexture(
                        {rw, rh, AttachmentTextureType::Resolve, 1,
                         renderer.GetSwapChainImageFormat(),
                         SamplingMode::DISABLED, true}));
    std::array<AttachmentTexture*, 3> attachments{
        pool.GetTexture("billboard.color").get(),
        pool.GetTexture("geometry.depth_stencil").get(),
        pool.GetTexture("billboard.color_resolve").get()};
    pool.SetFramebuffer(
        "billboard", render_pass_->CreateFramebuffer(0, attachments, {rw, rh}));
  } else {
    pool.SetTexture("billboard.color_resolve",
                    pool.GetTexture("billboard.color"));
    std::array<AttachmentTexture*, 2> attachments{
        pool.GetTexture("billboard.color").get(),
        pool.GetTexture("geometry.depth_stencil").get()};
    pool.SetFramebuffer(
        "billboard", render_pass_->CreateFramebuffer(0, attachments, {rw, rh}));
  }

  auto billboard_output_desc = std::make_shared<DescriptorSet>();
  billboard_output_desc->SetLayout(renderer.GetDescriptorLayout("Present"));
  billboard_output_desc->AddCombinedImageSampler(
      0, pool.GetTexture("billboard.color_resolve")->image_views_[0],
      renderer.GetDefaultLinearSampler());
  billboard_output_desc->Bake();
  pool.SetDescriptor("billboard.output", billboard_output_desc);

  // Composite output texture
  pool.SetTexture(
      "billboard_comp.color",
      renderer.CreateAttachmentTexture(
          {rw, rh, AttachmentTextureType::Offscreen, 1,
           renderer.GetSwapChainImageFormat(), SamplingMode::DISABLED, true}));

  std::array<AttachmentTexture*, 1> comp_attachments{
      pool.GetTexture("billboard_comp.color").get()};
  pool.SetFramebuffer("billboard_comp", comp_render_pass_->CreateFramebuffer(
                                            0, comp_attachments, {rw, rh}));

  // Descriptor to read previous PipelineOutput
  auto comp_input_desc = std::make_shared<DescriptorSet>();
  comp_input_desc->SetLayout(renderer.GetDescriptorLayout("Present"));
  comp_input_desc->AddCombinedImageSampler(
      0, pool.GetTexture("PipelineOutput")->image_views_[0],
      renderer.GetDefaultLinearSampler());
  comp_input_desc->Bake();
  pool.SetDescriptor("billboard_comp.input", comp_input_desc);

  // Update PipelineOutput for downstream features
  auto comp_output_desc = std::make_shared<DescriptorSet>();
  comp_output_desc->SetLayout(renderer.GetDescriptorLayout("Present"));
  comp_output_desc->AddCombinedImageSampler(
      0, pool.GetTexture("billboard_comp.color")->image_views_[0],
      renderer.GetDefaultLinearSampler());
  comp_output_desc->Bake();
  pool.SetTexture("PipelineOutput", pool.GetTexture("billboard_comp.color"));
  pool.SetDescriptor("PipelineOutputDescriptor", comp_output_desc);
}

void BillboardFeature::AddPasses(RenderGraph& graph,
                                 RenderResourceRegistry& registry,
                                 RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("BillboardFeature::AddPasses");

  CameraResourcePool* pool = &ctx.resources;
  Scene* scene = &ctx.scene;
  auto renderer = renderer_;
  auto pipeline = pipeline_;
  auto push_constant = push_constant_;
  auto quad_vb = quad_vb_;
  auto quad_ib = quad_ib_;

  // Get icon descriptors (captured by lambda)
  auto camera_icon_it = icon_descriptors_.find("camera");
  std::shared_ptr<DescriptorSet> camera_icon_desc =
      camera_icon_it != icon_descriptors_.end() ? camera_icon_it->second
                                                : nullptr;

  glm::mat4 vp = ctx.camera.projection * ctx.camera.view_matrix;
  glm::vec3 cam_pos = ctx.camera.inv_view_matrix[3];
  float fov = ctx.camera.field_of_view;
  bool is_ortho = ctx.camera.projection_mode == ProjectionMode::Orthographic;
  float ortho_size = ctx.camera.ortho_size;

  // Extract camera axes from view matrix for billboarding
  glm::mat4 view = ctx.camera.view_matrix;
  glm::vec3 cam_right = glm::vec3(view[0][0], view[1][0], view[2][0]);
  glm::vec3 cam_up = glm::vec3(view[0][1], view[1][1], view[2][1]);

  // Import resources
  RGResource billboard_out = graph.ImportTexture(
      "BillboardOut", pool->GetTexture("billboard.color_resolve"));

  // Draw pass
  uint32_t draw_pass = graph.AddPass(
      "Billboards", render_pass_,
      [pipeline, push_constant, scene, renderer, vp, cam_pos, cam_right, cam_up,
       fov, is_ortho, ortho_size, quad_vb, quad_ib,
       camera_icon_desc](VkCommandBuffer cmd) {
        if (!camera_icon_desc) {
          return;
        }

        pipeline->Bind(PipelineBindPointGraphics);

        auto draw_billboard = [&](const glm::vec3& world_pos,
                                  const std::shared_ptr<DescriptorSet>& icon) {
          float distance = glm::length(world_pos - cam_pos);
          float scale;
          if (is_ortho) {
            scale = ortho_size * 0.05f;
          } else {
            scale = distance * tanf(glm::radians(fov * 0.5f)) * 0.06f;
          }
          scale = glm::clamp(scale, 0.2f, 20.0f);

          glm::mat4 model = glm::mat4(1.0f);
          model[3] = glm::vec4(world_pos, 1.0f);
          model[0] = glm::vec4(cam_right * scale, 0.0f);
          model[1] = glm::vec4(cam_up * scale, 0.0f);
          model[2] = glm::vec4(glm::cross(cam_right, cam_up) * scale, 0.0f);

          push_constant->mvp = vp * model;
          push_constant->color = glm::vec4(1.0f);
          pipeline->PushConstants(cmd);

          VkBuffer vb_handle = quad_vb->buffer_handle_;
          VkDeviceSize offset = 0;
          vkCmdBindVertexBuffers(cmd, 0, 1, &vb_handle, &offset);
          vkCmdBindIndexBuffer(cmd, quad_ib->buffer_handle_, 0,
                               VK_INDEX_TYPE_UINT32);
          pipeline->BindDescriptorSets(cmd, {icon});
          vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
        };

        // Draw camera billboards
        for (const entt::entity& entity :
             scene->GetAllEntitiesWith<CameraComponent, TransformComponent>()) {
          auto& tc = scene->GetComponent<TransformComponent>(entity);
          draw_billboard(tc.GetWorldPosition(), camera_icon_desc);
        }

        // Draw light billboards (reuse camera icon for now until we have light icons)
        for (const entt::entity& entity :
             scene->GetAllEntitiesWith<LightDirectComponent,
                                       TransformComponent>()) {
          auto& tc = scene->GetComponent<TransformComponent>(entity);
          draw_billboard(tc.GetWorldPosition(), camera_icon_desc);
        }
        for (const entt::entity& entity :
             scene->GetAllEntitiesWith<LightPointComponent,
                                       TransformComponent>()) {
          auto& tc = scene->GetComponent<TransformComponent>(entity);
          draw_billboard(tc.GetWorldPosition(), camera_icon_desc);
        }
      });

  graph.PassWritesColor(draw_pass, billboard_out);
  graph.SetPassFramebuffer(draw_pass, pool->GetFramebuffer("billboard"));
  graph.SetPassViewport(draw_pass, ctx.viewport_size);
  graph.SetPassClearColor(draw_pass, {0, 0, 0, 0});

  // Composite pass: draw previous PipelineOutput then billboard overlay
  RGResource pipeline_out = registry.Get("PipelineOutput");
  RGResource comp_out = graph.ImportTexture(
      "BillboardCompOut", pool->GetTexture("billboard_comp.color"));

  std::shared_ptr<Pipeline> comp_pipeline = comp_pipeline_;
  uint32_t comp_pass = graph.AddPass(
      "BillboardComposite", comp_render_pass_,
      [pool, renderer, comp_pipeline](VkCommandBuffer) {
        comp_pipeline->Bind(PipelineBindPointGraphics);
        // Draw previous pipeline output first
        renderer->DrawFullscreen(comp_pipeline,
                                 {pool->GetDescriptor("billboard_comp.input")});
        // Then blend billboard overlay on top
        renderer->DrawFullscreen(comp_pipeline,
                                 {pool->GetDescriptor("billboard.output")});
      });

  if (pipeline_out.IsValid()) {
    graph.PassReadsTexture(comp_pass, pipeline_out);
  }
  graph.PassReadsTexture(comp_pass, billboard_out);
  graph.PassWritesColor(comp_pass, comp_out);
  graph.SetPassFramebuffer(comp_pass, pool->GetFramebuffer("billboard_comp"));
  graph.SetPassViewport(comp_pass, ctx.viewport_size);
  graph.SetPassClearColor(comp_pass, {0, 0, 0, 1});

  registry.Register("PipelineOutput", comp_out);
}

}  // namespace Wiesel