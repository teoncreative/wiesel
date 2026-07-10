
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/features/w_ibl_feature.h"

#include <glm/gtc/matrix_transform.hpp>

#include "rendering/w_pipeline.h"
#include "rendering/w_renderer.h"
#include "util/w_logger.h"
#include "w_engine.h"

namespace wiesel {

// 6 cubemap face view matrices (lookAt from origin).
// Vulkan cubemap layout follows a fixed RH convention regardless of the
// engine's world-space handedness, so we use explicit RH lookAt/perspective.
static glm::mat4 CubeFaceView(uint32_t face) {
  switch (face) {
    case 0:
      return glm::lookAtRH(glm::vec3(0), glm::vec3(1, 0, 0),
                           glm::vec3(0, -1, 0));  // +X
    case 1:
      return glm::lookAtRH(glm::vec3(0), glm::vec3(-1, 0, 0),
                           glm::vec3(0, -1, 0));  // -X
    case 2:
      return glm::lookAtRH(glm::vec3(0), glm::vec3(0, 1, 0),
                           glm::vec3(0, 0, 1));  // +Y
    case 3:
      return glm::lookAtRH(glm::vec3(0), glm::vec3(0, -1, 0),
                           glm::vec3(0, 0, -1));  // -Y
    case 4:
      return glm::lookAtRH(glm::vec3(0), glm::vec3(0, 0, 1),
                           glm::vec3(0, -1, 0));  // +Z
    case 5:
      return glm::lookAtRH(glm::vec3(0), glm::vec3(0, 0, -1),
                           glm::vec3(0, -1, 0));  // -Z
    default:
      return glm::mat4(1.0f);
  }
}

static glm::mat4 CubeProjection() {
  return glm::perspectiveRH_ZO(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
}

IBLFeature::IBLFeature(std::shared_ptr<Renderer> renderer)
    : renderer_(std::move(renderer)) {}

IBLFeature::~IBLFeature() {
  Cleanup();
}

void IBLFeature::Cleanup() {
  irradiance_map_ = nullptr;
  prefilter_map_ = nullptr;
  brdf_lut_ = nullptr;
  ibl_descriptor_ = nullptr;
  maps_generated_ = false;
  last_env_texture_ = nullptr;
}

bool IBLFeature::IsEnabled(const RenderContext& ctx) const {
  return ctx.renderer.options().ibl_enabled;
}

namespace {

// Helper: fill a VkRenderingAttachmentInfo for a single color target.
VkRenderingAttachmentInfo MakeColorInfo(VkImageView view) {
  VkRenderingAttachmentInfo info{};
  info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  info.imageView = view;
  info.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  info.clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
  return info;
}

void BeginRendering(VkCommandBuffer cmd, VkImageView view, uint32_t w,
                    uint32_t h) {
  VkRenderingAttachmentInfo color_info = MakeColorInfo(view);

  VkRenderingInfo ri{};
  ri.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  ri.renderArea.offset = {0, 0};
  ri.renderArea.extent = {w, h};
  ri.layerCount = 1;
  ri.colorAttachmentCount = 1;
  ri.pColorAttachments = &color_info;

  vkCmdBeginRendering(cmd, &ri);
}

}  // namespace

void IBLFeature::GenerateBRDFLUT() {
  if (brdf_generated_) {
    return;
  }

  LOG_INFO("Generating BRDF LUT...");

  // Create output texture
  brdf_lut_ = renderer_->CreateAttachmentTexture(
      {kBRDFLUTSize, kBRDFLUTSize, AttachmentTextureType::Offscreen, 1,
       VK_FORMAT_R16G16_SFLOAT, SamplingMode::DISABLED, true});

  // Create pipeline
  auto vert = renderer_->CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/fullscreen_shader.vert"});
  auto frag = renderer_->CreateShader({ShaderTypeFragment, ShaderLangGLSL,
                                       "main", ShaderSourceSource,
                                       "engine://shaders/brdf_lut.frag"});

  auto pipeline = std::make_shared<Pipeline>(PipelineProperties{});
  pipeline->AddColorAttachment(VK_FORMAT_R16G16_SFLOAT);
  pipeline->AddShader(vert);
  pipeline->AddShader(frag);
  pipeline->Bake();

  // Render
  VkCommandBuffer cmd = renderer_->BeginSingleTimeCommands();

  renderer_->TransitionImageLayout(
      cmd, brdf_lut_->images_[0], VK_FORMAT_R16G16_SFLOAT,
      VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1, 0,
      1);

  BeginRendering(cmd, brdf_lut_->image_views_[0]->handle_, kBRDFLUTSize,
                 kBRDFLUTSize);
  renderer_->SetViewport(glm::vec2{kBRDFLUTSize, kBRDFLUTSize}, cmd);
  pipeline->Bind(cmd);
  vkCmdDraw(cmd, 3, 1, 0, 0);
  vkCmdEndRendering(cmd);

  // Transition to shader read
  renderer_->TransitionImageLayout(
      cmd, brdf_lut_->images_[0], VK_FORMAT_R16G16_SFLOAT,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 0, 1);

  renderer_->EndSingleTimeCommands(cmd);

  brdf_generated_ = true;
  LOG_INFO("BRDF LUT generated ({}x{})", kBRDFLUTSize, kBRDFLUTSize);
}

void IBLFeature::GenerateIBLMaps(std::shared_ptr<Texture> env_cubemap) {
  LOG_INFO("Generating IBL maps from skybox...");

  // Create irradiance cubemap
  irradiance_map_ = renderer_->CreateAttachmentTexture(
      {kIrradianceSize, kIrradianceSize, AttachmentTextureType::Offscreen, 1,
       VK_FORMAT_R16G16B16A16_SFLOAT, SamplingMode::DISABLED, true, 1, 1,
       true});  // layer_count=1 (cubemap flag adds 6), mip_levels=1, is_cubemap=true

  // Create prefilter cubemap with mips
  prefilter_map_ = renderer_->CreateAttachmentTexture(
      {kPrefilterSize, kPrefilterSize, AttachmentTextureType::Offscreen, 1,
       VK_FORMAT_R16G16B16A16_SFLOAT, SamplingMode::DISABLED, true, 1,
       kPrefilterMipLevels, true});

  // Environment map descriptor for sampling
  auto env_desc_layout = renderer_->GetDescriptorLayout("CubemapSampler");
  // If the layout doesn't exist, create one
  if (!env_desc_layout) {
    auto layout = std::make_shared<DescriptorSetLayout>();
    layout->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                       VK_SHADER_STAGE_FRAGMENT_BIT);
    layout->Bake();
    renderer_->RegisterDescriptorLayout("CubemapSampler", layout);
    env_desc_layout = renderer_->GetDescriptorLayout("CubemapSampler");
  }

  auto env_desc = std::make_shared<DescriptorSet>();
  env_desc->SetLayout(env_desc_layout);
  env_desc->AddCombinedImageSampler(0, env_cubemap->image_view_,
                                    renderer_->GetDefaultLinearSampler());
  env_desc->Bake();

  // Compile shaders
  auto cube_vert = renderer_->CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/cubemap_filter.vert"});
  auto irr_frag = renderer_->CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/irradiance_conv.frag"});
  auto pref_frag = renderer_->CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/prefilter_env.frag"});

  // Push constant data for view-projection matrix
  struct IrrPushConstants {
    glm::mat4 view_projection;
  };

  auto irr_pc = std::make_shared<IrrPushConstants>();

  // Irradiance pipeline
  auto irr_pipeline = std::make_shared<Pipeline>(PipelineProperties{});
  irr_pipeline->AddColorAttachment(VK_FORMAT_R16G16B16A16_SFLOAT);
  irr_pipeline->AddInputLayout(env_desc_layout);
  irr_pipeline->AddShader(cube_vert);
  irr_pipeline->AddShader(irr_frag);
  irr_pipeline->AddPushConstant(irr_pc, VK_SHADER_STAGE_VERTEX_BIT);
  irr_pipeline->AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT);
  irr_pipeline->AddDynamicState(VK_DYNAMIC_STATE_SCISSOR);
  irr_pipeline->Bake();

