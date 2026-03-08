
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
#include "rendering/w_acceleration_structure.hpp"
#include "rendering/w_perf_marker.hpp"
#include "rendering/w_sampler.hpp"
#include "rendering/features/w_canvas_feature.hpp"
#include "ui/w_font.hpp"

#include "util/imgui/imgui_spectrum.hpp"
#include "util/w_spirv.hpp"
#include "util/w_vectors.hpp"
#include "w_engine.hpp"

#include <random>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include "animation/w_animation.hpp"
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
  rt_supported_ = CheckRayTracingSupport(physical_device_);
  if (rt_supported_) {
    LOG_INFO("Ray tracing extensions supported - enabling RT");
    device_extensions_.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
    device_extensions_.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
    device_extensions_.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
  } else {
    LOG_INFO("Ray tracing extensions not supported - RT disabled");
  }
  CreateLogicalDevice();
  LoadDeviceExtensions();
  CreateGlobalUniformBuffers();
  // ---
  CreateCommandPools();
  if (rt_supported_) {
    as_manager_ = CreateReference<AccelerationStructureManager>(
        Engine::renderer());
  }
  CreateDescriptorLayouts();
  CreateSwapChain();
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

  VkDeviceSize buffer_size = sizeof(T) * vertices.size();
  VkBuffer staging_buffer;
  VkDeviceMemory staging_buffer_memory;
  CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               staging_buffer, staging_buffer_memory);

  void* data;
  vkMapMemory(logical_device_, staging_buffer_memory, 0, buffer_size, 0, &data);
  memcpy(data, vertices.data(), buffer_size);
  vkUnmapMemory(logical_device_, staging_buffer_memory);

  VkBufferUsageFlags vertex_usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  if (rt_supported_) {
    vertex_usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                   VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
  }
  CreateBuffer(
      buffer_size, vertex_usage,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memoryBuffer->buffer_handle_,
      memoryBuffer->memory_handle_);

  CopyBuffer(staging_buffer, memoryBuffer->buffer_handle_, buffer_size);

  if (rt_supported_) {
    VkBufferDeviceAddressInfo addrInfo{};
    addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addrInfo.buffer = memoryBuffer->buffer_handle_;
    memoryBuffer->device_address_ = vkGetBufferDeviceAddress(logical_device_, &addrInfo);
  }

  vkDestroyBuffer(logical_device_, staging_buffer, nullptr);
  vkFreeMemory(logical_device_, staging_buffer_memory, nullptr);
  return memoryBuffer;
}

template Ref<MemoryBuffer> Renderer::CreateVertexBuffer<Vertex3D>(
    std::vector<Vertex3D>);

template Ref<MemoryBuffer> Renderer::CreateVertexBuffer<Vertex2DNoColor>(
    std::vector<Vertex2DNoColor>);

template Ref<MemoryBuffer> Renderer::CreateVertexBuffer<VertexSprite>(
    std::vector<VertexSprite>);

template Ref<MemoryBuffer> Renderer::CreateVertexBuffer<glm::vec3>(
    std::vector<glm::vec3>);

void Renderer::DestroyVertexBuffer(MemoryBuffer& buffer) {
  vkDestroyBuffer(logical_device_, buffer.buffer_handle_, nullptr);
  vkFreeMemory(logical_device_, buffer.memory_handle_, nullptr);
}

Ref<IndexBuffer> Renderer::CreateIndexBuffer(std::vector<Index> indices) {
  Ref<IndexBuffer> memoryBuffer = CreateReference<IndexBuffer>();

  static_assert(sizeof(Index) == sizeof(uint32_t));
  memoryBuffer->index_type_ = VK_INDEX_TYPE_UINT32;
  memoryBuffer->size_ = indices.size();
  VkDeviceSize buffer_size = sizeof(indices[0]) * indices.size();

  VkBuffer staging_buffer;
  VkDeviceMemory staging_buffer_memory;
  CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               staging_buffer, staging_buffer_memory);

  void* data;
  vkMapMemory(logical_device_, staging_buffer_memory, 0, buffer_size, 0, &data);
  memcpy(data, indices.data(), (size_t)buffer_size);
  vkUnmapMemory(logical_device_, staging_buffer_memory);

  VkBufferUsageFlags index_usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  if (rt_supported_) {
    index_usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
  }
  CreateBuffer(
      buffer_size, index_usage,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memoryBuffer->buffer_handle_,
      memoryBuffer->memory_handle_);

  CopyBuffer(staging_buffer, memoryBuffer->buffer_handle_, buffer_size);

  if (rt_supported_) {
    VkBufferDeviceAddressInfo addrInfo{};
    addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addrInfo.buffer = memoryBuffer->buffer_handle_;
    memoryBuffer->device_address_ = vkGetBufferDeviceAddress(logical_device_, &addrInfo);
  }

  vkDestroyBuffer(logical_device_, staging_buffer, nullptr);
  vkFreeMemory(logical_device_, staging_buffer_memory, nullptr);

  return memoryBuffer;
}

Ref<UniformBuffer> Renderer::CreateUniformBuffer(VkDeviceSize size) {
  Ref<UniformBuffer> uniformBuffer = CreateReference<UniformBuffer>();

  uniformBuffer->data_ = malloc(size);
  uniformBuffer->size_ = size;
  // TODO not use host coherent memory, use staging buffer and copy when it changes
  // like how I did in GlistEngine
  // This is slow af
  CreateBuffer(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               uniformBuffer->buffer_handle_, uniformBuffer->memory_handle_);

  WIESEL_CHECK_VKRESULT(vkMapMemory(logical_device_,
                                    uniformBuffer->memory_handle_, 0, size, 0,
                                    &uniformBuffer->data_));

  memset(uniformBuffer->data_, 0, size);

  return uniformBuffer;
}

