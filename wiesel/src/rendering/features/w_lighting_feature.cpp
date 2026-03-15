
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/features/w_lighting_feature.hpp"
#include "rendering/w_renderer.hpp"
#include "rendering/w_pipeline.hpp"
#include "rendering/w_renderpass.hpp"
#include "scene/w_scene.hpp"

namespace Wiesel {

LightingFeature::LightingFeature(Ref<Renderer> renderer)
    : renderer_(std::move(renderer)) {
  // Render pass (1 color + optional resolve for MSAA)
  render_pass_ = CreateReference<RenderPass>(PassType::Lighting,
                                             "Deferred Lightning RenderPass");
  render_pass_->AttachOutput(
      {.type = AttachmentTextureType::Offscreen,
       .format = renderer_->GetSwapChainImageFormat(),
       .msaa_mode = renderer_->options().msaa_mode});
  if (renderer_->options().msaa_mode > SamplingMode::DISABLED) {
    render_pass_->AttachOutput(
        {.type = AttachmentTextureType::Resolve,
         .format = renderer_->GetSwapChainImageFormat(),
         .msaa_mode = SamplingMode::DISABLED});
  }
  render_pass_->Bake();

  auto skybox_vert = renderer_->CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "/engine/internal_shaders/skybox_shader.vert"});
  auto skybox_frag = renderer_->CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "/engine/internal_shaders/skybox_shader.frag"});
  skybox_pipeline_ = CreateReference<Pipeline>(PipelineProperties{
      renderer_->options().msaa_mode, CullModeFront, false, false, true,
      false});
  skybox_pipeline_->SetRenderPass(render_pass_);
  skybox_pipeline_->AddInputLayout(renderer_->GetSkyboxDescriptorLayout());
  skybox_pipeline_->AddInputLayout(renderer_->GetGlobalDescriptorLayout());
  skybox_pipeline_->AddShader(skybox_vert);
  skybox_pipeline_->AddShader(skybox_frag);
  skybox_pipeline_->Bake();

  // Lighting pipeline
  auto fullscreen_vert = renderer_->CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "/engine/internal_shaders/fullscreen_shader.vert"});
  auto lighting_frag = renderer_->CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "/engine/internal_shaders/lighting_shader.frag"});
  lighting_pipeline_ = CreateReference<Pipeline>(PipelineProperties{
      renderer_->options().msaa_mode, CullModeFront, false, true, true,
      false});
  lighting_pipeline_->SetRenderPass(render_pass_);
  lighting_pipeline_->AddInputLayout(
      renderer_->GetGeometryOutputDescriptorLayout());
  lighting_pipeline_->AddInputLayout(
      renderer_->GetSSAOOutputDescriptorLayout());
  lighting_pipeline_->AddInputLayout(renderer_->GetGlobalDescriptorLayout());
  lighting_pipeline_->AddInputLayout(renderer_->GetSkyboxDescriptorLayout());
  lighting_pipeline_->AddShader(fullscreen_vert);
  lighting_pipeline_->AddShader(lighting_frag);
  lighting_pipeline_->Bake();

  // RT shadow variant: same pipeline but with USE_RT_SHADOWS define and extra descriptor set
  if (renderer_->IsRayTracingSupported()) {
    auto rt_lighting_frag = renderer_->CreateShader(
        {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
         "/engine/internal_shaders/lighting_shader.frag",
         {"USE_RT_SHADOWS"}});

    rt_shadow_desc_layout_ = CreateReference<DescriptorSetLayout>();
    rt_shadow_desc_layout_->AddBinding(
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        VK_SHADER_STAGE_FRAGMENT_BIT);
    rt_shadow_desc_layout_->Bake();

    rt_lighting_pipeline_ = CreateReference<Pipeline>(PipelineProperties{
        renderer_->options().msaa_mode, CullModeFront, false, true, true,
        false});
    rt_lighting_pipeline_->SetRenderPass(render_pass_);
    rt_lighting_pipeline_->AddInputLayout(
        renderer_->GetGeometryOutputDescriptorLayout());
    rt_lighting_pipeline_->AddInputLayout(
        renderer_->GetSSAOOutputDescriptorLayout());
    rt_lighting_pipeline_->AddInputLayout(
        renderer_->GetGlobalDescriptorLayout());
    rt_lighting_pipeline_->AddInputLayout(rt_shadow_desc_layout_);
    rt_lighting_pipeline_->AddShader(fullscreen_vert);
    rt_lighting_pipeline_->AddShader(rt_lighting_frag);
    rt_lighting_pipeline_->Bake();
  }
}