  // Prefilter push constants (mat4 + float roughness)
  struct PrefPushConstants {
    glm::mat4 view_projection;
    float roughness;
  };

  auto pref_pc = std::make_shared<PrefPushConstants>();

  auto pref_pipeline = std::make_shared<Pipeline>(PipelineProperties{});
  pref_pipeline->AddColorAttachment(VK_FORMAT_R16G16B16A16_SFLOAT);
  pref_pipeline->AddInputLayout(env_desc_layout);
  pref_pipeline->AddShader(cube_vert);
  pref_pipeline->AddShader(pref_frag);
  pref_pipeline->AddPushConstant(
      pref_pc, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
  pref_pipeline->AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT);
  pref_pipeline->AddDynamicState(VK_DYNAMIC_STATE_SCISSOR);
  pref_pipeline->Bake();

  glm::mat4 projection = CubeProjection();

  VkCommandBuffer cmd = renderer_->BeginSingleTimeCommands();

  // Keep views alive for the duration of the command buffer.
  std::vector<std::shared_ptr<ImageView>> kept_views;

  // Transition irradiance/prefilter to COLOR_ATTACHMENT_OPTIMAL before
  // drawing so that vkCmdBeginRendering's load op sees the correct layout.
  renderer_->TransitionImageLayout(
      cmd, irradiance_map_->images_[0], VK_FORMAT_R16G16B16A16_SFLOAT,
      VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1, 0,
      6);
  renderer_->TransitionImageLayout(cmd, prefilter_map_->images_[0],
                                   VK_FORMAT_R16G16B16A16_SFLOAT,
                                   VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                   kPrefilterMipLevels, 0, 6);