Ref<UniformBuffer> Renderer::CreateStorageBuffer(VkDeviceSize size) {
  Ref<UniformBuffer> buffer = CreateReference<UniformBuffer>();
  buffer->data_ = malloc(size);
  buffer->size_ = size;
  CreateBuffer(size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               buffer->buffer_handle_, buffer->memory_handle_);
  WIESEL_CHECK_VKRESULT(vkMapMemory(logical_device_,
                                    buffer->memory_handle_, 0, size, 0,
                                    &buffer->data_));
  memset(buffer->data_, 0, size);
  return buffer;
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
  // Deprecated: resource setup is now handled by RenderFeature::SetupResources.
  // This method just ensures aspect ratio is set and marks the component
  // as dirty so the pipeline-based setup runs on next Render().
  LOG_INFO("  Viewport: {}x{}", component.viewport_size.x,
           component.viewport_size.y);
  if (component.aspect_ratio <= 0) {
    component.aspect_ratio =
        component.viewport_size.x / component.viewport_size.y;
  }
  component.resources_dirty = true;
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
  VkBuffer staging_buffer;
  VkDeviceMemory staging_buffer_memory;
  CreateBuffer(texture->size_, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               staging_buffer, staging_buffer_memory);

  void* data;
  vkMapMemory(logical_device_, staging_buffer_memory, 0, texture->size_, 0,
              &data);
  memcpy(data, pixels, static_cast<size_t>(texture->size_));
  vkUnmapMemory(logical_device_, staging_buffer_memory);

  CreateImage(texture->width_, texture->height_, texture->mip_levels_,
              SamplingMode::DISABLED, format, VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                  VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture->image_,
              texture->device_memory_);

  TransitionImageLayout(texture->image_, format, VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        texture->mip_levels_);
  CopyBufferToImage(staging_buffer, texture->image_,
                    static_cast<uint32_t>(texture->width_),
                    static_cast<uint32_t>(texture->height_));

  vkDestroyBuffer(logical_device_, staging_buffer, nullptr);
  vkFreeMemory(logical_device_, staging_buffer_memory, nullptr);

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
  VkBuffer staging_buffer;
  VkDeviceMemory staging_buffer_memory;
  CreateBuffer(texture->size_, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               staging_buffer, staging_buffer_memory);

  void* data;
  vkMapMemory(logical_device_, staging_buffer_memory, 0, texture->size_, 0,
              &data);
  memcpy(data, pixels, static_cast<size_t>(texture->size_));
  vkUnmapMemory(logical_device_, staging_buffer_memory);

  CreateImage(texture->width_, texture->height_, texture->mip_levels_,
              SamplingMode::DISABLED, format, VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                  VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture->image_,
              texture->device_memory_);

  {
    VkCommandBuffer cmd = BeginSingleTimeCommands();
    TransitionImageLayout(texture->image_, format, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          texture->mip_levels_, cmd, 0, 1);
    CopyBufferToImage(cmd, staging_buffer, texture->image_,
                      static_cast<uint32_t>(texture->width_),
                      static_cast<uint32_t>(texture->height_));
    GenerateMipmaps(cmd, texture->image_, VK_FORMAT_R8G8B8A8_UNORM,
                    texture->width_, texture->height_, texture->mip_levels_);
    EndSingleTimeCommands(cmd);
  }

  vkDestroyBuffer(logical_device_, staging_buffer, nullptr);
  vkFreeMemory(logical_device_, staging_buffer_memory, nullptr);

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

  VfsFile vfs_file = Engine::vfs()->Open(path);
  stbi_uc* pixels =
      stbi_load_from_memory(vfs_file.Data(), static_cast<int>(vfs_file.Size()),
                            reinterpret_cast<int*>(&texture->width_),
                            reinterpret_cast<int*>(&texture->height_),
                            &texture->channels_, STBI_rgb_alpha);

  if (!pixels) {
    LOG_WARN("Failed to load texture from {}", path);
    return nullptr;
  }
  texture->size_ = texture->width_ * texture->height_ * STBI_rgb_alpha;
  if (texture_props.generate_mipmaps) {
    texture->mip_levels_ = static_cast<uint32_t>(std::floor(std::log2(
                               std::max(texture->width_, texture->height_)))) +
                           1;
  } else {
    texture->mip_levels_ = 1;
  }

  VkBuffer staging_buffer;
  VkDeviceMemory staging_buffer_memory;
  CreateBuffer(texture->size_, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               staging_buffer, staging_buffer_memory);

  void* data;
  vkMapMemory(logical_device_, staging_buffer_memory, 0, texture->size_, 0,
              &data);
  memcpy(data, pixels, static_cast<size_t>(texture->size_));
  vkUnmapMemory(logical_device_, staging_buffer_memory);

  stbi_image_free(pixels);

  CreateImage(texture->width_, texture->height_, texture->mip_levels_,
              SamplingMode::DISABLED, texture_props.image_format,
              VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                  VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture->image_,
              texture->device_memory_);

  {
    VkCommandBuffer cmd = BeginSingleTimeCommands();
    TransitionImageLayout(
        texture->image_, texture_props.image_format, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, texture->mip_levels_, cmd, 0, 1);
    CopyBufferToImage(cmd, staging_buffer, texture->image_,
                      static_cast<uint32_t>(texture->width_),
                      static_cast<uint32_t>(texture->height_));
    GenerateMipmaps(cmd, texture->image_, texture_props.image_format,
                    texture->width_, texture->height_, texture->mip_levels_);
    EndSingleTimeCommands(cmd);
  }

  vkDestroyBuffer(logical_device_, staging_buffer, nullptr);
  vkFreeMemory(logical_device_, staging_buffer_memory, nullptr);

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

  VkBuffer staging_buffer;
  VkDeviceMemory staging_buffer_memory;
  CreateBuffer(texture->size_, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               staging_buffer, staging_buffer_memory);

  void* data;
  vkMapMemory(logical_device_, staging_buffer_memory, 0, texture->size_, 0,
              &data);
  memcpy(data, buffer, static_cast<size_t>(texture->size_));
  vkUnmapMemory(logical_device_, staging_buffer_memory);

  CreateImage(texture->width_, texture->height_, texture->mip_levels_,
              SamplingMode::DISABLED, texture_props.image_format,
              VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                  VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture->image_,
              texture->device_memory_);

  {
    VkCommandBuffer cmd = BeginSingleTimeCommands();
    TransitionImageLayout(
        texture->image_, texture_props.image_format, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, texture->mip_levels_, cmd, 0, 1);
    CopyBufferToImage(cmd, staging_buffer, texture->image_, texture->width_,
                      texture->height_);
    GenerateMipmaps(cmd, texture->image_, texture_props.image_format,
                    texture->width_, texture->height_, texture->mip_levels_);
    EndSingleTimeCommands(cmd);
  }

  vkDestroyBuffer(logical_device_, staging_buffer, nullptr);
  vkFreeMemory(logical_device_, staging_buffer_memory, nullptr);

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
    VfsFile vfs_face = Engine::vfs()->Open(paths[i]);
    stbi_uc* pixels = stbi_load_from_memory(vfs_face.Data(),
                                            static_cast<int>(vfs_face.Size()),
                                            &w, &h, &channels, STBI_rgb_alpha);
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
  VkBuffer staging_buffer;
  VkDeviceMemory staging_buffer_memory;
  CreateBuffer(totalSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               staging_buffer, staging_buffer_memory);

  void* data;
  vkMapMemory(logical_device_, staging_buffer_memory, 0, totalSize, 0, &data);
  memcpy(data, allPixels, static_cast<size_t>(totalSize));
  vkUnmapMemory(logical_device_, staging_buffer_memory);

  CreateImage(texture->width_, texture->height_, texture->mip_levels_,
              SamplingMode::DISABLED, texture_props.image_format,
              VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                  VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture->image_,
              texture->device_memory_, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, 6);

  {
    VkCommandBuffer cmd = BeginSingleTimeCommands();
    for (uint32_t layer = 0; layer < 6; layer++) {
      TransitionImageLayout(
          texture->image_, texture_props.image_format, VK_IMAGE_LAYOUT_UNDEFINED,
          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, texture->mip_levels_, cmd,
          layer, 1);
      CopyBufferToImage(
          cmd, staging_buffer, texture->image_,
          static_cast<uint32_t>(texture->width_),
          static_cast<uint32_t>(texture->height_), texture->size_ * layer,
          layer);
    }
    for (uint32_t layer = 0; layer < 6; layer++) {
      TransitionImageLayout(texture->image_, texture_props.image_format,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            texture->mip_levels_, cmd, layer, 1);
    }
    EndSingleTimeCommands(cmd);
  }

  vkDestroyBuffer(logical_device_, staging_buffer, nullptr);
  vkFreeMemory(logical_device_, staging_buffer_memory, nullptr);
  delete[] allPixels;

  texture->sampler_ = CreateTextureSampler(texture->mip_levels_, sampler_props);
  texture->image_view_ = CreateImageView(
      texture->image_, texture_props.image_format, VK_IMAGE_ASPECT_COLOR_BIT,
      texture->mip_levels_, VK_IMAGE_VIEW_TYPE_CUBE, 0, 6);
  texture->is_allocated_ = true;
  return texture;
}

