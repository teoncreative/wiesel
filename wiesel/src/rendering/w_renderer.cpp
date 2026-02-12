
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/w_renderer.hpp"
#include "rendering/w_perf_marker.hpp"
#include "rendering/w_sampler.hpp"

#include "util/imgui/imgui_spectrum.hpp"
#include "util/w_spirv.hpp"
#include "util/w_vectors.hpp"
#include "w_engine.hpp"

#include <random>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include "asset/w_asset_manager.hpp"
#include "events/w_engineevents.hpp"

namespace Wiesel {

std::vector<SamplingMode> ConvertToSamplingModes(VkSampleCountFlags flags) {
  std::vector<SamplingMode> modes;
  modes.push_back(SamplingMode::DISABLED);
  if (flags & VK_SAMPLE_COUNT_64_BIT) {
    modes.push_back(SamplingMode::X64);
  }
  if (flags & VK_SAMPLE_COUNT_32_BIT) {
    modes.push_back(SamplingMode::X32);
  }
  if (flags & VK_SAMPLE_COUNT_16_BIT) {
    modes.push_back(SamplingMode::X16);
  }
  if (flags & VK_SAMPLE_COUNT_8_BIT) {
    modes.push_back(SamplingMode::X8);
  }
  if (flags & VK_SAMPLE_COUNT_4_BIT) {
    modes.push_back(SamplingMode::X4);
  }
  if (flags & VK_SAMPLE_COUNT_2_BIT) {
    modes.push_back(SamplingMode::X2);
  }
  return modes;
}

SamplingMode FindHighestSamplingMode(const std::vector<SamplingMode>& modes) {
  uint64_t highest = 0;
  for (SamplingMode mode : modes) {
    uint64_t value = static_cast<uint64_t>(mode);
    if (value > highest) {
      highest = value;
    }
  }
  return static_cast<SamplingMode>(highest);
}

Renderer::Renderer(Ref<AppWindow> window) : window_(window) {
  Spirv::Init();
#ifdef VULKAN_VALIDATION
  validation_layers_.push_back("VK_LAYER_KHRONOS_validation");
#endif

  device_extensions_.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
#ifdef __APPLE__
  device_extensions_.push_back("VK_KHR_portability_subset");
#endif
#ifdef TRACY_ENABLE
  device_extensions_.push_back(VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME);
#endif

  recreate_pipeline_ = false;
  options_ = {};
  recreate_swap_chain_ = false;
  swap_chain_created_ = false;
  image_index_ = 0;
  clear_color_ = {0.1f, 0.1f, 0.2f, 1.0f};
}

Renderer::~Renderer() {
  Cleanup();
}

void Renderer::Initialize(const RendererProperties&& properties) {
  options_.wireframe_enabled.SetHook(&recreate_pipeline_);
  options_.msaa_mode.SetHook(&recreate_swap_chain_);
  options_.vsync.SetHook(&recreate_swap_chain_);
  CreateVulkanInstance();
  LoadInstanceExtensions();
#ifdef VULKAN_VALIDATION
  SetupDebugMessenger();
#endif
  PerfMarker::Init(instance_);
  CreateSurface();
  PickPhysicalDevice();
  CreateLogicalDevice();
  LoadDeviceExtensions();
  CreateGlobalUniformBuffers();
  // ---
  CreateCommandPools();
  CreateDescriptorLayouts();
  CreateSwapChain();
  CreateGeometryRenderPass();
  CreateGeometryGraphicsPipelines();
  CreatePresentGraphicsPipelines();
  CreateCommandBuffers();
  CreatePermanentResources();
  CreateSyncObjects();
  CreateTracy();
  initialized_ = true;
}

VkDevice Renderer::GetLogicalDevice() {
  return logical_device_;
}

template <typename T>
Ref<MemoryBuffer> Renderer::CreateVertexBuffer(std::vector<T> vertices) {
  Ref<MemoryBuffer> memoryBuffer =
      CreateReference<MemoryBuffer>(MemoryTypeVertexBuffer);

  memoryBuffer->size_ = vertices.size();

  VkDeviceSize bufferSize = sizeof(T) * vertices.size();
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               stagingBuffer, stagingBufferMemory);

  void* data;
  vkMapMemory(logical_device_, stagingBufferMemory, 0, bufferSize, 0, &data);
  memcpy(data, vertices.data(), bufferSize);
  vkUnmapMemory(logical_device_, stagingBufferMemory);

  CreateBuffer(
      bufferSize,
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memoryBuffer->buffer_handle_,
      memoryBuffer->memory_handle_);

  CopyBuffer(stagingBuffer, memoryBuffer->buffer_handle_, bufferSize);

  vkDestroyBuffer(logical_device_, stagingBuffer, nullptr);
  vkFreeMemory(logical_device_, stagingBufferMemory, nullptr);
  return memoryBuffer;
}

template Ref<MemoryBuffer> Renderer::CreateVertexBuffer<Vertex3D>(
    std::vector<Vertex3D>);

template Ref<MemoryBuffer> Renderer::CreateVertexBuffer<Vertex2DNoColor>(
    std::vector<Vertex2DNoColor>);

template Ref<MemoryBuffer> Renderer::CreateVertexBuffer<VertexSprite>(
    std::vector<VertexSprite>);

void Renderer::DestroyVertexBuffer(MemoryBuffer& buffer) {
  vkDestroyBuffer(logical_device_, buffer.buffer_handle_, nullptr);
  vkFreeMemory(logical_device_, buffer.memory_handle_, nullptr);
}

Ref<IndexBuffer> Renderer::CreateIndexBuffer(std::vector<Index> indices) {
  Ref<IndexBuffer> memoryBuffer = CreateReference<IndexBuffer>();

  static_assert(sizeof(Index) == sizeof(uint32_t));
  memoryBuffer->index_type_ = VK_INDEX_TYPE_UINT32;
  memoryBuffer->size_ = indices.size();
  VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               stagingBuffer, stagingBufferMemory);

  void* data;
  vkMapMemory(logical_device_, stagingBufferMemory, 0, bufferSize, 0, &data);
  memcpy(data, indices.data(), (size_t)bufferSize);
  vkUnmapMemory(logical_device_, stagingBufferMemory);

  CreateBuffer(
      bufferSize,
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memoryBuffer->buffer_handle_,
      memoryBuffer->memory_handle_);

  CopyBuffer(stagingBuffer, memoryBuffer->buffer_handle_, bufferSize);

  vkDestroyBuffer(logical_device_, stagingBuffer, nullptr);
  vkFreeMemory(logical_device_, stagingBufferMemory, nullptr);

  return memoryBuffer;
}

Ref<UniformBuffer> Renderer::CreateUniformBuffer(VkDeviceSize size) {
  Ref<UniformBuffer> uniformBuffer = CreateReference<UniformBuffer>();

  uniformBuffer->data_ = malloc(size);
  uniformBuffer->size_ = size;
  // TODO not use host coherent memory, use staging buffer and copy when it changes
  // like how I did in GlistEngine
  // This is slow af
  CreateBuffer(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               uniformBuffer->buffer_handle_, uniformBuffer->memory_handle_);

  WIESEL_CHECK_VKRESULT(vkMapMemory(logical_device_,
                                    uniformBuffer->memory_handle_, 0, size, 0,
                                    &uniformBuffer->data_));

  memset(uniformBuffer->data_, 0, size);

  return uniformBuffer;
}

void Renderer::DestroyIndexBuffer(MemoryBuffer& buffer) {
  vkDeviceWaitIdle(logical_device_);
  vkDestroyBuffer(logical_device_, buffer.buffer_handle_, nullptr);
  vkFreeMemory(logical_device_, buffer.memory_handle_, nullptr);
}

void Renderer::DestroyUniformBuffer(UniformBuffer& buffer) {
  vkDeviceWaitIdle(logical_device_);
  vkDestroyBuffer(logical_device_, buffer.buffer_handle_, nullptr);
  vkFreeMemory(logical_device_, buffer.memory_handle_, nullptr);
}