  // --- Generate irradiance map ---
  for (uint32_t face = 0; face < 6; face++) {
    auto face_view = renderer_->CreateImageViewMip(
        irradiance_map_->images_[0], VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, VK_IMAGE_VIEW_TYPE_2D, face, 1);
    kept_views.push_back(face_view);

    BeginRendering(cmd, face_view->handle_, kIrradianceSize, kIrradianceSize);
    renderer_->SetViewport(glm::vec2(kIrradianceSize, kIrradianceSize), cmd);

    irr_pc->view_projection = projection * CubeFaceView(face);
    irr_pipeline->Bind(cmd);
    renderer_->DrawFullscreen(irr_pipeline, {env_desc}, cmd);

    vkCmdEndRendering(cmd);
  }

  // Transition irradiance to shader read (must use same command buffer)
  renderer_->TransitionImageLayout(
      cmd, irradiance_map_->images_[0], VK_FORMAT_R16G16B16A16_SFLOAT,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 0, 6);

  // --- Generate prefilter map ---
  for (uint32_t mip = 0; mip < kPrefilterMipLevels; mip++) {
    uint32_t mip_size = kPrefilterSize >> mip;
    float roughness =
        static_cast<float>(mip) / static_cast<float>(kPrefilterMipLevels - 1);

    for (uint32_t face = 0; face < 6; face++) {
      auto face_view = renderer_->CreateImageViewMip(
          prefilter_map_->images_[0], VK_FORMAT_R16G16B16A16_SFLOAT,
          VK_IMAGE_ASPECT_COLOR_BIT, mip, 1, VK_IMAGE_VIEW_TYPE_2D, face, 1);
      kept_views.push_back(face_view);

      BeginRendering(cmd, face_view->handle_, mip_size, mip_size);
      renderer_->SetViewport(glm::vec2(mip_size, mip_size), cmd);

      pref_pc->view_projection = projection * CubeFaceView(face);
      pref_pc->roughness = roughness;
      pref_pipeline->Bind(cmd);
      renderer_->DrawFullscreen(pref_pipeline, {env_desc}, cmd);

      vkCmdEndRendering(cmd);
    }
  }

  // Transition entire prefilter to shader read (must use same command buffer)
  renderer_->TransitionImageLayout(
      cmd, prefilter_map_->images_[0], VK_FORMAT_R16G16B16A16_SFLOAT,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, kPrefilterMipLevels, 0, 6);

  renderer_->EndSingleTimeCommands(cmd);
  kept_views.clear();

  maps_generated_ = true;
  last_env_texture_ = env_cubemap.get();
  LOG_INFO("IBL maps generated (irradiance {}x{}, prefilter {}x{} {} mips)",
           kIrradianceSize, kIrradianceSize, kPrefilterSize, kPrefilterSize,
           kPrefilterMipLevels);
}

void IBLFeature::SetupResources(RenderContext& ctx) {
  if (!IsEnabled(ctx)) {
    return;
  }

  // Generate BRDF LUT once
  GenerateBRDFLUT();

  // Check if skybox changed
  auto skybox = ctx.scenes.primary().GetSkybox();
  if (!skybox || !skybox->texture_) {
    LOG_DEBUG("IBL: no skybox or texture available");
    maps_generated_ = false;
    return;
  }

  if (!skybox->texture_->is_allocated_) {
    LOG_DEBUG("IBL: skybox texture not allocated yet");
    return;
  }

  if (skybox->texture_.get() != last_env_texture_) {
    GenerateIBLMaps(skybox->texture_);
  }

  if (!maps_generated_) {
    return;
  }

  // Register IBL descriptor layout if needed
  if (!renderer_->GetDescriptorLayout("IBL")) {
    auto layout = std::make_shared<DescriptorSetLayout>();
    layout->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                       VK_SHADER_STAGE_FRAGMENT_BIT);  // irradiance
    layout->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                       VK_SHADER_STAGE_FRAGMENT_BIT);  // prefilter
    layout->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                       VK_SHADER_STAGE_FRAGMENT_BIT);  // brdfLUT
    layout->Bake();
    renderer_->RegisterDescriptorLayout("IBL", layout);
  }

  // Create the IBL descriptor set
  ibl_descriptor_ = std::make_shared<DescriptorSet>();
  ibl_descriptor_->SetLayout(renderer_->GetDescriptorLayout("IBL"));
  ibl_descriptor_->AddCombinedImageSampler(
      0, irradiance_map_->image_views_[0],
      renderer_->GetDefaultLinearSampler());
  ibl_descriptor_->AddCombinedImageSampler(
      1, prefilter_map_->image_views_[0], renderer_->GetDefaultLinearSampler());
  ibl_descriptor_->AddCombinedImageSampler(
      2, brdf_lut_->image_views_[0], renderer_->GetDefaultLinearSampler());
  ibl_descriptor_->Bake();

  ctx.resources.SetDescriptor("ibl.descriptor", ibl_descriptor_);
}

void IBLFeature::AddPasses(RenderGraph& graph, RenderResourceRegistry& registry,
                           RenderContext& ctx) {
  // IBL maps are pre-generated, no per-frame render passes needed.
  // The descriptor is available via "ibl.descriptor" in the resource pool.
}

}  // namespace wiesel