Ref<Texture> Renderer::CreateCubemapTextureFromSingle(
    const std::string& virtual_path, const TextureProps& texture_props,
    const SamplerProps& sampler_props) {
  Ref<Texture> texture = CreateReference<Texture>(texture_props.type, "");

  int w, h, channels;
  VfsFile vfs_cubemap = Engine::vfs()->Open(virtual_path);
  stbi_uc* pixels = stbi_load_from_memory(vfs_cubemap.Data(),
                                          static_cast<int>(vfs_cubemap.Size()),
                                          &w, &h, &channels, STBI_rgb_alpha);
  if (!pixels) {
    throw std::runtime_error("failed to load cubemap: " + virtual_path);
  }
  const bool is_horizontal_strip = (w % 6 == 0) && (h > 0) && (h == w / 6);

  const bool is_vertical_cross =
      (w % 4 == 0) && (h % 3 == 0) && (w / 4 == h / 3);

  if (!is_horizontal_strip && !is_vertical_cross) {
    throw std::runtime_error("Unsupported cubemap layout: " + virtual_path);
  }

  uint32_t faceSize = 0;

  if (is_horizontal_strip)
    faceSize = w / 6;
  else
    faceSize = w / 4;

  texture->width_ = faceSize;
  texture->height_ = faceSize;
  texture->size_ = faceSize * faceSize * 4;
  texture->mip_levels_ = 1;

  VkDeviceSize totalSize = texture->size_ * 6;

  std::vector<stbi_uc> cubePixels(totalSize);

  auto copyFace = [&](uint32_t faceIndex, uint32_t gridX, uint32_t gridY) {
    for (uint32_t row = 0; row < faceSize; row++) {
      memcpy(
          cubePixels.data() + faceIndex * texture->size_ + row * faceSize * 4,
          pixels + ((gridY * faceSize + row) * w + gridX * faceSize) * 4,
          faceSize * 4);
    }
  };

  if (is_horizontal_strip) {
    for (uint32_t face = 0; face < 6; face++) {
      copyFace(face, face, 0);
    }
  } else {
    // Vertical cross (4x3 fold layout)
    // Grid layout:
    //       +Y        (1,0)
    // -X +Z +X -Z     (0,1)(1,1)(2,1)(3,1)
    //       -Y        (1,2)

    copyFace(0, 2, 1);  // +X
    copyFace(1, 0, 1);  // -X
    copyFace(2, 1, 0);  // +Y
    copyFace(3, 1, 2);  // -Y
    copyFace(4, 1, 1);  // +Z
    copyFace(5, 3, 1);  // -Z
  }

  stbi_image_free(pixels);

  // Create staging buffer
  VkBuffer staging_buffer;
  VkDeviceMemory stagingMemory;

  CreateBuffer(totalSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               staging_buffer, stagingMemory);

  void* data;
  vkMapMemory(logical_device_, stagingMemory, 0, totalSize, 0, &data);
  memcpy(data, cubePixels.data(), totalSize);
  vkUnmapMemory(logical_device_, stagingMemory);

  // Create cube image
  CreateImage(faceSize, faceSize, texture->mip_levels_, SamplingMode::DISABLED,
              texture_props.image_format, VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture->image_,
              texture->device_memory_, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, 6);

  // Transition entire cube
  TransitionImageLayout(
      texture->image_, texture_props.image_format, VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, texture->mip_levels_, 0, 6);

  // Copy all 6 faces at once
  CopyBufferToImage(staging_buffer, texture->image_, faceSize, faceSize, 0, 0,
                    6);

  TransitionImageLayout(texture->image_, texture_props.image_format,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        texture->mip_levels_, 0, 6);

  vkDestroyBuffer(logical_device_, staging_buffer, nullptr);
  vkFreeMemory(logical_device_, stagingMemory, nullptr);

  texture->sampler_ = CreateTextureSampler(texture->mip_levels_, sampler_props);

  texture->image_view_ = CreateImageView(
      texture->image_, texture_props.image_format, VK_IMAGE_ASPECT_COLOR_BIT,
      texture->mip_levels_, VK_IMAGE_VIEW_TYPE_CUBE, 0, 6);

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
  if (props.storage) {
    usage |= VK_IMAGE_USAGE_STORAGE_BIT;
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
  VkBuffer staging_buffer;
  VkDeviceMemory staging_buffer_memory;
  size_t size = texture->width_ * texture->height_ * sizePerPixel;
  CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               staging_buffer, staging_buffer_memory);

  void* data;
  vkMapMemory(logical_device_, staging_buffer_memory, 0, size, 0, &data);
  memcpy(data, buffer, static_cast<size_t>(size));
  vkUnmapMemory(logical_device_, staging_buffer_memory);

  TransitionImageLayout(texture->images_[0], texture->format_,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1);

  CopyBufferToImage(staging_buffer, texture->images_[0],
                    static_cast<uint32_t>(texture->width_),
                    static_cast<uint32_t>(texture->height_));

  TransitionImageLayout(texture->images_[0], texture->format_,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1);

  vkDestroyBuffer(logical_device_, staging_buffer, nullptr);
  vkFreeMemory(logical_device_, staging_buffer_memory, nullptr);
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
  samplerInfo.compareEnable = props.CompareEnable;
  samplerInfo.compareOp = props.CompareOp;

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

  object->allocated_ = true;
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

  object->allocated_ = true;
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
    auto shadow_view = camera.resource_pool.GetImageView("ShadowDepthViewArray");
    if (shadow_view == nullptr) {
      imageInfo.imageView = blank_texture_->image_view_->handle_;
      imageInfo.sampler = blank_texture_->sampler_;
    } else {
      imageInfo.imageView = shadow_view->handle_;
      imageInfo.sampler = shadow_sampler_->sampler_;
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

  object->allocated_ = true;
  return object;
}

Ref<DescriptorSet> Renderer::CreateBoneDescriptors(Ref<UniformBuffer> bone_ubo) {
  auto desc = CreateReference<DescriptorSet>();
  desc->SetLayout(bone_descriptor_layout_);
  desc->AddUniformBuffer(0, bone_ubo);
  desc->Bake();
  return desc;
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

  object->allocated_ = true;
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

  object->allocated_ = true;
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

void Renderer::WaitForGPU() {
  if (!initialized_) {
    return;
  }
  vkDeviceWaitIdle(logical_device_);
  deletion_queue_.FlushAll();
}

void Renderer::Cleanup() {
  if (!initialized_) {
    return;
  }

  WaitForGPU();
  LOG_DEBUG("Destroying Renderer");

  camera_ = nullptr;
  quad_index_buffer_ = nullptr;
  quad_vertex_buffer_ = nullptr;

  CleanupGlobalUniformBuffers();
  blank_texture_ = nullptr;
  ssao_noise_ = nullptr;
  pick_entity_id_image_ = nullptr;

  if (pick_staging_buffer_ != VK_NULL_HANDLE) {
    vkDestroyBuffer(logical_device_, pick_staging_buffer_, nullptr);
    vkFreeMemory(logical_device_, pick_staging_memory_, nullptr);
    pick_staging_buffer_ = VK_NULL_HANDLE;
    pick_staging_memory_ = VK_NULL_HANDLE;
  }

  LOG_DEBUG("Destroying graphics");
  CleanupPresentGraphics();

  LOG_DEBUG("Destroying descriptor set layout");
  CleanupDescriptorLayouts();

  LOG_DEBUG("Destroying semaphores and fences");
  for (uint32_t i = 0; i < kMaxFramesInFlight; i++) {
    vkDestroySemaphore(logical_device_, render_finished_semaphores_[i], nullptr);
    vkDestroySemaphore(logical_device_, image_available_semaphores_[i], nullptr);
    vkDestroySemaphore(logical_device_, render_order_semaphores_[i], nullptr);
    vkDestroyFence(logical_device_, fences_[i], nullptr);
  }

  LOG_DEBUG("Destroying acceleration structures");
  as_manager_ = nullptr;

  LOG_DEBUG("Destroying command pool");
  command_buffers_.clear();
  command_pool_ = nullptr;

  LOG_DEBUG("Destroying Tracy Vulkan context");
  TracyVkDestroy(tracy_ctx_);
  tracy_ctx_ = nullptr;

  // Flush any items added to the deletion queue during cleanup
  // (e.g. descriptors deferred by CameraResourcePool::SetDescriptor).
  deletion_queue_.FlushAll();

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
  appInfo.apiVersion = VK_API_VERSION_1_2;

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

  VkPhysicalDeviceFeatures2 deviceFeatures2{};
  deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  deviceFeatures2.features.fillModeNonSolid = true;
  deviceFeatures2.features.samplerAnisotropy = VK_TRUE;

  VkPhysicalDeviceVulkan12Features vulkan12Features{};
  vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
  vulkan12Features.bufferDeviceAddress = VK_TRUE;
  deviceFeatures2.pNext = &vulkan12Features;

  VkPhysicalDeviceVulkan11Features vulkan11Features{};
  vulkan11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
  vulkan12Features.pNext = &vulkan11Features;

  // RT feature structs (chained only when RT is supported)
  VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures{};
  asFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
  asFeatures.accelerationStructure = VK_TRUE;

  VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeatures{};
  rtPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
  rtPipelineFeatures.rayTracingPipeline = VK_TRUE;

  if (rt_supported_) {
    vulkan11Features.pNext = &asFeatures;
    asFeatures.pNext = &rtPipelineFeatures;

    // Query RT properties for SBT alignment
    rt_pipeline_properties_.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
    rt_as_properties_.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
    rt_pipeline_properties_.pNext = &rt_as_properties_;

    VkPhysicalDeviceProperties2 props2{};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &rt_pipeline_properties_;
    vkGetPhysicalDeviceProperties2(physical_device_, &props2);
  }

  VkDeviceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  createInfo.queueCreateInfoCount =
      static_cast<uint32_t>(queueCreateInfos.size());
  createInfo.pQueueCreateInfos = queueCreateInfos.data();
  createInfo.pEnabledFeatures = nullptr;  // Using pNext chain instead
  createInfo.pNext = &deviceFeatures2;
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

  if (rt_supported_) {
    pfn_vkCreateAccelerationStructureKHR_ =
        (PFN_vkCreateAccelerationStructureKHR)vkGetDeviceProcAddr(
            logical_device_, "vkCreateAccelerationStructureKHR");
    pfn_vkDestroyAccelerationStructureKHR_ =
        (PFN_vkDestroyAccelerationStructureKHR)vkGetDeviceProcAddr(
            logical_device_, "vkDestroyAccelerationStructureKHR");
    pfn_vkGetAccelerationStructureBuildSizesKHR_ =
        (PFN_vkGetAccelerationStructureBuildSizesKHR)vkGetDeviceProcAddr(
            logical_device_, "vkGetAccelerationStructureBuildSizesKHR");
    pfn_vkCmdBuildAccelerationStructuresKHR_ =
        (PFN_vkCmdBuildAccelerationStructuresKHR)vkGetDeviceProcAddr(
            logical_device_, "vkCmdBuildAccelerationStructuresKHR");
    pfn_vkGetAccelerationStructureDeviceAddressKHR_ =
        (PFN_vkGetAccelerationStructureDeviceAddressKHR)vkGetDeviceProcAddr(
            logical_device_, "vkGetAccelerationStructureDeviceAddressKHR");
    pfn_vkCreateRayTracingPipelinesKHR_ =
        (PFN_vkCreateRayTracingPipelinesKHR)vkGetDeviceProcAddr(
            logical_device_, "vkCreateRayTracingPipelinesKHR");
    pfn_vkGetRayTracingShaderGroupHandlesKHR_ =
        (PFN_vkGetRayTracingShaderGroupHandlesKHR)vkGetDeviceProcAddr(
            logical_device_, "vkGetRayTracingShaderGroupHandlesKHR");
    pfn_vkCmdTraceRaysKHR_ =
        (PFN_vkCmdTraceRaysKHR)vkGetDeviceProcAddr(
            logical_device_, "vkCmdTraceRaysKHR");
    LOG_INFO("Ray tracing function pointers loaded");
  }
}

bool Renderer::CheckRayTracingSupport(VkPhysicalDevice device) {
  uint32_t extensionCount;
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
  std::vector<VkExtensionProperties> extensions(extensionCount);
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data());

  bool has_as = false, has_rt_pipeline = false, has_deferred = false;
  for (const auto& ext : extensions) {
    if (strcmp(ext.extensionName, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) == 0) has_as = true;
    if (strcmp(ext.extensionName, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME) == 0) has_rt_pipeline = true;
    if (strcmp(ext.extensionName, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME) == 0) has_deferred = true;
  }
  return has_as && has_rt_pipeline && has_deferred;
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
  taa_descriptor_layout_->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                     VK_SHADER_STAGE_FRAGMENT_BIT);
  taa_descriptor_layout_->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                     VK_SHADER_STAGE_FRAGMENT_BIT);
  taa_descriptor_layout_->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                     VK_SHADER_STAGE_FRAGMENT_BIT);
  taa_descriptor_layout_->Bake();

  // Bone matrices UBO layout (set 2 for geometry and shadow passes)
  bone_descriptor_layout_ = CreateReference<DescriptorSetLayout>();
  bone_descriptor_layout_->AddBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                      VK_SHADER_STAGE_VERTEX_BIT);
  bone_descriptor_layout_->Bake();
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
  stats_.swap_chain_images = imageCount;
  stats_.frames_in_flight = kMaxFramesInFlight;

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