void Renderer::SetupCameraComponent(CameraComponent& component) {
  LOG_INFO("  Viewport: {}x{}", component.viewport_size.x, component.viewport_size.y);
  if (component.aspect_ratio <= 0) {
    component.aspect_ratio =
        component.viewport_size.x / component.viewport_size.y;
  }
  uint32_t rw = static_cast<uint32_t>(component.viewport_size.x);
  uint32_t rh = static_cast<uint32_t>(component.viewport_size.y);

  component.ssao_color_image = CreateAttachmentTexture(
      {rw / 2, rh / 2, AttachmentTextureType::Offscreen, 1, VK_FORMAT_R8_UNORM,
       SamplingMode::DISABLED, true});
  component.ssao_blur_horz_color_image = CreateAttachmentTexture(
      {rw, rh, AttachmentTextureType::Offscreen, 1, VK_FORMAT_R8_UNORM,
       SamplingMode::DISABLED, true});
  component.ssao_blur_vert_color_image = CreateAttachmentTexture(
      {rw, rh, AttachmentTextureType::Offscreen, 1, VK_FORMAT_R8_UNORM,
       SamplingMode::DISABLED, true});
  component.ssao_gen_framebuffer = ssao_gen_render_pass_->CreateFramebuffer(
      0, {component.ssao_color_image.get()}, {rw / 2, rh / 2});
  component.ssao_blur_horz_framebuffer =
      ssao_blur_horz_render_pass_->CreateFramebuffer(
          0, {component.ssao_blur_horz_color_image.get()}, {rw, rh});
  component.ssao_blur_vert_framebuffer =
      ssao_blur_vert_render_pass_->CreateFramebuffer(
          0, {component.ssao_blur_vert_color_image.get()}, {rw, rh});

  component.geometry_view_pos_image = CreateAttachmentTexture(
      {rw, rh, AttachmentTextureType::Offscreen, 1,
       VK_FORMAT_R32G32B32A32_SFLOAT, options_.msaa_mode, true});
  component.geometry_world_pos_image = CreateAttachmentTexture(
      {rw, rh, AttachmentTextureType::Offscreen, 1,
       VK_FORMAT_R32G32B32A32_SFLOAT, options_.msaa_mode, true});
  component.geometry_depth_image = CreateAttachmentTexture(
      {rw, rh, AttachmentTextureType::Offscreen, 1, VK_FORMAT_R32_SFLOAT,
       options_.msaa_mode, true});
  component.geometry_normal_image = CreateAttachmentTexture(
      {rw, rh, AttachmentTextureType::Offscreen, 1, VK_FORMAT_R8G8B8A8_UNORM,
       options_.msaa_mode, true});
  component.geometry_albedo_image = CreateAttachmentTexture(
      {rw, rh, AttachmentTextureType::Offscreen, 1, VK_FORMAT_R8G8B8A8_UNORM,
       options_.msaa_mode, true});
  component.geometry_material_image = CreateAttachmentTexture(
      {rw, rh, AttachmentTextureType::Offscreen, 1,
       VK_FORMAT_R16G16B16A16_SFLOAT, options_.msaa_mode, true});
  component.geometry_depth_stencil = CreateAttachmentTexture(
      {rw, rh, AttachmentTextureType::DepthStencil, 1, FindDepthFormat(),
       options_.msaa_mode, true});

  component.shadow_depth_stencil = CreateAttachmentTexture(
      {WIESEL_SHADOWMAP_DIM, WIESEL_SHADOWMAP_DIM,
       AttachmentTextureType::DepthStencil, 1, FindDepthFormat(),
       SamplingMode::DISABLED, true, WIESEL_SHADOW_CASCADE_COUNT});
  component.shadow_depth_view_array = CreateImageView(
      component.shadow_depth_stencil, VK_IMAGE_VIEW_TYPE_2D_ARRAY, 0,
      WIESEL_SHADOW_CASCADE_COUNT);
  for (int i = 0; i < WIESEL_SHADOW_CASCADE_COUNT; ++i) {
    component.shadow_depth_views[i] = CreateImageView(
        component.shadow_depth_stencil, VK_IMAGE_VIEW_TYPE_2D, i);
    std::array<ImageView*, 1> textures = {
        component.shadow_depth_views[i].get(),
    };
    component.shadow_framebuffers[i] = shadow_render_pass_->CreateFramebuffer(
        textures, {WIESEL_SHADOWMAP_DIM, WIESEL_SHADOWMAP_DIM});
  }

  if (options_.msaa_mode != SamplingMode::DISABLED) {
    component.geometry_view_pos_resolve_image = CreateAttachmentTexture(
        {rw, rh, AttachmentTextureType::Resolve, 1,
         VK_FORMAT_R32G32B32A32_SFLOAT, SamplingMode::DISABLED, true});
    component.geometry_world_pos_resolve_image = CreateAttachmentTexture(
        {rw, rh, AttachmentTextureType::Resolve, 1,
         VK_FORMAT_R32G32B32A32_SFLOAT, SamplingMode::DISABLED, true});
    component.geometry_depth_resolve_image = CreateAttachmentTexture(
        {rw, rh, AttachmentTextureType::Resolve, 1, VK_FORMAT_R32_SFLOAT,
         SamplingMode::DISABLED, true});
    component.geometry_normal_resolve_image = CreateAttachmentTexture(
        {rw, rh, AttachmentTextureType::Resolve, 1, VK_FORMAT_R8G8B8A8_UNORM,
         SamplingMode::DISABLED, true});
    component.geometry_albedo_resolve_image = CreateAttachmentTexture(
        {rw, rh, AttachmentTextureType::Resolve, 1, VK_FORMAT_R8G8B8A8_UNORM,
         SamplingMode::DISABLED, true});
    component.geometry_material_resolve_image = CreateAttachmentTexture(
        {rw, rh, AttachmentTextureType::Resolve, 1,
         VK_FORMAT_R16G16B16A16_SFLOAT, SamplingMode::DISABLED, true});
    std::array<AttachmentTexture*, 13> textures = {
        component.geometry_view_pos_image.get(),
        component.geometry_world_pos_image.get(),
        component.geometry_depth_image.get(),
        component.geometry_normal_image.get(),
        component.geometry_albedo_image.get(),
        component.geometry_material_image.get(),
        component.geometry_depth_stencil.get(),
        component.geometry_view_pos_resolve_image.get(),
        component.geometry_world_pos_resolve_image.get(),
        component.geometry_depth_resolve_image.get(),
        component.geometry_normal_resolve_image.get(),
        component.geometry_albedo_resolve_image.get(),
        component.geometry_material_resolve_image.get(),
    };
    component.geometry_framebuffer = geometry_render_pass_->CreateFramebuffer(
        0, textures, component.viewport_size);
  } else {
    component.geometry_view_pos_resolve_image =
        component.geometry_view_pos_image;
    component.geometry_world_pos_resolve_image =
        component.geometry_world_pos_image;
    component.geometry_depth_resolve_image = component.geometry_depth_image;
    component.geometry_normal_resolve_image = component.geometry_normal_image;
    component.geometry_albedo_resolve_image = component.geometry_albedo_image;
    component.geometry_material_resolve_image =
        component.geometry_material_image;
    std::array<AttachmentTexture*, 7> textures = {
        component.geometry_view_pos_image.get(),
        component.geometry_world_pos_image.get(),
        component.geometry_depth_image.get(),
        component.geometry_normal_image.get(),
        component.geometry_albedo_image.get(),
        component.geometry_material_image.get(),
        component.geometry_depth_stencil.get()};
    component.geometry_framebuffer = geometry_render_pass_->CreateFramebuffer(
        0, textures, component.viewport_size);
  }

  component.lighting_color_image = CreateAttachmentTexture(
      {rw, rh, AttachmentTextureType::Offscreen, 1, swap_chain_image_format_,
       options_.msaa_mode,
       options_.msaa_mode == SamplingMode::DISABLED});
  if (options_.msaa_mode > SamplingMode::DISABLED) {
    component.lighting_color_resolve_image = CreateAttachmentTexture(
        {rw, rh, AttachmentTextureType::Resolve, 1, swap_chain_image_format_,
         SamplingMode::DISABLED, true});

    std::array<AttachmentTexture*, 2> textures{
        component.lighting_color_image.get(),
        component.lighting_color_resolve_image.get()};
    component.lighting_framebuffer =
        lighting_render_pass_->CreateFramebuffer(0, textures, {rw, rh});
  } else {
    component.lighting_color_resolve_image = component.lighting_color_image;
    std::array<AttachmentTexture*, 1> textures{
        component.lighting_color_image.get()};
    component.lighting_framebuffer =
        lighting_render_pass_->CreateFramebuffer(0, textures, {rw, rh});
  }

  component.sprite_color_image = CreateAttachmentTexture(
      {rw, rh, AttachmentTextureType::Offscreen, 1, swap_chain_image_format_,
       SamplingMode::DISABLED, true});

  std::array<AttachmentTexture*, 1> sprite_attachments{
      component.sprite_color_image.get()};
  component.sprite_framebuffer =
      sprite_render_pass_->CreateFramebuffer(0, sprite_attachments, {rw, rh});

  component.composite_color_image = CreateAttachmentTexture(
      {rw, rh, AttachmentTextureType::Offscreen, 1, swap_chain_image_format_,
       options_.msaa_mode,
       options_.msaa_mode == SamplingMode::DISABLED});
  if (options_.msaa_mode > SamplingMode::DISABLED) {
    component.composite_color_resolve_image = CreateAttachmentTexture(
        {rw, rh, AttachmentTextureType::Resolve, 1, swap_chain_image_format_,
         SamplingMode::DISABLED, true});

    std::array<AttachmentTexture*, 2> composite_attachments{
        component.composite_color_image.get(),
        component.composite_color_resolve_image.get()};
    component.composite_framebuffer = composite_render_pass_->CreateFramebuffer(
        0, composite_attachments, {rw, rh});
  } else {
    component.composite_color_resolve_image = component.composite_color_image;
    std::array<AttachmentTexture*, 1> composite_attachments{
        component.composite_color_image.get()};
    component.composite_framebuffer = composite_render_pass_->CreateFramebuffer(
        0, composite_attachments, {rw, rh});
  }

  component.global_descriptor = CreateGlobalDescriptors(component);
  component.shadow_descriptor = CreateShadowGlobalDescriptors(component);
  component.geometry_output_descriptor = CreateReference<DescriptorSet>();
  component.geometry_output_descriptor->SetLayout(
      geometry_output_descriptor_layout_);
  component.geometry_output_descriptor->AddCombinedImageSampler(
      0, component.geometry_view_pos_resolve_image->image_views_[0],
      default_nearest_sampler_);
  component.geometry_output_descriptor->AddCombinedImageSampler(
      1, component.geometry_world_pos_resolve_image->image_views_[0],
      default_nearest_sampler_);
  component.geometry_output_descriptor->AddCombinedImageSampler(
      2, component.geometry_depth_resolve_image->image_views_[0],
      default_nearest_sampler_);
  component.geometry_output_descriptor->AddCombinedImageSampler(
      3, component.geometry_normal_resolve_image->image_views_[0],
      default_nearest_sampler_);
  component.geometry_output_descriptor->AddCombinedImageSampler(
      4, component.geometry_albedo_resolve_image->image_views_[0],
      default_nearest_sampler_);
  component.geometry_output_descriptor->AddCombinedImageSampler(
      5, component.geometry_material_resolve_image->image_views_[0],
      default_nearest_sampler_);
  component.geometry_output_descriptor->Bake();

  component.lighting_output_descriptor = CreateReference<DescriptorSet>();
  component.lighting_output_descriptor->SetLayout(present_descriptor_layout_);
  component.lighting_output_descriptor->AddCombinedImageSampler(
      0, component.lighting_color_resolve_image->image_views_[0],
      default_linear_sampler_);
  component.lighting_output_descriptor->Bake();

  component.sprite_output_descriptor = CreateReference<DescriptorSet>();
  component.sprite_output_descriptor->SetLayout(present_descriptor_layout_);
  component.sprite_output_descriptor->AddCombinedImageSampler(
      0, component.sprite_color_image->image_views_[0],
      default_linear_sampler_);
  component.sprite_output_descriptor->Bake();

  component.composite_output_descriptor = CreateReference<DescriptorSet>();
  component.composite_output_descriptor->SetLayout(present_descriptor_layout_);
  component.composite_output_descriptor->AddCombinedImageSampler(
      0, component.composite_color_resolve_image->image_views_[0],
      default_linear_sampler_);
  component.composite_output_descriptor->Bake();

  component.ssao_gen_descriptor = CreateReference<DescriptorSet>();
  component.ssao_gen_descriptor->SetLayout(ssao_gen_descriptor_layout_);
  component.ssao_gen_descriptor->AddCombinedImageSampler(
      0, component.geometry_view_pos_resolve_image->image_views_[0],
      default_nearest_sampler_);
  component.ssao_gen_descriptor->AddCombinedImageSampler(
      1, component.geometry_normal_resolve_image->image_views_[0],
      default_nearest_sampler_);
  component.ssao_gen_descriptor->AddCombinedImageSampler(
      2, component.geometry_depth_resolve_image->image_views_[0],
      default_nearest_sampler_);
  component.ssao_gen_descriptor->AddCombinedImageSampler(
      3, ssao_noise_->image_views_[0], default_linear_sampler_);
  component.ssao_gen_descriptor->AddUniformBuffer(4,
                                                  ssao_kernel_uniform_buffer_);
  component.ssao_gen_descriptor->Bake();

  component.ssao_output_descriptor = CreateReference<DescriptorSet>();
  component.ssao_output_descriptor->SetLayout(ssao_output_descriptor_layout_);
  component.ssao_output_descriptor->AddCombinedImageSampler(
      0, component.ssao_color_image->image_views_[0], default_nearest_sampler_);
  component.ssao_output_descriptor->AddCombinedImageSampler(
      1, component.geometry_depth_resolve_image->image_views_[0],
      default_nearest_sampler_);
  component.ssao_output_descriptor->Bake();

  component.ssao_blur_horz_output_descriptor = CreateReference<DescriptorSet>();
  component.ssao_blur_horz_output_descriptor->SetLayout(
      ssao_blur_descriptor_layout_);
  component.ssao_blur_horz_output_descriptor->AddCombinedImageSampler(
      0, component.ssao_blur_horz_color_image->image_views_[0],
      default_linear_sampler_);
  component.ssao_blur_horz_output_descriptor->AddCombinedImageSampler(
      1, component.geometry_depth_resolve_image->image_views_[0],
      default_nearest_sampler_);
  component.ssao_blur_horz_output_descriptor->Bake();

  component.ssao_blur_vert_output_descriptor = CreateReference<DescriptorSet>();
  component.ssao_blur_vert_output_descriptor->SetLayout(
      ssao_blur_descriptor_layout_);
  component.ssao_blur_vert_output_descriptor->AddCombinedImageSampler(
      0, component.ssao_blur_vert_color_image->image_views_[0],
      default_linear_sampler_);
  component.ssao_blur_vert_output_descriptor->AddCombinedImageSampler(
      1, component.geometry_depth_resolve_image->image_views_[0],
      default_nearest_sampler_);
  component.ssao_blur_vert_output_descriptor->Bake();

  // --- Bloom resources ---
  uint32_t hrw = rw / 2, hrh = rh / 2;

  component.bloom_extract_image = CreateAttachmentTexture(
      {hrw, hrh, AttachmentTextureType::Offscreen, 1, swap_chain_image_format_,
       SamplingMode::DISABLED, true});
  component.bloom_blur_h_image = CreateAttachmentTexture(
      {hrw, hrh, AttachmentTextureType::Offscreen, 1, swap_chain_image_format_,
       SamplingMode::DISABLED, true});
  component.bloom_blur_v_image = CreateAttachmentTexture(
      {hrw, hrh, AttachmentTextureType::Offscreen, 1, swap_chain_image_format_,
       SamplingMode::DISABLED, true});
  component.bloom_composite_image = CreateAttachmentTexture(
      {rw, rh, AttachmentTextureType::Offscreen, 1, swap_chain_image_format_,
       SamplingMode::DISABLED, true});

  component.bloom_extract_framebuffer =
      postprocess_render_pass_->CreateFramebuffer(
          0, {component.bloom_extract_image.get()}, {hrw, hrh});
  component.bloom_blur_h_framebuffer =
      postprocess_render_pass_->CreateFramebuffer(
          0, {component.bloom_blur_h_image.get()}, {hrw, hrh});
  component.bloom_blur_v_framebuffer =
      postprocess_render_pass_->CreateFramebuffer(
          0, {component.bloom_blur_v_image.get()}, {hrw, hrh});
  component.bloom_composite_framebuffer =
      postprocess_render_pass_->CreateFramebuffer(
          0, {component.bloom_composite_image.get()}, {rw, rh});

  // Bloom extract descriptor: reads composite output
  component.bloom_extract_descriptor = CreateReference<DescriptorSet>();
  component.bloom_extract_descriptor->SetLayout(present_descriptor_layout_);
  component.bloom_extract_descriptor->AddCombinedImageSampler(
      0, component.composite_color_resolve_image->image_views_[0],
      default_linear_sampler_);
  component.bloom_extract_descriptor->Bake();

  // Bloom blur H descriptor: reads bloom extract
  component.bloom_blur_h_descriptor = CreateReference<DescriptorSet>();
  component.bloom_blur_h_descriptor->SetLayout(present_descriptor_layout_);
  component.bloom_blur_h_descriptor->AddCombinedImageSampler(
      0, component.bloom_extract_image->image_views_[0],
      default_linear_sampler_);
  component.bloom_blur_h_descriptor->Bake();

  // Bloom blur V descriptor: reads bloom blur H
  component.bloom_blur_v_descriptor = CreateReference<DescriptorSet>();
  component.bloom_blur_v_descriptor->SetLayout(present_descriptor_layout_);
  component.bloom_blur_v_descriptor->AddCombinedImageSampler(
      0, component.bloom_blur_h_image->image_views_[0],
      default_linear_sampler_);
  component.bloom_blur_v_descriptor->Bake();

  // Bloom composite descriptor: reads composite + bloom blur V (2 inputs)
  component.bloom_composite_descriptor = CreateReference<DescriptorSet>();
  component.bloom_composite_descriptor->SetLayout(
      postprocess_2input_descriptor_layout_);
  component.bloom_composite_descriptor->AddCombinedImageSampler(
      0, component.composite_color_resolve_image->image_views_[0],
      default_linear_sampler_);
  component.bloom_composite_descriptor->AddCombinedImageSampler(
      1, component.bloom_blur_v_image->image_views_[0],
      default_linear_sampler_);
  component.bloom_composite_descriptor->Bake();

  // Bloom output descriptor: for present to read bloom composite result
  component.bloom_output_descriptor = CreateReference<DescriptorSet>();
  component.bloom_output_descriptor->SetLayout(present_descriptor_layout_);
  component.bloom_output_descriptor->AddCombinedImageSampler(
      0, component.bloom_composite_image->image_views_[0],
      default_linear_sampler_);
  component.bloom_output_descriptor->Bake();

  // --- Motion blur resources ---
  component.motion_blur_image = CreateAttachmentTexture(
      {rw, rh, AttachmentTextureType::Offscreen, 1, swap_chain_image_format_,
       SamplingMode::DISABLED, true});
  component.motion_blur_framebuffer =
      postprocess_render_pass_->CreateFramebuffer(
          0, {component.motion_blur_image.get()}, {rw, rh});

  // Motion blur after composite (when bloom is off): reads composite + world pos
  component.motion_blur_after_composite_desc = CreateReference<DescriptorSet>();
  component.motion_blur_after_composite_desc->SetLayout(
      postprocess_2input_descriptor_layout_);
  component.motion_blur_after_composite_desc->AddCombinedImageSampler(
      0, component.composite_color_resolve_image->image_views_[0],
      default_linear_sampler_);
  component.motion_blur_after_composite_desc->AddCombinedImageSampler(
      1, component.geometry_world_pos_resolve_image->image_views_[0],
      default_nearest_sampler_);
  component.motion_blur_after_composite_desc->Bake();

  // Motion blur after bloom (when bloom is on): reads bloom composite + world pos
  component.motion_blur_after_bloom_desc = CreateReference<DescriptorSet>();
  component.motion_blur_after_bloom_desc->SetLayout(
      postprocess_2input_descriptor_layout_);
  component.motion_blur_after_bloom_desc->AddCombinedImageSampler(
      0, component.bloom_composite_image->image_views_[0],
      default_linear_sampler_);
  component.motion_blur_after_bloom_desc->AddCombinedImageSampler(
      1, component.geometry_world_pos_resolve_image->image_views_[0],
      default_nearest_sampler_);
  component.motion_blur_after_bloom_desc->Bake();

  // Motion blur output descriptor: for present to read final result
  component.motion_blur_output_descriptor = CreateReference<DescriptorSet>();
  component.motion_blur_output_descriptor->SetLayout(
      present_descriptor_layout_);
  component.motion_blur_output_descriptor->AddCombinedImageSampler(
      0, component.motion_blur_image->image_views_[0], default_linear_sampler_);
  component.motion_blur_output_descriptor->Bake();

  // FXAA resources
  component.fxaa_image = CreateAttachmentTexture(
      {rw, rh, AttachmentTextureType::Offscreen, 1, swap_chain_image_format_,
       SamplingMode::DISABLED, true});
  {
    std::array<AttachmentTexture*, 1> att{component.fxaa_image.get()};
    component.fxaa_framebuffer = postprocess_render_pass_->CreateFramebuffer(0, att, {rw, rh});
  }

  component.fxaa_after_composite_desc = CreateReference<DescriptorSet>();
  component.fxaa_after_composite_desc->SetLayout(present_descriptor_layout_);
  component.fxaa_after_composite_desc->AddCombinedImageSampler(
      0, component.composite_color_resolve_image->image_views_[0], default_linear_sampler_);
  component.fxaa_after_composite_desc->Bake();

  component.fxaa_after_bloom_desc = CreateReference<DescriptorSet>();
  component.fxaa_after_bloom_desc->SetLayout(present_descriptor_layout_);
  component.fxaa_after_bloom_desc->AddCombinedImageSampler(
      0, component.bloom_composite_image->image_views_[0], default_linear_sampler_);
  component.fxaa_after_bloom_desc->Bake();

  component.fxaa_after_motion_blur_desc = CreateReference<DescriptorSet>();
  component.fxaa_after_motion_blur_desc->SetLayout(present_descriptor_layout_);
  component.fxaa_after_motion_blur_desc->AddCombinedImageSampler(
      0, component.motion_blur_image->image_views_[0], default_linear_sampler_);
  component.fxaa_after_motion_blur_desc->Bake();

  component.fxaa_output_descriptor = CreateReference<DescriptorSet>();
  component.fxaa_output_descriptor->SetLayout(present_descriptor_layout_);
  component.fxaa_output_descriptor->AddCombinedImageSampler(
      0, component.fxaa_image->image_views_[0], default_linear_sampler_);
  component.fxaa_output_descriptor->Bake();

  // TAA resources
  component.taa_output_image = CreateAttachmentTexture(
      {rw, rh, AttachmentTextureType::Offscreen, 1, swap_chain_image_format_,
       SamplingMode::DISABLED, true});
  component.taa_history_image = CreateAttachmentTexture(
      {rw, rh, AttachmentTextureType::Offscreen, 1, swap_chain_image_format_,
       SamplingMode::DISABLED, true});
  {
    std::array<AttachmentTexture*, 1> att{component.taa_output_image.get()};
    component.taa_framebuffer = postprocess_render_pass_->CreateFramebuffer(0, att, {rw, rh});
  }
  {
    std::array<AttachmentTexture*, 1> att{component.taa_history_image.get()};
    component.taa_history_framebuffer = postprocess_render_pass_->CreateFramebuffer(0, att, {rw, rh});
  }

  component.taa_descriptor = CreateReference<DescriptorSet>();
  component.taa_descriptor->SetLayout(taa_descriptor_layout_);
  component.taa_descriptor->AddCombinedImageSampler(
      0, component.composite_color_resolve_image->image_views_[0], default_linear_sampler_);
  component.taa_descriptor->AddCombinedImageSampler(
      1, component.taa_history_image->image_views_[0], default_linear_sampler_);
  component.taa_descriptor->AddCombinedImageSampler(
      2, component.geometry_depth_resolve_image->image_views_[0], default_nearest_sampler_);
  component.taa_descriptor->Bake();

  component.taa_copy_descriptor = CreateReference<DescriptorSet>();
  component.taa_copy_descriptor->SetLayout(present_descriptor_layout_);
  component.taa_copy_descriptor->AddCombinedImageSampler(
      0, component.taa_output_image->image_views_[0], default_linear_sampler_);
  component.taa_copy_descriptor->Bake();

  component.taa_output_descriptor = CreateReference<DescriptorSet>();
  component.taa_output_descriptor->SetLayout(present_descriptor_layout_);
  component.taa_output_descriptor->AddCombinedImageSampler(
      0, component.taa_output_image->image_views_[0], default_linear_sampler_);
  component.taa_output_descriptor->Bake();

  // TAA-aware bloom descriptors
  component.bloom_extract_after_taa_desc = CreateReference<DescriptorSet>();
  component.bloom_extract_after_taa_desc->SetLayout(present_descriptor_layout_);
  component.bloom_extract_after_taa_desc->AddCombinedImageSampler(
      0, component.taa_output_image->image_views_[0], default_linear_sampler_);
  component.bloom_extract_after_taa_desc->Bake();

  component.bloom_composite_after_taa_desc = CreateReference<DescriptorSet>();
  component.bloom_composite_after_taa_desc->SetLayout(postprocess_2input_descriptor_layout_);
  component.bloom_composite_after_taa_desc->AddCombinedImageSampler(
      0, component.taa_output_image->image_views_[0], default_linear_sampler_);
  component.bloom_composite_after_taa_desc->AddCombinedImageSampler(
      1, component.bloom_blur_v_image->image_views_[0], default_linear_sampler_);
  component.bloom_composite_after_taa_desc->Bake();

  // TAA-aware motion blur descriptor
  component.motion_blur_after_taa_desc = CreateReference<DescriptorSet>();
  component.motion_blur_after_taa_desc->SetLayout(postprocess_2input_descriptor_layout_);
  component.motion_blur_after_taa_desc->AddCombinedImageSampler(
      0, component.taa_output_image->image_views_[0], default_linear_sampler_);
  component.motion_blur_after_taa_desc->AddCombinedImageSampler(
      1, component.geometry_world_pos_resolve_image->image_views_[0], default_nearest_sampler_);
  component.motion_blur_after_taa_desc->Bake();

  component.resources_dirty = false;
  component.view_changed = true;
  component.pos_changed = true;
}

Ref<Texture> Renderer::CreateBlankTexture() {
  Ref<Texture> texture = CreateReference<Texture>(TextureTypeDiffuse, "");

  stbi_uc pixels[] = {255, 255, 255, 255};  // full white
  texture->width_ = 1;
  texture->height_ = 1;
  texture->size_ = texture->width_ * texture->height_ * STBI_rgb_alpha;
  texture->mip_levels_ =
      static_cast<uint32_t>(
          std::floor(std::log2(std::max(texture->width_, texture->height_)))) +
      1;

  VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  CreateBuffer(texture->size_, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               stagingBuffer, stagingBufferMemory);

  void* data;
  vkMapMemory(logical_device_, stagingBufferMemory, 0, texture->size_, 0,
              &data);
  memcpy(data, pixels, static_cast<size_t>(texture->size_));
  vkUnmapMemory(logical_device_, stagingBufferMemory);

  CreateImage(texture->width_, texture->height_, texture->mip_levels_,
              SamplingMode::DISABLED, format, VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                  VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture->image_,
              texture->device_memory_);

  TransitionImageLayout(texture->image_, format, VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        texture->mip_levels_);
  CopyBufferToImage(stagingBuffer, texture->image_,
                    static_cast<uint32_t>(texture->width_),
                    static_cast<uint32_t>(texture->height_));

  vkDestroyBuffer(logical_device_, stagingBuffer, nullptr);
  vkFreeMemory(logical_device_, stagingBufferMemory, nullptr);

  // todo loading pregenerated mipmaps
  GenerateMipmaps(texture->image_, VK_FORMAT_R8G8B8A8_UNORM, texture->width_,
                  texture->height_, texture->mip_levels_);

  texture->format_ = format;
  texture->sampler_ = CreateTextureSampler(texture->mip_levels_, {});
  texture->image_view_ = CreateImageView(
      texture->image_, format, VK_IMAGE_ASPECT_COLOR_BIT, texture->mip_levels_);

  texture->is_allocated_ = true;
  return texture;
}

Ref<Texture> Renderer::CreateBlankTexture(const TextureProps& texture_props,
                                          const SamplerProps& sampler_props) {
  Ref<Texture> texture = CreateReference<Texture>(TextureTypeDiffuse, "");

  texture->width_ = texture_props.width;
  texture->height_ = texture_props.height;
  texture->size_ = texture->width_ * texture->height_ * STBI_rgb_alpha;
  stbi_uc* pixels = new stbi_uc[texture->size_];
  std::memset(pixels, 0, texture->size_);  // full black
  texture->mip_levels_ =
      static_cast<uint32_t>(
          std::floor(std::log2(std::max(texture->width_, texture->height_)))) +
      1;

  VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  CreateBuffer(texture->size_, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               stagingBuffer, stagingBufferMemory);

  void* data;
  vkMapMemory(logical_device_, stagingBufferMemory, 0, texture->size_, 0,
              &data);
  memcpy(data, pixels, static_cast<size_t>(texture->size_));
  vkUnmapMemory(logical_device_, stagingBufferMemory);

  CreateImage(texture->width_, texture->height_, texture->mip_levels_,
              SamplingMode::DISABLED, format, VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                  VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture->image_,
              texture->device_memory_);

  TransitionImageLayout(texture->image_, format, VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        texture->mip_levels_);
  CopyBufferToImage(stagingBuffer, texture->image_,
                    static_cast<uint32_t>(texture->width_),
                    static_cast<uint32_t>(texture->height_));

  vkDestroyBuffer(logical_device_, stagingBuffer, nullptr);
  vkFreeMemory(logical_device_, stagingBufferMemory, nullptr);

  // todo loading pregenerated mipmaps
  GenerateMipmaps(texture->image_, VK_FORMAT_R8G8B8A8_UNORM, texture->width_,
                  texture->height_, texture->mip_levels_);

  texture->sampler_ = CreateTextureSampler(1, sampler_props);
  texture->image_view_ = CreateImageView(
      texture->image_, format, VK_IMAGE_ASPECT_COLOR_BIT, texture->mip_levels_);

  texture->is_allocated_ = true;
  return texture;
}