void LightingFeature::SetupResources(RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("LightingFeature::SetupResources");
  auto& pool = ctx.resources;
  auto& renderer = *renderer_;
  uint32_t rw = static_cast<uint32_t>(ctx.viewport_size.x);
  uint32_t rh = static_cast<uint32_t>(ctx.viewport_size.y);
  SamplingMode msaa = renderer.options().msaa_mode;
  bool use_msaa = msaa > SamplingMode::DISABLED;

  // Textures
  pool.SetTexture("lighting.color", renderer.CreateAttachmentTexture(
      {rw, rh, AttachmentTextureType::Offscreen, 1,
       renderer.GetSwapChainImageFormat(), msaa,
       msaa == SamplingMode::DISABLED}));

  if (use_msaa) {
    pool.SetTexture("lighting.color_resolve",
        renderer.CreateAttachmentTexture(
            {rw, rh, AttachmentTextureType::Resolve, 1,
             renderer.GetSwapChainImageFormat(), SamplingMode::DISABLED,
             true}));

    std::array<AttachmentTexture*, 2> textures{
        pool.GetTexture("lighting.color").get(),
        pool.GetTexture("lighting.color_resolve").get()};
    pool.SetFramebuffer("lighting",
        render_pass_->CreateFramebuffer(0, textures, {rw, rh}));
  } else {
    pool.SetTexture("lighting.color_resolve",
                    pool.GetTexture("lighting.color"));

    std::array<AttachmentTexture*, 1> textures{
        pool.GetTexture("lighting.color").get()};
    pool.SetFramebuffer("lighting",
        render_pass_->CreateFramebuffer(0, textures, {rw, rh}));
  }

  // Descriptors
  // Lighting output descriptor: reads resolved lighting color, linear sampler
  auto lighting_output_desc = CreateReference<DescriptorSet>();
  lighting_output_desc->SetLayout(renderer.GetPresentDescriptorLayout());
  lighting_output_desc->AddCombinedImageSampler(
      0, pool.GetTexture("lighting.color_resolve")->image_views_[0],
      renderer.GetDefaultLinearSampler());
  lighting_output_desc->Bake();
  pool.SetDescriptor("lighting.output", lighting_output_desc);

  // RT shadow descriptor for reading the shadow mask in the lighting shader
  if (rt_shadow_desc_layout_ && pool.HasTexture("rt_shadow.mask")) {
    auto rt_shadow_desc = CreateReference<DescriptorSet>();
    rt_shadow_desc->SetLayout(rt_shadow_desc_layout_);
    rt_shadow_desc->AddStorageImage(
        0, pool.GetTexture("rt_shadow.mask")->image_views_[0]);
    rt_shadow_desc->Bake();
    pool.SetDescriptor("lighting.rt_shadow", rt_shadow_desc);
  }
}

void LightingFeature::AddPasses(RenderGraph& graph,
                                RenderResourceRegistry& registry,
                                RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("LightingFeature::AddPasses");
  CameraResourcePool* pool = &ctx.resources;
  std::shared_ptr<Renderer> renderer = renderer_;
  Scene* scene = &ctx.scene;
  bool use_resolve = ctx.use_msaa_resolve;

  // Import lighting output texture from pool
  std::shared_ptr<AttachmentTexture> lighting_tex =
      use_resolve ? pool->GetTexture("lighting.color_resolve")
                  : pool->GetTexture("lighting.color");
  RGResource lighting_out =
      graph.ImportTexture("LightingOut", lighting_tex);

  // Get references to geometry and SSAO outputs from the registry
  RGResource geo_view_pos = registry.Get("GeoViewPos");
  RGResource geo_world_pos = registry.Get("GeoWorldPos");
  RGResource geo_normal = registry.Get("GeoNormal");
  RGResource geo_albedo = registry.Get("GeoAlbedo");
  RGResource geo_material = registry.Get("GeoMaterial");
  RGResource ssao_blur_v = registry.Get("SSAOBlurV");
  RGResource shadow_depth = registry.Get("ShadowDepth");

  // Check if RT shadow mask is available and shadows are enabled
  bool use_rt_shadows = renderer_->options().shadows_enabled
                        && renderer_->options().rt_shadows_enabled
                        && registry.Has("RTShadowMask")
                        && rt_lighting_pipeline_
                        && pool->HasDescriptor("lighting.rt_shadow");
  RGResource rt_shadow_mask;
  if (use_rt_shadows) {
    rt_shadow_mask = registry.Get("RTShadowMask");
  }

  // Lighting pass
  std::shared_ptr<Pipeline> skybox_pipeline = skybox_pipeline_;
  std::shared_ptr<Pipeline> lighting_pipeline =
      use_rt_shadows ? rt_lighting_pipeline_ : lighting_pipeline_;
  uint32_t lighting = graph.AddPass(
      "Lighting", render_pass_,
      [pool, renderer, scene, skybox_pipeline,
       lighting_pipeline, use_rt_shadows](VkCommandBuffer) {
        skybox_pipeline->Bind(PipelineBindPointGraphics);
        auto skybox = scene->GetSkybox();
        if (skybox) {
          renderer->DrawSkybox(skybox);
        }
        lighting_pipeline->Bind(PipelineBindPointGraphics);
        if (use_rt_shadows) {
          renderer->DrawFullscreen(lighting_pipeline,
              {pool->GetDescriptor("geometry.output"),
               pool->GetDescriptor("ssao.blur_v.output"),
               pool->GetDescriptor("GlobalDescriptor"),
               pool->GetDescriptor("lighting.rt_shadow")});
        } else {
          renderer->DrawFullscreen(lighting_pipeline,
              {pool->GetDescriptor("geometry.output"),
               pool->GetDescriptor("ssao.blur_v.output"),
               pool->GetDescriptor("GlobalDescriptor")});
        }
      });

  graph.PassReadsTexture(lighting, geo_view_pos);
  graph.PassReadsTexture(lighting, geo_world_pos);
  graph.PassReadsTexture(lighting, geo_normal);
  graph.PassReadsTexture(lighting, geo_albedo);
  graph.PassReadsTexture(lighting, geo_material);
  if (ssao_blur_v.IsValid()) {
    graph.PassReadsTexture(lighting, ssao_blur_v);
  }
  if (shadow_depth.IsValid()) {
    graph.PassReadsTexture(lighting, shadow_depth);
  }
  if (rt_shadow_mask.IsValid()) {
    graph.PassReadsStorageImage(lighting, rt_shadow_mask);
  }
  graph.PassWritesColor(lighting, lighting_out);
  graph.SetPassFramebuffer(lighting, pool->GetFramebuffer("lighting"));
  graph.SetPassViewport(lighting, ctx.viewport_size);
  graph.SetPassClearColor(lighting, renderer->GetClearColor());

  // Register output for downstream features (e.g. Composite)
  registry.Register("LightingOut", lighting_out);
}

}  // namespace Wiesel