void Renderer::CreatePresentGraphicsPipelines() {
  auto present_vertex_shader = CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "/engine/internal_shaders/fullscreen_shader.vert"});
  auto present_fragment_shader = CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "/engine/internal_shaders/quad_shader.frag"});
  present_pipeline_ = CreateReference<Pipeline>(
      PipelineProperties{options_.msaa_mode, CullModeNone, false, true});
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

  VkMemoryAllocateFlagsInfo allocFlagsInfo{};
  if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
    allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
    allocInfo.pNext = &allocFlagsInfo;
  }

  WIESEL_CHECK_VKRESULT(
      vkAllocateMemory(logical_device_, &allocInfo, nullptr, &bufferMemory));
  WIESEL_CHECK_VKRESULT(
      vkBindBufferMemory(logical_device_, buffer, bufferMemory, 0));
}

void Renderer::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer,
                          VkDeviceSize size) {
  PROFILE_ZONE_SCOPED();
  VkCommandBuffer commandBuffer = BeginSingleTimeCommands();
  CopyBuffer(commandBuffer, srcBuffer, dstBuffer, size);
  EndSingleTimeCommands(commandBuffer);
}

void Renderer::CopyBuffer(VkCommandBuffer cmd, VkBuffer srcBuffer,
                           VkBuffer dstBuffer, VkDeviceSize size) {
  VkBufferCopy copyRegion{};
  copyRegion.size = size;
  vkCmdCopyBuffer(cmd, srcBuffer, dstBuffer, 1, &copyRegion);
}

void Renderer::CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width,
                                 uint32_t height, VkDeviceSize base_offset,
                                 uint32_t layer, uint32_t layer_count) {
  PROFILE_ZONE_SCOPED();
  VkCommandBuffer commandBuffer = BeginSingleTimeCommands();
  CopyBufferToImage(commandBuffer, buffer, image, width, height, base_offset,
                    layer, layer_count);
  EndSingleTimeCommands(commandBuffer);
}