Ref<Texture> Renderer::CreateTexture(const std::string& path,
                                     const TextureProps& texture_props,
                                     const SamplerProps& sampler_props) {
  Ref<Texture> texture = CreateReference<Texture>(texture_props.type, path);

  stbi_uc* pixels =
      stbi_load(path.c_str(), reinterpret_cast<int*>(&texture->width_),
                reinterpret_cast<int*>(&texture->height_), &texture->channels_,
                STBI_rgb_alpha);

  if (!pixels) {
    LOG_WARN("Failed to load texture from {}", path);
    return nullptr;
    //throw std::runtime_error("failed to load texture image: " + path);
  }
  texture->size_ = texture->width_ * texture->height_ * STBI_rgb_alpha;
  if (texture_props.generate_mipmaps) {
    texture->mip_levels_ = static_cast<uint32_t>(std::floor(std::log2(
                               std::max(texture->width_, texture->height_)))) +
                           1;
  } else {
    texture->mip_levels_ = 1;
  }

  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  CreateBuffer(texture->size_, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               stagingBuffer, stagingBufferMemory);

  void* data;
  vkMapMemory(logical_device_, stagingBufferMemory, 0, texture->size_, 0,
              &data);
  memcpy(data, pixels, static_cast<size_t>(texture->size_));
  vkUnmapMemory(logical_device_, stagingBufferMemory);

  stbi_image_free(pixels);

  CreateImage(texture->width_, texture->height_, texture->mip_levels_,
              SamplingMode::DISABLED, texture_props.image_format,
              VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                  VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture->image_,
              texture->device_memory_);

  TransitionImageLayout(
      texture->image_, texture_props.image_format, VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, texture->mip_levels_);
  CopyBufferToImage(stagingBuffer, texture->image_,
                    static_cast<uint32_t>(texture->width_),
                    static_cast<uint32_t>(texture->height_));

  vkDestroyBuffer(logical_device_, stagingBuffer, nullptr);
  vkFreeMemory(logical_device_, stagingBufferMemory, nullptr);

  // todo loading pregenerated mipmaps
  GenerateMipmaps(texture->image_, texture_props.image_format, texture->width_,
                  texture->height_, texture->mip_levels_);
  texture->sampler_ = CreateTextureSampler(texture->mip_levels_, sampler_props);
  texture->image_view_ =
      CreateImageView(texture->image_, texture_props.image_format,
                      VK_IMAGE_ASPECT_COLOR_BIT, texture->mip_levels_);

  texture->is_allocated_ = true;
  return texture;
}

Ref<Texture> Renderer::CreateTexture(void* buffer, size_t size_per_pixel,
                                     const TextureProps& texture_props,
                                     const SamplerProps& sampler_props) {
  Ref<Texture> texture = CreateReference<Texture>(texture_props.type, "");
  texture->width_ = texture_props.width;
  texture->height_ = texture_props.height;
  texture->size_ = texture->width_ * texture->height_ * size_per_pixel;

  if (texture_props.generate_mipmaps) {
    texture->mip_levels_ = static_cast<uint32_t>(std::floor(std::log2(
                               std::max(texture->width_, texture->height_)))) +
                           1;
  } else {
    texture->mip_levels_ = 1;
  }

  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  CreateBuffer(texture->size_, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               stagingBuffer, stagingBufferMemory);

  void* data;
  vkMapMemory(logical_device_, stagingBufferMemory, 0, texture->size_, 0,
              &data);
  memcpy(data, buffer, static_cast<size_t>(texture->size_));
  vkUnmapMemory(logical_device_, stagingBufferMemory);

  CreateImage(texture->width_, texture->height_, texture->mip_levels_,
              SamplingMode::DISABLED, texture_props.image_format,
              VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                  VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture->image_,
              texture->device_memory_);

  TransitionImageLayout(
      texture->image_, texture_props.image_format, VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, texture->mip_levels_);
  CopyBufferToImage(stagingBuffer, texture->image_, texture->width_,
                    texture->height_);

  vkDestroyBuffer(logical_device_, stagingBuffer, nullptr);
  vkFreeMemory(logical_device_, stagingBufferMemory, nullptr);

  // todo loading pregenerated mipmaps
  GenerateMipmaps(texture->image_, texture_props.image_format, texture->width_,
                  texture->height_, texture->mip_levels_);
  texture->sampler_ = CreateTextureSampler(texture->mip_levels_, sampler_props);
  texture->image_view_ =
      CreateImageView(texture->image_, texture_props.image_format,
                      VK_IMAGE_ASPECT_COLOR_BIT, texture->mip_levels_);

  texture->is_allocated_ = true;
  return texture;
}

Ref<Texture> Renderer::CreateCubemapTexture(
    const std::array<std::string, 6>& paths, const TextureProps& texture_props,
    const SamplerProps& sampler_props) {
  Ref<Texture> texture = CreateReference<Texture>(texture_props.type, "");
  VkDeviceSize totalSize;
  stbi_uc* allPixels;
  for (size_t i = 0; i < 6; ++i) {
    int w, h, channels;
    stbi_uc* pixels =
        stbi_load(paths[i].c_str(), &w, &h, &channels, STBI_rgb_alpha);
    if (!pixels) {
      throw std::runtime_error("failed to load texture image: " + paths[i]);
    }

    if (i == 0) {
      texture->width_ = w;
      texture->height_ = h;
      texture->size_ = texture->width_ * texture->height_ * STBI_rgb_alpha;
      texture->mip_levels_ = 1;
      totalSize = texture->size_ * 6;
      allPixels = new stbi_uc[totalSize];
    }

    if (w != texture->width_ || h != texture->height_) {
      throw std::runtime_error("cubemap face size mismatch!");
    }

    memcpy(allPixels + i * texture->size_, pixels, texture->size_);
    stbi_image_free(pixels);
  }
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  CreateBuffer(totalSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               stagingBuffer, stagingBufferMemory);

  void* data;
  vkMapMemory(logical_device_, stagingBufferMemory, 0, totalSize, 0, &data);
  memcpy(data, allPixels, static_cast<size_t>(totalSize));
  vkUnmapMemory(logical_device_, stagingBufferMemory);

  CreateImage(texture->width_, texture->height_, texture->mip_levels_,
              SamplingMode::DISABLED, texture_props.image_format,
              VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                  VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture->image_,
              texture->device_memory_, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, 6);

  for (uint32_t layer = 0; layer < 6; layer++) {
    TransitionImageLayout(
        texture->image_, texture_props.image_format, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, texture->mip_levels_, layer, 1);

    CopyBufferToImage(
        stagingBuffer, texture->image_, static_cast<uint32_t>(texture->width_),
        static_cast<uint32_t>(texture->height_), texture->size_ * layer, layer);
  }

  vkDestroyBuffer(logical_device_, stagingBuffer, nullptr);
  vkFreeMemory(logical_device_, stagingBufferMemory, nullptr);
  delete[] allPixels;

  texture->sampler_ = CreateTextureSampler(texture->mip_levels_, sampler_props);
  texture->image_view_ = CreateImageView(
      texture->image_, texture_props.image_format, VK_IMAGE_ASPECT_COLOR_BIT,
      texture->mip_levels_, VK_IMAGE_VIEW_TYPE_CUBE, 0, 6);
  for (uint32_t layer = 0; layer < 6; layer++) {
    TransitionImageLayout(texture->image_, texture_props.image_format,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          texture->mip_levels_, layer, 1);
  }
  texture->is_allocated_ = true;
  return texture;
}Ref<Texture> Renderer::CreateCubemapTextureFromSingle(
    const std::string& path,
    const TextureProps& texture_props,
    const SamplerProps& sampler_props)
{
    Ref<Texture> texture = CreateReference<Texture>(texture_props.type, "");

    int w, h, channels;
    stbi_uc* pixels = stbi_load(path.c_str(), &w, &h, &channels, STBI_rgb_alpha);
    if (!pixels)
        throw std::runtime_error("failed to load cubemap: " + path);

    const bool isHorizontalStrip =
        (w % 6 == 0) && (h > 0) && (h == w / 6);

    const bool isVerticalCross =
        (w % 4 == 0) && (h % 3 == 0) && (w / 4 == h / 3);

    if (!isHorizontalStrip && !isVerticalCross)
        throw std::runtime_error("Unsupported cubemap layout: " + path);

    uint32_t faceSize = 0;

    if (isHorizontalStrip)
        faceSize = w / 6;
    else
        faceSize = w / 4;

    texture->width_  = faceSize;
    texture->height_ = faceSize;
    texture->size_   = faceSize * faceSize * 4;
    texture->mip_levels_ = 1;

    VkDeviceSize totalSize = texture->size_ * 6;

    std::vector<stbi_uc> cubePixels(totalSize);

    auto copyFace = [&](uint32_t faceIndex, uint32_t gridX, uint32_t gridY)
    {
        for (uint32_t row = 0; row < faceSize; row++)
        {
            memcpy(
                cubePixels.data() + faceIndex * texture->size_ + row * faceSize * 4,
                pixels + ((gridY * faceSize + row) * w + gridX * faceSize) * 4,
                faceSize * 4
            );
        }
    };

    if (isHorizontalStrip)
    {
        for (uint32_t face = 0; face < 6; face++)
        {
            copyFace(face, face, 0);
        }
    }
    else
    {
        // Vertical cross (4x3 fold layout)
        // Grid layout:
        //       +Y        (1,0)
        // -X +Z +X -Z     (0,1)(1,1)(2,1)(3,1)
        //       -Y        (1,2)

        copyFace(0, 2, 1); // +X
        copyFace(1, 0, 1); // -X
        copyFace(2, 1, 0); // +Y
        copyFace(3, 1, 2); // -Y
        copyFace(4, 1, 1); // +Z
        copyFace(5, 3, 1); // -Z
    }

    stbi_image_free(pixels);

    // Create staging buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;

    CreateBuffer(totalSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer,
                 stagingMemory);

    void* data;
    vkMapMemory(logical_device_, stagingMemory, 0, totalSize, 0, &data);
    memcpy(data, cubePixels.data(), totalSize);
    vkUnmapMemory(logical_device_, stagingMemory);

    // Create cube image
    CreateImage(faceSize, faceSize,
                texture->mip_levels_,
                SamplingMode::DISABLED,
                texture_props.image_format,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                texture->image_,
                texture->device_memory_,
                VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
                6);

    // Transition entire cube
    TransitionImageLayout(
        texture->image_,
        texture_props.image_format,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        texture->mip_levels_,
        0,
        6);

    // Copy all 6 faces at once
    CopyBufferToImage(
        stagingBuffer,
        texture->image_,
        faceSize,
        faceSize,
        0,
        0,
        6);

    TransitionImageLayout(
        texture->image_,
        texture_props.image_format,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        texture->mip_levels_,
        0,
        6);

    vkDestroyBuffer(logical_device_, stagingBuffer, nullptr);
    vkFreeMemory(logical_device_, stagingMemory, nullptr);

    texture->sampler_ =
        CreateTextureSampler(texture->mip_levels_, sampler_props);

    texture->image_view_ =
        CreateImageView(texture->image_,
                        texture_props.image_format,
                        VK_IMAGE_ASPECT_COLOR_BIT,
                        texture->mip_levels_,
                        VK_IMAGE_VIEW_TYPE_CUBE,
                        0,
                        6);

    texture->is_allocated_ = true;
    return texture;
}


Ref<AttachmentTexture> Renderer::CreateAttachmentTexture(
    const AttachmentTextureProps& props) {
  if (props.type == AttachmentTextureType::SwapChain) {
    throw new std::runtime_error(
        "AttachmentTextureType::SwapChain cannot be created!");
  }
  Ref<AttachmentTexture> texture = CreateReference<AttachmentTexture>();
  texture->type_ = props.type;
  texture->format_ = props.image_format;
  texture->width_ = props.width;
  texture->height_ = props.height;
  texture->sampling_mode_ = props.sampling_mode;
  int usage;
  if (props.type == AttachmentTextureType::DepthStencil) {
    usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  } else {
    usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  }

  if (props.sampled) {
    usage |= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  } else {
    usage |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
  }
  if (props.transfer_dest) {
    usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  }

  int aspectFlags;
  if (props.type == AttachmentTextureType::DepthStencil) {
    aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
  } else {
    aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
  }
  texture->aspect_flags_ = aspectFlags;
  texture->mip_levels_ = 1;
  texture->images_.resize(props.image_count);
  texture->device_memories_.resize(props.image_count);
  texture->image_views_.resize(props.image_count);

  for (uint32_t i = 0; i < props.image_count; i++) {
    CreateImage(props.width, props.height, 1, props.sampling_mode,
                props.image_format, VK_IMAGE_TILING_OPTIMAL, usage,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture->images_[i],
                texture->device_memories_[i], 0, props.layer_count);

    if (props.layer_count != 1)
      texture->image_views_[i] =
          CreateImageView(texture->images_[i], props.image_format, aspectFlags,
                          1, VK_IMAGE_VIEW_TYPE_2D_ARRAY, 0, props.layer_count);
    else {
      texture->image_views_[i] =
          CreateImageView(texture->images_[i], props.image_format, aspectFlags,
                          1, VK_IMAGE_VIEW_TYPE_2D, 0, 1);
    }

    if (props.type == AttachmentTextureType::DepthStencil) {
      TransitionImageLayout(texture->images_[i], props.image_format,
                            VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1,
                            0, props.layer_count);
    } else if (props.type == AttachmentTextureType::Color ||
               props.type == AttachmentTextureType::Resolve ||
               props.type == AttachmentTextureType::Offscreen) {
      TransitionImageLayout(
          texture->images_[i], props.image_format, VK_IMAGE_LAYOUT_UNDEFINED,
          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1, 0, props.layer_count);
    } else if (props.type == AttachmentTextureType::SwapChain) {
      TransitionImageLayout(
          texture->images_[i], props.image_format, VK_IMAGE_LAYOUT_UNDEFINED,
          VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 1, 0, props.layer_count);
    }
  }

  texture->is_allocated_ = true;
  return texture;
}

void Renderer::SetAttachmentTextureBuffer(Ref<AttachmentTexture> texture,
                                          void* buffer, size_t sizePerPixel) {
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  size_t size = texture->width_ * texture->height_ * sizePerPixel;
  CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               stagingBuffer, stagingBufferMemory);

  void* data;
  vkMapMemory(logical_device_, stagingBufferMemory, 0, size, 0, &data);
  memcpy(data, buffer, static_cast<size_t>(size));
  vkUnmapMemory(logical_device_, stagingBufferMemory);

  TransitionImageLayout(texture->images_[0], texture->format_,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1);

  CopyBufferToImage(stagingBuffer, texture->images_[0],
                    static_cast<uint32_t>(texture->width_),
                    static_cast<uint32_t>(texture->height_));

  TransitionImageLayout(texture->images_[0], texture->format_,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1);

  vkDestroyBuffer(logical_device_, stagingBuffer, nullptr);
  vkFreeMemory(logical_device_, stagingBufferMemory, nullptr);
}

void Renderer::DestroyTexture(Texture& texture) {
  if (!texture.is_allocated_) {
    return;
  }

  texture.image_view_ = nullptr;
  vkDeviceWaitIdle(logical_device_);
  vkDestroySampler(logical_device_, texture.sampler_, nullptr);
  vkDestroyImage(logical_device_, texture.image_, nullptr);
  vkFreeMemory(logical_device_, texture.device_memory_, nullptr);

  texture.is_allocated_ = false;
}

VkSampler Renderer::CreateTextureSampler(uint32_t mip_levels,
                                         const SamplerProps& props) {
  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = props.MagFilter;
  samplerInfo.minFilter = props.MinFilter;
  samplerInfo.addressModeU = props.AddressMode;
  samplerInfo.addressModeV = props.AddressMode;
  samplerInfo.addressModeW = props.AddressMode;

  if (props.MaxAnisotropy <= 0) {
    samplerInfo.anisotropyEnable = VK_FALSE;
  } else {
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy =
        std::min(props.MaxAnisotropy,
                 physical_device_properties_.limits.maxSamplerAnisotropy);
  }
  samplerInfo.borderColor = props.BorderColor;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = VK_FALSE;
  samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerInfo.maxLod = static_cast<float>(mip_levels);

  VkSampler sampler;
  WIESEL_CHECK_VKRESULT(
      vkCreateSampler(logical_device_, &samplerInfo, nullptr, &sampler));
  return sampler;
}

void Renderer::DestroyAttachmentTexture(AttachmentTexture& texture) {
  if (!texture.is_allocated_) {
    return;
  }
  vkDeviceWaitIdle(logical_device_);
  texture.image_views_.clear();
  if (texture.type_ != AttachmentTextureType::SwapChain) {
    for (VkImage& image : texture.images_) {
      vkDestroyImage(logical_device_, image, nullptr);
    }
    for (VkDeviceMemory& memory : texture.device_memories_) {
      vkFreeMemory(logical_device_, memory, nullptr);
    }
  }
  texture.is_allocated_ = false;
}

