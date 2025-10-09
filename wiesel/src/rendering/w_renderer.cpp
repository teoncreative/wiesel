
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

namespace Wiesel {

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
  enable_wireframe_ = false;
  enable_ssao_ = true;
  only_ssao_ = false;
  recreate_swap_chain_ = false;
  swap_chain_created_ = false;
  enable_vsync_ = true;
  image_index_ = 0;
  msaa_samples_ = VK_SAMPLE_COUNT_1_BIT;
  previous_msaa_samples_ = VK_SAMPLE_COUNT_1_BIT;
  clear_color_ = {0.1f, 0.1f, 0.2f, 1.0f};
}

Renderer::~Renderer() {
  Cleanup();
}

void Renderer::Initialize(const RendererProperties&& properties) {
  CreateVulkanInstance();
#ifdef VULKAN_VALIDATION
  SetupDebugMessenger();
#endif
  PerfMarker::Init(instance_);
  CreateSurface();
  PickPhysicalDevice();
  CreateLogicalDevice();
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
  component.aspect_ratio = Engine::GetRenderer()->GetAspectRatio();
  VkExtent2D extent = Engine::GetRenderer()->GetExtent();
  component.viewport_size.x = extent.width;
  component.viewport_size.y = extent.height;

  component.ssao_color_image = CreateAttachmentTexture(
      {extent.width / 2, extent.height / 2, AttachmentTextureType::Offscreen, 1,
       VK_FORMAT_R8_UNORM, VK_SAMPLE_COUNT_1_BIT, true});
  component.ssao_blur_horz_color_image = CreateAttachmentTexture(
      {extent.width, extent.height, AttachmentTextureType::Offscreen, 1,
       VK_FORMAT_R8_UNORM, VK_SAMPLE_COUNT_1_BIT, true});
  component.ssao_blur_vert_color_image = CreateAttachmentTexture(
      {extent.width, extent.height, AttachmentTextureType::Offscreen, 1,
       VK_FORMAT_R8_UNORM, VK_SAMPLE_COUNT_1_BIT, true});
  component.ssao_gen_framebuffer = ssao_gen_render_pass_->CreateFramebuffer(
      0, {component.ssao_color_image->image_views_[0]},
      {extent.width / 2, extent.height / 2});
  component.ssao_blur_horz_framebuffer = ssao_blur_horz_render_pass_->CreateFramebuffer(
      0, {component.ssao_blur_horz_color_image->image_views_[0]},
      {extent.width, extent.height});
  component.ssao_blur_vert_framebuffer = ssao_blur_vert_render_pass_->CreateFramebuffer(
      0, {component.ssao_blur_vert_color_image->image_views_[0]},
      {extent.width, extent.height});

  component.geometry_view_pos_image = CreateAttachmentTexture(
      {extent.width, extent.height, AttachmentTextureType::Offscreen, 1,
       VK_FORMAT_R32G32B32A32_SFLOAT, msaa_samples_, true});
  component.geometry_world_pos_image = CreateAttachmentTexture(
      {extent.width, extent.height, AttachmentTextureType::Offscreen, 1,
       VK_FORMAT_R32G32B32A32_SFLOAT, msaa_samples_, true});
  component.GeometryDepthImage = CreateAttachmentTexture(
      {extent.width, extent.height, AttachmentTextureType::Offscreen, 1,
       VK_FORMAT_R32_SFLOAT, msaa_samples_, true});
  component.geometry_normal_image = CreateAttachmentTexture(
      {extent.width, extent.height, AttachmentTextureType::Offscreen, 1,
       VK_FORMAT_R8G8B8A8_UNORM, msaa_samples_, true});
  component.geometry_albedo_image = CreateAttachmentTexture(
      {extent.width, extent.height, AttachmentTextureType::Offscreen, 1,
       VK_FORMAT_R8G8B8A8_UNORM, msaa_samples_, true});
  component.geometry_material_image = CreateAttachmentTexture(
      {extent.width, extent.height, AttachmentTextureType::Offscreen, 1,
       VK_FORMAT_R16G16B16A16_SFLOAT, msaa_samples_, true});
  component.geometry_depth_stencil = CreateAttachmentTexture(
      {extent.width, extent.height, AttachmentTextureType::DepthStencil, 1,
       FindDepthFormat(), msaa_samples_, true});

  component.shadow_depth_stencil = CreateAttachmentTexture(
      {WIESEL_SHADOWMAP_DIM, WIESEL_SHADOWMAP_DIM,
       AttachmentTextureType::DepthStencil, 1, FindDepthFormat(),
       VK_SAMPLE_COUNT_1_BIT, true, WIESEL_SHADOW_CASCADE_COUNT});
  component.shadow_depth_view_array =
      CreateImageView(component.shadow_depth_stencil, VK_IMAGE_VIEW_TYPE_2D_ARRAY,
                      0, WIESEL_SHADOW_CASCADE_COUNT);
  for (int i = 0; i < WIESEL_SHADOW_CASCADE_COUNT; ++i) {
    component.shadow_depth_views[i] =
        CreateImageView(component.shadow_depth_stencil, VK_IMAGE_VIEW_TYPE_2D, i);
    std::array<ImageView*, 1> textures = {
        component.shadow_depth_views[i].get(),
    };
    component.shadow_framebuffers[i] = shadow_render_pass_->CreateFramebuffer(
        0, textures, {WIESEL_SHADOWMAP_DIM, WIESEL_SHADOWMAP_DIM});
  }

  if (msaa_samples_ > VK_SAMPLE_COUNT_1_BIT) {
    component.geometry_view_pos_resolve_image = CreateAttachmentTexture(
        {extent.width, extent.height, AttachmentTextureType::Resolve, 1,
         VK_FORMAT_R32G32B32A32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, true});
    component.geometry_world_pos_resolve_image = CreateAttachmentTexture(
        {extent.width, extent.height, AttachmentTextureType::Resolve, 1,
         VK_FORMAT_R32G32B32A32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, true});
    component.geometry_depth_resolve_image = CreateAttachmentTexture(
        {extent.width, extent.height, AttachmentTextureType::Resolve, 1,
         VK_FORMAT_R32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, true});
    component.geometry_normal_resolve_image = CreateAttachmentTexture(
        {extent.width, extent.height, AttachmentTextureType::Resolve, 1,
         VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT, true});
    component.geometry_albedo_resolve_image = CreateAttachmentTexture(
        {extent.width, extent.height, AttachmentTextureType::Resolve, 1,
         VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT, true});
    component.geometry_material_resolve_image = CreateAttachmentTexture(
        {extent.width, extent.height, AttachmentTextureType::Resolve, 1,
         VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT, true});
    std::array<AttachmentTexture*, 13> textures = {
        component.geometry_view_pos_image.get(),
        component.geometry_world_pos_image.get(),
        component.GeometryDepthImage.get(),
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
    component.geometry_view_pos_resolve_image = component.geometry_view_pos_image;
    component.geometry_world_pos_resolve_image = component.geometry_world_pos_image;
    component.geometry_depth_resolve_image = component.GeometryDepthImage;
    component.geometry_normal_resolve_image = component.geometry_normal_image;
    component.geometry_albedo_resolve_image = component.geometry_albedo_image;
    component.geometry_material_resolve_image = component.geometry_material_image;
    std::array<AttachmentTexture*, 7> textures = {
        component.geometry_view_pos_image.get(),
        component.geometry_world_pos_image.get(),
        component.GeometryDepthImage.get(),
        component.geometry_normal_image.get(),
        component.geometry_albedo_image.get(),
        component.geometry_depth_stencil.get(),
        component.geometry_material_image.get(),
    };
    component.geometry_framebuffer = geometry_render_pass_->CreateFramebuffer(
        0, textures, component.viewport_size);
  }

  component.lighting_color_image = CreateAttachmentTexture(
      {extent.width, extent.height, AttachmentTextureType::Offscreen, 1,
       swap_chain_image_format_, msaa_samples_});
  if (msaa_samples_ > VK_SAMPLE_COUNT_1_BIT) {
    component.lighting_color_resolve_image = CreateAttachmentTexture(
        {extent.width, extent.height, AttachmentTextureType::Resolve, 1,
         swap_chain_image_format_, VK_SAMPLE_COUNT_1_BIT, true});

    std::array<AttachmentTexture*, 2> textures{
        component.lighting_color_image.get(),
        component.lighting_color_resolve_image.get()};
    component.lighting_framebuffer = lighting_render_pass_->CreateFramebuffer(
        0, textures, {extent.width, extent.height});
  } else {
    component.lighting_color_resolve_image = component.lighting_color_image;
    std::array<AttachmentTexture*, 1> textures{
        component.lighting_color_image.get()};
    component.lighting_framebuffer = lighting_render_pass_->CreateFramebuffer(
        0, textures, {extent.width, extent.height});
  }

  component.sprite_color_image = CreateAttachmentTexture(
      {extent.width, extent.height, AttachmentTextureType::Offscreen, 1,
       swap_chain_image_format_, VK_SAMPLE_COUNT_1_BIT, true});

  std::array<AttachmentTexture*, 1> textures{component.sprite_color_image.get()};
  component.sprite_framebuffer = sprite_render_pass_->CreateFramebuffer(
      0, textures, {extent.width, extent.height});

  component.composite_color_image = CreateAttachmentTexture(
      {extent.width, extent.height, AttachmentTextureType::Offscreen, 1,
       swap_chain_image_format_, msaa_samples_});
  if (msaa_samples_ > VK_SAMPLE_COUNT_1_BIT) {
    component.composite_color_resolve_image = CreateAttachmentTexture(
        {extent.width, extent.height, AttachmentTextureType::Resolve, 1,
         swap_chain_image_format_, VK_SAMPLE_COUNT_1_BIT, true});

    std::array<AttachmentTexture*, 2> textures{
        component.composite_color_image.get(),
        component.composite_color_resolve_image.get()};
    component.composite_framebuffer = lighting_render_pass_->CreateFramebuffer(
        0, textures, {extent.width, extent.height});
  } else {
    component.composite_color_resolve_image = component.lighting_color_image;
    std::array<AttachmentTexture*, 1> textures{
        component.composite_color_image.get()};
    component.composite_framebuffer = lighting_render_pass_->CreateFramebuffer(
        0, textures, {extent.width, extent.height});
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
  component.ssao_gen_descriptor->AddUniformBuffer(4, ssao_kernel_uniform_buffer_);
  component.ssao_gen_descriptor->Bake();

  component.ssao_output_descriptor = CreateReference<DescriptorSet>();
  component.ssao_output_descriptor->SetLayout(ssao_output_descriptor_layout_);
  component.ssao_output_descriptor->AddCombinedImageSampler(
      0, component.ssao_color_image->image_views_[0], default_nearest_sampler_);
  component.ssao_output_descriptor->AddCombinedImageSampler(
      1, component.geometry_depth_resolve_image->image_views_[0],default_nearest_sampler_);
  component.ssao_output_descriptor->Bake();

  component.ssao_blur_horz_output_descriptor = CreateReference<DescriptorSet>();
  component.ssao_blur_horz_output_descriptor->SetLayout(ssao_blur_descriptor_layout_);
  component.ssao_blur_horz_output_descriptor->AddCombinedImageSampler(
      0, component.ssao_blur_horz_color_image->image_views_[0], default_linear_sampler_);
  component.ssao_blur_horz_output_descriptor->AddCombinedImageSampler(
      1, component.geometry_depth_resolve_image->image_views_[0],default_nearest_sampler_);
  component.ssao_blur_horz_output_descriptor->Bake();

  component.ssao_blur_vert_output_descriptor = CreateReference<DescriptorSet>();
  component.ssao_blur_vert_output_descriptor->SetLayout(ssao_blur_descriptor_layout_);
  component.ssao_blur_vert_output_descriptor->AddCombinedImageSampler(
      0, component.ssao_blur_vert_color_image->image_views_[0], default_linear_sampler_);
  component.ssao_blur_horz_output_descriptor->AddCombinedImageSampler(
      1, component.geometry_depth_resolve_image->image_views_[0],default_nearest_sampler_);
  component.ssao_blur_vert_output_descriptor->Bake();

  component.view_changed = true;
  component.pos_changed = true;
}

Ref<Texture> Renderer::CreateBlankTexture() {
  Ref<Texture> texture = CreateReference<Texture>(TextureTypeDiffuse, "");

  stbi_uc pixels[] = {255, 255, 255, 255};  // full white
  texture->width_ = 1;
  texture->height_ = 1;
  texture->size_ = texture->width_ * texture->height_ * STBI_rgb_alpha;
  texture->mip_levels_ = static_cast<uint32_t>(std::floor(std::log2(
                             std::max(texture->width_, texture->height_)))) +
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
              VK_SAMPLE_COUNT_1_BIT, format, VK_IMAGE_TILING_OPTIMAL,
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
  texture->image_view_ =
      CreateImageView(texture->image_, format, VK_IMAGE_ASPECT_COLOR_BIT,
                      texture->mip_levels_);

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
  texture->mip_levels_ = static_cast<uint32_t>(std::floor(std::log2(
                             std::max(texture->width_, texture->height_)))) +
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
              VK_SAMPLE_COUNT_1_BIT, format, VK_IMAGE_TILING_OPTIMAL,
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
  texture->image_view_ =
      CreateImageView(texture->image_, format, VK_IMAGE_ASPECT_COLOR_BIT,
                      texture->mip_levels_);

  texture->is_allocated_ = true;
  return texture;
}

Ref<Texture> Renderer::CreateTexture(const std::string& path,
                                     const TextureProps& texture_props,
                                     const SamplerProps& sampler_props) {
  Ref<Texture> texture = CreateReference<Texture>(texture_props.type, path);

  stbi_uc* pixels =
      stbi_load(path.c_str(), reinterpret_cast<int*>(&texture->width_),
                reinterpret_cast<int*>(&texture->height_),
                &texture->channels_, STBI_rgb_alpha);

  if (!pixels) {
    LOG_WARN("Failed to load texture from {}", path);
    return nullptr;
    //throw std::runtime_error("failed to load texture image: " + path);
  }
  texture->size_ = texture->width_ * texture->height_ * STBI_rgb_alpha;
  if (texture_props.generate_mipmaps) {
    texture->mip_levels_ = static_cast<uint32_t>(std::floor(std::log2(std::max(
                               texture->width_, texture->height_)))) +
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
              VK_SAMPLE_COUNT_1_BIT, texture_props.image_format,
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
    texture->mip_levels_ = static_cast<uint32_t>(std::floor(std::log2(std::max(
                               texture->width_, texture->height_)))) +
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
              VK_SAMPLE_COUNT_1_BIT, texture_props.image_format,
              VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                  VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture->image_,
              texture->device_memory_);

  TransitionImageLayout(
      texture->image_, texture_props.image_format, VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, texture->mip_levels_);
  CopyBufferToImage(stagingBuffer, texture->image_,
                    texture->width_,
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
    const std::array<std::string, 6>& paths,
    const TextureProps& texture_props,
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
              VK_SAMPLE_COUNT_1_BIT, texture_props.image_format,
              VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                  VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture->image_,
              texture->device_memory_, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, 6);

  for (uint32_t layer = 0; layer < 6; layer++) {
    TransitionImageLayout(
        texture->image_, texture_props.image_format, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, texture->mip_levels_, layer, 1);

    CopyBufferToImage(stagingBuffer, texture->image_,
                      static_cast<uint32_t>(texture->width_),
                      static_cast<uint32_t>(texture->height_),
                      texture->size_ * layer, layer);
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
  texture->msaa_samples_ = props.msaa_samples;
  int flags;
  if (props.type == AttachmentTextureType::DepthStencil) {
    flags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  } else {
    flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  }

  if (props.sampled) {
    flags |= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  } else {
    flags |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
  }
  if (props.transfer_dest) {
    flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
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
    CreateImage(props.width, props.height, 1, props.msaa_samples,
                props.image_format, VK_IMAGE_TILING_OPTIMAL, flags,
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

  std::vector<VkDescriptorSetLayout> layouts{
      1, geometry_mesh_descriptor_layout_->layout_};
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
  imageInfo.sampler = texture->samplers_.empty()
                          ? default_linear_sampler_->sampler_
                          : texture->samplers_[0]->sampler_;
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
  imageInfo.sampler = texture->sampler_ ? texture->sampler_
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

void Renderer::SetMsaaSamples(VkSampleCountFlagBits samples) {
  msaa_samples_ = samples;
}

VkSampleCountFlagBits Renderer::GetMsaaSamples() {
  return msaa_samples_;
}

void Renderer::SetVsync(bool vsync) {
  if (vsync == enable_vsync_) {
    return;
  }

  enable_vsync_ = vsync;
  if (swap_chain_created_) {
    recreate_swap_chain_ = true;
  }
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
  createInfo.enabledLayerCount = static_cast<uint32_t>(validation_layers_.size());
  createInfo.ppEnabledLayerNames = validation_layers_.data();

  PopulateDebugMessengerCreateInfo(debugCreateInfo);
  createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
#else
  createInfo.enabledLayerCount = 0;
  createInfo.pNext = nullptr;
#endif

  WIESEL_CHECK_VKRESULT(vkCreateInstance(&createInfo, nullptr, &instance_));
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
    msaa_samples_ = GetMaxUsableSampleCount();
    previous_msaa_samples_ = msaa_samples_;
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
  global_shadow_descriptor_layout_->AddBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                             VK_SHADER_STAGE_VERTEX_BIT);
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
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT); // samplerSSAO
  ssao_output_descriptor_layout_->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT); // samplerDepth
  ssao_output_descriptor_layout_->Bake();

  ssao_blur_descriptor_layout_ = CreateReference<DescriptorSetLayout>();
  ssao_blur_descriptor_layout_->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT); // samplerSSAO
  ssao_blur_descriptor_layout_->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT); // samplerDepth
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
  sprite_draw_descriptor_layout_->AddBinding(
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
  sprite_draw_descriptor_layout_->Bake();
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

  std::vector<VkImage> swapChainImages;
  vkGetSwapchainImagesKHR(logical_device_, swap_chain_, &imageCount, nullptr);
  swapChainImages.resize(imageCount);
  vkGetSwapchainImagesKHR(logical_device_, swap_chain_, &imageCount,
                          swapChainImages.data());
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
  texture->msaa_samples_ = msaa_samples_;
  for (VkImage& image : swapChainImages) {
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
       static_cast<uint32_t>(swapChainImages.size()), FindDepthFormat(),
       msaa_samples_});

  present_color_image_ = CreateAttachmentTexture(
      {extent_.width, extent_.height, AttachmentTextureType::Color,
       static_cast<uint32_t>(swapChainImages.size()), swap_chain_image_format_,
       msaa_samples_});

  present_render_pass_ = CreateReference<RenderPass>(PassType::Present);
  present_render_pass_->AttachOutput(present_color_image_);
  present_render_pass_->AttachOutput(present_depth_stencil_);
  present_render_pass_->AttachOutput(swap_chain_texture_);
  present_render_pass_->Bake();
  present_framebuffers_.resize(swapChainImages.size());
  std::array<AttachmentTexture*, 3> textures{present_color_image_.get(),
                                             present_depth_stencil_.get(),
                                             swap_chain_texture_.get()};
  for (uint32_t i = 0; i < swapChainImages.size(); i++) {
    present_framebuffers_[i] = present_render_pass_->CreateFramebuffer(
        i, textures, {extent_.width, extent_.height});
  }
}

void Renderer::CreateGeometryRenderPass() {
  LOG_DEBUG("Creating render pass");

  geometry_render_pass_ = CreateReference<RenderPass>(PassType::Geometry);
  geometry_render_pass_->AttachOutput({.type = AttachmentTextureType::Offscreen,
                                      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                                      .msaa_samples = msaa_samples_});
  geometry_render_pass_->AttachOutput({.type = AttachmentTextureType::Offscreen,
                                      .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                                      .msaa_samples = msaa_samples_});
  geometry_render_pass_->AttachOutput({.type = AttachmentTextureType::Offscreen,
                                      .format = VK_FORMAT_R32_SFLOAT,
                                      .msaa_samples = msaa_samples_});
  geometry_render_pass_->AttachOutput({.type = AttachmentTextureType::Offscreen,
                                      .format = VK_FORMAT_R8G8B8A8_UNORM,
                                      .msaa_samples = msaa_samples_});
  geometry_render_pass_->AttachOutput({.type = AttachmentTextureType::Offscreen,
                                      .format = VK_FORMAT_R8G8B8A8_UNORM,
                                      .msaa_samples = msaa_samples_});
  geometry_render_pass_->AttachOutput({.type = AttachmentTextureType::Offscreen,
                                      .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                                      .msaa_samples = msaa_samples_});
  geometry_render_pass_->AttachOutput(
      {.type = AttachmentTextureType::DepthStencil,
       .format = FindDepthFormat(),
       .msaa_samples = msaa_samples_});
  if (msaa_samples_ > VK_SAMPLE_COUNT_1_BIT) {
    geometry_render_pass_->AttachOutput({.type = AttachmentTextureType::Resolve,
                                        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                                        .msaa_samples = VK_SAMPLE_COUNT_1_BIT});
    geometry_render_pass_->AttachOutput({.type = AttachmentTextureType::Resolve,
                                        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                                        .msaa_samples = VK_SAMPLE_COUNT_1_BIT});
    geometry_render_pass_->AttachOutput({.type = AttachmentTextureType::Resolve,
                                        .format = VK_FORMAT_R32_SFLOAT,
                                        .msaa_samples = VK_SAMPLE_COUNT_1_BIT});
    geometry_render_pass_->AttachOutput({.type = AttachmentTextureType::Resolve,
                                        .format = VK_FORMAT_R8G8B8A8_UNORM,
                                        .msaa_samples = VK_SAMPLE_COUNT_1_BIT});
    geometry_render_pass_->AttachOutput({.type = AttachmentTextureType::Resolve,
                                        .format = VK_FORMAT_R8G8B8A8_UNORM,
                                        .msaa_samples = VK_SAMPLE_COUNT_1_BIT});
    geometry_render_pass_->AttachOutput({.type = AttachmentTextureType::Resolve,
                                        .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                                        .msaa_samples = VK_SAMPLE_COUNT_1_BIT});
  }
  geometry_render_pass_->Bake();

  lighting_render_pass_ = CreateReference<RenderPass>(PassType::Lighting);
  lighting_render_pass_->AttachOutput({.type = AttachmentTextureType::Offscreen,
                                      .format = swap_chain_image_format_,
                                      .msaa_samples = msaa_samples_});
  if (msaa_samples_ > VK_SAMPLE_COUNT_1_BIT) {
    lighting_render_pass_->AttachOutput({.type = AttachmentTextureType::Resolve,
                                        .format = swap_chain_image_format_,
                                        .msaa_samples = VK_SAMPLE_COUNT_1_BIT});
  }
  lighting_render_pass_->Bake();

  composite_render_pass_ = CreateReference<RenderPass>(PassType::PostProcess);
  composite_render_pass_->AttachOutput({.type = AttachmentTextureType::Offscreen,
                                       .format = swap_chain_image_format_,
                                       .msaa_samples = msaa_samples_});
  if (msaa_samples_ > VK_SAMPLE_COUNT_1_BIT) {
    composite_render_pass_->AttachOutput({.type = AttachmentTextureType::Resolve,
                                         .format = swap_chain_image_format_,
                                         .msaa_samples = VK_SAMPLE_COUNT_1_BIT});
  }
  composite_render_pass_->Bake();

  sprite_render_pass_ = CreateReference<RenderPass>(PassType::PostProcess);
  sprite_render_pass_->AttachOutput({.type = AttachmentTextureType::Offscreen,
                                    .format = swap_chain_image_format_,
                                    .msaa_samples = VK_SAMPLE_COUNT_1_BIT});
  sprite_render_pass_->Bake();

  ssao_gen_render_pass_ = CreateReference<RenderPass>(PassType::PostProcess);
  ssao_gen_render_pass_->AttachOutput({.type = AttachmentTextureType::Offscreen,
                                     .format = VK_FORMAT_R8_UNORM,
                                     .msaa_samples = VK_SAMPLE_COUNT_1_BIT});
  ssao_gen_render_pass_->Bake();

  ssao_blur_horz_render_pass_ = CreateReference<RenderPass>(PassType::PostProcess);
  ssao_blur_horz_render_pass_->AttachOutput({.type = AttachmentTextureType::Offscreen,
                                      .format = VK_FORMAT_R8_UNORM,
                                      .msaa_samples = VK_SAMPLE_COUNT_1_BIT});
  ssao_blur_horz_render_pass_->Bake();

  ssao_blur_vert_render_pass_ = CreateReference<RenderPass>(PassType::PostProcess);
  ssao_blur_vert_render_pass_->AttachOutput({.type = AttachmentTextureType::Offscreen,
                                      .format = VK_FORMAT_R8_UNORM,
                                      .msaa_samples = VK_SAMPLE_COUNT_1_BIT});
  ssao_blur_vert_render_pass_->Bake();

  shadow_render_pass_ = CreateReference<RenderPass>(PassType::Shadow);
  shadow_render_pass_->AttachOutput({.type = AttachmentTextureType::DepthStencil,
                                    .format = FindDepthFormat(),
                                    .msaa_samples = VK_SAMPLE_COUNT_1_BIT});
  shadow_render_pass_->Bake();
}

void Renderer::CreateGeometryGraphicsPipelines() {
  LOG_DEBUG("Creating graphics pipeline");
  auto geometryVertexShader =
      CreateShader({ShaderTypeVertex, ShaderLangGLSL, "main",
                    ShaderSourceSource, "assets/internal_shaders/geometry_shader.vert"});
  auto geometryFragmentShader =
      CreateShader({ShaderTypeFragment, ShaderLangGLSL, "main",
                    ShaderSourceSource, "assets/internal_shaders/geometry_shader.frag"});
  geometry_pipeline_ = CreateReference<Pipeline>(PipelineProperties{
      msaa_samples_, CullModeBack, enable_wireframe_, false});
  geometry_pipeline_->SetVertexData(Vertex3D::GetBindingDescription(),
                                    Vertex3D::GetAttributeDescriptions());
  geometry_pipeline_->SetRenderPass(geometry_render_pass_);
  geometry_pipeline_->AddInputLayout(geometry_mesh_descriptor_layout_);
  geometry_pipeline_->AddInputLayout(global_descriptor_layout_);
  geometry_pipeline_->AddShader(geometryVertexShader);
  geometry_pipeline_->AddShader(geometryFragmentShader);
  geometry_pipeline_->Bake();

  auto skyboxVertexShader =
      CreateShader({ShaderTypeVertex, ShaderLangGLSL, "main",
                    ShaderSourceSource, "assets/internal_shaders/skybox_shader.vert"});
  auto skyboxFragmentShader =
      CreateShader({ShaderTypeFragment, ShaderLangGLSL, "main",
                    ShaderSourceSource, "assets/internal_shaders/skybox_shader.frag"});
  skybox_pipeline_ = CreateReference<Pipeline>(PipelineProperties{
      msaa_samples_, CullModeFront, false, false, true, false});
  skybox_pipeline_->SetRenderPass(lighting_render_pass_);
  skybox_pipeline_->AddInputLayout(skybox_descriptor_layout_);
  skybox_pipeline_->AddInputLayout(global_descriptor_layout_);
  skybox_pipeline_->AddShader(skyboxVertexShader);
  skybox_pipeline_->AddShader(skyboxFragmentShader);
  skybox_pipeline_->Bake();

  auto fullscreenVertexShader = CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "assets/internal_shaders/fullscreen_shader.vert"});
  auto lightingFragmentShader =
      CreateShader({ShaderTypeFragment, ShaderLangGLSL, "main",
                    ShaderSourceSource, "assets/internal_shaders/lighting_shader.frag"});

  lighting_pipeline_ = CreateReference<Pipeline>(PipelineProperties{
      msaa_samples_, CullModeFront, false, true, true, false});
  lighting_pipeline_->SetRenderPass(lighting_render_pass_);
  lighting_pipeline_->AddInputLayout(geometry_output_descriptor_layout_);
  lighting_pipeline_->AddInputLayout(ssao_output_descriptor_layout_);
  lighting_pipeline_->AddInputLayout(global_descriptor_layout_);
  lighting_pipeline_->AddInputLayout(skybox_descriptor_layout_);
  lighting_pipeline_->AddShader(fullscreenVertexShader);
  lighting_pipeline_->AddShader(lightingFragmentShader);
  lighting_pipeline_->Bake();

  auto shadowVertexShader =
      CreateShader({ShaderTypeVertex, ShaderLangGLSL, "main",
                    ShaderSourceSource, "assets/internal_shaders/shadow_shader.vert"});
  auto shadowFragmentShader =
      CreateShader({ShaderTypeFragment, ShaderLangGLSL, "main",
                    ShaderSourceSource, "assets/internal_shaders/shadow_shader.frag"});
  shadow_pipeline_ = CreateReference<Pipeline>(PipelineProperties{
      VK_SAMPLE_COUNT_1_BIT, CullModeFront, false, false, true, true});
  shadow_pipeline_->SetRenderPass(shadow_render_pass_);
  shadow_pipeline_->SetVertexData(Vertex3D::GetBindingDescription(),
                                  Vertex3D::GetAttributeDescriptions());
  shadow_pipeline_->AddPushConstant(shadow_pipeline_push_constant_,
                                    VK_SHADER_STAGE_VERTEX_BIT);
  shadow_pipeline_->AddInputLayout(shadow_mesh_descriptor_layout_);
  shadow_pipeline_->AddInputLayout(global_shadow_descriptor_layout_);
  shadow_pipeline_->AddShader(shadowVertexShader);
  shadow_pipeline_->AddShader(shadowFragmentShader);
  shadow_pipeline_->Bake();

  auto ssaoFragmentShader =
      CreateShader({ShaderTypeFragment, ShaderLangGLSL, "main",
                    ShaderSourceSource, "assets/internal_shaders/ssao_gen_shader.frag"});

  ssao_gen_pipeline_ = CreateReference<Pipeline>(PipelineProperties{
      VK_SAMPLE_COUNT_1_BIT, CullModeFront, false, false, false, false});
  ssao_gen_pipeline_->SetRenderPass(ssao_gen_render_pass_);
  ssao_gen_pipeline_->AddInputLayout(ssao_gen_descriptor_layout_);
  ssao_gen_pipeline_->AddInputLayout(global_descriptor_layout_);
  ssao_gen_pipeline_->AddShader(fullscreenVertexShader);
  ssao_gen_pipeline_->AddShader(ssaoFragmentShader);
  ssao_gen_pipeline_->Bake();

  auto ssaoBlurHorzFragmentShader = CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "assets/internal_shaders/ssao_blur_shader.frag"});

  ssao_blur_horz_pipeline_ = CreateReference<Pipeline>(PipelineProperties{
      VK_SAMPLE_COUNT_1_BIT, CullModeFront, false, false, false, false});
  ssao_blur_horz_pipeline_->SetRenderPass(ssao_blur_horz_render_pass_);
  ssao_blur_horz_pipeline_->AddInputLayout(ssao_blur_descriptor_layout_);
  ssao_blur_horz_pipeline_->AddShader(fullscreenVertexShader);
  ssao_blur_horz_pipeline_->AddShader(ssaoBlurHorzFragmentShader);
  ssao_blur_horz_pipeline_->Bake();

  auto ssaoBlurVertFragmentShader = CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "assets/internal_shaders/ssao_blur_shader.frag", {"BLUR_VERTICAL"}});

  ssao_blur_vert_pipeline_ = CreateReference<Pipeline>(PipelineProperties{
      VK_SAMPLE_COUNT_1_BIT, CullModeFront, false, false, false, false});
  ssao_blur_vert_pipeline_->SetRenderPass(ssao_blur_vert_render_pass_);
  ssao_blur_vert_pipeline_->AddInputLayout(ssao_blur_descriptor_layout_);
  ssao_blur_vert_pipeline_->AddShader(fullscreenVertexShader);
  ssao_blur_vert_pipeline_->AddShader(ssaoBlurVertFragmentShader);
  ssao_blur_vert_pipeline_->Bake();

  auto spriteVertexShader =
      CreateShader({ShaderTypeVertex, ShaderLangGLSL, "main",
                    ShaderSourceSource, "assets/internal_shaders/sprite_shader.vert"});
  auto spriteFragmentShader =
      CreateShader({ShaderTypeFragment, ShaderLangGLSL, "main",
                    ShaderSourceSource, "assets/internal_shaders/sprite_shader.frag"});

  sprite_pipeline_ = CreateReference<Pipeline>(PipelineProperties{
      VK_SAMPLE_COUNT_1_BIT, CullModeNone, false, true, false, false});
  sprite_pipeline_->SetVertexData(VertexSprite::GetBindingDescriptions(),
                                  VertexSprite::GetAttributeDescriptions());
  sprite_pipeline_->SetRenderPass(sprite_render_pass_);
  sprite_pipeline_->AddInputLayout(sprite_draw_descriptor_layout_);
  sprite_pipeline_->AddInputLayout(global_descriptor_layout_);
  sprite_pipeline_->AddShader(spriteVertexShader);
  sprite_pipeline_->AddShader(spriteFragmentShader);
  sprite_pipeline_->Bake();

  auto compositeFragmentShader =
      CreateShader({ShaderTypeFragment, ShaderLangGLSL, "main",
                    ShaderSourceSource, "assets/internal_shaders/quad_shader.frag"});

  composite_pipeline_ = CreateReference<Pipeline>(PipelineProperties{
      msaa_samples_, CullModeFront, false, true, true, false});
  composite_pipeline_->SetRenderPass(composite_render_pass_);
  composite_pipeline_->AddInputLayout(skybox_descriptor_layout_);
  composite_pipeline_->AddShader(fullscreenVertexShader);
  composite_pipeline_->AddShader(compositeFragmentShader);
  composite_pipeline_->Bake();
}