void Renderer::CopyBufferToImage(VkCommandBuffer cmd, VkBuffer buffer,
                                 VkImage image, uint32_t width, uint32_t height,
                                 VkDeviceSize base_offset, uint32_t layer,
                                 uint32_t layer_count) {
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

  vkCmdCopyBufferToImage(cmd, buffer, image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
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
  command_buffers_.resize(kMaxFramesInFlight);
  for (uint32_t i = 0; i < kMaxFramesInFlight; i++) {
    command_buffers_[i] = command_pool_->CreateBuffer();
  }
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

  quad_index_buffer_ = Engine::renderer()->CreateIndexBuffer(quadIndices);

  quad_vertex_buffer_ = Engine::renderer()->CreateVertexBuffer(quadVertices);

  default_linear_sampler_ = CreateReference<Sampler>(1, SamplerProps{});
  default_nearest_sampler_ = CreateReference<Sampler>(
      1, SamplerProps{VK_FILTER_NEAREST, VK_FILTER_NEAREST, -1.0f});
  shadow_sampler_ = CreateReference<Sampler>(1, SamplerProps{
      VK_FILTER_LINEAR, VK_FILTER_LINEAR, -1.0f,
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
      VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
      VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL});

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

  // Entity pick staging buffer (4 bytes for one float)
  CreateBuffer(sizeof(float),
               VK_BUFFER_USAGE_TRANSFER_DST_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               pick_staging_buffer_, pick_staging_memory_);

  // Identity bone UBO (shared by all static / non-animated models)
  identity_bone_ubo_ = CreateUniformBuffer(sizeof(BoneMatricesUniformData));
  {
    BoneMatricesUniformData identity{};
    for (auto& m : identity.bone_matrices) {
      m = glm::mat4(1.0f);
    }
    memcpy(identity_bone_ubo_->data_, &identity, sizeof(BoneMatricesUniformData));
  }
  identity_bone_descriptor_ = CreateBoneDescriptors(identity_bone_ubo_);
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

void Renderer::GenerateMipmaps(VkCommandBuffer cmd, VkImage image,
                               VkFormat imageFormat, int32_t texWidth,
                               int32_t texHeight, uint32_t mipLevels) {
  VkFormatProperties formatProperties;
  vkGetPhysicalDeviceFormatProperties(physical_device_, imageFormat,
                                      &formatProperties);
  if (!(formatProperties.optimalTilingFeatures &
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
    throw std::runtime_error(
        "texture image format does not support linear blitting!");
  }

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

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
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

    vkCmdBlitImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                   VK_FILTER_LINEAR);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
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

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);
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
      command_buffers_[current_frame_]->handle_,
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

  image_available_semaphores_.resize(kMaxFramesInFlight);
  render_finished_semaphores_.resize(kMaxFramesInFlight);
  render_order_semaphores_.resize(kMaxFramesInFlight);
  fences_.resize(kMaxFramesInFlight);

  for (uint32_t i = 0; i < kMaxFramesInFlight; i++) {
    WIESEL_CHECK_VKRESULT(vkCreateSemaphore(
        logical_device_, &semaphoreInfo, nullptr, &image_available_semaphores_[i]));
    WIESEL_CHECK_VKRESULT(vkCreateSemaphore(
        logical_device_, &semaphoreInfo, nullptr, &render_finished_semaphores_[i]));
    WIESEL_CHECK_VKRESULT(vkCreateSemaphore(
        logical_device_, &semaphoreInfo, nullptr, &render_order_semaphores_[i]));
    WIESEL_CHECK_VKRESULT(
        vkCreateFence(logical_device_, &fenceInfo, nullptr, &fences_[i]));
  }
}

void Renderer::CleanupDescriptorLayouts() {
  geometry_mesh_descriptor_layout_ = nullptr;
  shadow_mesh_descriptor_layout_ = nullptr;
  global_descriptor_layout_ = nullptr;
  global_shadow_descriptor_layout_ = nullptr;
  ssao_gen_descriptor_layout_ = nullptr;
  ssao_blur_descriptor_layout_ = nullptr;
  ssao_output_descriptor_layout_ = nullptr;
  geometry_output_descriptor_layout_ = nullptr;
  sprite_draw_descriptor_layout_ = nullptr;
  skybox_descriptor_layout_ = nullptr;
  postprocess_2input_descriptor_layout_ = nullptr;
  taa_descriptor_layout_ = nullptr;
  bone_descriptor_layout_ = nullptr;
  present_descriptor_layout_ = nullptr;
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
  shadow_camera_uniform_buffer_ = nullptr;
  ssao_kernel_uniform_buffer_ = nullptr;
  identity_bone_ubo_ = nullptr;
  identity_bone_descriptor_ = nullptr;
  default_linear_sampler_ = nullptr;
  default_nearest_sampler_ = nullptr;
  shadow_sampler_ = nullptr;
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
  CreateSwapChain();
  CreatePresentGraphicsPipelines();

  // Notify that pipelines were recreated so cameras can recreate their resources
  PipelineRecreatedEvent event{};
  Engine::window()->GetEventHandler()(event);
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
  vkCmdSetViewport(command_buffers_[current_frame_]->handle_, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = extent;
  vkCmdSetScissor(command_buffers_[current_frame_]->handle_, 0, 1, &scissor);
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
  vkCmdSetViewport(command_buffers_[current_frame_]->handle_, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent.width = extent.x;
  scissor.extent.height = extent.y;
  vkCmdSetScissor(command_buffers_[current_frame_]->handle_, 0, 1, &scissor);
}

void Renderer::BeginRender() {
  PROFILE_ZONE_SCOPED();
  stats_.Reset();

  // Wait for this frame slot's previous work to complete
  vkWaitForFences(logical_device_, 1, &fences_[current_frame_], VK_TRUE, UINT64_MAX);
  vkResetFences(logical_device_, 1, &fences_[current_frame_]);

  command_buffers_[current_frame_]->Reset();
  command_buffers_[current_frame_]->Begin();

  // Reloading stuff
  if (recreate_swap_chain_) {
    PROFILE_ZONE_SCOPED_N("Renderer::BeginRender: Recreate swap chain");
    RecreateSwapChain();
    recreate_swap_chain_ = false;
    recreate_pipeline_ = false;  // Already handled in RecreateSwapChain
  }
  if (recreate_pipeline_) {
    PROFILE_ZONE_SCOPED_N("Renderer::BeginRender: Recreate Pipeline");
    // Pipelines are now owned by features. Mark camera resources dirty
    // so features recreate their pipelines with updated options.
    recreate_pipeline_ = false;
    PipelineRecreatedEvent event{};
    Engine::window()->GetEventHandler()(event);
  }
}

bool Renderer::BeginPresent() {
  PROFILE_ZONE_SCOPED();
  VkResult result = vkAcquireNextImageKHR(
      logical_device_, swap_chain_, UINT64_MAX, image_available_semaphores_[current_frame_],
      VK_NULL_HANDLE, &image_index_);
  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    LOG_INFO(
        "Received VK_ERROR_OUT_OF_DATE_KHR, trying to recreate swap chain.");
    recreate_swap_chain_ = true;
    // End the command buffer and submit empty work so the fence and
    // render_finished semaphore get signaled (prevents next frame deadlock).
    command_buffers_[current_frame_]->End();
    VkSubmitInfo empty_submit{};
    empty_submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    empty_submit.commandBufferCount = 1;
    empty_submit.pCommandBuffers = &command_buffers_[current_frame_]->handle_;
    VkSemaphore empty_signal[] = {
        render_finished_semaphores_[current_frame_],
        render_order_semaphores_[current_frame_]};
    empty_submit.signalSemaphoreCount = 2;
    empty_submit.pSignalSemaphores = empty_signal;
    {
      std::lock_guard<std::mutex> lock(queue_submit_mutex_);
      vkQueueSubmit(graphics_queue_, 1, &empty_submit, fences_[current_frame_]);
    }
    frame_counter_++;
    current_frame_ = (current_frame_ + 1) % kMaxFramesInFlight;
    return false;
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    throw std::runtime_error("failed to acquire swap chain image!");
  }

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
                            command_buffers_[current_frame_]->handle_, 0, 1);
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
                            command_buffers_[current_frame_]->handle_, 0, 1);
    }
  }
  /*
  for (const auto& item : textures) {
    TransitionImageLayout(item->m_Images[0], item->m_Format,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1,
                          m_CommandBuffer->m_Handle);
  }*/

  PROFILE_GPU_COLLECT(tracy_ctx_, command_buffers_[current_frame_]->handle_);
  command_buffers_[current_frame_]->End();

  // Presentation
  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &command_buffers_[current_frame_]->handle_;

  // Wait on image availability AND the previous frame's render order semaphore.
  // The latter serializes GPU execution across frames so shared intermediate
  // render targets don't have layout conflicts.
  uint32_t prev_frame = (current_frame_ + kMaxFramesInFlight - 1) % kMaxFramesInFlight;
  VkSemaphore waitSemaphores[] = {
      image_available_semaphores_[current_frame_],
      render_order_semaphores_[prev_frame]};
  VkPipelineStageFlags waitStages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};
  // On the very first frame, no previous frame has signaled the order
  // semaphore yet. Skip the cross-frame wait in that case.
  uint32_t wait_count = frame_counter_ > 0 ? 2 : 1;
  submitInfo.waitSemaphoreCount = wait_count;
  submitInfo.pWaitSemaphores = waitSemaphores;
  submitInfo.pWaitDstStageMask = waitStages;

  // Signal both render_finished (for present) and render_order (for next frame)
  VkSemaphore signalSemaphores[] = {
      render_finished_semaphores_[current_frame_],
      render_order_semaphores_[current_frame_]};
  submitInfo.signalSemaphoreCount = 2;
  submitInfo.pSignalSemaphores = signalSemaphores;

  VkResult result;
  {
    std::lock_guard<std::mutex> lock(queue_submit_mutex_);
    WIESEL_CHECK_VKRESULT(vkQueueSubmit(graphics_queue_, 1, &submitInfo, fences_[current_frame_]));
  }

  VkPresentInfoKHR presentInfo{};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = signalSemaphores;

  VkSwapchainKHR swapChains[] = {swap_chain_};
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = swapChains;

  presentInfo.pImageIndices = &image_index_;
  presentInfo.pResults = nullptr;  // Optional

  {
    std::lock_guard<std::mutex> lock(queue_submit_mutex_);
    result = vkQueuePresentKHR(present_queue_, &presentInfo);
  }
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    RecreateSwapChain();
  } else if (result != VK_SUCCESS) {
    throw std::runtime_error("failed to present swap chain image!");
  }

  deletion_queue_.Flush();
  frame_counter_++;
  current_frame_ = (current_frame_ + 1) % kMaxFramesInFlight;
}