Ref<DescriptorSet> Renderer::CreateMeshDescriptors(
    Ref<UniformBuffer> uniform_buffer, Ref<Material> material) {
  Ref<DescriptorSet> object = CreateReference<DescriptorSet>();

  VkDescriptorPoolSize poolSizes[] = {
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kMaterialTextureCount}};

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = std::size(poolSizes);
  poolInfo.pPoolSizes = poolSizes;
  poolInfo.maxSets = 1;
  // Allocate pool
  WIESEL_CHECK_VKRESULT(vkCreateDescriptorPool(
      logical_device_, &poolInfo, nullptr, &object->descriptor_pool_));

  std::vector<VkDescriptorSetLayout> layouts(
      1, geometry_mesh_descriptor_layout_->layout_);
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = object->descriptor_pool_;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = layouts.data();
  WIESEL_CHECK_VKRESULT(vkAllocateDescriptorSets(logical_device_, &allocInfo,
                                                 &object->descriptor_set_));

  std::vector<VkWriteDescriptorSet> writes;
  writes.reserve(8);
  std::vector<VkDescriptorBufferInfo> bufferInfos;
  bufferInfos.reserve(1);
  std::vector<VkDescriptorImageInfo> imageInfos;
  imageInfos.reserve(7);

  {
    bufferInfos.push_back({
        .buffer = uniform_buffer->buffer_handle_,
        .offset = 0,
        .range = sizeof(MatricesUniformData),
    });
    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = object->descriptor_set_;
    set.dstBinding = 0;
    set.dstArrayElement = 0;
    set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    set.descriptorCount = 1;
    set.pBufferInfo = &bufferInfos.back();
    set.pNext = nullptr;
    writes.emplace_back(set);
  }

  {  // base texture
    VkDescriptorImageInfo imageInfo;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    if (material->base_texture == nullptr) {
      imageInfo.imageView = blank_texture_->image_view_->handle_;
      imageInfo.sampler = blank_texture_->sampler_;
    } else {
      imageInfo.imageView = material->base_texture->image_view_->handle_;
      imageInfo.sampler = material->base_texture->sampler_;
    }
    imageInfos.emplace_back(imageInfo);

    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = object->descriptor_set_;
    set.dstBinding = 1;
    set.dstArrayElement = 0;
    set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    set.descriptorCount = 1;
    set.pImageInfo = &imageInfos.back();
    set.pNext = nullptr;
    writes.emplace_back(set);
  }

  {  // normal texture
    VkDescriptorImageInfo imageInfo;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    if (material->normal_map == nullptr) {
      imageInfo.imageView = blank_texture_->image_view_->handle_;
      imageInfo.sampler = blank_texture_->sampler_;
    } else {
      imageInfo.imageView = material->normal_map->image_view_->handle_;
      imageInfo.sampler = material->normal_map->sampler_;
    }
    imageInfos.emplace_back(imageInfo);

    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = object->descriptor_set_;
    set.dstBinding = 2;
    set.dstArrayElement = 0;
    set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    set.descriptorCount = 1;
    set.pImageInfo = &imageInfos.back();
    set.pNext = nullptr;
    writes.emplace_back(set);
  }

  {  // specular texture
    VkDescriptorImageInfo imageInfo;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    if (material->specular_map == nullptr) {
      imageInfo.imageView = blank_texture_->image_view_->handle_;
      imageInfo.sampler = blank_texture_->sampler_;
    } else {
      imageInfo.imageView = material->specular_map->image_view_->handle_;
      imageInfo.sampler = material->specular_map->sampler_;
    }
    imageInfos.emplace_back(imageInfo);

    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = object->descriptor_set_;
    set.dstBinding = 3;
    set.dstArrayElement = 0;
    set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    set.descriptorCount = 1;
    set.pImageInfo = &imageInfos.back();
    set.pNext = nullptr;
    writes.emplace_back(set);
  }

  {  // height texture
    VkDescriptorImageInfo imageInfo;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    if (material->height_map == nullptr) {
      imageInfo.imageView = blank_texture_->image_view_->handle_;
      imageInfo.sampler = blank_texture_->sampler_;
    } else {
      imageInfo.imageView = material->height_map->image_view_->handle_;
      imageInfo.sampler = material->height_map->sampler_;
    }
    imageInfos.emplace_back(imageInfo);

    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = object->descriptor_set_;
    set.dstBinding = 4;
    set.dstArrayElement = 0;
    set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    set.descriptorCount = 1;
    set.pImageInfo = &imageInfos.back();
    set.pNext = nullptr;
    writes.emplace_back(set);
  }

  {  // albedo texture
    VkDescriptorImageInfo imageInfo;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    if (material->albedo_map == nullptr) {
      imageInfo.imageView = blank_texture_->image_view_->handle_;
      imageInfo.sampler = blank_texture_->sampler_;
    } else {
      imageInfo.imageView = material->albedo_map->image_view_->handle_;
      imageInfo.sampler = material->albedo_map->sampler_;
    }
    imageInfos.emplace_back(imageInfo);

    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = object->descriptor_set_;
    set.dstBinding = 5;
    set.dstArrayElement = 0;
    set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    set.descriptorCount = 1;
    set.pImageInfo = &imageInfos.back();
    set.pNext = nullptr;
    writes.emplace_back(set);
  }

  {  // roughness texture
    VkDescriptorImageInfo imageInfo;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    if (material->roughness_map == nullptr) {
      imageInfo.imageView = blank_texture_->image_view_->handle_;
      imageInfo.sampler = blank_texture_->sampler_;
    } else {
      imageInfo.imageView = material->roughness_map->image_view_->handle_;
      imageInfo.sampler = material->roughness_map->sampler_;
    }
    imageInfos.emplace_back(imageInfo);

    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = object->descriptor_set_;
    set.dstBinding = 6;
    set.dstArrayElement = 0;
    set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    set.descriptorCount = 1;
    set.pImageInfo = &imageInfos.back();
    set.pNext = nullptr;
    writes.emplace_back(set);
  }

  {  // metallic texture
    VkDescriptorImageInfo imageInfo;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    if (material->metallic_map == nullptr) {
      imageInfo.imageView = blank_texture_->image_view_->handle_;
      imageInfo.sampler = blank_texture_->sampler_;
    } else {
      imageInfo.imageView = material->metallic_map->image_view_->handle_;
      imageInfo.sampler = material->metallic_map->sampler_;
    }
    imageInfos.emplace_back(imageInfo);

    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = object->descriptor_set_;
    set.dstBinding = 7;
    set.dstArrayElement = 0;
    set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    set.descriptorCount = 1;
    set.pImageInfo = &imageInfos.back();
    set.pNext = nullptr;
    writes.emplace_back(set);
  }

  vkUpdateDescriptorSets(logical_device_, static_cast<uint32_t>(writes.size()),
                         writes.data(), 0, nullptr);

  return object;
}

Ref<DescriptorSet> Renderer::CreateShadowMeshDescriptors(
    Ref<UniformBuffer> uniformBuffer, Ref<Material> material) {
  Ref<DescriptorSet> object = CreateReference<DescriptorSet>();

  VkDescriptorPoolSize poolSizes[] = {
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
  };

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = std::size(poolSizes);
  poolInfo.pPoolSizes = poolSizes;
  poolInfo.maxSets = 1;
  // Allocate pool
  WIESEL_CHECK_VKRESULT(vkCreateDescriptorPool(
      logical_device_, &poolInfo, nullptr, &object->descriptor_pool_));

  std::vector<VkDescriptorSetLayout> layouts{
      1, shadow_mesh_descriptor_layout_->layout_};
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = object->descriptor_pool_;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = layouts.data();
  WIESEL_CHECK_VKRESULT(vkAllocateDescriptorSets(logical_device_, &allocInfo,
                                                 &object->descriptor_set_));

  std::vector<VkWriteDescriptorSet> writes;
  writes.reserve(2);
  std::vector<VkDescriptorBufferInfo> bufferInfos;
  bufferInfos.reserve(1);
  std::vector<VkDescriptorImageInfo> imageInfos;
  imageInfos.reserve(1);

  {
    bufferInfos.push_back({
        .buffer = uniformBuffer->buffer_handle_,
        .offset = 0,
        .range = sizeof(MatricesUniformData),
    });
    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = object->descriptor_set_;
    set.dstBinding = 0;
    set.dstArrayElement = 0;
    set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    set.descriptorCount = 1;
    set.pBufferInfo = &bufferInfos.back();
    set.pNext = nullptr;
    writes.emplace_back(set);
  }

  {  // metallic texture
    VkDescriptorImageInfo imageInfo;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    if (material->base_texture == nullptr) {
      imageInfo.imageView = blank_texture_->image_view_->handle_;
      imageInfo.sampler = blank_texture_->sampler_;
    } else {
      imageInfo.imageView = material->base_texture->image_view_->handle_;
      imageInfo.sampler = material->base_texture->sampler_;
    }
    imageInfos.emplace_back(imageInfo);

    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = object->descriptor_set_;
    set.dstBinding = 1;
    set.dstArrayElement = 0;
    set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    set.descriptorCount = 1;
    set.pImageInfo = &imageInfos.back();
    set.pNext = nullptr;
    writes.emplace_back(set);
  }

  vkUpdateDescriptorSets(logical_device_, static_cast<uint32_t>(writes.size()),
                         writes.data(), 0, nullptr);

  return object;
}

Ref<DescriptorSet> Renderer::CreateGlobalDescriptors(CameraComponent& camera) {
  Ref<DescriptorSet> object = CreateReference<DescriptorSet>();

  VkDescriptorPoolSize poolSizes[] = {
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}};

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = std::size(poolSizes);
  poolInfo.pPoolSizes = poolSizes;
  poolInfo.maxSets = 1;
  // Allocate pool
  WIESEL_CHECK_VKRESULT(vkCreateDescriptorPool(
      logical_device_, &poolInfo, nullptr, &object->descriptor_pool_));

  std::vector<VkDescriptorSetLayout> layouts{
      1, global_descriptor_layout_->layout_};
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = object->descriptor_pool_;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = layouts.data();
  WIESEL_CHECK_VKRESULT(vkAllocateDescriptorSets(logical_device_, &allocInfo,
                                                 &object->descriptor_set_));

  std::vector<VkWriteDescriptorSet> writes;
  writes.reserve(4);
  std::vector<VkDescriptorBufferInfo> bufferInfos;
  bufferInfos.reserve(3);
  std::vector<VkDescriptorImageInfo> imageInfos;
  imageInfos.reserve(1);

  {
    VkDescriptorBufferInfo bufferInfo;
    bufferInfo.buffer = lights_uniform_buffer_->buffer_handle_;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(LightsUniformData);
    bufferInfos.emplace_back(bufferInfo);

    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = object->descriptor_set_;
    set.dstBinding = 0;
    set.dstArrayElement = 0;
    set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    set.descriptorCount = 1;
    set.pBufferInfo = &bufferInfos[bufferInfos.size() - 1];
    set.pNext = nullptr;

    writes.emplace_back(set);
  }

  {
    VkDescriptorBufferInfo bufferInfo;
    bufferInfo.buffer = camera_uniform_buffer_->buffer_handle_;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(CameraUniformData);
    bufferInfos.emplace_back(bufferInfo);

    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = object->descriptor_set_;
    set.dstBinding = 1;
    set.dstArrayElement = 0;
    set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    set.descriptorCount = 1;
    set.pBufferInfo = &bufferInfos[bufferInfos.size() - 1];
    set.pNext = nullptr;

    writes.emplace_back(set);
  }

  {
    VkDescriptorBufferInfo bufferInfo;
    bufferInfo.buffer = shadow_camera_uniform_buffer_->buffer_handle_;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(ShadowMapMatricesUniformData);
    bufferInfos.emplace_back(bufferInfo);

    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = object->descriptor_set_;
    set.dstBinding = 2;
    set.dstArrayElement = 0;
    set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    set.descriptorCount = 1;
    set.pBufferInfo = &bufferInfos[bufferInfos.size() - 1];
    set.pNext = nullptr;

    writes.emplace_back(set);
  }

  {
    VkDescriptorImageInfo imageInfo;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    if (camera.shadow_depth_view_array == nullptr) {
      imageInfo.imageView = blank_texture_->image_view_->handle_;
      imageInfo.sampler = blank_texture_->sampler_;
    } else {
      imageInfo.imageView = camera.shadow_depth_view_array->handle_;
      imageInfo.sampler = default_linear_sampler_->sampler_;
    }
    imageInfos.emplace_back(imageInfo);

    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = object->descriptor_set_;
    set.dstBinding = 3;
    set.dstArrayElement = 0;
    set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    set.descriptorCount = 1;
    set.pImageInfo = &imageInfos.back();
    set.pNext = nullptr;
    writes.emplace_back(set);
  }

  vkUpdateDescriptorSets(logical_device_, static_cast<uint32_t>(writes.size()),
                         writes.data(), 0, nullptr);

  object->allocated_ = true;

  return object;
}

Ref<DescriptorSet> Renderer::CreateShadowGlobalDescriptors(
    CameraComponent& camera) {
  Ref<DescriptorSet> object = CreateReference<DescriptorSet>();

  VkDescriptorPoolSize poolSizes[] = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}};

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = std::size(poolSizes);
  poolInfo.pPoolSizes = poolSizes;
  poolInfo.maxSets = 1;
  // Allocate pool
  WIESEL_CHECK_VKRESULT(vkCreateDescriptorPool(
      logical_device_, &poolInfo, nullptr, &object->descriptor_pool_));

  std::vector<VkDescriptorSetLayout> layouts{
      1, global_shadow_descriptor_layout_->layout_};
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = object->descriptor_pool_;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = layouts.data();
  WIESEL_CHECK_VKRESULT(vkAllocateDescriptorSets(logical_device_, &allocInfo,
                                                 &object->descriptor_set_));

  std::vector<VkWriteDescriptorSet> writes;
  writes.reserve(1);
  std::vector<VkDescriptorBufferInfo> bufferInfos;
  bufferInfos.reserve(1);

  {
    VkDescriptorBufferInfo bufferInfo;
    bufferInfo.buffer = shadow_camera_uniform_buffer_->buffer_handle_;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(ShadowMapMatricesUniformData);
    bufferInfos.emplace_back(bufferInfo);

    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = object->descriptor_set_;
    set.dstBinding = 0;
    set.dstArrayElement = 0;
    set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    set.descriptorCount = 1;
    set.pBufferInfo = &bufferInfos[bufferInfos.size() - 1];
    set.pNext = nullptr;

    writes.emplace_back(set);
  }

  vkUpdateDescriptorSets(logical_device_, static_cast<uint32_t>(writes.size()),
                         writes.data(), 0, nullptr);

  return object;
}

Ref<DescriptorSet> Renderer::CreateDescriptors(Ref<AttachmentTexture> texture) {
  Ref<DescriptorSet> object = CreateReference<DescriptorSet>();

  VkDescriptorPoolSize poolSizes[] = {
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}};

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = std::size(poolSizes);
  poolInfo.pPoolSizes = poolSizes;
  poolInfo.maxSets = 1;

  // Allocate pool
  WIESEL_CHECK_VKRESULT(vkCreateDescriptorPool(
      logical_device_, &poolInfo, nullptr, &object->descriptor_pool_));

  std::vector<VkDescriptorSetLayout> layouts{
      1, present_descriptor_layout_->layout_};
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = object->descriptor_pool_;
  allocInfo.descriptorSetCount = layouts.size();
  allocInfo.pSetLayouts = layouts.data();
  WIESEL_CHECK_VKRESULT(vkAllocateDescriptorSets(logical_device_, &allocInfo,
                                                 &object->descriptor_set_));

  std::vector<VkWriteDescriptorSet> writes{};

  VkDescriptorImageInfo imageInfo;
  imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  imageInfo.imageView = texture->image_views_[0]->handle_;
  imageInfo.sampler = texture->sampler_ ? texture->sampler_->sampler_
                                        : default_linear_sampler_->sampler_;
  VkWriteDescriptorSet set{};
  set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  set.dstSet = object->descriptor_set_;
  set.dstBinding = 0;
  set.dstArrayElement = 0;
  set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  set.descriptorCount = 1;
  set.pImageInfo = &imageInfo;
  set.pNext = nullptr;

  writes.push_back(set);

  vkUpdateDescriptorSets(logical_device_, static_cast<uint32_t>(writes.size()),
                         writes.data(), 0, nullptr);

  return object;
}

Ref<DescriptorSet> Renderer::CreateSkyboxDescriptors(Ref<Texture> texture) {
  Ref<DescriptorSet> object = CreateReference<DescriptorSet>();

  VkDescriptorPoolSize poolSizes[] = {
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}};

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = std::size(poolSizes);
  poolInfo.pPoolSizes = poolSizes;
  poolInfo.maxSets = 1;

  // Allocate pool
  WIESEL_CHECK_VKRESULT(vkCreateDescriptorPool(
      logical_device_, &poolInfo, nullptr, &object->descriptor_pool_));

  std::vector<VkDescriptorSetLayout> layouts{
      1, present_descriptor_layout_->layout_};
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = object->descriptor_pool_;
  allocInfo.descriptorSetCount = layouts.size();
  allocInfo.pSetLayouts = layouts.data();
  WIESEL_CHECK_VKRESULT(vkAllocateDescriptorSets(logical_device_, &allocInfo,
                                                 &object->descriptor_set_));

  std::vector<VkWriteDescriptorSet> writes{};

  VkDescriptorImageInfo imageInfo;
  imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  imageInfo.imageView = texture->image_view_->handle_;
  imageInfo.sampler =
      texture->sampler_ ? texture->sampler_ : default_linear_sampler_->sampler_;

  VkWriteDescriptorSet set{};
  set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  set.dstSet = object->descriptor_set_;
  set.dstBinding = 0;
  set.dstArrayElement = 0;
  set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  set.descriptorCount = 1;
  set.pImageInfo = &imageInfo;
  set.pNext = nullptr;

  writes.push_back(set);

  vkUpdateDescriptorSets(logical_device_, static_cast<uint32_t>(writes.size()),
                         writes.data(), 0, nullptr);

  return object;
}

void Renderer::DestroyDescriptorLayout(DescriptorSetLayout& layout) {
  if (!layout.allocated_) {
    return;
  }
  layout.allocated_ = false;
  vkDestroyDescriptorSetLayout(logical_device_, layout.layout_, nullptr);
}

void Renderer::SetClearColor(float r, float g, float b, float a) {
  clear_color_.red = r;
  clear_color_.green = g;
  clear_color_.blue = b;
  clear_color_.alpha = a;
}

void Renderer::SetClearColor(const Colorf& color) {
  clear_color_ = color;
}

Colorf& Renderer::GetClearColor() {
  return clear_color_;
}

void Renderer::Cleanup() {
  if (!initialized_) {
    return;
  }

  vkDeviceWaitIdle(logical_device_);
  LOG_DEBUG("Destroying Renderer");

  camera_ = nullptr;
  quad_index_buffer_ = nullptr;
  quad_vertex_buffer_ = nullptr;

  CleanupGlobalUniformBuffers();
  blank_texture_ = nullptr;

  LOG_DEBUG("Destroying graphics");
  CleanupGeometryGraphics();
  CleanupPresentGraphics();

  LOG_DEBUG("Destroying descriptor set layout");
  CleanupDescriptorLayouts();

  LOG_DEBUG("Destroying semaphores and fences");
  vkDestroySemaphore(logical_device_, render_finished_semaphore_, nullptr);
  vkDestroySemaphore(logical_device_, image_available_semaphore_, nullptr);
  vkDestroyFence(logical_device_, fence_, nullptr);

  LOG_DEBUG("Destroying command pool");
  command_buffer_ = nullptr;
  command_pool_ = nullptr;

  LOG_DEBUG("Destroying device");
  vkDestroyDevice(logical_device_, nullptr);

#ifdef VULKAN_VALIDATION
  LOG_DEBUG("Destroying debug messanger");
  DestroyDebugUtilsMessengerEXT(instance_, debug_messenger_, nullptr);
#endif

  LOG_DEBUG("Destroying surface khr");
  vkDestroySurfaceKHR(instance_, surface_, nullptr);
  LOG_DEBUG("Destroying vulkan instance");
  vkDestroyInstance(instance_, nullptr);

  LOG_DEBUG("Renderer destroyed");
  Spirv::Cleanup();
  initialized_ = false;
}