void Renderer::CreatePresentGraphicsPipelines() {
  auto presentVertexShader = CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "assets/internal_shaders/fullscreen_shader.vert"});
  auto presentFragmentShader =
      CreateShader({ShaderTypeFragment, ShaderLangGLSL, "main",
                    ShaderSourceSource, "assets/internal_shaders/quad_shader.frag"});
  present_pipeline_ = CreateReference<Pipeline>(
      PipelineProperties{msaa_samples_, CullModeNone, false, true});
  present_pipeline_->SetRenderPass(present_render_pass_);
  present_pipeline_->AddInputLayout(present_descriptor_layout_);
  present_pipeline_->AddShader(presentVertexShader);
  present_pipeline_->AddShader(presentFragmentShader);
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
                                 uint32_t height, VkDeviceSize baseOffset,
                                 uint32_t layer) {
  PROFILE_ZONE_SCOPED();
  VkCommandBuffer commandBuffer = BeginSingleTimeCommands();
  VkBufferImageCopy region{};
  region.bufferOffset = baseOffset;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;

  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = layer;
  region.imageSubresource.layerCount = 1;

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

  VkPipelineStageFlags sourceStage;
  VkPipelineStageFlags destinationStage;

  if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
      newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
             newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR &&
             newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    sourceStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = 0;

    sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
             newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = 0;

    sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.dstAccessMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

    sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  } else {
    throw std::invalid_argument("unsupported layout transition!");
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
  shadow_pipeline_push_constant_ = CreateReference<ShadowPipelinePushConstant>();

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
                           VkSampleCountFlagBits numSamples, VkFormat format,
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
  imageInfo.samples = numSamples;
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

VkSampleCountFlagBits Renderer::GetMaxUsableSampleCount() {
  PROFILE_ZONE_SCOPED();
  VkSampleCountFlags counts =
      physical_device_properties_.limits.framebufferColorSampleCounts &
      physical_device_properties_.limits.framebufferDepthSampleCounts;
  if (counts & VK_SAMPLE_COUNT_64_BIT) {
    return VK_SAMPLE_COUNT_64_BIT;
  }
  if (counts & VK_SAMPLE_COUNT_32_BIT) {
    return VK_SAMPLE_COUNT_32_BIT;
  }
  if (counts & VK_SAMPLE_COUNT_16_BIT) {
    return VK_SAMPLE_COUNT_16_BIT;
  }
  if (counts & VK_SAMPLE_COUNT_8_BIT) {
    return VK_SAMPLE_COUNT_8_BIT;
  }
  if (counts & VK_SAMPLE_COUNT_4_BIT) {
    return VK_SAMPLE_COUNT_4_BIT;
  }
  if (counts & VK_SAMPLE_COUNT_2_BIT) {
    return VK_SAMPLE_COUNT_2_BIT;
  }
  return VK_SAMPLE_COUNT_1_BIT;
}

void Renderer::CreateTracy() {
  tracy_ctx_ = TracyVkContext(physical_device_, logical_device_,
                                       graphics_queue_, command_buffer_->handle_);
}

void Renderer::CreateSyncObjects() {
  PROFILE_ZONE_SCOPED();
  VkSemaphoreCreateInfo semaphoreInfo{};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fenceInfo{};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  WIESEL_CHECK_VKRESULT(vkCreateSemaphore(logical_device_, &semaphoreInfo,
                                          nullptr, &image_available_semaphore_));
  WIESEL_CHECK_VKRESULT(vkCreateSemaphore(logical_device_, &semaphoreInfo,
                                          nullptr, &render_finished_semaphore_));
  WIESEL_CHECK_VKRESULT(
      vkCreateFence(logical_device_, &fenceInfo, nullptr, &fence_));
}

void Renderer::CleanupDescriptorLayouts() {
  geometry_mesh_descriptor_layout_ = nullptr;
  present_descriptor_layout_ = nullptr;
}

void Renderer::CleanupGeometryGraphics() {
  geometry_pipeline_ = nullptr;
  geometry_render_pass_ = nullptr;
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

  CleanupPresentGraphics();
  CleanupGeometryGraphics();
  CreateSwapChain();
  CreatePresentGraphicsPipelines();
  CreateGeometryRenderPass();
  CreateGeometryGraphicsPipelines();
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
  if (previous_msaa_samples_ != msaa_samples_) {
    LOG_INFO("Msaa samples changed to {} from {}!",
             std::to_string(msaa_samples_),
             std::to_string(previous_msaa_samples_));
    previous_msaa_samples_ = msaa_samples_;
  }

  // Reloading stuff
  if (recreate_swap_chain_) {
    PROFILE_ZONE_SCOPED_N("Renderer::BeginRender: Recreate swap chain");
    RecreateSwapChain();
    recreate_swap_chain_ = false;
    recreate_pipeline_ = false;
  }
  if (recreate_pipeline_) {
    PROFILE_ZONE_SCOPED_N("Renderer::BeginRender: Recreate Pipeline");
    vkDeviceWaitIdle(logical_device_);
    LOG_INFO("Recreating graphics pipeline...");
    geometry_pipeline_->properties_.enable_wireframe =
        enable_wireframe_;  // Update wireframe mode
    RecreatePipeline(geometry_pipeline_);
    recreate_pipeline_ = false;
  }
}

bool Renderer::BeginPresent() {
  PROFILE_ZONE_SCOPED();
  VkResult result = vkAcquireNextImageKHR(logical_device_, swap_chain_,
                                          UINT64_MAX, image_available_semaphore_,
                                          VK_NULL_HANDLE, &image_index_);
  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
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
    TransitionImageLayout(camera_->composite_color_resolve_image->images_[0],
                          camera_->composite_color_resolve_image->format_,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                          command_buffer_->handle_, 0, 1);
  }

  present_pipeline_->Bind(PipelineBindPointGraphics);
  present_render_pass_->Begin(present_framebuffers_[image_index_], clear_color_);
  SetViewport(extent_);
  return true;
}