void Renderer::UpdateUniformData() {
  PROFILE_ZONE_SCOPED();
  // Use vkCmdUpdateBuffer for all UBOs so writes are recorded in the
  // command buffer and safe with multiple frames in flight.
  VkCommandBuffer cmd = command_buffers_[current_frame_]->handle_;
  vkCmdUpdateBuffer(cmd, lights_uniform_buffer_->buffer_handle_, 0,
                    sizeof(lights_uniform_data_), &lights_uniform_data_);
  vkCmdUpdateBuffer(cmd, camera_uniform_buffer_->buffer_handle_, 0,
                    sizeof(camera_uniform_data_), &camera_uniform_data_);
  vkCmdUpdateBuffer(cmd, shadow_camera_uniform_buffer_->buffer_handle_, 0,
                    sizeof(shadow_camera_uniform_data_),
                    &shadow_camera_uniform_data_);

  // Barrier: transfer writes must be visible before shaders read the UBOs.
  VkMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_UNIFORM_READ_BIT;
  vkCmdPipelineBarrier(cmd,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
      0, 1, &barrier, 0, nullptr, 0, nullptr);
}

void Renderer::AllocateModelRenderData(ModelComponent& model,
                                       const Model& model_data) {
  // Create one UBO per entity (shared across all meshes, same transform)
  model.uniform_buffer = CreateUniformBuffer(sizeof(MatricesUniformData));

  // Create per-mesh descriptor sets that bind this entity's UBO + mesh's material
  model.geometry_descriptors.clear();
  model.shadow_descriptors.clear();
  model.geometry_descriptors.reserve(model_data.meshes.size());
  model.shadow_descriptors.reserve(model_data.meshes.size());

  for (const auto& mesh : model_data.meshes) {
    // Apply default texture to meshes that lack a diffuse texture
    if (model.default_texture && mesh->mat && !mesh->mat->base_texture) {
      mesh->mat->base_texture = model.default_texture;
      // Update vertex flags so the shader samples the texture
      for (auto& v : mesh->vertices) {
        v.Flags |= VertexFlagHasTexture;
      }
      // Re-upload vertex data with updated flags
      mesh->Deallocate();
      mesh->Allocate();
    }
    model.geometry_descriptors.push_back(
        CreateMeshDescriptors(model.uniform_buffer, mesh->mat));
    model.shadow_descriptors.push_back(
        CreateShadowMeshDescriptors(model.uniform_buffer, mesh->mat));
  }

  // Bone animation GPU resources
  if (model_data.has_skeleton) {
    // Animated model: per-entity bone UBO, initialized to identity
    model.bone_ubo_ = CreateUniformBuffer(sizeof(BoneMatricesUniformData));
    {
      BoneMatricesUniformData identity{};
      for (auto& m : identity.bone_matrices) {
        m = glm::mat4(1.0f);
      }
      memcpy(model.bone_ubo_->data_, &identity, sizeof(BoneMatricesUniformData));
    }
    model.bone_descriptor_ = CreateBoneDescriptors(model.bone_ubo_);
  } else {
    // Static model: share the identity bone descriptor
    model.bone_ubo_ = nullptr;
    model.bone_descriptor_ = identity_bone_descriptor_;
  }

  // Per-mesh UBOs for node animation (each mesh may have a different node transform)
  model.mesh_uniform_buffers_.clear();
  if (model_data.has_animations && model_data.node_hierarchy.nodes.size() > 1) {
    model.mesh_uniform_buffers_.resize(model_data.meshes.size());
    for (size_t i = 0; i < model_data.meshes.size(); i++) {
      model.mesh_uniform_buffers_[i] =
          CreateUniformBuffer(sizeof(MatricesUniformData));
    }
  }

  model.render_model = model.model_handle;
}

void Renderer::DrawModel(ModelComponent& model,
                         const TransformComponent& transform, bool shadowPass,
                         entt::entity entity_handle) {
  PROFILE_ZONE_SCOPED();
  AssetManager& assets = Engine::asset_manager();
  const Ref<Model>& ptr = assets.GetOrLoad<Model>(model.model_handle);
  if (!ptr) {
    return;
  }

  // Lazily allocate per-entity render data (or re-allocate if model changed)
  if (model.render_model != model.model_handle || !model.uniform_buffer) {
    AllocateModelRenderData(model, *ptr);
  }

  // Upload bone matrices if this entity has a bone UBO
  if (model.bone_ubo_ && model.bone_ubo_->data_) {
    // bone_matrices are written by the animation system each frame
    // (AnimatorComponent → bone_matrices → bone_ubo_)
    // Nothing to do here; the scene's animation update already uploaded them.
  }

  // Determine the bone descriptor to bind
  Ref<DescriptorSet> bone_desc = model.bone_descriptor_
      ? model.bone_descriptor_
      : identity_bone_descriptor_;

  // Update this entity's transform UBO
  MatricesUniformData matrices{};
  matrices.model_matrix = transform.transform_matrix;
  matrices.normal_matrix = transform.normal_matrix;
  if (entity_handle != entt::null) {
    matrices.entity_id = static_cast<float>(static_cast<uint32_t>(entity_handle) + 1);
  }
  memcpy(model.uniform_buffer->data_, &matrices, sizeof(MatricesUniformData));

  stats_.models++;
  for (size_t i = 0; i < ptr->meshes.size(); i++) {
    Ref<DescriptorSet> descriptors = shadowPass ? model.shadow_descriptors[i]
                                                : model.geometry_descriptors[i];
    DrawMesh(ptr->meshes[i], descriptors, bone_desc, shadowPass);
  }
}