void Renderer::CreateVulkanInstance() {
#ifdef VULKAN_VALIDATION
  if (!CheckValidationLayerSupport()) {
    throw std::runtime_error("validation layers requested, but not available!");
  }
#endif

  VkApplicationInfo appInfo{};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = "Wiesel";
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.pEngineName = "No Engine";
  appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.apiVersion = VK_API_VERSION_1_0;

  VkInstanceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.pApplicationInfo = &appInfo;

  auto extensions = GetRequiredExtensions();
  extensions.emplace_back(
      VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#ifdef __APPLE__
  extensions.emplace_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
  createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

  createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
  createInfo.ppEnabledExtensionNames = extensions.data();

#ifdef VULKAN_VALIDATION
  VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
  createInfo.enabledLayerCount =
      static_cast<uint32_t>(validation_layers_.size());
  createInfo.ppEnabledLayerNames = validation_layers_.data();

  PopulateDebugMessengerCreateInfo(debugCreateInfo);
  createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
#else
  createInfo.enabledLayerCount = 0;
  createInfo.pNext = nullptr;
#endif

  WIESEL_CHECK_VKRESULT(vkCreateInstance(&createInfo, nullptr, &instance_));
}

void Renderer::LoadInstanceExtensions() {
  pfn_create_debug_utils_messenger_ext_ =
      (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
          instance_, "vkCreateDebugUtilsMessengerEXT");
  pfn_destroy_debug_utils_messenger_ext_ =
      (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
          instance_, "vkDestroyDebugUtilsMessengerEXT");
}

void Renderer::CreateSurface() {
  window_->CreateWindowSurface(instance_, &surface_);
}

void Renderer::PickPhysicalDevice() {
  uint32_t deviceCount = 0;
  vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);
  LOG_DEBUG("{} devices found!", deviceCount);
  if (deviceCount == 0) {
    throw std::runtime_error("failed to find GPUs with Vulkan support!");
  }

  std::vector<VkPhysicalDevice> devices(deviceCount);
  vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());

  // Use an ordered map to automatically sort candidates by increasing score
  std::multimap<int, VkPhysicalDevice> candidates;

  for (const auto& device : devices) {
    if (!IsDeviceSuitable(device)) {
      continue;
    }
    int32_t score = RateDeviceSuitability(device);
    candidates.insert(std::make_pair(score, device));
  }

  // Check if the best candidate is suitable at all
  if (candidates.rbegin()->first > 0) {
    physical_device_ = candidates.rbegin()->second;
    vkGetPhysicalDeviceProperties(physical_device_,
                                  &physical_device_properties_);
    vkGetPhysicalDeviceFeatures(physical_device_, &physical_device_features_);
    supported_sampling_modes_ = ConvertToSamplingModes(
      physical_device_properties_.limits.framebufferColorSampleCounts &
        physical_device_properties_.limits.framebufferDepthSampleCounts);
    max_sampling_mode_ = FindHighestSamplingMode(supported_sampling_modes_);
    options_.msaa_mode = max_sampling_mode_;
    if (physical_device_features_.shaderImageGatherExtended) {
      shader_features_.push_back("USE_GATHER");
    }
  } else {
    throw std::runtime_error("failed to find a suitable GPU!");
  }
}

void Renderer::CreateLogicalDevice() {
  LOG_DEBUG("Creating logical device");
  queue_family_indices_ = FindQueueFamilies(physical_device_);

  std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
  std::set<uint32_t> uniqueQueueFamilies = {GetGraphicsQueueFamilyIndex(),
                                            GetPresentQueueFamilyIndex()};

  float queuePriority = 1.0f;
  for (uint32_t queueFamily : uniqueQueueFamilies) {
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    queueCreateInfos.push_back(queueCreateInfo);
  }

  VkPhysicalDeviceFeatures deviceFeatures{};
  deviceFeatures.fillModeNonSolid = true;
  deviceFeatures.samplerAnisotropy = VK_TRUE;

  VkDeviceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  createInfo.queueCreateInfoCount =
      static_cast<uint32_t>(queueCreateInfos.size());
  createInfo.pQueueCreateInfos = queueCreateInfos.data();
  createInfo.pEnabledFeatures = &deviceFeatures;
  createInfo.enabledExtensionCount =
      static_cast<uint32_t>(device_extensions_.size());
  createInfo.ppEnabledExtensionNames = device_extensions_.data();
  createInfo.enabledLayerCount = 0;

  if (vkCreateDevice(physical_device_, &createInfo, nullptr,
                     &logical_device_) != VK_SUCCESS) {
    throw std::runtime_error("failed to create logical device!");
  }

  vkGetDeviceQueue(logical_device_, GetPresentQueueFamilyIndex(), 0,
                   &present_queue_);
  vkGetDeviceQueue(logical_device_, GetGraphicsQueueFamilyIndex(), 0,
                   &graphics_queue_);
}

void Renderer::LoadDeviceExtensions() {
  pfn_set_debug_utils_object_name_ext_ =
      (PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(
          logical_device_, "vkSetDebugUtilsObjectNameEXT");
}

void Renderer::CreateDescriptorLayouts() {
  geometry_mesh_descriptor_layout_ = CreateReference<DescriptorSetLayout>();
  geometry_mesh_descriptor_layout_->AddBinding(
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

  for (int i = 0; i < kMaterialTextureCount; i++) {
    geometry_mesh_descriptor_layout_->AddBinding(
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        VK_SHADER_STAGE_FRAGMENT_BIT);
  }
  geometry_mesh_descriptor_layout_->Bake();

  shadow_mesh_descriptor_layout_ = CreateReference<DescriptorSetLayout>();
  shadow_mesh_descriptor_layout_->AddBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                             VK_SHADER_STAGE_VERTEX_BIT);
  shadow_mesh_descriptor_layout_->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
  shadow_mesh_descriptor_layout_->Bake();

  global_shadow_descriptor_layout_ = CreateReference<DescriptorSetLayout>();
  global_shadow_descriptor_layout_->AddBinding(
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
  global_shadow_descriptor_layout_->Bake();

  global_descriptor_layout_ = CreateReference<DescriptorSetLayout>();
  global_descriptor_layout_->AddBinding(
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
  global_descriptor_layout_->AddBinding(
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
  global_descriptor_layout_->AddBinding(
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
  global_descriptor_layout_->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
  global_descriptor_layout_->Bake();

  present_descriptor_layout_ = CreateReference<DescriptorSetLayout>();
  present_descriptor_layout_->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
  present_descriptor_layout_->Bake();

  skybox_descriptor_layout_ = CreateReference<DescriptorSetLayout>();
  skybox_descriptor_layout_->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
  skybox_descriptor_layout_->Bake();

  ssao_gen_descriptor_layout_ = CreateReference<DescriptorSetLayout>();
  ssao_gen_descriptor_layout_->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
  ssao_gen_descriptor_layout_->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
  ssao_gen_descriptor_layout_->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
  ssao_gen_descriptor_layout_->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
  ssao_gen_descriptor_layout_->AddBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                          VK_SHADER_STAGE_FRAGMENT_BIT);
  ssao_gen_descriptor_layout_->Bake();

  ssao_output_descriptor_layout_ = CreateReference<DescriptorSetLayout>();
  ssao_output_descriptor_layout_->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      VK_SHADER_STAGE_FRAGMENT_BIT);  // samplerSSAO
  ssao_output_descriptor_layout_->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      VK_SHADER_STAGE_FRAGMENT_BIT);  // samplerDepth
  ssao_output_descriptor_layout_->Bake();

  ssao_blur_descriptor_layout_ = CreateReference<DescriptorSetLayout>();
  ssao_blur_descriptor_layout_->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      VK_SHADER_STAGE_FRAGMENT_BIT);  // samplerSSAO
  ssao_blur_descriptor_layout_->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      VK_SHADER_STAGE_FRAGMENT_BIT);  // samplerDepth
  ssao_blur_descriptor_layout_->Bake();

  geometry_output_descriptor_layout_ = CreateReference<DescriptorSetLayout>();
  geometry_output_descriptor_layout_->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
  geometry_output_descriptor_layout_->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
  geometry_output_descriptor_layout_->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
  geometry_output_descriptor_layout_->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
  geometry_output_descriptor_layout_->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
  geometry_output_descriptor_layout_->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
  geometry_output_descriptor_layout_->Bake();

  sprite_draw_descriptor_layout_ = CreateReference<DescriptorSetLayout>();
  sprite_draw_descriptor_layout_->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
  sprite_draw_descriptor_layout_->AddBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                             VK_SHADER_STAGE_VERTEX_BIT);
  sprite_draw_descriptor_layout_->Bake();

  // 2-sampler layout for bloom composite and motion blur
  postprocess_2input_descriptor_layout_ =
      CreateReference<DescriptorSetLayout>();
  postprocess_2input_descriptor_layout_->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
  postprocess_2input_descriptor_layout_->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
  postprocess_2input_descriptor_layout_->Bake();

  taa_descriptor_layout_ = CreateReference<DescriptorSetLayout>();
  taa_descriptor_layout_->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
  taa_descriptor_layout_->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
  taa_descriptor_layout_->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
  taa_descriptor_layout_->Bake();
}