void Renderer::EndPresent() {
  PROFILE_ZONE_SCOPED();
  present_render_pass_->End();
  // This was done here to prevent some errors caused by doing it inside the pass
  // I'm not sure if this is a correct solution, find out and move this to the present image if not required
  if (camera_) {
    TransitionImageLayout(camera_->composite_color_resolve_image->images_[0],
                          camera_->composite_color_resolve_image->format_,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1,
                          command_buffer_->handle_, 0, 1);
  }
  /*
  for (const auto& item : textures) {
    TransitionImageLayout(item->m_Images[0], item->m_Format,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1,
                          m_CommandBuffer->m_Handle);
  }*/

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

  WIESEL_CHECK_VKRESULT(
      vkQueueSubmit(graphics_queue_, 1, &submitInfo, fence_));
  PROFILE_GPU_COLLECT(tracy_ctx_, command_buffer_->handle_);

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

void Renderer::BeginShadowPass(uint32_t cascade) {
  PROFILE_ZONE_SCOPED();
  memcpy(shadow_camera_uniform_buffer_->data_, &shadow_camera_uniform_data_,
         sizeof(shadow_camera_uniform_data_));
  shadow_pipeline_push_constant_->cascade_index = cascade;

  shadow_pipeline_->Bind(PipelineBindPointGraphics);
  shadow_render_pass_->Begin(camera_->shadow_framebuffers[cascade],
                            {0, 0, 0, 1});
  SetViewport(glm::vec2{WIESEL_SHADOWMAP_DIM, WIESEL_SHADOWMAP_DIM});
}

void Renderer::EndShadowPass() {
  PROFILE_ZONE_SCOPED();
  shadow_render_pass_->End();
}

void Renderer::BeginGeometryPass() {
  PROFILE_ZONE_SCOPED();
  geometry_pipeline_->Bind(PipelineBindPointGraphics);
  geometry_render_pass_->Begin(camera_->geometry_framebuffer, {0, 0, 0, 0});
  SetViewport(viewport_size_);
}

void Renderer::EndGeometryPass() {
  PROFILE_ZONE_SCOPED();
  geometry_render_pass_->End();
}

void Renderer::DrawModel(ModelComponent& model, const TransformComponent& transform, bool shadowPass) {
  PROFILE_ZONE_SCOPED();
  for (int i = 0; i < model.data.meshes.size(); i++) {
    const auto& mesh = model.data.meshes[i];
    DrawMesh(mesh, transform, shadowPass);
  }
}

void Renderer::DrawMesh(Ref<Mesh> mesh, const TransformComponent& transform, bool shadowPass) {
  PROFILE_ZONE_SCOPED();
  if (!mesh->allocated_) {
    return;
  }
  mesh->UpdateTransform(transform.transform_matrix, transform.normal_matrix);

  VkBuffer vertexBuffers[] = {mesh->vertex_buffer->buffer_handle_};
  VkDeviceSize offsets[] = {0};
  static_assert(std::size(vertexBuffers) == std::size(offsets));
  vkCmdBindVertexBuffers(command_buffer_->handle_, 0, std::size(vertexBuffers),
                         vertexBuffers, offsets);
  // Todo get the index type from index buffer instead of hardcoding it.
  vkCmdBindIndexBuffer(command_buffer_->handle_, mesh->index_buffer->buffer_handle_,
                       0, mesh->index_buffer->index_type_);

  VkPipelineLayout layout =
      shadowPass ? shadow_pipeline_->layout_ : geometry_pipeline_->layout_;

  VkDescriptorSet sets[2] = {
      shadowPass ? mesh->shadow_descriptors->descriptor_set_
                 : mesh->geometry_descriptors->descriptor_set_,
      shadowPass ? camera_->shadow_descriptor->descriptor_set_
                 : camera_->global_descriptor->descriptor_set_};

  vkCmdBindDescriptorSets(command_buffer_->handle_,
                          VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 2, sets,
                          0, nullptr);

  vkCmdDrawIndexed(command_buffer_->handle_,
                   static_cast<uint32_t>(mesh->indices.size()), 1, 0, 0, 0);
}

void Renderer::DrawSprite(SpriteComponent& sprite, const TransformComponent& transform) {
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

void Renderer::BeginSSAOGenPass() {
  PROFILE_ZONE_SCOPED();
  TransitionImageLayout(camera_->geometry_view_pos_resolve_image->images_[0],
                        camera_->geometry_view_pos_resolve_image->format_,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                        command_buffer_->handle_, 0, 1);
  TransitionImageLayout(camera_->geometry_world_pos_resolve_image->images_[0],
                        camera_->geometry_world_pos_resolve_image->format_,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                        command_buffer_->handle_, 0, 1);
  TransitionImageLayout(camera_->geometry_depth_resolve_image->images_[0],
                        camera_->geometry_depth_resolve_image->format_,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                        command_buffer_->handle_, 0, 1);
  TransitionImageLayout(camera_->geometry_normal_resolve_image->images_[0],
                        camera_->geometry_normal_resolve_image->format_,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                        command_buffer_->handle_, 0, 1);
  TransitionImageLayout(ssao_noise_->images_[0], ssao_noise_->format_,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                        command_buffer_->handle_, 0, 1);
  ssao_gen_render_pass_->Begin(camera_->ssao_gen_framebuffer, {0, 0, 0, 0});
  SetViewport(glm::vec2{viewport_size_.x / 2, viewport_size_.y / 2});
}

void Renderer::EndSSAOGenPass() {
  ssao_gen_render_pass_->End();
  TransitionImageLayout(camera_->geometry_view_pos_resolve_image->images_[0],
                        camera_->geometry_view_pos_resolve_image->format_,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1,
                        command_buffer_->handle_, 0, 1);
  TransitionImageLayout(camera_->geometry_world_pos_resolve_image->images_[0],
                        camera_->geometry_world_pos_resolve_image->format_,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1,
                        command_buffer_->handle_, 0, 1);
  TransitionImageLayout(camera_->geometry_depth_resolve_image->images_[0],
                        camera_->geometry_depth_resolve_image->format_,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1,
                        command_buffer_->handle_, 0, 1);
  TransitionImageLayout(camera_->geometry_normal_resolve_image->images_[0],
                        camera_->geometry_normal_resolve_image->format_,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1,
                        command_buffer_->handle_, 0, 1);
  TransitionImageLayout(ssao_noise_->images_[0], ssao_noise_->format_,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1,
                        command_buffer_->handle_, 0, 1);
}

void Renderer::BeginSSAOBlurHorzPass() {
  TransitionImageLayout(camera_->ssao_color_image->images_[0],
                        camera_->ssao_color_image->format_,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                        command_buffer_->handle_, 0, 1);
  TransitionImageLayout(camera_->geometry_depth_resolve_image->images_[0],
                        camera_->geometry_depth_resolve_image->format_,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                        command_buffer_->handle_, 0, 1);
  ssao_blur_horz_render_pass_->Begin(camera_->ssao_blur_horz_framebuffer, {0, 0, 0, 0});
  SetViewport(viewport_size_);
}

void Renderer::EndSSAOBlurHorzPass() {
  ssao_blur_horz_render_pass_->End();
  TransitionImageLayout(camera_->ssao_color_image->images_[0],
                        camera_->ssao_color_image->format_,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1,
                        command_buffer_->handle_, 0, 1);
  TransitionImageLayout(camera_->geometry_depth_resolve_image->images_[0],
                        camera_->geometry_depth_resolve_image->format_,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1,
                        command_buffer_->handle_, 0, 1);
}

void Renderer::BeginSSAOBlurVertPass() {
  TransitionImageLayout(camera_->ssao_blur_horz_color_image->images_[0],
                        camera_->ssao_blur_horz_color_image->format_,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                        command_buffer_->handle_, 0, 1);
  TransitionImageLayout(camera_->geometry_depth_resolve_image->images_[0],
                        camera_->geometry_depth_resolve_image->format_,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                        command_buffer_->handle_, 0, 1);
  ssao_blur_vert_render_pass_->Begin(camera_->ssao_blur_vert_framebuffer, {0, 0, 0, 0});
  SetViewport(viewport_size_);
}

void Renderer::EndSSAOBlurVertPass() {
  ssao_blur_vert_render_pass_->End();
  TransitionImageLayout(camera_->ssao_blur_horz_color_image->images_[0],
                        camera_->ssao_blur_horz_color_image->format_,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1,
                        command_buffer_->handle_, 0, 1);
  TransitionImageLayout(camera_->geometry_depth_resolve_image->images_[0],
                        camera_->geometry_depth_resolve_image->format_,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1,
                        command_buffer_->handle_, 0, 1);
}

void Renderer::BeginLightingPass() {
  TransitionImageLayout(camera_->geometry_view_pos_resolve_image->images_[0],
                        camera_->geometry_view_pos_resolve_image->format_,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                        command_buffer_->handle_, 0, 1);
  TransitionImageLayout(camera_->geometry_world_pos_resolve_image->images_[0],
                        camera_->geometry_world_pos_resolve_image->format_,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                        command_buffer_->handle_, 0, 1);
  TransitionImageLayout(camera_->geometry_normal_resolve_image->images_[0],
                        camera_->geometry_normal_resolve_image->format_,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                        command_buffer_->handle_, 0, 1);
  TransitionImageLayout(camera_->geometry_albedo_resolve_image->images_[0],
                        camera_->geometry_albedo_resolve_image->format_,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                        command_buffer_->handle_, 0, 1);
  TransitionImageLayout(camera_->geometry_material_resolve_image->images_[0],
                        camera_->geometry_material_resolve_image->format_,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                        command_buffer_->handle_, 0, 1);
  TransitionImageLayout(camera_->ssao_blur_vert_color_image->images_[0],
                        camera_->ssao_blur_vert_color_image->format_,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                        command_buffer_->handle_, 0, 1);
  if (camera_->shadow_depth_stencil) {
    TransitionImageLayout(camera_->shadow_depth_stencil->images_[0],
                          camera_->shadow_depth_stencil->format_,
                          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                          command_buffer_->handle_, 0,
                          WIESEL_SHADOW_CASCADE_COUNT);
  }
  lighting_render_pass_->Begin(camera_->lighting_framebuffer, clear_color_);
}

void Renderer::EndLightingPass() {
  lighting_render_pass_->End();
  TransitionImageLayout(camera_->geometry_view_pos_resolve_image->images_[0],
                        camera_->geometry_view_pos_resolve_image->format_,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1,
                        command_buffer_->handle_, 0, 1);
  TransitionImageLayout(camera_->geometry_world_pos_resolve_image->images_[0],
                        camera_->geometry_world_pos_resolve_image->format_,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1,
                        command_buffer_->handle_, 0, 1);
  TransitionImageLayout(camera_->geometry_normal_resolve_image->images_[0],
                        camera_->geometry_normal_resolve_image->format_,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1,
                        command_buffer_->handle_, 0, 1);
  TransitionImageLayout(camera_->geometry_albedo_resolve_image->images_[0],
                        camera_->geometry_albedo_resolve_image->format_,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1,
                        command_buffer_->handle_, 0, 1);
  TransitionImageLayout(camera_->geometry_material_resolve_image->images_[0],
                        camera_->geometry_material_resolve_image->format_,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1,
                        command_buffer_->handle_, 0, 1);
  TransitionImageLayout(camera_->ssao_blur_vert_color_image->images_[0],
                        camera_->ssao_blur_vert_color_image->format_,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1,
                        command_buffer_->handle_, 0, 1);
  if (camera_->shadow_depth_stencil) {
    TransitionImageLayout(camera_->shadow_depth_stencil->images_[0],
                          camera_->shadow_depth_stencil->format_,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1,
                          command_buffer_->handle_, 0,
                          WIESEL_SHADOW_CASCADE_COUNT);
  }
}

void Renderer::BeginSpritePass() {
  sprite_render_pass_->Begin(camera_->sprite_framebuffer, {0, 0, 0, 0});
}

void Renderer::EndSpritePass() {
  sprite_render_pass_->End();
}

void Renderer::BeginCompositePass() {
  TransitionImageLayout(camera_->lighting_color_resolve_image->images_[0],
                        camera_->lighting_color_resolve_image->format_,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                        command_buffer_->handle_, 0, 1);
  TransitionImageLayout(camera_->sprite_color_image->images_[0],
                        camera_->sprite_color_image->format_,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                        command_buffer_->handle_, 0, 1);
  composite_render_pass_->Begin(camera_->composite_framebuffer, clear_color_);
}

void Renderer::EndCompositePass() {
  composite_render_pass_->End();
  TransitionImageLayout(camera_->lighting_color_resolve_image->images_[0],
                        camera_->lighting_color_resolve_image->format_,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1,
                        command_buffer_->handle_, 0, 1);
  TransitionImageLayout(camera_->sprite_color_image->images_[0],
                        camera_->sprite_color_image->format_,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1,
                        command_buffer_->handle_, 0, 1);
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
                          VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->layout_,
                          0, sets.size(), sets.data(), 0, nullptr);

  // Draw the quad.
  vkCmdDraw(command_buffer_->handle_, 3, 1, 0, 0);
}

void Renderer::SetCameraData(Ref<CameraData> cameraData) {
  camera_ = cameraData;
  viewport_size_ = cameraData->viewport_size;
  camera_uniform_data_.Position = cameraData->position;
  camera_uniform_data_.ViewMatrix = cameraData->view_matrix;
  camera_uniform_data_.Projection = cameraData->projection;
  camera_uniform_data_.InvProjection = cameraData->inv_projection;
  camera_uniform_data_.NearPlane = cameraData->near_plane;
  camera_uniform_data_.FarPlane = cameraData->far_plane;
  shadow_camera_uniform_data_.EnableShadows = cameraData->does_shadow_pass;
  for (int i = 0; i < WIESEL_SHADOW_CASCADE_COUNT; ++i) {
    shadow_camera_uniform_data_.ViewProjectionMatrix[i] =
        cameraData->shadow_map_cascades[i].ViewProjMatrix;
    camera_uniform_data_.CascadeSplits[i] =
        cameraData->shadow_map_cascades[i].SplitDepth;
  }
  // Todo move this to another ubo for options maybe
  camera_uniform_data_.EnableSSAO = enable_ssao_;
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
    const std::vector<VkSurfaceFormatKHR>& availableFormats) {
  for (const auto& availableFormat : availableFormats) {
    if (availableFormat.format == VK_FORMAT_R8G8B8A8_UNORM &&
        availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return availableFormat;
    }
  }

  return availableFormats[0];
}

VkPresentModeKHR Renderer::ChooseSwapPresentMode(
    const std::vector<VkPresentModeKHR>& availablePresentModes) {
  /*
     * VK_PRESENT_MODE_IMMEDIATE_KHR: Images submitted by your application are transferred to the screen right away, which may result in tearing.
     * VK_PRESENT_MODE_FIFO_KHR: The swap chain is a queue where the display takes an image from the front of the queue when the display is refreshed and the program inserts rendered images at the back of the queue. If the queue is full then the program has to wait. This is most similar to vertical sync as found in modern games. The moment that the display is refreshed is known as "vertical blank".
     * VK_PRESENT_MODE_FIFO_RELAXED_KHR: This mode only differs from the previous one if the application is late and the queue was empty at the last vertical blank. Instead of waiting for the next vertical blank, the image is transferred right away when it finally arrives. This may result in visible tearing.
     * VK_PRESENT_MODE_MAILBOX_KHR: This is another variation of the second mode. Instead of blocking the application when the queue is full, the images that are already queued are simply replaced with the newer ones. This mode can be used to render frames as fast as possible while still avoiding tearing, resulting in fewer latency issues than standard vertical sync. This is commonly known as "triple buffering", although the existence of three buffers alone does not necessarily mean that the framerate is unlocked.
     */
  if (!enable_vsync_) {
    return VK_PRESENT_MODE_IMMEDIATE_KHR;
  }

  for (const auto& availablePresentMode : availablePresentModes) {
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
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount,
                                       nullptr);

  if (formatCount != 0) {
    details.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount,
                                         details.formats.data());
  }

  uint32_t presentModeCount;
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_,
                                            &presentModeCount, nullptr);

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
  auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      instance, "vkCreateDebugUtilsMessengerEXT");
  if (func != nullptr) {
    return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
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
  auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      instance, "vkDestroyDebugUtilsMessengerEXT");

  if (func != nullptr) {
    func(instance, debugMessenger, pAllocator);
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
  } else if (message_severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
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