void Renderer::DrawMesh(Ref<Mesh> mesh, Ref<DescriptorSet> mesh_descriptors,
                        Ref<DescriptorSet> bone_descriptors, bool shadowPass) {
  PROFILE_ZONE_SCOPED();
  if (!mesh->allocated_) {
    return;
  }

  VkBuffer vertexBuffers[] = {mesh->vertex_buffer->buffer_handle_};
  VkDeviceSize offsets[] = {0};
  static_assert(std::size(vertexBuffers) == std::size(offsets));
  vkCmdBindVertexBuffers(command_buffers_[current_frame_]->handle_, 0, std::size(vertexBuffers),
                         vertexBuffers, offsets);
  vkCmdBindIndexBuffer(command_buffers_[current_frame_]->handle_,
                       mesh->index_buffer->buffer_handle_, 0,
                       mesh->index_buffer->index_type_);

  VkPipelineLayout layout = bound_pipeline_->layout_;

  auto global_desc = shadowPass
      ? camera_->resource_pool->GetDescriptor("ShadowGlobalDescriptor")
      : camera_->resource_pool->GetDescriptor("GlobalDescriptor");
  VkDescriptorSet sets[3] = {mesh_descriptors->descriptor_set_,
                             global_desc->descriptor_set_,
                             bone_descriptors->descriptor_set_};

  vkCmdBindDescriptorSets(command_buffers_[current_frame_]->handle_,
                          VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 3, sets,
                          0, nullptr);

  uint32_t index_count = static_cast<uint32_t>(mesh->indices.size());
  vkCmdDrawIndexed(command_buffers_[current_frame_]->handle_, index_count, 1, 0, 0, 0);
  stats_.draw_calls++;
  stats_.meshes++;
  stats_.vertices += static_cast<uint32_t>(mesh->vertices.size());
  stats_.triangles += index_count / 3;
}

void Renderer::RequestEntityPick(uint32_t x, uint32_t y,
                                 Ref<AttachmentTexture> entity_id_texture) {
  pick_x_ = x;
  pick_y_ = y;
  pick_entity_id_image_ = entity_id_texture;
  pick_pending_ = true;
}

bool Renderer::ExecuteEntityPick(entt::entity& out_entity) {
  out_entity = entt::null;
  if (!pick_pending_ || !pick_entity_id_image_) {
    return false;
  }
  pick_pending_ = false;

  // Clamp to image bounds
  uint32_t w = pick_entity_id_image_->width_;
  uint32_t h = pick_entity_id_image_->height_;
  if (pick_x_ >= w || pick_y_ >= h) {
    pick_entity_id_image_ = nullptr;
    return true;  // Pick executed, but out of bounds → background
  }

  VkImage src_image = pick_entity_id_image_->images_[0];

  VkCommandBuffer cmd = BeginSingleTimeCommands();

  // Transition to transfer src
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = src_image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);

  // Copy 1 pixel
  VkBufferImageCopy region{};
  region.bufferOffset = 0;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = {static_cast<int32_t>(pick_x_),
                        static_cast<int32_t>(pick_y_), 0};
  region.imageExtent = {1, 1, 1};
  vkCmdCopyImageToBuffer(cmd, src_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         pick_staging_buffer_, 1, &region);

  // Transition back
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);

  EndSingleTimeCommands(cmd);

  // Read back the value
  void* data;
  vkMapMemory(logical_device_, pick_staging_memory_, 0, sizeof(float), 0,
              &data);
  float value = *static_cast<float*>(data);
  vkUnmapMemory(logical_device_, pick_staging_memory_);

  pick_entity_id_image_ = nullptr;

  if (value > 0.5f) {
    uint32_t id = static_cast<uint32_t>(value) - 1;
    out_entity = static_cast<entt::entity>(id);
  }
  return true;
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

  Ref<MemoryBuffer> vertexBuffer = Engine::renderer()->GetQuadVertexBuffer();
  VkBuffer buffers[] = {frame.vertex_buffer->buffer_handle_};
  VkDeviceSize offsets[] = {0};
  static_assert(std::size(buffers) == std::size(offsets));
  vkCmdBindVertexBuffers(command_buffers_[current_frame_]->handle_, 0, std::size(buffers),
                         buffers, offsets);

  VkDescriptorSet sets[] = {frame.descriptor->descriptor_set_,
                            camera_->resource_pool->GetDescriptor("GlobalDescriptor")->descriptor_set_};

  vkCmdBindDescriptorSets(
      command_buffers_[current_frame_]->handle_, VK_PIPELINE_BIND_POINT_GRAPHICS,
      bound_pipeline_->layout_, 0, std::size(sets), sets, 0, nullptr);

  vkCmdDraw(command_buffers_[current_frame_]->handle_, 6, 1, 0, 0);
}

void Renderer::DrawCanvasRect(const RectangleTransformComponent& rt,
                              CanvasRectComponent& rect,
                              Ref<DescriptorSetLayout> layout) {
  // Lazily allocate GPU resources
  if (!rect.ubo_) {
    rect.ubo_ = CreateUniformBuffer(sizeof(CanvasElementUniformData));
    rect.descriptor_ = CreateReference<DescriptorSet>();
    rect.descriptor_->SetLayout(layout);
    rect.descriptor_->AddUniformBuffer(0, rect.ubo_);
    rect.descriptor_->Bake();
    rect.gpu_dirty_ = true;
  }

  // Update UBO
  CanvasElementUniformData data{};
  data.position = rt.computed_position;
  data.size = rt.computed_size;
  data.color = rect.color;
  data.uv_rect = {0, 0, 1, 1};
  memcpy(rect.ubo_->data_, &data, sizeof(CanvasElementUniformData));

  VkDescriptorSet sets[] = {rect.descriptor_->descriptor_set_};
  vkCmdBindDescriptorSets(command_buffers_[current_frame_]->handle_,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          bound_pipeline_->layout_, 0, 1, sets, 0, nullptr);
  vkCmdDraw(command_buffers_[current_frame_]->handle_, 6, 1, 0, 0);
}

void Renderer::DrawCanvasImage(const RectangleTransformComponent& rt,
                               CanvasImageComponent& img,
                               Ref<DescriptorSetLayout> layout) {
  if (!img.texture || !img.texture->is_allocated_) {
    return;
  }

  // Lazily allocate GPU resources
  if (!img.ubo_ || img.gpu_dirty_) {
    if (!img.ubo_) {
      img.ubo_ = CreateUniformBuffer(sizeof(CanvasElementUniformData));
    }
    img.descriptor_ = CreateReference<DescriptorSet>();
    img.descriptor_->SetLayout(layout);
    img.descriptor_->AddUniformBuffer(0, img.ubo_);
    img.descriptor_->AddCombinedImageSampler(1, img.texture->image_view_,
                                             GetDefaultLinearSampler());
    img.descriptor_->Bake();
    img.gpu_dirty_ = false;
  }

  // Update UBO
  CanvasElementUniformData data{};
  data.position = rt.computed_position;
  data.size = rt.computed_size;
  data.color = img.tint;
  data.uv_rect = img.uv_rect;
  memcpy(img.ubo_->data_, &data, sizeof(CanvasElementUniformData));

  VkDescriptorSet sets[] = {img.descriptor_->descriptor_set_};
  vkCmdBindDescriptorSets(command_buffers_[current_frame_]->handle_,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          bound_pipeline_->layout_, 0, 1, sets, 0, nullptr);
  vkCmdDraw(command_buffers_[current_frame_]->handle_, 6, 1, 0, 0);
}