void Renderer::CreateSwapChain() {
  LOG_DEBUG("Creating swap chain");
  swap_chain_details_ = QuerySwapChainSupport(physical_device_);

  VkSurfaceFormatKHR surfaceFormat =
      ChooseSwapSurfaceFormat(swap_chain_details_.formats);
  VkPresentModeKHR presentMode =
      ChooseSwapPresentMode(swap_chain_details_.presentModes);
  extent_ = ChooseSwapExtent(swap_chain_details_.capabilities);

  uint32_t imageCount = swap_chain_details_.capabilities.minImageCount + 1;

  if (swap_chain_details_.capabilities.maxImageCount > 0 &&
      imageCount > swap_chain_details_.capabilities.maxImageCount) {
    imageCount = swap_chain_details_.capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  createInfo.surface = surface_;
  createInfo.minImageCount = imageCount;
  createInfo.imageFormat = surfaceFormat.format;
  createInfo.imageColorSpace = surfaceFormat.colorSpace;
  createInfo.imageExtent = extent_;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage =
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

  uint32_t graphicsQueueFamilyIndex = GetGraphicsQueueFamilyIndex();
  uint32_t presentQueueFamilyIndex = GetPresentQueueFamilyIndex();

  uint32_t queueFamilyIndices[] = {graphicsQueueFamilyIndex,
                                   presentQueueFamilyIndex};

  if (graphicsQueueFamilyIndex != presentQueueFamilyIndex) {
    createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices = queueFamilyIndices;
  } else {
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;      // Optional
    createInfo.pQueueFamilyIndices = nullptr;  // Optional
  }
  createInfo.preTransform = swap_chain_details_.capabilities.currentTransform;
  // The compositeAlpha field specifies if the alpha channel should be used for blending with other windows in the window system.
  // You'll almost always want to simply ignore the alpha channel, hence VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR.
  createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  createInfo.presentMode = presentMode;
  // If it's clipped, obscured pixels will be ignored hence increasing the performance.
  createInfo.clipped = VK_TRUE;
  createInfo.oldSwapchain = VK_NULL_HANDLE;

  if (vkCreateSwapchainKHR(logical_device_, &createInfo, nullptr,
                           &swap_chain_) != VK_SUCCESS) {
    throw std::runtime_error("failed to create swap chain!");
  }

  std::vector<VkImage> swap_chain_images;
  vkGetSwapchainImagesKHR(logical_device_, swap_chain_, &imageCount, nullptr);
  swap_chain_images.resize(imageCount);
  vkGetSwapchainImagesKHR(logical_device_, swap_chain_, &imageCount,
                          swap_chain_images.data());
  swap_chain_image_format_ = surfaceFormat.format;

  aspect_ratio_ = extent_.width / (float)extent_.height;
  window_size_.width = extent_.width;
  window_size_.height = extent_.height;
  recreate_swap_chain_ = false;
  swap_chain_created_ = true;

  Ref<AttachmentTexture> texture = CreateReference<AttachmentTexture>();
  texture->format_ = surfaceFormat.format;
  texture->width_ = extent_.width;
  texture->height_ = extent_.height;
  texture->type_ = AttachmentTextureType::SwapChain;
  texture->is_allocated_ = true;
  texture->sampling_mode_ = options_.msaa_mode;
  for (VkImage& image : swap_chain_images) {
    TransitionImageLayout(image, swap_chain_image_format_,
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1);

    TransitionImageLayout(image, swap_chain_image_format_,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 1);
    texture->images_.push_back(image);
    texture->image_views_.push_back(CreateImageView(
        image, swap_chain_image_format_, VK_IMAGE_ASPECT_COLOR_BIT, 1));
  }
  swap_chain_texture_ = texture;

  present_depth_stencil_ = CreateAttachmentTexture(
      {extent_.width, extent_.height, AttachmentTextureType::DepthStencil,
       static_cast<uint32_t>(swap_chain_images.size()), FindDepthFormat(),
       options_.msaa_mode});

  present_render_pass_ =
      CreateReference<RenderPass>(PassType::Present, "Present RenderPass");
  present_framebuffers_.resize(swap_chain_images.size());

  if (options_.msaa_mode > SamplingMode::DISABLED) {
    // With MSAA, render to MSAA color attachment and resolve to swapchain
    present_color_image_ = CreateAttachmentTexture(
        {extent_.width, extent_.height, AttachmentTextureType::Color,
         static_cast<uint32_t>(swap_chain_images.size()),
         swap_chain_image_format_, options_.msaa_mode});
    present_render_pass_->AttachOutput(present_color_image_);
    present_render_pass_->AttachOutput(present_depth_stencil_);
    present_render_pass_->AttachOutput(swap_chain_texture_);
    present_render_pass_->Bake();

    std::array<AttachmentTexture*, 3> textures{present_color_image_.get(),
                                               present_depth_stencil_.get(),
                                               swap_chain_texture_.get()};
    for (uint32_t i = 0; i < swap_chain_images.size(); i++) {
      present_framebuffers_[i] = present_render_pass_->CreateFramebuffer(
          i, textures, {extent_.width, extent_.height});
    }
  } else {
    // Without MSAA, render directly to swapchain
    present_color_image_ = swap_chain_texture_;
    present_render_pass_->AttachOutput(swap_chain_texture_);
    present_render_pass_->AttachOutput(present_depth_stencil_);
    present_render_pass_->Bake();

    std::array<AttachmentTexture*, 2> textures{swap_chain_texture_.get(),
                                               present_depth_stencil_.get()};
    for (uint32_t i = 0; i < swap_chain_images.size(); i++) {
      present_framebuffers_[i] = present_render_pass_->CreateFramebuffer(
          i, textures, {extent_.width, extent_.height});
    }
  }
}

void Renderer::CreateGeometryRenderPass() {
  LOG_DEBUG("Creating render pass");

  geometry_render_pass_ =
      CreateReference<RenderPass>(PassType::Geometry, "Geometry RenderPass");
  geometry_render_pass_->AttachOutput(
      {.type = AttachmentTextureType::Offscreen,
       .format = VK_FORMAT_R32G32B32A32_SFLOAT,
       .msaa_mode = options_.msaa_mode});
  geometry_render_pass_->AttachOutput(
      {.type = AttachmentTextureType::Offscreen,
       .format = VK_FORMAT_R32G32B32A32_SFLOAT,
       .msaa_mode = options_.msaa_mode});
  geometry_render_pass_->AttachOutput(
      {.type = AttachmentTextureType::Offscreen,
       .format = VK_FORMAT_R32_SFLOAT,
       .msaa_mode = options_.msaa_mode});
  geometry_render_pass_->AttachOutput(
      {.type = AttachmentTextureType::Offscreen,
       .format = VK_FORMAT_R8G8B8A8_UNORM,
       .msaa_mode = options_.msaa_mode});
  geometry_render_pass_->AttachOutput(
      {.type = AttachmentTextureType::Offscreen,
       .format = VK_FORMAT_R8G8B8A8_UNORM,
       .msaa_mode = options_.msaa_mode});
  geometry_render_pass_->AttachOutput(
      {.type = AttachmentTextureType::Offscreen,
       .format = VK_FORMAT_R16G16B16A16_SFLOAT,
       .msaa_mode = options_.msaa_mode});
  geometry_render_pass_->AttachOutput(
      {.type = AttachmentTextureType::DepthStencil,
       .format = FindDepthFormat(),
       .msaa_mode = options_.msaa_mode});
  if (options_.msaa_mode > SamplingMode::DISABLED) {
    geometry_render_pass_->AttachOutput(
        {.type = AttachmentTextureType::Resolve,
         .format = VK_FORMAT_R32G32B32A32_SFLOAT,
         .msaa_mode = SamplingMode::DISABLED});
    geometry_render_pass_->AttachOutput(
        {.type = AttachmentTextureType::Resolve,
         .format = VK_FORMAT_R32G32B32A32_SFLOAT,
         .msaa_mode = SamplingMode::DISABLED});
    geometry_render_pass_->AttachOutput(
        {.type = AttachmentTextureType::Resolve,
         .format = VK_FORMAT_R32_SFLOAT,
         .msaa_mode = SamplingMode::DISABLED});
    geometry_render_pass_->AttachOutput(
        {.type = AttachmentTextureType::Resolve,
         .format = VK_FORMAT_R8G8B8A8_UNORM,
         .msaa_mode = SamplingMode::DISABLED});
    geometry_render_pass_->AttachOutput(
        {.type = AttachmentTextureType::Resolve,
         .format = VK_FORMAT_R8G8B8A8_UNORM,
         .msaa_mode = SamplingMode::DISABLED});
    geometry_render_pass_->AttachOutput(
        {.type = AttachmentTextureType::Resolve,
         .format = VK_FORMAT_R16G16B16A16_SFLOAT,
         .msaa_mode = SamplingMode::DISABLED});
  }
  geometry_render_pass_->Bake();

  lighting_render_pass_ = CreateReference<RenderPass>(
      PassType::Lighting, "Deferred Lightning RenderPass");
  lighting_render_pass_->AttachOutput(
      {.type = AttachmentTextureType::Offscreen,
       .format = swap_chain_image_format_,
       .msaa_mode = options_.msaa_mode});
  if (options_.msaa_mode > SamplingMode::DISABLED) {
    lighting_render_pass_->AttachOutput(
        {.type = AttachmentTextureType::Resolve,
         .format = swap_chain_image_format_,
         .msaa_mode = SamplingMode::DISABLED});
  }
  lighting_render_pass_->Bake();

  composite_render_pass_ = CreateReference<RenderPass>(PassType::PostProcess,
                                                       "Composite RenderPass");
  composite_render_pass_->AttachOutput(
      {.type = AttachmentTextureType::Offscreen,
       .format = swap_chain_image_format_,
       .msaa_mode = options_.msaa_mode});
  if (options_.msaa_mode > SamplingMode::DISABLED) {
    composite_render_pass_->AttachOutput(
        {.type = AttachmentTextureType::Resolve,
         .format = swap_chain_image_format_,
         .msaa_mode = SamplingMode::DISABLED});
  }
  composite_render_pass_->Bake();

  sprite_render_pass_ =
      CreateReference<RenderPass>(PassType::PostProcess, "Sprite RenderPass");
  sprite_render_pass_->AttachOutput({.type = AttachmentTextureType::Offscreen,
                                     .format = swap_chain_image_format_,
                                     .msaa_mode = SamplingMode::DISABLED});
  sprite_render_pass_->Bake();

  ssao_gen_render_pass_ = CreateReference<RenderPass>(
      PassType::PostProcess, "SSAO Generate RenderPass");
  ssao_gen_render_pass_->AttachOutput({.type = AttachmentTextureType::Offscreen,
                                       .format = VK_FORMAT_R8_UNORM,
                                       .msaa_mode = SamplingMode::DISABLED});
  ssao_gen_render_pass_->Bake();

  ssao_blur_horz_render_pass_ = CreateReference<RenderPass>(
      PassType::PostProcess, "SSAO Horizontal Blur RenderPass");
  ssao_blur_horz_render_pass_->AttachOutput(
      {.type = AttachmentTextureType::Offscreen,
       .format = VK_FORMAT_R8_UNORM,
       .msaa_mode = SamplingMode::DISABLED});
  ssao_blur_horz_render_pass_->Bake();

  ssao_blur_vert_render_pass_ = CreateReference<RenderPass>(
      PassType::PostProcess, "SSAO Vertical Blur RenderPass");
  ssao_blur_vert_render_pass_->AttachOutput(
      {.type = AttachmentTextureType::Offscreen,
       .format = VK_FORMAT_R8_UNORM,
       .msaa_mode = SamplingMode::DISABLED});
  ssao_blur_vert_render_pass_->Bake();

  shadow_render_pass_ = CreateReference<RenderPass>(
      PassType::Shadow, "Deferred Shadow RenderPass");
  shadow_render_pass_->AttachOutput(
      {.type = AttachmentTextureType::DepthStencil,
       .format = FindDepthFormat(),
       .msaa_mode = SamplingMode::DISABLED});
  shadow_render_pass_->Bake();

  // Shared post-process render pass for bloom and motion blur
  postprocess_render_pass_ = CreateReference<RenderPass>(
      PassType::PostProcess, "PostProcess RenderPass");
  postprocess_render_pass_->AttachOutput(
      {.type = AttachmentTextureType::Offscreen,
       .format = swap_chain_image_format_,
       .msaa_mode = SamplingMode::DISABLED});
  postprocess_render_pass_->Bake();
}

void Renderer::CreateGeometryGraphicsPipelines() {
  LOG_DEBUG("Creating graphics pipeline");
  auto geometry_vertex_shader = CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "assets/internal_shaders/geometry_shader.vert"});
  auto geometry_fragment_shader = CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "assets/internal_shaders/geometry_shader.frag"});
  geometry_pipeline_ = CreateReference<Pipeline>(
      PipelineProperties{options_.msaa_mode, CullModeBack,
                         options_.wireframe_enabled, false});
  geometry_pipeline_->SetVertexData(Vertex3D::GetBindingDescription(),
                                    Vertex3D::GetAttributeDescriptions());
  geometry_pipeline_->SetRenderPass(geometry_render_pass_);
  geometry_pipeline_->AddInputLayout(geometry_mesh_descriptor_layout_);
  geometry_pipeline_->AddInputLayout(global_descriptor_layout_);
  geometry_pipeline_->AddShader(geometry_vertex_shader);
  geometry_pipeline_->AddShader(geometry_fragment_shader);
  geometry_pipeline_->Bake();

  auto skybox_vertex_shader = CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "assets/internal_shaders/skybox_shader.vert"});
  auto skybox_fragment_shader = CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "assets/internal_shaders/skybox_shader.frag"});
  skybox_pipeline_ = CreateReference<Pipeline>(PipelineProperties{
      options_.msaa_mode, CullModeFront, false, false, true, false});
  skybox_pipeline_->SetRenderPass(lighting_render_pass_);
  skybox_pipeline_->AddInputLayout(skybox_descriptor_layout_);
  skybox_pipeline_->AddInputLayout(global_descriptor_layout_);
  skybox_pipeline_->AddShader(skybox_vertex_shader);
  skybox_pipeline_->AddShader(skybox_fragment_shader);
  skybox_pipeline_->Bake();

  auto fullscreen_vertex_shader = CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "assets/internal_shaders/fullscreen_shader.vert"});
  auto lighting_fragment_shader = CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "assets/internal_shaders/lighting_shader.frag"});

  lighting_pipeline_ = CreateReference<Pipeline>(PipelineProperties{
      options_.msaa_mode, CullModeFront, false, true, true, false});
  lighting_pipeline_->SetRenderPass(lighting_render_pass_);
  lighting_pipeline_->AddInputLayout(geometry_output_descriptor_layout_);
  lighting_pipeline_->AddInputLayout(ssao_output_descriptor_layout_);
  lighting_pipeline_->AddInputLayout(global_descriptor_layout_);
  lighting_pipeline_->AddInputLayout(skybox_descriptor_layout_);
  lighting_pipeline_->AddShader(fullscreen_vertex_shader);
  lighting_pipeline_->AddShader(lighting_fragment_shader);
  lighting_pipeline_->Bake();

  auto shadow_vertex_shader = CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "assets/internal_shaders/shadow_shader.vert"});
  auto shadow_fragment_shader = CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "assets/internal_shaders/shadow_shader.frag"});
  shadow_pipeline_ = CreateReference<Pipeline>(PipelineProperties{
      SamplingMode::DISABLED, CullModeFront, false, false, true, true});
  shadow_pipeline_->SetRenderPass(shadow_render_pass_);
  shadow_pipeline_->SetVertexData(Vertex3D::GetBindingDescription(),
                                  Vertex3D::GetAttributeDescriptions());
  shadow_pipeline_push_constant_ =
      std::make_shared<ShadowPipelinePushConstant>();
  shadow_pipeline_->AddPushConstant(shadow_pipeline_push_constant_,
                                    VK_SHADER_STAGE_VERTEX_BIT);
  shadow_pipeline_->AddInputLayout(shadow_mesh_descriptor_layout_);
  shadow_pipeline_->AddInputLayout(global_shadow_descriptor_layout_);
  shadow_pipeline_->AddShader(shadow_vertex_shader);
  shadow_pipeline_->AddShader(shadow_fragment_shader);
  shadow_pipeline_->Bake();

  auto ssao_fragment_shader = CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "assets/internal_shaders/ssao_gen_shader.frag"});

  ssao_gen_pipeline_ = CreateReference<Pipeline>(PipelineProperties{
      SamplingMode::DISABLED, CullModeFront, false, false, false, false});
  ssao_gen_pipeline_->SetRenderPass(ssao_gen_render_pass_);
  ssao_gen_pipeline_->AddInputLayout(ssao_gen_descriptor_layout_);
  ssao_gen_pipeline_->AddInputLayout(global_descriptor_layout_);
  ssao_gen_pipeline_->AddShader(fullscreen_vertex_shader);
  ssao_gen_pipeline_->AddShader(ssao_fragment_shader);
  ssao_gen_pipeline_->Bake();

  auto ssao_blur_horz_fragment_shader = CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "assets/internal_shaders/ssao_blur_shader.frag"});

  ssao_blur_horz_pipeline_ = CreateReference<Pipeline>(PipelineProperties{
      SamplingMode::DISABLED, CullModeFront, false, false, false, false});
  ssao_blur_horz_pipeline_->SetRenderPass(ssao_blur_horz_render_pass_);
  ssao_blur_horz_pipeline_->AddInputLayout(ssao_blur_descriptor_layout_);
  ssao_blur_horz_pipeline_->AddShader(fullscreen_vertex_shader);
  ssao_blur_horz_pipeline_->AddShader(ssao_blur_horz_fragment_shader);
  ssao_blur_horz_pipeline_->Bake();

  auto ssao_blur_vert_fragment_shader =
      CreateShader({ShaderTypeFragment,
                    ShaderLangGLSL,
                    "main",
                    ShaderSourceSource,
                    "assets/internal_shaders/ssao_blur_shader.frag",
                    {"BLUR_VERTICAL"}});

  ssao_blur_vert_pipeline_ = CreateReference<Pipeline>(PipelineProperties{
      SamplingMode::DISABLED, CullModeFront, false, false, false, false});
  ssao_blur_vert_pipeline_->SetRenderPass(ssao_blur_vert_render_pass_);
  ssao_blur_vert_pipeline_->AddInputLayout(ssao_blur_descriptor_layout_);
  ssao_blur_vert_pipeline_->AddShader(fullscreen_vertex_shader);
  ssao_blur_vert_pipeline_->AddShader(ssao_blur_vert_fragment_shader);
  ssao_blur_vert_pipeline_->Bake();

  auto sprite_vertex_shader = CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "assets/internal_shaders/sprite_shader.vert"});
  auto sprite_fragment_shader = CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "assets/internal_shaders/sprite_shader.frag"});

  sprite_pipeline_ = CreateReference<Pipeline>(PipelineProperties{
      SamplingMode::DISABLED, CullModeNone, false, true, false, false});
  sprite_pipeline_->SetVertexData(VertexSprite::GetBindingDescriptions(),
                                  VertexSprite::GetAttributeDescriptions());
  sprite_pipeline_->SetRenderPass(sprite_render_pass_);
  sprite_pipeline_->AddInputLayout(sprite_draw_descriptor_layout_);
  sprite_pipeline_->AddInputLayout(global_descriptor_layout_);
  sprite_pipeline_->AddShader(sprite_vertex_shader);
  sprite_pipeline_->AddShader(sprite_fragment_shader);
  sprite_pipeline_->Bake();

  auto composite_fragment_shader = CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "assets/internal_shaders/quad_shader.frag"});

  composite_pipeline_ = CreateReference<Pipeline>(PipelineProperties{
      options_.msaa_mode, CullModeFront, false, true, true, false});
  composite_pipeline_->SetRenderPass(composite_render_pass_);
  composite_pipeline_->AddInputLayout(skybox_descriptor_layout_);
  composite_pipeline_->AddShader(fullscreen_vertex_shader);
  composite_pipeline_->AddShader(composite_fragment_shader);
  composite_pipeline_->Bake();

  // --- Bloom pipelines ---
  auto bloom_extract_frag = CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "assets/internal_shaders/bloom_extract.frag"});
  bloom_push_constants_ = std::make_shared<BloomPushConstants>();
  bloom_extract_pipeline_ = CreateReference<Pipeline>(PipelineProperties{
      SamplingMode::DISABLED, CullModeFront, false, false, false, false});
  bloom_extract_pipeline_->SetRenderPass(postprocess_render_pass_);
  bloom_extract_pipeline_->AddInputLayout(present_descriptor_layout_);
  bloom_extract_pipeline_->AddPushConstant(bloom_push_constants_,
                                           VK_SHADER_STAGE_FRAGMENT_BIT);
  bloom_extract_pipeline_->AddShader(fullscreen_vertex_shader);
  bloom_extract_pipeline_->AddShader(bloom_extract_frag);
  bloom_extract_pipeline_->Bake();

  auto bloom_blur_h_frag = CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "assets/internal_shaders/bloom_blur.frag"});
  bloom_blur_h_pipeline_ = CreateReference<Pipeline>(PipelineProperties{
      SamplingMode::DISABLED, CullModeFront, false, false, false, false});
  bloom_blur_h_pipeline_->SetRenderPass(postprocess_render_pass_);
  bloom_blur_h_pipeline_->AddInputLayout(present_descriptor_layout_);
  bloom_blur_h_pipeline_->AddShader(fullscreen_vertex_shader);
  bloom_blur_h_pipeline_->AddShader(bloom_blur_h_frag);
  bloom_blur_h_pipeline_->Bake();

  auto bloom_blur_v_frag =
      CreateShader({ShaderTypeFragment,
                    ShaderLangGLSL,
                    "main",
                    ShaderSourceSource,
                    "assets/internal_shaders/bloom_blur.frag",
                    {"BLUR_VERTICAL"}});
  bloom_blur_v_pipeline_ = CreateReference<Pipeline>(PipelineProperties{
      SamplingMode::DISABLED, CullModeFront, false, false, false, false});
  bloom_blur_v_pipeline_->SetRenderPass(postprocess_render_pass_);
  bloom_blur_v_pipeline_->AddInputLayout(present_descriptor_layout_);
  bloom_blur_v_pipeline_->AddShader(fullscreen_vertex_shader);
  bloom_blur_v_pipeline_->AddShader(bloom_blur_v_frag);
  bloom_blur_v_pipeline_->Bake();

  auto bloom_composite_frag = CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "assets/internal_shaders/bloom_composite.frag"});
  bloom_composite_pipeline_ = CreateReference<Pipeline>(PipelineProperties{
      SamplingMode::DISABLED, CullModeFront, false, false, false, false});
  bloom_composite_pipeline_->SetRenderPass(postprocess_render_pass_);
  bloom_composite_pipeline_->AddInputLayout(
      postprocess_2input_descriptor_layout_);
  bloom_composite_pipeline_->AddPushConstant(bloom_push_constants_,
                                             VK_SHADER_STAGE_FRAGMENT_BIT);
  bloom_composite_pipeline_->AddShader(fullscreen_vertex_shader);
  bloom_composite_pipeline_->AddShader(bloom_composite_frag);
  bloom_composite_pipeline_->Bake();

  // --- Motion blur pipeline ---
  auto motion_blur_frag = CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "assets/internal_shaders/motion_blur.frag"});
  motion_blur_push_constants_ = std::make_shared<MotionBlurPushConstants>();
  motion_blur_pipeline_ = CreateReference<Pipeline>(PipelineProperties{
      SamplingMode::DISABLED, CullModeFront, false, false, false, false});
  motion_blur_pipeline_->SetRenderPass(postprocess_render_pass_);
  motion_blur_pipeline_->AddInputLayout(postprocess_2input_descriptor_layout_);
  motion_blur_pipeline_->AddInputLayout(global_descriptor_layout_);
  motion_blur_pipeline_->AddPushConstant(motion_blur_push_constants_,
                                         VK_SHADER_STAGE_FRAGMENT_BIT);
  motion_blur_pipeline_->AddShader(fullscreen_vertex_shader);
  motion_blur_pipeline_->AddShader(motion_blur_frag);
  motion_blur_pipeline_->Bake();

  // FXAA pipeline
  fxaa_push_constants_ = std::make_shared<FxaaPushConstants>();
  fxaa_pipeline_ = CreateReference<Pipeline>(PipelineProperties{
      SamplingMode::DISABLED, CullModeFront, false, false, false, false});
  fxaa_pipeline_->SetRenderPass(postprocess_render_pass_);
  fxaa_pipeline_->AddInputLayout(present_descriptor_layout_);
  fxaa_pipeline_->AddPushConstant(fxaa_push_constants_, VK_SHADER_STAGE_FRAGMENT_BIT);
  fxaa_pipeline_->AddShader(fullscreen_vertex_shader);
  fxaa_pipeline_->AddShader(CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "assets/internal_shaders/fxaa.frag"}));
  fxaa_pipeline_->Bake();

  // TAA pipeline
  taa_pipeline_ = CreateReference<Pipeline>(PipelineProperties{
      SamplingMode::DISABLED, CullModeFront, false, false, false, false});
  taa_pipeline_->SetRenderPass(postprocess_render_pass_);
  taa_pipeline_->AddInputLayout(taa_descriptor_layout_);
  taa_pipeline_->AddInputLayout(global_descriptor_layout_);
  taa_pipeline_->AddShader(fullscreen_vertex_shader);
  taa_pipeline_->AddShader(CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "assets/internal_shaders/taa.frag"}));
  taa_pipeline_->Bake();

  // TAA history copy pipeline (simple passthrough)
  taa_copy_pipeline_ = CreateReference<Pipeline>(PipelineProperties{
      SamplingMode::DISABLED, CullModeFront, false, false, false, false});
  taa_copy_pipeline_->SetRenderPass(postprocess_render_pass_);
  taa_copy_pipeline_->AddInputLayout(present_descriptor_layout_);
  taa_copy_pipeline_->AddShader(fullscreen_vertex_shader);
  taa_copy_pipeline_->AddShader(CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "assets/internal_shaders/quad_shader.frag"}));
  taa_copy_pipeline_->Bake();
}

void Renderer::CreatePresentGraphicsPipelines() {
  auto present_vertex_shader = CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "assets/internal_shaders/fullscreen_shader.vert"});
  auto present_fragment_shader = CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "assets/internal_shaders/quad_shader.frag"});
  present_pipeline_ = CreateReference<Pipeline>(PipelineProperties{
      options_.msaa_mode, CullModeNone, false, true});
  present_pipeline_->SetRenderPass(present_render_pass_);
  present_pipeline_->AddInputLayout(present_descriptor_layout_);
  present_pipeline_->AddShader(present_vertex_shader);
  present_pipeline_->AddShader(present_fragment_shader);
  present_pipeline_->Bake();
}

void Renderer::RecreatePipeline(Ref<Pipeline> pipeline) {
  pipeline->Bake();
}

Ref<Shader> Renderer::CreateShader(ShaderProperties properties) {
  for (const auto& item : shader_features_) {
    properties.defines.push_back(item);
  }
  return CreateReference<Shader>(properties);
}

void Renderer::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                            VkMemoryPropertyFlags properties, VkBuffer& buffer,
                            VkDeviceMemory& bufferMemory) {
  PROFILE_ZONE_SCOPED();
  VkBufferCreateInfo bufferInfo{};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = size;
  bufferInfo.usage = usage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  WIESEL_CHECK_VKRESULT(
      vkCreateBuffer(logical_device_, &bufferInfo, nullptr, &buffer));

  VkMemoryRequirements memRequirements;
  vkGetBufferMemoryRequirements(logical_device_, buffer, &memRequirements);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex =
      FindMemoryType(memRequirements.memoryTypeBits, properties);

  WIESEL_CHECK_VKRESULT(
      vkAllocateMemory(logical_device_, &allocInfo, nullptr, &bufferMemory));
  WIESEL_CHECK_VKRESULT(
      vkBindBufferMemory(logical_device_, buffer, bufferMemory, 0));
}

void Renderer::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer,
                          VkDeviceSize size) {
  PROFILE_ZONE_SCOPED();
  VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

  VkBufferCopy copyRegion{};
  copyRegion.size = size;
  vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

  EndSingleTimeCommands(commandBuffer);
}

void Renderer::CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width,
                                 uint32_t height, VkDeviceSize base_offset,
                                 uint32_t layer, uint32_t layer_count) {
  PROFILE_ZONE_SCOPED();
  VkCommandBuffer commandBuffer = BeginSingleTimeCommands();
  VkBufferImageCopy region{};
  region.bufferOffset = base_offset;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;

  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = layer;
  region.imageSubresource.layerCount = layer_count;

  region.imageOffset = {0, 0, 0};
  region.imageExtent = {width, height, 1};

  vkCmdCopyBufferToImage(commandBuffer, buffer, image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  EndSingleTimeCommands(commandBuffer);
}

void Renderer::TransitionImageLayout(VkImage image, VkFormat format,
                                     VkImageLayout oldLayout,
                                     VkImageLayout newLayout,
                                     uint32_t mipLevels, uint32_t baseLayer,
                                     uint32_t layerCount) {
  PROFILE_ZONE_SCOPED();
  VkCommandBuffer commandBuffer = BeginSingleTimeCommands();
  TransitionImageLayout(image, format, oldLayout, newLayout, mipLevels,
                        commandBuffer, baseLayer, layerCount);
  EndSingleTimeCommands(commandBuffer);
}

void Renderer::TransitionImageLayout(VkImage image, VkFormat format,
                                     VkImageLayout oldLayout,
                                     VkImageLayout newLayout,
                                     uint32_t mipLevels,
                                     VkCommandBuffer commandBuffer,
                                     uint32_t baseLayer, uint32_t layerCount) {
  PROFILE_ZONE_SCOPED();
  // I hate this
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = oldLayout;
  barrier.newLayout = newLayout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ||
      oldLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

    if (HasStencilComponent(format)) {
      barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
  } else {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  }
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = mipLevels;
  barrier.subresourceRange.baseArrayLayer = baseLayer;
  barrier.subresourceRange.layerCount = layerCount;

  // Derive src access mask and pipeline stage from old layout
  VkPipelineStageFlags sourceStage;
  switch (oldLayout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
      barrier.srcAccessMask = 0;
      sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
      break;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      break;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      sourceStage = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
      break;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
      sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      break;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
      break;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
      break;
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
      barrier.srcAccessMask = 0;
      sourceStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
      break;
    default:
      barrier.srcAccessMask = 0;
      sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
      break;
  }

  // Derive dst access mask and pipeline stage from new layout
  VkPipelineStageFlags destinationStage;
  switch (newLayout) {
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      break;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                              VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
      break;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      break;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
      break;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
      break;
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
      barrier.dstAccessMask = 0;
      destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
      break;
    default:
      barrier.dstAccessMask = 0;
      destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
      break;
  }

  vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0,
                       nullptr, 0, nullptr, 1, &barrier);
}

