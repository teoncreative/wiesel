
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/features/w_rt_shadow_feature.h"
#include "rendering/w_renderer.h"
#include "scene/w_scene.h"

namespace wiesel {

RTShadowFeature::RTShadowFeature(std::shared_ptr<Renderer> renderer)
    : renderer_(std::move(renderer)) {
  // Descriptor layout for the RT shadow pass
  rt_descriptor_layout_ = std::make_shared<DescriptorSetLayout>();
  // binding 0: TLAS
  rt_descriptor_layout_->AddBinding(
      VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
      VK_SHADER_STAGE_RAYGEN_BIT_KHR);
  // binding 1: output r32ui storage image (shadow bitmask)
  rt_descriptor_layout_->AddBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                    VK_SHADER_STAGE_RAYGEN_BIT_KHR);
  // binding 2: world position G-buffer
  rt_descriptor_layout_->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                    VK_SHADER_STAGE_RAYGEN_BIT_KHR);
  // binding 3: world normal G-buffer
  rt_descriptor_layout_->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                    VK_SHADER_STAGE_RAYGEN_BIT_KHR);
  // binding 4: shadow lights UBO
  rt_descriptor_layout_->AddBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                    VK_SHADER_STAGE_RAYGEN_BIT_KHR);
  rt_descriptor_layout_->Bake();

  // UBO for shadow light data
  shadow_lights_ubo_ = renderer_->CreateUniformBuffer(
      "RTShadowFeature::shadow_lights_ubo_", sizeof(RTShadowLightUBO));

  // Compile RT shaders
  auto raygen = renderer_->CreateShader({ShaderTypeRayGen, ShaderLangGLSL,
                                         "main", ShaderSourceSource,
                                         "engine://shaders/rt_shadow.rgen"});
  auto miss = renderer_->CreateShader({ShaderTypeMiss, ShaderLangGLSL, "main",
                                       ShaderSourceSource,
                                       "engine://shaders/rt_shadow.rmiss"});
  auto closesthit = renderer_->CreateShader(
      {ShaderTypeClosestHit, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/rt_shadow.rchit"});

  // Build RT pipeline
  rt_pipeline_ = std::make_shared<RTPipeline>(renderer_);
  rt_pipeline_->AddRayGenShader(raygen);
  rt_pipeline_->AddMissShader(miss);
  rt_pipeline_->AddHitGroup(closesthit);
  rt_pipeline_->AddInputLayout(rt_descriptor_layout_);
  rt_pipeline_->Bake();
}

bool RTShadowFeature::IsEnabled(const RenderContext& ctx) const {
  return ctx.renderer.IsRayTracingSupported() &&
         ctx.renderer.options().shadows_enabled &&
         ctx.renderer.options().rt_shadows_enabled;
}

void RTShadowFeature::SetupResources(RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("RTShadowFeature::SetupResources");
  uint32_t rw = static_cast<uint32_t>(ctx.viewport_size.x);
  uint32_t rh = static_cast<uint32_t>(ctx.viewport_size.y);

  ctx.resources.SetTexture("rt_shadow.mask",
                           renderer_->CreateAttachmentTexture(
                               {.width = rw,
                                .height = rh,
                                .type = AttachmentTextureType::Offscreen,
                                .image_format = VK_FORMAT_R32_UINT,
                                .sampled = true,
                                .storage = true}));
}

void RTShadowFeature::AddPasses(RenderGraph& graph,
                                RenderResourceRegistry& registry,
                                RenderContext& ctx) {
  PROFILE_ZONE_SCOPED_N("RTShadowFeature::AddPasses");
  CameraResourcePool* pool = &ctx.resources;
  std::shared_ptr<Renderer> renderer = renderer_;
  MultiScene& scenes = ctx.scenes;
  std::shared_ptr<RTPipeline> rt_pipeline = rt_pipeline_;
  std::shared_ptr<DescriptorSetLayout> rt_layout = rt_descriptor_layout_;
  std::shared_ptr<UniformBuffer> lights_ubo = shadow_lights_ubo_;

  // Import the shadow mask texture into the render graph
  auto shadow_mask_tex = pool->GetTexture("rt_shadow.mask");
  RGResource shadow_mask_res =
      graph.ImportTexture("RTShadowMask", shadow_mask_tex);
  uint32_t trace_width = shadow_mask_tex->width_;
  uint32_t trace_height = shadow_mask_tex->height_;

  // Get G-buffer outputs from registry
  RGResource geo_world_pos = registry.Get("GeoWorldPos");
  RGResource geo_normal = registry.Get("GeoNormal");

  uint32_t pass_idx = graph.AddPass(
      "RTShadow",
      [pool, renderer, &scenes, rt_pipeline, rt_layout, lights_ubo, trace_width,
       trace_height](VkCommandBuffer cmd) {
        auto as_manager = renderer->GetASManager();
        if (!as_manager) {
          return;
        }

        // Build TLAS for the current frame
        as_manager->BuildTLAS(cmd, scenes.primary());
        if (!as_manager->HasTLAS()) {
          return;
        }

        // Collect shadow lights (directional + point)
        RTShadowLightUBO ubo_data{};
        ubo_data.count = 0;

        scenes.ForEach<LightDirectComponent, TransformComponent>(
            [&](Scene& scene, entt::entity entity) {
              if (ubo_data.count >= kMaxRTShadowLights) {
                return;
              }
              auto& transform = scene.GetComponent<TransformComponent>(entity);
              glm::vec3 worldDir = -glm::normalize(glm::vec3(
                  transform.GetTransformMatrix() * glm::vec4(0, 0, -1, 0)));
              ubo_data.lights[ubo_data.count].pos_or_dir =
                  glm::vec4(worldDir, 0.0f);
              ubo_data.lights[ubo_data.count].params = glm::vec4(0.0f);
              ubo_data.count++;
            });

        scenes.ForEach<LightPointComponent, TransformComponent>(
            [&](Scene& scene, entt::entity entity) {
              if (ubo_data.count >= kMaxRTShadowLights) {
                return;
              }
              auto& transform = scene.GetComponent<TransformComponent>(entity);
              glm::vec3 worldPos = transform.GetWorldPosition();
              ubo_data.lights[ubo_data.count].pos_or_dir =
                  glm::vec4(worldPos, 1.0f);
              ubo_data.lights[ubo_data.count].params = glm::vec4(0.0f);
              ubo_data.count++;
            });

        vkCmdUpdateBuffer(cmd, lights_ubo->buffer_handle_, 0,
                          sizeof(RTShadowLightUBO), &ubo_data);
        VkMemoryBarrier2 ubo_barrier{};
        ubo_barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
        ubo_barrier.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
        ubo_barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        ubo_barrier.dstStageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
        ubo_barrier.dstAccessMask = VK_ACCESS_2_UNIFORM_READ_BIT;
        VkDependencyInfo ubo_dep{};
        ubo_dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        ubo_dep.memoryBarrierCount = 1;
        ubo_dep.pMemoryBarriers = &ubo_barrier;
        vkCmdPipelineBarrier2(cmd, &ubo_dep);

        // New descriptor each frame (TLAS handle changes); old one is deferred
        // by SetDescriptor via the DeletionQueue for safe multi-frame-in-flight.
        auto desc = std::make_shared<DescriptorSet>();
        desc->SetLayout(rt_layout);
        desc->AddAccelerationStructure(0, as_manager->GetTLAS());
        desc->AddStorageImage(
            1, pool->GetTexture("rt_shadow.mask")->image_views_[0]);
        desc->AddCombinedImageSampler(
            2, pool->GetTexture("geometry.world_pos_resolve")->image_views_[0],
            renderer->GetDefaultNearestSampler());
        desc->AddCombinedImageSampler(
            3, pool->GetTexture("geometry.normal_resolve")->image_views_[0],
            renderer->GetDefaultNearestSampler());
        desc->AddUniformBuffer(4, lights_ubo);
        desc->Bake();
        pool->SetDescriptor("rt_shadow.desc", desc);

        // Bind and trace
        rt_pipeline->Bind(cmd);
        rt_pipeline->BindDescriptorSet(cmd, desc->descriptor_set_);

        renderer->vkCmdTraceRaysKHR()(
            cmd, &rt_pipeline->GetRayGenRegion(), &rt_pipeline->GetMissRegion(),
            &rt_pipeline->GetHitRegion(), &rt_pipeline->GetCallableRegion(),
            trace_width, trace_height, 1);
      });

  graph.SetPassAutoBeginRendering(pass_idx, false);
  graph.PassReadsTexture(pass_idx, geo_world_pos);
  graph.PassReadsTexture(pass_idx, geo_normal);
  graph.PassWritesStorageImage(pass_idx, shadow_mask_res);

  // Register for downstream features (lighting)
  registry.Register("RTShadowMask", shadow_mask_res);
}

}  // namespace wiesel