void Renderer::DrawCanvasText(const RectangleTransformComponent& rt,
                              TextComponent& text,
                              Ref<DescriptorSetLayout> layout) {
  if (text.text.empty()) {
    return;
  }

  Ref<Font> font = FontCache::Get(text.font_path, text.font_size);
  if (!font || !font->IsLoaded()) {
    return;
  }

  float scale = text.font_size / font->GetNativeSize();
  float cursor_x = rt.computed_position.x;
  float cursor_y = rt.computed_position.y + font->GetAscent() * scale;

  // Rebuild per-glyph GPU resources if text changed
  if (text.gpu_dirty_ || text.prev_text_ != text.text) {
    // Count visible glyphs via UTF-8 decoding
    size_t visible = 0;
    for (size_t i = 0; i < text.text.size();) {
      uint32_t cp = Font::DecodeUTF8(text.text, i);
      const GlyphInfo* g = font->GetGlyph(cp);
      if (g && g->size.x > 0 && g->size.y > 0) {
        visible++;
      }
    }

    // Grow pool if needed, reuse existing allocations
    while (text.glyph_gpu_.size() < visible) {
      TextGlyphGPU gpu;
      gpu.ubo = CreateUniformBuffer(sizeof(CanvasElementUniformData));
      gpu.descriptor = CreateReference<DescriptorSet>();
      gpu.descriptor->SetLayout(layout);
      gpu.descriptor->AddUniformBuffer(0, gpu.ubo);
      gpu.descriptor->AddCombinedImageSampler(
          1, font->GetAtlasImageView(), GetDefaultLinearSampler());
      gpu.descriptor->Bake();
      text.glyph_gpu_.push_back(std::move(gpu));
    }
    text.prev_text_ = text.text;
    text.gpu_dirty_ = false;
  }

  // Update UBOs and draw each glyph
  size_t glyph_idx = 0;
  for (size_t i = 0; i < text.text.size();) {
    uint32_t cp = Font::DecodeUTF8(text.text, i);
    const GlyphInfo* glyph = font->GetGlyph(cp);
    if (!glyph || glyph->size.x == 0 || glyph->size.y == 0) {
      if (glyph) {
        cursor_x += std::round((glyph->advance >> 6) * scale);
      }
      continue;
    }

    if (glyph_idx >= text.glyph_gpu_.size()) {
      break;
    }

    float x = std::round(cursor_x + glyph->bearing.x * scale);
    float y = std::round(cursor_y - glyph->bearing.y * scale);
    float w = std::round(glyph->size.x * scale);
    float h = std::round(glyph->size.y * scale);

    auto& gpu = text.glyph_gpu_[glyph_idx];

    CanvasElementUniformData data{};
    data.position = {x, y};
    data.size = {w, h};
    data.color = text.color;
    data.uv_rect = {glyph->uv_min.x, glyph->uv_min.y, glyph->uv_max.x,
                    glyph->uv_max.y};
    memcpy(gpu.ubo->data_, &data, sizeof(CanvasElementUniformData));

    VkDescriptorSet sets[] = {gpu.descriptor->descriptor_set_};
    vkCmdBindDescriptorSets(command_buffers_[current_frame_]->handle_,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            bound_pipeline_->layout_, 0, 1, sets, 0, nullptr);
    vkCmdDraw(command_buffers_[current_frame_]->handle_, 6, 1, 0, 0);

    glyph_idx++;
    cursor_x += std::round((glyph->advance >> 6) * scale);
  }
}

void Renderer::DrawSkybox(Ref<Skybox> skybox) {
  std::array<VkDescriptorSet, 2> sets{
      skybox->descriptors_->descriptor_set_,
      camera_->resource_pool->GetDescriptor("GlobalDescriptor")->descriptor_set_};

  vkCmdBindDescriptorSets(
      command_buffers_[current_frame_]->handle_, VK_PIPELINE_BIND_POINT_GRAPHICS,
      bound_pipeline_->layout_, 0, 2, sets.data(), 0, nullptr);

  // draw cube via gl_VertexIndex (no vertex/index buffer needed)
  vkCmdDraw(command_buffers_[current_frame_]->handle_, 36, 1, 0, 0);
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
  vkCmdBindDescriptorSets(command_buffers_[current_frame_]->handle_,
                          VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->layout_, 0,
                          sets.size(), sets.data(), 0, nullptr);

  // Draw the quad.
  vkCmdDraw(command_buffers_[current_frame_]->handle_, 3, 1, 0, 0);
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
    camera_uniform_data_.TaaJitterOffset =
        glm::vec2(jitter_x / viewport_size_.x, jitter_y / viewport_size_.y);

    prev_jittered_vp_ =
        camera_uniform_data_.Projection * camera_uniform_data_.ViewMatrix;
    taa_frame_index_++;
  } else {
    camera_uniform_data_.PrevViewProjection = camera_data->prev_view_projection;
    camera_uniform_data_.TaaJitterOffset = glm::vec2(0.0f);
  }
}

Ref<DescriptorSet> Renderer::GetFinalOutputDescriptor() const {
  if (!camera_ || !camera_->resource_pool)
    return nullptr;
  return camera_->resource_pool->GetDescriptor("PipelineOutputDescriptor");
}

Ref<AttachmentTexture> Renderer::GetFinalOutputImage() const {
  if (!camera_ || !camera_->resource_pool)
    return nullptr;
  return camera_->resource_pool->GetTexture("PipelineOutput");
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

VkCommandPool Renderer::CreateTransientCommandPool() {
  VkCommandPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
  poolInfo.queueFamilyIndex = queue_family_indices_.graphicsFamily.value();
  VkCommandPool pool;
  WIESEL_CHECK_VKRESULT(
      vkCreateCommandPool(logical_device_, &poolInfo, nullptr, &pool));
  return pool;
}

thread_local VkCommandPool Renderer::tl_command_pool_ = VK_NULL_HANDLE;

void Renderer::SetThreadCommandPool(VkCommandPool pool) {
  tl_command_pool_ = pool;
}

VkCommandBuffer Renderer::BeginSingleTimeCommands() {
  VkCommandPool pool = tl_command_pool_ != VK_NULL_HANDLE
                            ? tl_command_pool_
                            : command_pool_->handle_;
  return BeginSingleTimeCommands(pool);
}

VkCommandBuffer Renderer::BeginSingleTimeCommands(VkCommandPool pool) {
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = pool;
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
  VkCommandPool pool = tl_command_pool_ != VK_NULL_HANDLE
                            ? tl_command_pool_
                            : command_pool_->handle_;
  EndSingleTimeCommands(commandBuffer, pool);
}

void Renderer::EndSingleTimeCommands(VkCommandBuffer commandBuffer,
                                     VkCommandPool pool) {
  vkEndCommandBuffer(commandBuffer);

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  VkFenceCreateInfo fenceInfo{};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  VkFence fence;
  WIESEL_CHECK_VKRESULT(
      vkCreateFence(logical_device_, &fenceInfo, nullptr, &fence));

  {
    std::lock_guard<std::mutex> lock(queue_submit_mutex_);
    WIESEL_CHECK_VKRESULT(
        vkQueueSubmit(graphics_queue_, 1, &submitInfo, fence));
  }

  vkWaitForFences(logical_device_, 1, &fence, VK_TRUE, UINT64_MAX);
  vkDestroyFence(logical_device_, fence, nullptr);

  vkFreeCommandBuffers(logical_device_, pool, 1, &commandBuffer);
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
    WindowSize size{};
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