void Renderer::CreateCommandPools() {
  command_pool_ = CreateReference<CommandPool>();
}

void Renderer::CreateCommandBuffers() {
  command_buffer_ = command_pool_->CreateBuffer();
}

void Renderer::CreatePermanentResources() {
  blank_texture_ = CreateBlankTexture();

  std::vector<Index> quadIndices = {0, 1, 2, 2, 3, 0};
  std::vector<Vertex2DNoColor> quadVertices = {
      {{-1.0f, -1.0f}, {0.0f, 0.0f}},
      {{1.0f, -1.0f}, {1.0f, 0.0f}},
      {{1.0f, 1.0f}, {1.0f, 1.0f}},
      {{-1.0f, 1.0f}, {0.0f, 1.0f}},
  };

  quad_index_buffer_ = Engine::GetRenderer()->CreateIndexBuffer(quadIndices);

  quad_vertex_buffer_ = Engine::GetRenderer()->CreateVertexBuffer(quadVertices);

  default_linear_sampler_ = CreateReference<Sampler>(1, SamplerProps{});
  default_nearest_sampler_ = CreateReference<Sampler>(
      1, SamplerProps{VK_FILTER_NEAREST, VK_FILTER_NEAREST, -1.0f});

  // SSAO
  ssao_kernel_uniform_buffer_ =
      CreateUniformBuffer(sizeof(SSAOKernelUniformData));
  std::default_random_engine rndEngine((unsigned)time(nullptr));
  std::uniform_real_distribution<float> rndDist(0.0f, 1.0f);

  // Sample kernel
  for (uint32_t i = 0; i < WIESEL_SSAO_KERNEL_SIZE; ++i) {
    glm::vec3 sample(rndDist(rndEngine) * 2.0 - 1.0,
                     rndDist(rndEngine) * 2.0 - 1.0, rndDist(rndEngine));
    sample = glm::normalize(sample);
    sample *= rndDist(rndEngine);
    float scale = float(i) / float(WIESEL_SSAO_KERNEL_SIZE);
    scale = std::lerp(0.1f, 1.0f, scale * scale);
    ssao_kernel_uniform_data_.Samples[i] = glm::vec4(sample * scale, 0.0f);
  }
  memcpy(ssao_kernel_uniform_buffer_->data_, &ssao_kernel_uniform_data_,
         sizeof(ssao_kernel_uniform_data_));

  // Random noise
  std::vector<glm::vec4> noiseValues(WIESEL_SSAO_NOISE_DIM *
                                     WIESEL_SSAO_NOISE_DIM);
  for (uint32_t i = 0; i < static_cast<uint32_t>(noiseValues.size()); i++) {
    noiseValues[i] = glm::vec4(rndDist(rndEngine) * 2.0f - 1.0f,
                               rndDist(rndEngine) * 2.0f - 1.0f, 0.0f, 0.0f);
  }
  // Upload as texture
  ssao_noise_ = CreateAttachmentTexture(
      AttachmentTextureProps{.width = WIESEL_SSAO_NOISE_DIM,
                             .height = WIESEL_SSAO_NOISE_DIM,
                             .type = AttachmentTextureType::Offscreen,
                             .image_format = VK_FORMAT_R32G32B32A32_SFLOAT,
                             .sampled = true,
                             .transfer_dest = true});
  SetAttachmentTextureBuffer(ssao_noise_, noiseValues.data(),
                             sizeof(glm::vec4));
}

void Renderer::CreateImage(uint32_t width, uint32_t height, uint32_t mipLevels,
                           SamplingMode sampling_mode, VkFormat format,
                           VkImageTiling tiling, VkImageUsageFlags usage,
                           VkMemoryPropertyFlags properties, VkImage& image,
                           VkDeviceMemory& imageMemory,
                           VkImageCreateFlags flags, uint32_t arrayLayers) {
  PROFILE_ZONE_SCOPED();
  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width = width;
  imageInfo.extent.height = height;
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = mipLevels;
  imageInfo.arrayLayers = arrayLayers;
  imageInfo.format = format;
  /*
     * VK_IMAGE_TILING_LINEAR: Texels are laid out in row-major order like our pixels array
     * VK_IMAGE_TILING_OPTIMAL: Texels are laid out in an implementation defined order for optimal access
     */
  imageInfo.tiling = tiling;
  /*
     * VK_IMAGE_LAYOUT_UNDEFINED: Not usable by the GPU and the very first transition will discard the texels.
     * VK_IMAGE_LAYOUT_PREINITIALIZED: Not usable by the GPU, but the first transition will preserve the texels.
     */
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage = usage;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageInfo.samples = ToVkSampleCountFlagBits(sampling_mode);
  imageInfo.flags = flags;
  WIESEL_CHECK_VKRESULT(
      vkCreateImage(logical_device_, &imageInfo, nullptr, &image));

  VkMemoryRequirements memRequirements;
  vkGetImageMemoryRequirements(logical_device_, image, &memRequirements);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex =
      FindMemoryType(memRequirements.memoryTypeBits, properties);

  WIESEL_CHECK_VKRESULT(
      vkAllocateMemory(logical_device_, &allocInfo, nullptr, &imageMemory));

  vkBindImageMemory(logical_device_, image, imageMemory, 0);
}

Ref<ImageView> Renderer::CreateImageView(VkImage image, VkFormat format,
                                         VkImageAspectFlags aspectFlags,
                                         uint32_t mipLevels,
                                         VkImageViewType viewType,
                                         uint32_t layer, uint32_t layerCount) {
  PROFILE_ZONE_SCOPED();
  Ref<ImageView> view = CreateReference<ImageView>();
  view->layer_ = layer;
  view->layer_count_ = layerCount;

  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = image;
  viewInfo.viewType = viewType;
  viewInfo.format = format;
  viewInfo.subresourceRange.aspectMask = aspectFlags;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = mipLevels;
  viewInfo.subresourceRange.baseArrayLayer = layer;
  viewInfo.subresourceRange.layerCount = layerCount;

  WIESEL_CHECK_VKRESULT(
      vkCreateImageView(logical_device_, &viewInfo, nullptr, &view->handle_));

  return view;
}

void Renderer::SetObjectName(VkObjectType type, uint64_t handle,
                             const char* name) {
  if (!pfn_set_debug_utils_object_name_ext_) {
    return;  // Silently skip if not available
  }
  VkDebugUtilsObjectNameInfoEXT nameInfo = {};
  nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
  nameInfo.objectType = type;
  nameInfo.objectHandle = handle;
  nameInfo.pObjectName = name;
  pfn_set_debug_utils_object_name_ext_(logical_device_, &nameInfo);
}

Ref<ImageView> Renderer::CreateImageView(Ref<AttachmentTexture> image,
                                         VkImageViewType viewType,
                                         uint32_t layer, uint32_t layerCount) {
  return CreateImageView(image->images_[0], image->format_,
                         image->aspect_flags_, image->mip_levels_, viewType,
                         layer, layerCount);
}

VkFormat Renderer::FindSupportedFormat(const std::vector<VkFormat>& candidates,
                                       VkImageTiling tiling,
                                       VkFormatFeatureFlags features) {
  for (VkFormat format : candidates) {
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(physical_device_, format, &props);
    if (tiling == VK_IMAGE_TILING_LINEAR &&
        (props.linearTilingFeatures & features) == features) {
      return format;
    } else if (tiling == VK_IMAGE_TILING_OPTIMAL &&
               (props.optimalTilingFeatures & features) == features) {
      return format;
    }
  }

  throw std::runtime_error("failed to find supported format!");
}

VkFormat Renderer::FindDepthFormat() {
  return FindSupportedFormat(
      {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT,
       VK_FORMAT_D24_UNORM_S8_UINT},
      VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

bool Renderer::HasStencilComponent(VkFormat format) {
  return format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
         format == VK_FORMAT_D24_UNORM_S8_UINT;
}

void Renderer::GenerateMipmaps(VkImage image, VkFormat imageFormat,
                               int32_t texWidth, int32_t texHeight,
                               uint32_t mipLevels) {
  PROFILE_ZONE_SCOPED();
  VkFormatProperties formatProperties;
  vkGetPhysicalDeviceFormatProperties(physical_device_, imageFormat,
                                      &formatProperties);
  if (!(formatProperties.optimalTilingFeatures &
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
    // todo generate mipmaps with stbimage
    throw std::runtime_error(
        "texture image format does not support linear blitting!");
  }

  VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.image = image;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.subresourceRange.levelCount = 1;

  int32_t mipWidth = texWidth;
  int32_t mipHeight = texHeight;

  for (uint32_t i = 1; i < mipLevels; i++) {
    barrier.subresourceRange.baseMipLevel = i - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier);

    VkImageBlit blit{};
    blit.srcOffsets[0] = {0, 0, 0};
    blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.mipLevel = i - 1;
    blit.srcSubresource.baseArrayLayer = 0;
    blit.srcSubresource.layerCount = 1;
    blit.dstOffsets[0] = {0, 0, 0};
    blit.dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1,
                          mipHeight > 1 ? mipHeight / 2 : 1, 1};
    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.mipLevel = i;
    blit.dstSubresource.baseArrayLayer = 0;
    blit.dstSubresource.layerCount = 1;

    vkCmdBlitImage(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                   VK_FILTER_LINEAR);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &barrier);

    if (mipWidth > 1)
      mipWidth /= 2;
    if (mipHeight > 1)
      mipHeight /= 2;
  }

  barrier.subresourceRange.baseMipLevel = mipLevels - 1;
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);

  EndSingleTimeCommands(commandBuffer);
}

void Renderer::CreateTracy() {
  auto vk_get_physical_device_calibrateable_time_domains_ext =
      (PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT)vkGetInstanceProcAddr(
          instance_, "vkGetPhysicalDeviceCalibrateableTimeDomainsEXT");

  auto vk_get_calibrated_timestamps_ext =
      (PFN_vkGetCalibratedTimestampsEXT)vkGetDeviceProcAddr(
          logical_device_, "vkGetCalibratedTimestampsEXT");

  tracy_ctx_ = TracyVkContextCalibrated(
      physical_device_, logical_device_, graphics_queue_,
      command_buffer_->handle_,
      vk_get_physical_device_calibrateable_time_domains_ext,
      vk_get_calibrated_timestamps_ext);
}

void Renderer::CreateSyncObjects() {
  PROFILE_ZONE_SCOPED();
  VkSemaphoreCreateInfo semaphoreInfo{};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fenceInfo{};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  WIESEL_CHECK_VKRESULT(vkCreateSemaphore(
      logical_device_, &semaphoreInfo, nullptr, &image_available_semaphore_));
  WIESEL_CHECK_VKRESULT(vkCreateSemaphore(
      logical_device_, &semaphoreInfo, nullptr, &render_finished_semaphore_));
  WIESEL_CHECK_VKRESULT(
      vkCreateFence(logical_device_, &fenceInfo, nullptr, &fence_));
}

void Renderer::CleanupDescriptorLayouts() {
  geometry_mesh_descriptor_layout_ = nullptr;
  present_descriptor_layout_ = nullptr;
  taa_descriptor_layout_ = nullptr;
}

void Renderer::CleanupGeometryGraphics() {
  // Cleanup all pipelines
  geometry_pipeline_ = nullptr;
  shadow_pipeline_ = nullptr;
  skybox_pipeline_ = nullptr;
  lighting_pipeline_ = nullptr;
  ssao_gen_pipeline_ = nullptr;
  ssao_blur_horz_pipeline_ = nullptr;
  ssao_blur_vert_pipeline_ = nullptr;
  sprite_pipeline_ = nullptr;
  composite_pipeline_ = nullptr;
  bloom_extract_pipeline_ = nullptr;
  bloom_blur_h_pipeline_ = nullptr;
  bloom_blur_v_pipeline_ = nullptr;
  bloom_composite_pipeline_ = nullptr;
  motion_blur_pipeline_ = nullptr;
  fxaa_pipeline_ = nullptr;
  taa_pipeline_ = nullptr;
  taa_copy_pipeline_ = nullptr;

  // Cleanup all render passes
  geometry_render_pass_ = nullptr;
  lighting_render_pass_ = nullptr;
  composite_render_pass_ = nullptr;
  sprite_render_pass_ = nullptr;
  ssao_gen_render_pass_ = nullptr;
  ssao_blur_horz_render_pass_ = nullptr;
  ssao_blur_vert_render_pass_ = nullptr;
  shadow_render_pass_ = nullptr;
  postprocess_render_pass_ = nullptr;
}

void Renderer::CleanupPresentGraphics() {
  present_pipeline_ = nullptr;
  present_color_image_ = nullptr;
  present_depth_stencil_ = nullptr;
  swap_chain_texture_ = nullptr;
  present_render_pass_ = nullptr;
  present_framebuffers_.clear();
  present_framebuffers_.clear();
  vkDestroySwapchainKHR(logical_device_, swap_chain_, nullptr);
}

void Renderer::CreateGlobalUniformBuffers() {
  lights_uniform_buffer_ = CreateUniformBuffer(sizeof(LightsUniformData));
  camera_uniform_buffer_ = CreateUniformBuffer(sizeof(CameraUniformData));
  shadow_camera_uniform_buffer_ =
      CreateUniformBuffer(sizeof(ShadowMapMatricesUniformData));
}

void Renderer::CleanupGlobalUniformBuffers() {
  lights_uniform_buffer_ = nullptr;
  camera_uniform_buffer_ = nullptr;
}

void Renderer::RecreateSwapChain() {
  PROFILE_ZONE_SCOPED();
  LOG_INFO("Recreating swap chains...");
  WindowSize size{};
  window_->GetWindowFramebufferSize(size);
  while (size.width == 0 || size.height == 0) {
    window_->GetWindowFramebufferSize(size);
    window_->OnUpdate();
  }

  vkDeviceWaitIdle(logical_device_);

  CleanupGeometryGraphics();
  CleanupPresentGraphics();
  CreateSwapChain();
  CreateGeometryRenderPass();
  CreateGeometryGraphicsPipelines();
  CreatePresentGraphicsPipelines();

  // Notify that pipelines were recreated so cameras can recreate their resources
  PipelineRecreatedEvent event{};
  Engine::GetWindow()->GetEventHandler()(event);
}

void Renderer::SetViewport(VkExtent2D extent) {
  PROFILE_ZONE_SCOPED();
  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = static_cast<float>(extent.width);
  viewport.height = static_cast<float>(extent.height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(command_buffer_->handle_, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = extent;
  vkCmdSetScissor(command_buffer_->handle_, 0, 1, &scissor);
}

void Renderer::SetViewport(glm::vec2 extent) {
  PROFILE_ZONE_SCOPED();
  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = extent.x;
  viewport.height = extent.y;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(command_buffer_->handle_, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent.width = extent.x;
  scissor.extent.height = extent.y;
  vkCmdSetScissor(command_buffer_->handle_, 0, 1, &scissor);
}

void Renderer::BeginRender() {
  PROFILE_ZONE_SCOPED();
  vkResetFences(logical_device_, 1, &fence_);
  command_buffer_->Reset();
  command_buffer_->Begin();

  // Reloading stuff
  if (recreate_swap_chain_) {
    PROFILE_ZONE_SCOPED_N("Renderer::BeginRender: Recreate swap chain");
    RecreateSwapChain();
    recreate_swap_chain_ = false;
    recreate_pipeline_ = false; // Already handled in RecreateSwapChain
  }
  if (recreate_pipeline_) {
    PROFILE_ZONE_SCOPED_N("Renderer::BeginRender: Recreate Pipeline");
    vkDeviceWaitIdle(logical_device_);
    LOG_INFO("Recreating graphics pipeline...");
    geometry_pipeline_->properties_.enable_wireframe =
      options_.wireframe_enabled;
    geometry_pipeline_->properties_.sampling_mode =
        options_.msaa_mode;
    RecreatePipeline(geometry_pipeline_);
    recreate_pipeline_ = false;
    PipelineRecreatedEvent event{};
    Engine::GetWindow()->GetEventHandler()(event);
  }
}

bool Renderer::BeginPresent() {
  PROFILE_ZONE_SCOPED();
  VkResult result = vkAcquireNextImageKHR(
      logical_device_, swap_chain_, UINT64_MAX, image_available_semaphore_,
      VK_NULL_HANDLE, &image_index_);
  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    LOG_INFO(
        "Received VK_ERROR_OUT_OF_DATE_KHR, trying to recreate swap chain.");
    recreate_swap_chain_ = true;
    return false;
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    throw std::runtime_error("failed to acquire swap chain image!");
  }
  // Setup
  vkResetFences(logical_device_, 1, &fence_);

  /*TransitionImageLayout(GeometryColorResolveImage->m_Images[0],
                        GeometryColorResolveImage->m_Format,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                        m_CommandBuffer->m_Handle);
  for (const auto& item : textures) {
    TransitionImageLayout(item->m_Images[0], item->m_Format,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                          m_CommandBuffer->m_Handle);
  }*/

  if (camera_) {
    auto final_image = GetFinalOutputImage();
    if (final_image) {
      TransitionImageLayout(final_image->images_[0], final_image->format_,
                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                            command_buffer_->handle_, 0, 1);
    }
  }

  present_pipeline_->Bind(PipelineBindPointGraphics);
  present_render_pass_->Begin(present_framebuffers_[image_index_],
                              clear_color_);
  SetViewport(extent_);
  return true;
}

void Renderer::EndPresent() {
  PROFILE_ZONE_SCOPED();
  present_render_pass_->End();
  if (camera_) {
    auto final_image = GetFinalOutputImage();
    if (final_image) {
      TransitionImageLayout(final_image->images_[0], final_image->format_,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1,
                            command_buffer_->handle_, 0, 1);
    }
  }
  /*
  for (const auto& item : textures) {
    TransitionImageLayout(item->m_Images[0], item->m_Format,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1,
                          m_CommandBuffer->m_Handle);
  }*/

  PROFILE_GPU_COLLECT(tracy_ctx_, command_buffer_->handle_);
  command_buffer_->End();

  // Presentation
  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &command_buffer_->handle_;

  VkSemaphore waitSemaphores[] = {image_available_semaphore_};
  VkPipelineStageFlags waitStages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = waitSemaphores;
  submitInfo.pWaitDstStageMask = waitStages;

  VkSemaphore signalSemaphores[] = {render_finished_semaphore_};
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = signalSemaphores;

  WIESEL_CHECK_VKRESULT(vkQueueSubmit(graphics_queue_, 1, &submitInfo, fence_));

  VkPresentInfoKHR presentInfo{};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = signalSemaphores;

  VkSwapchainKHR swapChains[] = {swap_chain_};
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = swapChains;

  presentInfo.pImageIndices = &image_index_;
  presentInfo.pResults = nullptr;  // Optional

  VkResult result = vkQueuePresentKHR(present_queue_, &presentInfo);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    RecreateSwapChain();
  } else if (result != VK_SUCCESS) {
    throw std::runtime_error("failed to present swap chain image!");
  }

  vkWaitForFences(logical_device_, 1, &fence_, VK_TRUE, UINT64_MAX);
}

void Renderer::UpdateUniformData() {
  PROFILE_ZONE_SCOPED();
  memcpy(lights_uniform_buffer_->data_, &lights_uniform_data_,
         sizeof(lights_uniform_data_));
  memcpy(camera_uniform_buffer_->data_, &camera_uniform_data_,
         sizeof(camera_uniform_data_));
}

void Renderer::AllocateModelRenderData(ModelComponent& model,
                                       const Model& model_data) {
  // Create one UBO per entity (shared across all meshes - same transform)
  model.uniform_buffer = CreateUniformBuffer(sizeof(MatricesUniformData));

  // Create per-mesh descriptor sets that bind this entity's UBO + mesh's material
  model.geometry_descriptors.clear();
  model.shadow_descriptors.clear();
  model.geometry_descriptors.reserve(model_data.meshes.size());
  model.shadow_descriptors.reserve(model_data.meshes.size());

  for (const auto& mesh : model_data.meshes) {
    model.geometry_descriptors.push_back(
        CreateMeshDescriptors(model.uniform_buffer, mesh->mat));
    model.shadow_descriptors.push_back(
        CreateShadowMeshDescriptors(model.uniform_buffer, mesh->mat));
  }

  model.render_model_ = model.model_handle;
}

void Renderer::DrawModel(ModelComponent& model,
                         const TransformComponent& transform, bool shadowPass) {
  PROFILE_ZONE_SCOPED();
  AssetManager& assets = AssetManager::Get();
  const Ref<Model>& ptr = assets.GetOrLoad<Model>(model.model_handle);
  if (!ptr) {
    return;
  }

  // Lazily allocate per-entity render data (or re-allocate if model changed)
  if (model.render_model_ != model.model_handle || !model.uniform_buffer) {
    AllocateModelRenderData(model, *ptr);
  }

  // Update this entity's transform UBO
  MatricesUniformData matrices{};
  matrices.ModelMatrix = transform.transform_matrix;
  matrices.NormalMatrix = transform.normal_matrix;
  memcpy(model.uniform_buffer->data_, &matrices, sizeof(MatricesUniformData));

  for (size_t i = 0; i < ptr->meshes.size(); i++) {
    Ref<DescriptorSet> descriptors = shadowPass ? model.shadow_descriptors[i]
                                                : model.geometry_descriptors[i];
    DrawMesh(ptr->meshes[i], descriptors, shadowPass);
  }
}

void Renderer::DrawMesh(Ref<Mesh> mesh, Ref<DescriptorSet> mesh_descriptors,
                        bool shadowPass) {
  PROFILE_ZONE_SCOPED();
  if (!mesh->allocated_) {
    return;
  }

  VkBuffer vertexBuffers[] = {mesh->vertex_buffer->buffer_handle_};
  VkDeviceSize offsets[] = {0};
  static_assert(std::size(vertexBuffers) == std::size(offsets));
  vkCmdBindVertexBuffers(command_buffer_->handle_, 0, std::size(vertexBuffers),
                         vertexBuffers, offsets);
  vkCmdBindIndexBuffer(command_buffer_->handle_,
                       mesh->index_buffer->buffer_handle_, 0,
                       mesh->index_buffer->index_type_);

  VkPipelineLayout layout =
      shadowPass ? shadow_pipeline_->layout_ : geometry_pipeline_->layout_;

  VkDescriptorSet sets[2] = {mesh_descriptors->descriptor_set_,
                             shadowPass
                                 ? camera_->shadow_descriptor->descriptor_set_
                                 : camera_->global_descriptor->descriptor_set_};

  vkCmdBindDescriptorSets(command_buffer_->handle_,
                          VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 2, sets,
                          0, nullptr);

  vkCmdDrawIndexed(command_buffer_->handle_,
                   static_cast<uint32_t>(mesh->indices.size()), 1, 0, 0, 0);
}

void Renderer::DrawSprite(SpriteComponent& sprite,
                          const TransformComponent& transform) {
  PROFILE_ZONE_SCOPED();
  if (!sprite.asset_handle_->is_allocated_) {
    return;
  }
  sprite.asset_handle_->UpdateTransform(transform.transform_matrix);
  // TODO: In the feature, we can use instanced sprites for atlas sprites
  const SpriteAsset::Frame& frame =
      sprite.asset_handle_->frames_[sprite.current_frame_];

  Ref<MemoryBuffer> vertexBuffer = Engine::GetRenderer()->GetQuadVertexBuffer();
  VkBuffer buffers[] = {frame.vertex_buffer->buffer_handle_};
  VkDeviceSize offsets[] = {0};
  static_assert(std::size(buffers) == std::size(offsets));
  vkCmdBindVertexBuffers(command_buffer_->handle_, 0, std::size(buffers),
                         buffers, offsets);

  VkDescriptorSet sets[] = {frame.descriptor->descriptor_set_,
                            camera_->global_descriptor->descriptor_set_};

  vkCmdBindDescriptorSets(
      command_buffer_->handle_, VK_PIPELINE_BIND_POINT_GRAPHICS,
      sprite_pipeline_->layout_, 0, std::size(sets), sets, 0, nullptr);

  vkCmdDraw(command_buffer_->handle_, 6, 1, 0, 0);
}

void Renderer::DrawSkybox(Ref<Skybox> skybox) {
  std::array<VkDescriptorSet, 2> sets{
      skybox->descriptors_->descriptor_set_,
      camera_->global_descriptor->descriptor_set_};

  vkCmdBindDescriptorSets(
      command_buffer_->handle_, VK_PIPELINE_BIND_POINT_GRAPHICS,
      skybox_pipeline_->layout_, 0, 2, sets.data(), 0, nullptr);

  // draw cube via gl_VertexIndex (no vertex/index buffer needed)
  vkCmdDraw(command_buffer_->handle_, 36, 1, 0, 0);
}

void Renderer::DrawFullscreen(
    Ref<Pipeline> pipeline,
    std::initializer_list<Ref<DescriptorSet>> descriptors) {
  /*if (!texture->m_Descriptors) {
    texture->m_Descriptors = CreateDescriptors(texture);
  }
  vkCmdBindDescriptorSets(m_CommandBuffer->m_Handle,
                          VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->m_Layout,
                          0, 1, &texture->m_Descriptors->m_DescriptorSet, 0,
                          nullptr);*/
  std::vector<VkDescriptorSet> sets;
  for (const auto& item : descriptors) {
    if (!item) {
      continue;
    }
    sets.push_back(item->descriptor_set_);
  }
  vkCmdBindDescriptorSets(command_buffer_->handle_,
                          VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->layout_, 0,
                          sets.size(), sets.data(), 0, nullptr);

  // Draw the quad.
  vkCmdDraw(command_buffer_->handle_, 3, 1, 0, 0);
}

static float Halton(int index, int base) {
  float result = 0.0f;
  float f = 1.0f / static_cast<float>(base);
  int i = index;
  while (i > 0) {
    result += f * static_cast<float>(i % base);
    i /= base;
    f /= static_cast<float>(base);
  }
  return result;
}

void Renderer::SetCameraData(Ref<CameraData> camera_data) {
  camera_ = camera_data;
  viewport_size_ = camera_data->viewport_size;
  camera_uniform_data_.Position = camera_data->position;
  camera_uniform_data_.ViewMatrix = camera_data->view_matrix;
  camera_uniform_data_.Projection = camera_data->projection;
  camera_uniform_data_.InvProjection = camera_data->inv_projection;
  camera_uniform_data_.NearPlane = camera_data->near_plane;
  camera_uniform_data_.FarPlane = camera_data->far_plane;
  shadow_camera_uniform_data_.EnableShadows = camera_data->does_shadow_pass;
  for (int i = 0; i < WIESEL_SHADOW_CASCADE_COUNT; ++i) {
    shadow_camera_uniform_data_.ViewProjectionMatrix[i] =
        camera_data->shadow_map_cascades[i].ViewProjMatrix;
    camera_uniform_data_.CascadeSplits[i] =
        camera_data->shadow_map_cascades[i].SplitDepth;
  }
  camera_uniform_data_.enable_ssao = options_.ssao_enabled;
  camera_uniform_data_.debug_cascades = options_.debug_cascades;

  // TAA jitter
  if (options_.aa_mode == AntiAliasingMode::TAA) {
    camera_uniform_data_.PrevViewProjection = prev_jittered_vp_;

    int idx = static_cast<int>((taa_frame_index_ % 16) + 1);
    float jitter_x = Halton(idx, 2) - 0.5f;
    float jitter_y = Halton(idx, 3) - 0.5f;
    camera_uniform_data_.Projection[2][0] += jitter_x * 2.0f / viewport_size_.x;
    camera_uniform_data_.Projection[2][1] += jitter_y * 2.0f / viewport_size_.y;
    camera_uniform_data_.TaaJitterOffset = glm::vec2(
        jitter_x / viewport_size_.x,
        jitter_y / viewport_size_.y
    );

    prev_jittered_vp_ = camera_uniform_data_.Projection * camera_uniform_data_.ViewMatrix;
    taa_frame_index_++;
  } else {
    camera_uniform_data_.PrevViewProjection = camera_data->prev_view_projection;
    camera_uniform_data_.TaaJitterOffset = glm::vec2(0.0f);
  }
}

Ref<DescriptorSet> Renderer::GetFinalOutputDescriptor() const {
  if (!camera_)
    return nullptr;
  if (options_.aa_mode == AntiAliasingMode::FXAA)
    return camera_->fxaa_output_descriptor;
  if (options_.motion_blur_enabled)
    return camera_->motion_blur_output_descriptor;
  if (options_.bloom_enabled)
    return camera_->bloom_output_descriptor;
  if (options_.aa_mode == AntiAliasingMode::TAA)
    return camera_->taa_output_descriptor;
  return camera_->composite_output_descriptor;
}

Ref<AttachmentTexture> Renderer::GetFinalOutputImage() const {
  if (!camera_)
    return nullptr;
  if (options_.aa_mode == AntiAliasingMode::FXAA)
    return camera_->fxaa_image;
  if (options_.motion_blur_enabled)
    return camera_->motion_blur_image;
  if (options_.bloom_enabled)
    return camera_->bloom_composite_image;
  if (options_.aa_mode == AntiAliasingMode::TAA)
    return camera_->taa_output_image;
  return camera_->composite_color_resolve_image;
}

std::vector<const char*> Renderer::GetRequiredExtensions() {
  uint32_t extensionsCount = 0;
  const char** windowExtensions;
  windowExtensions = window_->GetRequiredInstanceExtensions(&extensionsCount);

  std::vector<const char*> extensions(windowExtensions,
                                      windowExtensions + extensionsCount);
#ifdef VULKAN_VALIDATION
  extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
  return extensions;
}

int32_t Renderer::RateDeviceSuitability(VkPhysicalDevice device) {
  VkPhysicalDeviceProperties deviceProperties;
  VkPhysicalDeviceFeatures deviceFeatures;
  vkGetPhysicalDeviceProperties(device, &deviceProperties);
  vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
  int32_t score = 0;

  // Discrete GPUs have a significant performance advantage
  if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
    score += 1000;
  }

  // Maximum possible size of textures affects graphics quality
  score += deviceProperties.limits.maxImageDimension2D;
  return score;
}

VkCommandBuffer Renderer::BeginSingleTimeCommands() {
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = command_pool_->handle_;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(logical_device_, &allocInfo, &commandBuffer);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  WIESEL_CHECK_VKRESULT(vkBeginCommandBuffer(commandBuffer, &beginInfo));

  return commandBuffer;
}

void Renderer::EndSingleTimeCommands(VkCommandBuffer commandBuffer) {
  vkEndCommandBuffer(commandBuffer);

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  vkQueueSubmit(graphics_queue_, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(graphics_queue_);

  vkFreeCommandBuffers(logical_device_, command_pool_->handle_, 1,
                       &commandBuffer);
}

bool Renderer::IsDeviceSuitable(VkPhysicalDevice device) {
  QueueFamilyIndices indices = FindQueueFamilies(device);

  bool extensionsSupported = CheckDeviceExtensionSupport(device);
  bool swapChainAdequate = false;
  if (extensionsSupported) {
    SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(device);
    swapChainAdequate = !swapChainSupport.formats.empty() &&
                        !swapChainSupport.presentModes.empty();
  }

  VkPhysicalDeviceFeatures supportedFeatures;
  vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

  return indices.IsComplete() && extensionsSupported && swapChainAdequate &&
         supportedFeatures.samplerAnisotropy;
}

VkSurfaceFormatKHR Renderer::ChooseSwapSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& available_formats) {
  /*// Try HDR10 first
  for (const auto& format : available_formats) {
    if ((format.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 ||
         format.format == VK_FORMAT_R16G16B16A16_SFLOAT) &&
        format.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT) {
      return format;
    }
  }

  // Try scRGB (linear HDR)
  for (const auto& format : available_formats) {
    if (format.format == VK_FORMAT_R16G16B16A16_SFLOAT &&
        format.colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT) {
      return format;
    }
  }*/

  // Fallback to SDR
  for (const auto& format : available_formats) {
    if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
        format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return format;
    }
  }

  return available_formats[0];
}

VkPresentModeKHR Renderer::ChooseSwapPresentMode(
    const std::vector<VkPresentModeKHR>& available_present_modes) {
  /*
     * VK_PRESENT_MODE_IMMEDIATE_KHR: Images submitted by your application are transferred to the screen right away, which may result in tearing.
     * VK_PRESENT_MODE_FIFO_KHR: The swap chain is a queue where the display takes an image from the front of the queue when the display is refreshed and the program inserts rendered images at the back of the queue. If the queue is full then the program has to wait. This is most similar to vertical sync as found in modern games. The moment that the display is refreshed is known as "vertical blank".
     * VK_PRESENT_MODE_FIFO_RELAXED_KHR: This mode only differs from the previous one if the application is late and the queue was empty at the last vertical blank. Instead of waiting for the next vertical blank, the image is transferred right away when it finally arrives. This may result in visible tearing.
     * VK_PRESENT_MODE_MAILBOX_KHR: This is another variation of the second mode. Instead of blocking the application when the queue is full, the images that are already queued are simply replaced with the newer ones. This mode can be used to render frames as fast as possible while still avoiding tearing, resulting in fewer latency issues than standard vertical sync. This is commonly known as "triple buffering", although the existence of three buffers alone does not necessarily mean that the framerate is unlocked.
     */
  if (!options().vsync) {
    return VK_PRESENT_MODE_IMMEDIATE_KHR;
  }

  for (const auto& availablePresentMode : available_present_modes) {
    if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
      return availablePresentMode;
    }
  }

  return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Renderer::ChooseSwapExtent(
    const VkSurfaceCapabilitiesKHR& capabilities) {
  if (capabilities.currentExtent.width !=
      std::numeric_limits<uint32_t>::max()) {
    return capabilities.currentExtent;
  } else {
    Wiesel::WindowSize size{};
    window_->GetWindowFramebufferSize(size);

    VkExtent2D actualExtent = {static_cast<uint32_t>(size.width),
                               static_cast<uint32_t>(size.height)};

    actualExtent.width =
        std::clamp(actualExtent.width, capabilities.minImageExtent.width,
                   capabilities.maxImageExtent.width);
    actualExtent.height =
        std::clamp(actualExtent.height, capabilities.minImageExtent.height,
                   capabilities.maxImageExtent.height);

    return actualExtent;
  }
}

bool Renderer::CheckDeviceExtensionSupport(VkPhysicalDevice device) {
  uint32_t extensionCount;
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
                                       nullptr);

  std::vector<VkExtensionProperties> availableExtensions(extensionCount);
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
                                       availableExtensions.data());

  std::set<std::string> requiredExtensions(device_extensions_.begin(),
                                           device_extensions_.end());

  for (const auto& extension : availableExtensions) {
    requiredExtensions.erase(extension.extensionName);
  }

  return requiredExtensions.empty();
}

QueueFamilyIndices Renderer::FindQueueFamilies(VkPhysicalDevice device) {
  QueueFamilyIndices indices;
  // Logic to find queue family indices to populate struct with

  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
                                           queueFamilies.data());
  uint32_t i = 0;
  for (const auto& queueFamily : queueFamilies) {
    if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      indices.graphicsFamily = i;
    }

    VkBool32 presentSupport = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &presentSupport);

    if (presentSupport) {
      indices.presentFamily = i;
    }

    if (indices.IsComplete()) {
      break;
    }

    i++;
  }
  return indices;
}

SwapChainSupportDetails Renderer::QuerySwapChainSupport(
    VkPhysicalDevice device) {
  SwapChainSupportDetails details;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_,
                                            &details.capabilities);

  uint32_t formatCount;
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, nullptr);

  if (formatCount != 0) {
    details.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount,
                                         details.formats.data());
  }

  uint32_t presentModeCount;
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentModeCount,
                                            nullptr);

  if (presentModeCount != 0) {
    details.presentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        device, surface_, &presentModeCount, details.presentModes.data());
  }

  return details;
}

uint32_t Renderer::FindMemoryType(uint32_t typeFilter,
                                  VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(physical_device_, &memProperties);
  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags &
                                    properties) == properties) {
      return i;
    }
  }

  throw std::runtime_error("failed to find suitable memory type!");
}

#ifdef VULKAN_VALIDATION

void Renderer::SetupDebugMessenger() {
  VkDebugUtilsMessengerCreateInfoEXT createInfo;
  PopulateDebugMessengerCreateInfo(createInfo);

  WIESEL_CHECK_VKRESULT(CreateDebugUtilsMessengerEXT(
      instance_, &createInfo, nullptr, &debug_messenger_));
}

VkResult Renderer::CreateDebugUtilsMessengerEXT(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger) {
  if (pfn_create_debug_utils_messenger_ext_) {
    return pfn_create_debug_utils_messenger_ext_(instance, pCreateInfo,
                                                 pAllocator, pDebugMessenger);
  } else {
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }
}

void Renderer::PopulateDebugMessengerCreateInfo(
    VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
  createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  createInfo.pfnUserCallback = DebugCallback;
}

bool Renderer::CheckValidationLayerSupport() {
  uint32_t layerCount;
  vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

  std::vector<VkLayerProperties> availableLayers(layerCount);
  vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

  for (const char* layerName : validation_layers_) {
    bool layerFound = false;

    for (const auto& layerProperties : availableLayers) {
      if (strcmp(layerName, layerProperties.layerName) == 0) {
        layerFound = true;
        break;
      }
    }

    if (!layerFound) {
      return false;
    }
  }

  return true;
}

void Renderer::DestroyDebugUtilsMessengerEXT(
    VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks* pAllocator) {
  if (pfn_destroy_debug_utils_messenger_ext_) {
    pfn_destroy_debug_utils_messenger_ext_(instance, debugMessenger,
                                           pAllocator);
  }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
              VkDebugUtilsMessageTypeFlagsEXT message_type,
              const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
              void* user_data) {
  if (message_severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
    LOG_DEBUG("{}", std::string(callback_data->pMessage));
    std::cout << std::flush;
  } else if (message_severity ==
             VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    LOG_WARN("{}", std::string(callback_data->pMessage));
    std::cout << std::flush;
  } else if (message_severity ==
             VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    LOG_ERROR("{}", std::string(callback_data->pMessage));
    std::cout << std::flush;
  } else {
    LOG_INFO("{}", std::string(callback_data->pMessage));
    std::cout << std::flush;
  }

  return VK_FALSE;
}

#endif

}  // namespace Wiesel