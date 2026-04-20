
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/w_renderer.h"
#include "asset/w_asset_properties.h"
#include "rendering/features/w_billboard_feature.h"
#include "rendering/features/w_canvas_feature.h"
#include "rendering/features/w_debug_collider_feature.h"
#include "rendering/w_acceleration_structure.h"
#include "rendering/w_perf_marker.h"
#include "rendering/w_sampler.h"
#include "rendering/w_transient_resource_pool.h"
#include "util/w_spirv.h"
#include "w_engine.h"

// clang-format off
// Implementation macros must precede their includes
#include <random>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#include <stb_image.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize2.h>
// clang-format on

#include "animation/w_animation.h"
#include "asset/w_asset_manager.h"
#include "events/w_engineevents.h"

#include "ui/w_rmlui_renderer.h"

namespace wiesel {

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

Renderer::Renderer(std::shared_ptr<AppWindow> window) : window_(window) {
  spirv::Init();
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
  transient_resource_pool_ = std::make_unique<TransientResourcePool>(*this);
}

TransientResourcePool& Renderer::GetTransientResourcePool() {
  return *transient_resource_pool_;
}

Renderer::~Renderer() {
  Cleanup();
}

void Renderer::Initialize(const RendererProperties&& properties) {
  options_.wireframe_enabled.SetHook(&recreate_pipeline_);
  options_.msaa_mode.SetHook(&recreate_swap_chain_);
  options_.vsync.SetHook(&recreate_swap_chain_);
  // Feature toggles need resource recreation (SetupResources re-runs)
  options_.ssao_enabled.SetHook(&recreate_resources_);
  options_.ibl_enabled.SetHook(&recreate_resources_);
  options_.bloom_enabled.SetHook(&recreate_resources_);
  options_.motion_blur_enabled.SetHook(&recreate_resources_);
  options_.shadows_enabled.SetHook(&recreate_resources_);
  options_.aa_mode.SetHook(&recreate_resources_);
  options_.shadow_map_resolution.SetHook(&recreate_resources_);
  options_.anisotropic_filtering.SetHook(&recreate_resources_);
  options_.texture_quality.SetHook(&recreate_resources_);
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
    device_extensions_.push_back(
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
  } else {
    LOG_INFO("Ray tracing extensions not supported - RT disabled");
  }

#ifdef VULKAN_VALIDATION
  // Enable device fault diagnostics if available (gives info instead of hard GPU crash)
  {
    uint32_t ext_count = 0;
    vkEnumerateDeviceExtensionProperties(physical_device_, nullptr, &ext_count,
                                         nullptr);
    std::vector<VkExtensionProperties> exts(ext_count);
    vkEnumerateDeviceExtensionProperties(physical_device_, nullptr, &ext_count,
                                         exts.data());
    for (const auto& ext : exts) {
      if (std::string(ext.extensionName) ==
          VK_EXT_DEVICE_FAULT_EXTENSION_NAME) {
        device_extensions_.push_back(VK_EXT_DEVICE_FAULT_EXTENSION_NAME);
        LOG_INFO(
            "VK_EXT_device_fault enabled - GPU fault diagnostics available");
        break;
      }
    }
  }
#endif

  CreateLogicalDevice();
  LoadDeviceExtensions();
  InitMemoryAllocator();
  CreateGlobalUniformBuffers();
  // ---
  CreateCommandPools();
  if (rt_supported_) {
    as_manager_ =
        std::make_shared<AccelerationStructureManager>(Engine::renderer());
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
std::shared_ptr<MemoryBuffer> Renderer::CreateVertexBuffer(
    const std::string& debug_name, std::vector<T> vertices) {
  PROFILE_ZONE_SCOPED();
  std::shared_ptr<MemoryBuffer> memory_buffer =
      std::make_shared<MemoryBuffer>(MemoryTypeVertexBuffer);

  memory_buffer->size_ = vertices.size();

  VkDeviceSize buffer_size = sizeof(T) * vertices.size();
  VkBuffer staging_buffer;
  VmaAllocation staging_alloc;
  CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               staging_buffer, staging_alloc);

  void* data;
  WIESEL_CHECK_VKRESULT(vmaMapMemory(vma_allocator_, staging_alloc, &data));
  memcpy(data, vertices.data(), buffer_size);
  vmaUnmapMemory(vma_allocator_, staging_alloc);

  VkBufferUsageFlags vertex_usage =
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  if (rt_supported_) {
    vertex_usage |=
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
  }
  VkBuffer buffer;
  VmaAllocation alloc;
  CreateBuffer(buffer_size, vertex_usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
               buffer, alloc);
  memory_buffer->buffer_handle_ = buffer;
  memory_buffer->vma_buffer_ =
      std::make_unique<VmaBuffer>(vma_allocator_, buffer, alloc, debug_name);

  CopyBuffer(staging_buffer, memory_buffer->buffer_handle_, buffer_size);

  if (rt_supported_) {
    VkBufferDeviceAddressInfo addr_info{};
    addr_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addr_info.buffer = memory_buffer->buffer_handle_;
    memory_buffer->device_address_ =
        vkGetBufferDeviceAddress(logical_device_, &addr_info);
  }

  if (tl_batch_active_) {
    DeferStagingCleanup(staging_buffer, staging_alloc);
  } else {
    vmaDestroyBuffer(vma_allocator_, staging_buffer, staging_alloc);
  }
  return memory_buffer;
}

template std::shared_ptr<MemoryBuffer> Renderer::CreateVertexBuffer<Vertex3D>(
    const std::string& debug_name, std::vector<Vertex3D>);

template std::shared_ptr<MemoryBuffer>
Renderer::CreateVertexBuffer<Vertex2DNoColor>(const std::string& debug_name,
                                              std::vector<Vertex2DNoColor>);

template std::shared_ptr<MemoryBuffer>
Renderer::CreateVertexBuffer<VertexSprite>(const std::string& debug_name,
                                           std::vector<VertexSprite>);

template std::shared_ptr<MemoryBuffer> Renderer::CreateVertexBuffer<glm::vec3>(
    const std::string& debug_name, std::vector<glm::vec3>);

template std::shared_ptr<MemoryBuffer>
Renderer::CreateVertexBuffer<OverlayVertex>(const std::string& debug_name,
                                            std::vector<OverlayVertex>);

template std::shared_ptr<MemoryBuffer>
Renderer::CreateVertexBuffer<BillboardVertex>(const std::string& debug_name,
                                              std::vector<BillboardVertex>);

template std::shared_ptr<MemoryBuffer> Renderer::CreateVertexBuffer<RmlVertex>(
    const std::string& debug_name, std::vector<RmlVertex>);

std::shared_ptr<IndexBuffer> Renderer::CreateIndexBuffer(
    const std::string& debug_name, std::vector<Index> indices) {
  PROFILE_ZONE_SCOPED();
  std::shared_ptr<IndexBuffer> index_buffer = std::make_shared<IndexBuffer>();

  static_assert(sizeof(Index) == sizeof(uint32_t));
  index_buffer->index_type_ = VK_INDEX_TYPE_UINT32;
  index_buffer->size_ = indices.size();
  VkDeviceSize buffer_size = sizeof(indices[0]) * indices.size();

  VkBuffer staging_buffer;
  VmaAllocation staging_alloc;
  CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               staging_buffer, staging_alloc);

  void* data;
  WIESEL_CHECK_VKRESULT(vmaMapMemory(vma_allocator_, staging_alloc, &data));
  memcpy(data, indices.data(), (size_t)buffer_size);
  vmaUnmapMemory(vma_allocator_, staging_alloc);

  VkBufferUsageFlags index_usage =
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  if (rt_supported_) {
    index_usage |=
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
  }
  VkBuffer buffer;
  VmaAllocation alloc;
  CreateBuffer(buffer_size, index_usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
               buffer, alloc);
  index_buffer->buffer_handle_ = buffer;
  index_buffer->vma_buffer_ =
      std::make_unique<VmaBuffer>(vma_allocator_, buffer, alloc, debug_name);

  CopyBuffer(staging_buffer, index_buffer->buffer_handle_, buffer_size);

  if (rt_supported_) {
    VkBufferDeviceAddressInfo addr_info{};
    addr_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addr_info.buffer = index_buffer->buffer_handle_;
    index_buffer->device_address_ =
        vkGetBufferDeviceAddress(logical_device_, &addr_info);
  }

  if (tl_batch_active_) {
    DeferStagingCleanup(staging_buffer, staging_alloc);
  } else {
    vmaDestroyBuffer(vma_allocator_, staging_buffer, staging_alloc);
  }

  return index_buffer;
}

std::shared_ptr<UniformBuffer> Renderer::CreateUniformBuffer(
    const std::string& debug_name, VkDeviceSize size) {
  PROFILE_ZONE_SCOPED();
  std::shared_ptr<UniformBuffer> uniform_buffer =
      std::make_shared<UniformBuffer>();

  uniform_buffer->size_ = size;
  VkBuffer buffer;
  VmaAllocation alloc;
  CreateBuffer(
      size,
      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
      buffer, alloc);
  uniform_buffer->buffer_handle_ = buffer;
  uniform_buffer->vma_buffer_ =
      std::make_unique<VmaBuffer>(vma_allocator_, buffer, alloc, debug_name);

  WIESEL_CHECK_VKRESULT(
      vmaMapMemory(vma_allocator_, alloc, &uniform_buffer->data_));

  memset(uniform_buffer->data_, 0, size);

  return uniform_buffer;
}

std::shared_ptr<UniformBuffer> Renderer::CreateStorageBuffer(
    const std::string& debug_name, VkDeviceSize size) {
  PROFILE_ZONE_SCOPED();
  std::shared_ptr<UniformBuffer> buffer = std::make_shared<UniformBuffer>();
  buffer->size_ = size;
  VkBuffer buf;
  VmaAllocation alloc;
  CreateBuffer(
      size,
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
      buf, alloc);
  buffer->buffer_handle_ = buf;
  buffer->vma_buffer_ = std::make_unique<VmaBuffer>(vma_allocator_, buf, alloc);
  buffer->vma_buffer_->SetDebugName(debug_name);
  WIESEL_CHECK_VKRESULT(vmaMapMemory(vma_allocator_, alloc, &buffer->data_));
  memset(buffer->data_, 0, size);
  return buffer;
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
  component.resource_pipeline_version = 0;  // force rebuild on next render
  component.view_changed = true;
  component.pos_changed = true;
}

std::shared_ptr<Texture> Renderer::CreateBlankTexture(
    const std::string& debug_name) {
  PROFILE_ZONE_SCOPED();
  std::shared_ptr<Texture> texture =
      std::make_shared<Texture>(TextureTypeDiffuse, "");

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
  VmaAllocation staging_alloc;
  CreateBuffer(texture->size_, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               staging_buffer, staging_alloc);

  void* data;
  WIESEL_CHECK_VKRESULT(vmaMapMemory(vma_allocator_, staging_alloc, &data));
  memcpy(data, pixels, static_cast<size_t>(texture->size_));
  vmaUnmapMemory(vma_allocator_, staging_alloc);

  VkImage image;
  VmaAllocation image_alloc;
  CreateImage(texture->width_, texture->height_, texture->mip_levels_,
              SamplingMode::DISABLED, format, VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                  VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, image_alloc);
  texture->image_ = image;
  texture->vma_image_ = std::make_unique<VmaImage>(vma_allocator_, image,
                                                   image_alloc, debug_name);

  TransitionImageLayout(texture->image_, format, VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        texture->mip_levels_);
  CopyBufferToImage(staging_buffer, texture->image_,
                    static_cast<uint32_t>(texture->width_),
                    static_cast<uint32_t>(texture->height_));

  vmaDestroyBuffer(vma_allocator_, staging_buffer, staging_alloc);

  // todo loading pregenerated mipmaps
  GenerateMipmaps(texture->image_, VK_FORMAT_R8G8B8A8_UNORM, texture->width_,
                  texture->height_, texture->mip_levels_);

  texture->format_ = format;
  texture->sampler_ =
      std::make_shared<Sampler>(texture->mip_levels_, SamplerProps{});
  texture->image_view_ = CreateImageView(
      texture->image_, format, VK_IMAGE_ASPECT_COLOR_BIT, texture->mip_levels_);

  texture->MarkAllocated();
  return texture;
}

std::shared_ptr<Texture> Renderer::CreateBlankTexture(
    const std::string& debug_name, const TextureProps& texture_props,
    const SamplerProps& sampler_props) {
  PROFILE_ZONE_SCOPED();
  std::shared_ptr<Texture> texture =
      std::make_shared<Texture>(TextureTypeDiffuse, "");

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
  VmaAllocation staging_alloc;
  CreateBuffer(texture->size_, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               staging_buffer, staging_alloc);

  void* data;
  WIESEL_CHECK_VKRESULT(vmaMapMemory(vma_allocator_, staging_alloc, &data));
  memcpy(data, pixels, static_cast<size_t>(texture->size_));
  vmaUnmapMemory(vma_allocator_, staging_alloc);

  VkImage image;
  VmaAllocation image_alloc;
  CreateImage(texture->width_, texture->height_, texture->mip_levels_,
              SamplingMode::DISABLED, format, VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                  VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, image_alloc);
  texture->image_ = image;
  texture->vma_image_ = std::make_unique<VmaImage>(vma_allocator_, image,
                                                   image_alloc, debug_name);

  {
    VkCommandBuffer cmd = BeginSingleTimeCommands();
    TransitionImageLayout(
        cmd, texture->image_, format, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, texture->mip_levels_, 0, 1);
    CopyBufferToImage(cmd, staging_buffer, texture->image_,
                      static_cast<uint32_t>(texture->width_),
                      static_cast<uint32_t>(texture->height_));
    GenerateMipmaps(cmd, texture->image_, VK_FORMAT_R8G8B8A8_UNORM,
                    texture->width_, texture->height_, texture->mip_levels_);
    EndSingleTimeCommands(cmd);
  }

  if (tl_batch_active_) {
    DeferStagingCleanup(staging_buffer, staging_alloc);
  } else {
    vmaDestroyBuffer(vma_allocator_, staging_buffer, staging_alloc);
  }

  texture->sampler_ = std::make_shared<Sampler>(1, sampler_props);
  texture->image_view_ = CreateImageView(
      texture->image_, format, VK_IMAGE_ASPECT_COLOR_BIT, texture->mip_levels_);

  texture->MarkAllocated();
  return texture;
}

std::shared_ptr<Texture> Renderer::CreateTexture(
    const std::string& path, const TextureProps& texture_props,
    const SamplerProps& sampler_props) {
  PROFILE_ZONE_SCOPED();
  std::shared_ptr<Texture> texture =
      std::make_shared<Texture>(texture_props.type, path);

  VfsFile vfs_file = Engine::vfs()->Open(path);
  if (!vfs_file) {
    LOG_WARN("Attempted to load a texture that does not exist {}", path);
    return nullptr;
  }
  stbi_uc* pixels =
      stbi_load_from_memory(vfs_file.Data(), static_cast<int>(vfs_file.Size()),
                            reinterpret_cast<int*>(&texture->width_),
                            reinterpret_cast<int*>(&texture->height_),
                            &texture->channels_, STBI_rgb_alpha);

  if (!pixels) {
    LOG_WARN("Failed to load texture from {}", path);
    return nullptr;
  }

  // Downscale based on texture quality setting
  uint8_t* upload_pixels = pixels;
  std::unique_ptr<uint8_t[]> resized;
  int quality = options_.texture_quality.Get();
  if (quality > 0 && texture_props.generate_mipmaps && texture->width_ > 4 &&
      texture->height_ > 4) {
    uint32_t new_w = std::max(texture->width_ >> quality, 1u);
    uint32_t new_h = std::max(texture->height_ >> quality, 1u);
    if (new_w < texture->width_ || new_h < texture->height_) {
      resized = std::make_unique<uint8_t[]>(new_w * new_h * STBI_rgb_alpha);
      stbir_resize_uint8_linear(
          pixels, texture->width_, texture->height_,
          texture->width_ * STBI_rgb_alpha, resized.get(), new_w, new_h,
          new_w * STBI_rgb_alpha,
          static_cast<stbir_pixel_layout>(STBI_rgb_alpha));
      texture->width_ = new_w;
      texture->height_ = new_h;
      upload_pixels = resized.get();
    }
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
  VmaAllocation staging_alloc;
  CreateBuffer(texture->size_, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               staging_buffer, staging_alloc);

  void* data;
  WIESEL_CHECK_VKRESULT(vmaMapMemory(vma_allocator_, staging_alloc, &data));
  memcpy(data, upload_pixels, static_cast<size_t>(texture->size_));
  vmaUnmapMemory(vma_allocator_, staging_alloc);

  stbi_image_free(pixels);

  VkImage image;
  VmaAllocation image_alloc;
  CreateImage(texture->width_, texture->height_, texture->mip_levels_,
              SamplingMode::DISABLED, texture_props.image_format,
              VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                  VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, image_alloc);
  texture->image_ = image;
  texture->vma_image_ =
      std::make_unique<VmaImage>(vma_allocator_, image, image_alloc, "Image");

  {
    VkCommandBuffer cmd = BeginSingleTimeCommands();
    TransitionImageLayout(cmd, texture->image_, texture_props.image_format,
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          texture->mip_levels_, 0, 1);
    CopyBufferToImage(cmd, staging_buffer, texture->image_,
                      static_cast<uint32_t>(texture->width_),
                      static_cast<uint32_t>(texture->height_));
    GenerateMipmaps(cmd, texture->image_, texture_props.image_format,
                    texture->width_, texture->height_, texture->mip_levels_);
    EndSingleTimeCommands(cmd);
  }

  if (tl_batch_active_) {
    DeferStagingCleanup(staging_buffer, staging_alloc);
  } else {
    vmaDestroyBuffer(vma_allocator_, staging_buffer, staging_alloc);
  }

  texture->sampler_ =
      std::make_shared<Sampler>(texture->mip_levels_, sampler_props);
  texture->image_view_ =
      CreateImageView(texture->image_, texture_props.image_format,
                      VK_IMAGE_ASPECT_COLOR_BIT, texture->mip_levels_);

  texture->MarkAllocated();
  return texture;
}

std::shared_ptr<Texture> Renderer::CreateTexture(
    void* buffer, size_t size_per_pixel, const TextureProps& texture_props,
    const SamplerProps& sampler_props) {
  PROFILE_ZONE_SCOPED();
  std::shared_ptr<Texture> texture =
      std::make_shared<Texture>(texture_props.type, "");

  // Downscale based on texture quality setting (0=full, 1=half, 2=quarter, 3=eighth)
  uint32_t w = texture_props.width;
  uint32_t h = texture_props.height;
  void* src_pixels = buffer;
  std::unique_ptr<uint8_t[]> resized_pixels;

  int quality = options_.texture_quality.Get();
  if (quality > 0 && texture_props.generate_mipmaps && w > 4 && h > 4) {
    uint32_t new_w = std::max(w >> quality, 1u);
    uint32_t new_h = std::max(h >> quality, 1u);
    if (new_w < w || new_h < h) {
      LOG_INFO("Texture quality downscale: {}x{} -> {}x{} (quality={})", w, h,
               new_w, new_h, quality);
      resized_pixels =
          std::make_unique<uint8_t[]>(new_w * new_h * size_per_pixel);
      stbir_resize_uint8_linear(
          static_cast<const uint8_t*>(buffer), w, h, w * size_per_pixel,
          resized_pixels.get(), new_w, new_h, new_w * size_per_pixel,
          static_cast<stbir_pixel_layout>(size_per_pixel));
      w = new_w;
      h = new_h;
      src_pixels = resized_pixels.get();
    }
  }

  texture->width_ = w;
  texture->height_ = h;
  texture->size_ = texture->width_ * texture->height_ * size_per_pixel;

  if (texture_props.generate_mipmaps) {
    texture->mip_levels_ = static_cast<uint32_t>(std::floor(std::log2(
                               std::max(texture->width_, texture->height_)))) +
                           1;
  } else {
    texture->mip_levels_ = 1;
  }

  VkBuffer staging_buffer;
  VmaAllocation staging_alloc;
  CreateBuffer(texture->size_, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               staging_buffer, staging_alloc);

  void* data;
  WIESEL_CHECK_VKRESULT(vmaMapMemory(vma_allocator_, staging_alloc, &data));
  memcpy(data, src_pixels, static_cast<size_t>(texture->size_));
  vmaUnmapMemory(vma_allocator_, staging_alloc);

  VkImage image;
  VmaAllocation image_alloc;
  CreateImage(texture->width_, texture->height_, texture->mip_levels_,
              SamplingMode::DISABLED, texture_props.image_format,
              VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                  VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, image_alloc);
  texture->image_ = image;
  texture->vma_image_ =
      std::make_unique<VmaImage>(vma_allocator_, image, image_alloc, "Image");

  {
    VkCommandBuffer cmd = BeginSingleTimeCommands();
    TransitionImageLayout(cmd, texture->image_, texture_props.image_format,
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          texture->mip_levels_, 0, 1);
    CopyBufferToImage(cmd, staging_buffer, texture->image_, texture->width_,
                      texture->height_);
    GenerateMipmaps(cmd, texture->image_, texture_props.image_format,
                    texture->width_, texture->height_, texture->mip_levels_);
    EndSingleTimeCommands(cmd);
  }

  if (tl_batch_active_) {
    DeferStagingCleanup(staging_buffer, staging_alloc);
  } else {
    vmaDestroyBuffer(vma_allocator_, staging_buffer, staging_alloc);
  }

  texture->sampler_ =
      std::make_shared<Sampler>(texture->mip_levels_, sampler_props);
  texture->image_view_ =
      CreateImageView(texture->image_, texture_props.image_format,
                      VK_IMAGE_ASPECT_COLOR_BIT, texture->mip_levels_);

  texture->MarkAllocated();
  return texture;
}

std::shared_ptr<Texture> Renderer::CreateCubemapTexture(
    const std::array<std::string, 6>& paths, const TextureProps& texture_props,
    const SamplerProps& sampler_props) {
  PROFILE_ZONE_SCOPED();
  std::shared_ptr<Texture> texture =
      std::make_shared<Texture>(texture_props.type, "");
  VkDeviceSize total_size = 0;
  stbi_uc* all_pixels = nullptr;

  // Load all 6 faces
  for (size_t i = 0; i < 6; ++i) {
    int w, h, channels;
    VfsFile vfs_face = Engine::vfs()->Open(paths[i]);
    if (!vfs_face) {
      LOG_ERROR("Cubemap face not found: {}", paths[i]);
      delete[] all_pixels;
      return nullptr;
    }
    stbi_uc* pixels = stbi_load_from_memory(vfs_face.Data(),
                                            static_cast<int>(vfs_face.Size()),
                                            &w, &h, &channels, STBI_rgb_alpha);
    if (!pixels) {
      LOG_ERROR("Failed to load cubemap face: {}", paths[i]);
      delete[] all_pixels;
      return nullptr;
    }

    if (i == 0) {
      texture->width_ = w;
      texture->height_ = h;
      texture->size_ = texture->width_ * texture->height_ * STBI_rgb_alpha;
      texture->mip_levels_ = 1;
      total_size = texture->size_ * 6;
      all_pixels = new stbi_uc[total_size];
    }

    if (w != static_cast<int>(texture->width_) ||
        h != static_cast<int>(texture->height_)) {
      LOG_ERROR("Cubemap face size mismatch: {} (expected {}x{}, got {}x{})",
                paths[i], texture->width_, texture->height_, w, h);
      stbi_image_free(pixels);
      delete[] all_pixels;
      return nullptr;
    }

    memcpy(all_pixels + i * texture->size_, pixels, texture->size_);
    stbi_image_free(pixels);
  }
  VkBuffer staging_buffer;
  VmaAllocation staging_alloc;
  CreateBuffer(total_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               staging_buffer, staging_alloc);

  void* data;
  WIESEL_CHECK_VKRESULT(vmaMapMemory(vma_allocator_, staging_alloc, &data));
  memcpy(data, all_pixels, static_cast<size_t>(total_size));
  vmaUnmapMemory(vma_allocator_, staging_alloc);

  VkImage image;
  VmaAllocation image_alloc;
  CreateImage(texture->width_, texture->height_, texture->mip_levels_,
              SamplingMode::DISABLED, texture_props.image_format,
              VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                  VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, image_alloc,
              VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, 6);
  texture->image_ = image;
  texture->vma_image_ =
      std::make_unique<VmaImage>(vma_allocator_, image, image_alloc, "Image");

  {
    VkCommandBuffer cmd = BeginSingleTimeCommands();
    for (uint32_t layer = 0; layer < 6; layer++) {
      TransitionImageLayout(cmd, texture->image_, texture_props.image_format,
                            VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            texture->mip_levels_, layer, 1);
      CopyBufferToImage(cmd, staging_buffer, texture->image_,
                        static_cast<uint32_t>(texture->width_),
                        static_cast<uint32_t>(texture->height_),
                        texture->size_ * layer, layer);
    }
    for (uint32_t layer = 0; layer < 6; layer++) {
      TransitionImageLayout(cmd, texture->image_, texture_props.image_format,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            texture->mip_levels_, layer, 1);
    }
    EndSingleTimeCommands(cmd);
  }

  vmaDestroyBuffer(vma_allocator_, staging_buffer, staging_alloc);
  delete[] all_pixels;

  texture->sampler_ =
      std::make_shared<Sampler>(texture->mip_levels_, sampler_props);
  texture->image_view_ = CreateImageView(
      texture->image_, texture_props.image_format, VK_IMAGE_ASPECT_COLOR_BIT,
      texture->mip_levels_, VK_IMAGE_VIEW_TYPE_CUBE, 0, 6);
  texture->MarkAllocated();
  return texture;
}

std::shared_ptr<Texture> Renderer::CreateCubemapTextureFromSingle(
    const std::string& virtual_path, const TextureProps& texture_props,
    const SamplerProps& sampler_props) {
  PROFILE_ZONE_SCOPED();
  std::shared_ptr<Texture> texture =
      std::make_shared<Texture>(texture_props.type, "");

  int w, h, channels;
  VfsFile vfs_cubemap = Engine::vfs()->Open(virtual_path);
  if (!vfs_cubemap) {
    LOG_ERROR("Cubemap file not found: {}", virtual_path);
    return nullptr;
  }
  stbi_uc* pixels = stbi_load_from_memory(vfs_cubemap.Data(),
                                          static_cast<int>(vfs_cubemap.Size()),
                                          &w, &h, &channels, STBI_rgb_alpha);
  if (!pixels) {
    LOG_ERROR("Failed to load cubemap image: {}", virtual_path);
    return nullptr;
  }
  const bool is_horizontal_strip = (w % 6 == 0) && (h > 0) && (h == w / 6);
  const bool is_vertical_cross =
      (w % 4 == 0) && (h % 3 == 0) && (w / 4 == h / 3);
  // Equirectangular: 2:1 aspect ratio (e.g. 2048x1024, 4096x2048)
  const bool is_equirectangular = (w == h * 2) && !is_horizontal_strip;

  if (!is_horizontal_strip && !is_vertical_cross && !is_equirectangular) {
    stbi_image_free(pixels);
    throw std::runtime_error("Unsupported cubemap layout: " + virtual_path);
  }

  uint32_t faceSize = 0;

  if (is_horizontal_strip) {
    faceSize = w / 6;
  } else if (is_vertical_cross) {
    faceSize = w / 4;
  } else {
    faceSize = h;  // equirectangular: full height for better quality
  }

  texture->width_ = faceSize;
  texture->height_ = faceSize;
  texture->size_ = faceSize * faceSize * 4;
  texture->mip_levels_ = 1;

  VkDeviceSize total_size = texture->size_ * 6;

  std::vector<stbi_uc> cube_pixels(total_size);

  auto copy_face = [&](uint32_t faceIndex, uint32_t gridX, uint32_t gridY) {
    for (uint32_t row = 0; row < faceSize; row++) {
      memcpy(
          cube_pixels.data() + faceIndex * texture->size_ + row * faceSize * 4,
          pixels + ((gridY * faceSize + row) * w + gridX * faceSize) * 4,
          faceSize * 4);
    }
  };

  if (is_equirectangular) {
    // Project equirectangular panorama onto 6 cubemap faces
    // Face order: +X, -X, +Y, -Y, +Z, -Z
    auto sampleEquirect = [&](float x, float y, float z, stbi_uc* out) {
      float theta = std::atan2(z, x);
      float phi = std::asin(std::clamp(y, -1.0f, 1.0f));
      float u = (theta / (2.0f * PI)) + 0.5f;
      float v = 0.5f - (phi / PI);
      // Bilinear interpolation
      float fx = u * w - 0.5f;
      float fy = v * h - 0.5f;
      int x0 = static_cast<int>(std::floor(fx));
      int y0 = static_cast<int>(std::floor(fy));
      float sx = fx - x0;
      float sy = fy - y0;
      // Wrap X for seamless panorama, clamp Y
      auto sample = [&](int px, int py) -> const stbi_uc* {
        px = ((px % w) + w) % w;
        py = std::clamp(py, 0, h - 1);
        return pixels + (py * w + px) * 4;
      };
      const stbi_uc* p00 = sample(x0, y0);
      const stbi_uc* p10 = sample(x0 + 1, y0);
      const stbi_uc* p01 = sample(x0, y0 + 1);
      const stbi_uc* p11 = sample(x0 + 1, y0 + 1);
      for (int c = 0; c < 4; c++) {
        float top = p00[c] * (1.0f - sx) + p10[c] * sx;
        float bot = p01[c] * (1.0f - sx) + p11[c] * sx;
        out[c] = static_cast<stbi_uc>(
            std::clamp(top * (1.0f - sy) + bot * sy, 0.0f, 255.0f));
      }
    };

    for (uint32_t face = 0; face < 6; face++) {
      for (uint32_t py = 0; py < faceSize; py++) {
        for (uint32_t px = 0; px < faceSize; px++) {
          // Map pixel to [-1, 1] on the face
          float s = (2.0f * (px + 0.5f) / faceSize) - 1.0f;
          float t = (2.0f * (py + 0.5f) / faceSize) - 1.0f;
          float x = 0, y = 0, z = 0;
          switch (face) {
            case 0:
              x = 1;
              y = -t;
              z = -s;
              break;  // +X
            case 1:
              x = -1;
              y = -t;
              z = s;
              break;  // -X
            case 2:
              x = s;
              y = 1;
              z = t;
              break;  // +Y
            case 3:
              x = s;
              y = -1;
              z = -t;
              break;  // -Y
            case 4:
              x = s;
              y = -t;
              z = 1;
              break;  // +Z
            case 5:
              x = -s;
              y = -t;
              z = -1;
              break;  // -Z
            default:
              break;
          }
          float len = std::sqrt(x * x + y * y + z * z);
          x /= len;
          y /= len;
          z /= len;

          uint32_t outIdx =
              face * faceSize * faceSize * 4 + (py * faceSize + px) * 4;
          sampleEquirect(x, y, z, cube_pixels.data() + outIdx);
        }
      }
    }
  } else if (is_horizontal_strip) {
    for (uint32_t face = 0; face < 6; face++) {
      copy_face(face, face, 0);
    }
  } else {
    // Vertical cross (4x3 fold layout)
    // Grid layout:
    //       +Y        (1,0)
    // -X +Z +X -Z     (0,1)(1,1)(2,1)(3,1)
    //       -Y        (1,2)

    copy_face(0, 2, 1);  // +X
    copy_face(1, 0, 1);  // -X
    copy_face(2, 1, 0);  // +Y
    copy_face(3, 1, 2);  // -Y
    copy_face(4, 1, 1);  // +Z
    copy_face(5, 3, 1);  // -Z
  }

  stbi_image_free(pixels);

  // Create staging buffer
  VkBuffer staging_buffer;
  VmaAllocation staging_alloc;

  CreateBuffer(total_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               staging_buffer, staging_alloc);

  void* data;
  WIESEL_CHECK_VKRESULT(vmaMapMemory(vma_allocator_, staging_alloc, &data));
  memcpy(data, cube_pixels.data(), total_size);
  vmaUnmapMemory(vma_allocator_, staging_alloc);

  // Create cube image
  VkImage image;
  VmaAllocation image_alloc;
  CreateImage(faceSize, faceSize, texture->mip_levels_, SamplingMode::DISABLED,
              texture_props.image_format, VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, image_alloc,
              VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, 6);
  texture->image_ = image;
  texture->vma_image_ =
      std::make_unique<VmaImage>(vma_allocator_, image, image_alloc, "Image");

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

  vmaDestroyBuffer(vma_allocator_, staging_buffer, staging_alloc);

  texture->sampler_ =
      std::make_shared<Sampler>(texture->mip_levels_, sampler_props);

  texture->image_view_ = CreateImageView(
      texture->image_, texture_props.image_format, VK_IMAGE_ASPECT_COLOR_BIT,
      texture->mip_levels_, VK_IMAGE_VIEW_TYPE_CUBE, 0, 6);

  texture->MarkAllocated();
  return texture;
}

std::shared_ptr<AttachmentTexture> Renderer::CreateAttachmentTexture(
    const AttachmentTextureProps& props) {
  PROFILE_ZONE_SCOPED();
  if (props.type == AttachmentTextureType::SwapChain) {
    throw new std::runtime_error(
        "AttachmentTextureType::SwapChain cannot be created!");
  }
  std::shared_ptr<AttachmentTexture> texture =
      std::make_shared<AttachmentTexture>();
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
  texture->mip_levels_ = props.mip_levels;
  texture->images_.resize(props.image_count);
  texture->vma_images_.reserve(props.image_count);
  texture->image_views_.resize(props.image_count);

  uint32_t actual_layers =
      props.is_cubemap ? props.layer_count * 6 : props.layer_count;
  VkImageCreateFlags create_flags = 0;
  if (props.is_cubemap) {
    create_flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
  }

  for (uint32_t i = 0; i < props.image_count; i++) {
    VkImage image;
    VmaAllocation alloc;
    CreateImage(props.width, props.height, props.mip_levels,
                props.sampling_mode, props.image_format,
                VK_IMAGE_TILING_OPTIMAL, usage,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, alloc, create_flags,
                actual_layers);
    texture->images_[i] = image;
    texture->vma_images_.push_back(
        std::make_unique<VmaImage>(vma_allocator_, image, alloc));

    VkImageViewType view_type;
    if (props.is_cubemap) {
      view_type = VK_IMAGE_VIEW_TYPE_CUBE;
    } else if (actual_layers > 1) {
      view_type = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    } else {
      view_type = VK_IMAGE_VIEW_TYPE_2D;
    }

    texture->image_views_[i] =
        CreateImageView(texture->images_[i], props.image_format, aspectFlags,
                        props.mip_levels, view_type, 0, actual_layers);

    if (props.type == AttachmentTextureType::DepthStencil) {
      TransitionImageLayout(texture->images_[i], props.image_format,
                            VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                            props.mip_levels, 0, actual_layers);
    } else if (props.type == AttachmentTextureType::Color ||
               props.type == AttachmentTextureType::Resolve ||
               props.type == AttachmentTextureType::Offscreen) {
      TransitionImageLayout(texture->images_[i], props.image_format,
                            VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                            props.mip_levels, 0, actual_layers);
    } else if (props.type == AttachmentTextureType::SwapChain) {
      TransitionImageLayout(
          texture->images_[i], props.image_format, VK_IMAGE_LAYOUT_UNDEFINED,
          VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, props.mip_levels, 0, actual_layers);
    }
  }

  texture->is_allocated_ = true;
  return texture;
}

void Renderer::SetAttachmentTextureBuffer(
    std::shared_ptr<AttachmentTexture> texture, void* buffer,
    size_t sizePerPixel) {
  VkBuffer staging_buffer;
  VmaAllocation staging_alloc;
  size_t size = texture->width_ * texture->height_ * sizePerPixel;
  CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               staging_buffer, staging_alloc);

  void* data;
  WIESEL_CHECK_VKRESULT(vmaMapMemory(vma_allocator_, staging_alloc, &data));
  memcpy(data, buffer, static_cast<size_t>(size));
  vmaUnmapMemory(vma_allocator_, staging_alloc);

  TransitionImageLayout(texture->images_[0], texture->format_,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1);

  CopyBufferToImage(staging_buffer, texture->images_[0],
                    static_cast<uint32_t>(texture->width_),
                    static_cast<uint32_t>(texture->height_));

  TransitionImageLayout(texture->images_[0], texture->format_,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1);

  vmaDestroyBuffer(vma_allocator_, staging_buffer, staging_alloc);
}

VkSampler Renderer::CreateTextureSampler(uint32_t mip_levels,
                                         const SamplerProps& props) {
  PROFILE_ZONE_SCOPED();
  VkSamplerCreateInfo sampler_info{};
  sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler_info.magFilter = props.mag_filter;
  sampler_info.minFilter = props.min_filter;
  sampler_info.addressModeU = props.address_mode;
  sampler_info.addressModeV = props.address_mode;
  sampler_info.addressModeW = props.address_mode;

  if (props.max_anisotropy <= 0) {
    sampler_info.anisotropyEnable = VK_FALSE;
  } else {
    sampler_info.anisotropyEnable = VK_TRUE;
    sampler_info.maxAnisotropy =
        std::min(props.max_anisotropy,
                 physical_device_properties_.limits.maxSamplerAnisotropy);
  }
  sampler_info.borderColor = props.border_color;
  sampler_info.unnormalizedCoordinates = VK_FALSE;
  sampler_info.compareEnable = props.compare_enable;
  sampler_info.compareOp = props.compare_op;

  sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  sampler_info.maxLod = static_cast<float>(mip_levels);

  VkSampler sampler;
  WIESEL_CHECK_VKRESULT(
      vkCreateSampler(logical_device_, &sampler_info, nullptr, &sampler));
  return sampler;
}

std::shared_ptr<DescriptorSet> Renderer::CreateMeshDescriptors(
    std::shared_ptr<Material> material) {
  PROFILE_ZONE_SCOPED();
  std::shared_ptr<DescriptorSet> object = std::make_shared<DescriptorSet>();

  VkDescriptorPoolSize pool_sizes[] = {
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kMaterialTextureCount}};

  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.poolSizeCount = std::size(pool_sizes);
  pool_info.pPoolSizes = pool_sizes;
  pool_info.maxSets = 1;
  // Allocate pool
  WIESEL_CHECK_VKRESULT(vkCreateDescriptorPool(
      logical_device_, &pool_info, nullptr, &object->descriptor_pool_));

  std::vector<VkDescriptorSetLayout> layouts(
      1, GetDescriptorLayout("GeometryMesh")->layout_);
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = object->descriptor_pool_;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = layouts.data();
  WIESEL_CHECK_VKRESULT(vkAllocateDescriptorSets(logical_device_, &allocInfo,
                                                 &object->descriptor_set_));

  std::vector<VkWriteDescriptorSet> writes;
  writes.reserve(1 + kMaterialTextureCount);
  std::vector<VkDescriptorBufferInfo> buffer_infos;
  buffer_infos.reserve(1);
  std::vector<VkDescriptorImageInfo> image_infos;
  image_infos.reserve(kMaterialTextureCount);

  {
    buffer_infos.push_back({
        .buffer = instance_storage_buffer_->buffer_handle_,
        .offset = 0,
        .range = instance_storage_buffer_->size_,
    });
    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = object->descriptor_set_;
    set.dstBinding = 0;
    set.dstArrayElement = 0;
    set.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    set.descriptorCount = 1;
    set.pBufferInfo = &buffer_infos.back();
    set.pNext = nullptr;
    writes.emplace_back(set);
  }

  // Texture slots: binding 1-7
  TextureSlot* texture_slots[] = {
      &material->base_texture, &material->normal_map,  &material->specular_map,
      &material->height_map,   &material->albedo_map,  &material->roughness_map,
      &material->metallic_map, &material->opacity_map,
  };

  for (int i = 0; i < kMaterialTextureCount; i++) {
    std::shared_ptr<Texture> tex;
    texture_slots[i]->Resolve(tex);

    VkDescriptorImageInfo image_info;
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    if (!tex) {
      image_info.imageView = blank_texture_->image_view_->handle_;
      image_info.sampler = blank_texture_->sampler_->handle();
    } else {
      image_info.imageView = tex->image_view_->handle_;
      image_info.sampler = tex->sampler_->handle();
    }
    image_infos.emplace_back(image_info);

    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = object->descriptor_set_;
    set.dstBinding = static_cast<uint32_t>(i + 1);
    set.dstArrayElement = 0;
    set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    set.descriptorCount = 1;
    set.pImageInfo = &image_infos.back();
    set.pNext = nullptr;
    writes.emplace_back(set);
  }

  vkUpdateDescriptorSets(logical_device_, static_cast<uint32_t>(writes.size()),
                         writes.data(), 0, nullptr);

  object->allocated_ = true;
  return object;
}

std::shared_ptr<DescriptorSet> Renderer::CreateShadowMeshDescriptors(
    std::shared_ptr<Material> material) {
  std::shared_ptr<DescriptorSet> object = std::make_shared<DescriptorSet>();

  VkDescriptorPoolSize pool_sizes[] = {
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kMaterialTextureCount},
  };

  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.poolSizeCount = std::size(pool_sizes);
  pool_info.pPoolSizes = pool_sizes;
  pool_info.maxSets = 1;
  // Allocate pool
  WIESEL_CHECK_VKRESULT(vkCreateDescriptorPool(
      logical_device_, &pool_info, nullptr, &object->descriptor_pool_));

  std::vector<VkDescriptorSetLayout> layouts{
      1, GetDescriptorLayout("ShadowMesh")->layout_};
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = object->descriptor_pool_;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = layouts.data();
  WIESEL_CHECK_VKRESULT(vkAllocateDescriptorSets(logical_device_, &allocInfo,
                                                 &object->descriptor_set_));

  std::vector<VkWriteDescriptorSet> writes;
  writes.reserve(1 + kMaterialTextureCount);
  std::vector<VkDescriptorBufferInfo> buffer_infos;
  buffer_infos.reserve(1);
  std::vector<VkDescriptorImageInfo> image_infos;
  image_infos.reserve(kMaterialTextureCount);

  {
    buffer_infos.push_back({
        .buffer = instance_storage_buffer_->buffer_handle_,
        .offset = 0,
        .range = instance_storage_buffer_->size_,
    });
    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = object->descriptor_set_;
    set.dstBinding = 0;
    set.dstArrayElement = 0;
    set.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    set.descriptorCount = 1;
    set.pBufferInfo = &buffer_infos.back();
    set.pNext = nullptr;
    writes.emplace_back(set);
  }

  // Bind all material textures (same as geometry descriptor)
  TextureSlot* texture_slots[] = {
      &material->base_texture, &material->normal_map,  &material->specular_map,
      &material->height_map,   &material->albedo_map,  &material->roughness_map,
      &material->metallic_map, &material->opacity_map,
  };

  for (int i = 0; i < kMaterialTextureCount; i++) {
    std::shared_ptr<Texture> tex;
    texture_slots[i]->Resolve(tex);

    VkDescriptorImageInfo image_info;
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    if (!tex) {
      image_info.imageView = blank_texture_->image_view_->handle_;
      image_info.sampler = blank_texture_->sampler_->handle();
    } else {
      image_info.imageView = tex->image_view_->handle_;
      image_info.sampler = tex->sampler_->handle();
    }
    image_infos.emplace_back(image_info);

    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = object->descriptor_set_;
    set.dstBinding = static_cast<uint32_t>(i + 1);
    set.dstArrayElement = 0;
    set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    set.descriptorCount = 1;
    set.pImageInfo = &image_infos.back();
    set.pNext = nullptr;
    writes.emplace_back(set);
  }

  vkUpdateDescriptorSets(logical_device_, static_cast<uint32_t>(writes.size()),
                         writes.data(), 0, nullptr);

  object->allocated_ = true;
  return object;
}

std::shared_ptr<DescriptorSet> Renderer::CreateGlobalDescriptors(
    CameraComponent& camera) {
  std::shared_ptr<DescriptorSet> object = std::make_shared<DescriptorSet>();

  VkDescriptorPoolSize pool_sizes[] = {
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}};

  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.poolSizeCount = std::size(pool_sizes);
  pool_info.pPoolSizes = pool_sizes;
  pool_info.maxSets = 1;
  // Allocate pool
  WIESEL_CHECK_VKRESULT(vkCreateDescriptorPool(
      logical_device_, &pool_info, nullptr, &object->descriptor_pool_));

  std::vector<VkDescriptorSetLayout> layouts{
      1, GetDescriptorLayout("Global")->layout_};
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = object->descriptor_pool_;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = layouts.data();
  WIESEL_CHECK_VKRESULT(vkAllocateDescriptorSets(logical_device_, &allocInfo,
                                                 &object->descriptor_set_));

  std::vector<VkWriteDescriptorSet> writes;
  writes.reserve(4);
  std::vector<VkDescriptorBufferInfo> buffer_infos;
  buffer_infos.reserve(3);
  std::vector<VkDescriptorImageInfo> image_infos;
  image_infos.reserve(1);

  {
    VkDescriptorBufferInfo buffer_info;
    buffer_info.buffer = lights_uniform_buffer_->buffer_handle_;
    buffer_info.offset = 0;
    buffer_info.range = sizeof(LightsUniformData);
    buffer_infos.emplace_back(buffer_info);

    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = object->descriptor_set_;
    set.dstBinding = 0;
    set.dstArrayElement = 0;
    set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    set.descriptorCount = 1;
    set.pBufferInfo = &buffer_infos[buffer_infos.size() - 1];
    set.pNext = nullptr;

    writes.emplace_back(set);
  }

  {
    VkDescriptorBufferInfo buffer_info;
    buffer_info.buffer = camera_uniform_buffer_->buffer_handle_;
    buffer_info.offset = 0;
    buffer_info.range = sizeof(CameraUniformData);
    buffer_infos.emplace_back(buffer_info);

    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = object->descriptor_set_;
    set.dstBinding = 1;
    set.dstArrayElement = 0;
    set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    set.descriptorCount = 1;
    set.pBufferInfo = &buffer_infos[buffer_infos.size() - 1];
    set.pNext = nullptr;

    writes.emplace_back(set);
  }

  {
    VkDescriptorBufferInfo buffer_info;
    buffer_info.buffer = shadow_camera_uniform_buffer_->buffer_handle_;
    buffer_info.offset = 0;
    buffer_info.range = sizeof(ShadowMapMatricesUniformData);
    buffer_infos.emplace_back(buffer_info);

    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = object->descriptor_set_;
    set.dstBinding = 2;
    set.dstArrayElement = 0;
    set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    set.descriptorCount = 1;
    set.pBufferInfo = &buffer_infos[buffer_infos.size() - 1];
    set.pNext = nullptr;

    writes.emplace_back(set);
  }

  {
    VkDescriptorImageInfo image_info;
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    auto shadow_view =
        camera.resource_pool.GetImageView("ShadowDepthViewArray");
    if (shadow_view == nullptr) {
      image_info.imageView = blank_texture_->image_view_->handle_;
      image_info.sampler = blank_texture_->sampler_->handle();
    } else {
      image_info.imageView = shadow_view->handle_;
      image_info.sampler = shadow_sampler_->handle_;
    }
    image_infos.emplace_back(image_info);

    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = object->descriptor_set_;
    set.dstBinding = 3;
    set.dstArrayElement = 0;
    set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    set.descriptorCount = 1;
    set.pImageInfo = &image_infos.back();
    set.pNext = nullptr;
    writes.emplace_back(set);
  }

  vkUpdateDescriptorSets(logical_device_, static_cast<uint32_t>(writes.size()),
                         writes.data(), 0, nullptr);

  object->allocated_ = true;

  return object;
}

std::shared_ptr<DescriptorSet> Renderer::CreateShadowGlobalDescriptors(
    CameraComponent& camera) {
  std::shared_ptr<DescriptorSet> object = std::make_shared<DescriptorSet>();

  VkDescriptorPoolSize pool_sizes[] = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}};

  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.poolSizeCount = std::size(pool_sizes);
  pool_info.pPoolSizes = pool_sizes;
  pool_info.maxSets = 1;
  // Allocate pool
  WIESEL_CHECK_VKRESULT(vkCreateDescriptorPool(
      logical_device_, &pool_info, nullptr, &object->descriptor_pool_));

  std::vector<VkDescriptorSetLayout> layouts{
      1, GetDescriptorLayout("GlobalShadow")->layout_};
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = object->descriptor_pool_;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = layouts.data();
  WIESEL_CHECK_VKRESULT(vkAllocateDescriptorSets(logical_device_, &allocInfo,
                                                 &object->descriptor_set_));

  std::vector<VkWriteDescriptorSet> writes;
  writes.reserve(1);
  std::vector<VkDescriptorBufferInfo> buffer_infos;
  buffer_infos.reserve(1);

  {
    VkDescriptorBufferInfo buffer_info;
    buffer_info.buffer = shadow_camera_uniform_buffer_->buffer_handle_;
    buffer_info.offset = 0;
    buffer_info.range = sizeof(ShadowMapMatricesUniformData);
    buffer_infos.emplace_back(buffer_info);

    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = object->descriptor_set_;
    set.dstBinding = 0;
    set.dstArrayElement = 0;
    set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    set.descriptorCount = 1;
    set.pBufferInfo = &buffer_infos[buffer_infos.size() - 1];
    set.pNext = nullptr;

    writes.emplace_back(set);
  }

  vkUpdateDescriptorSets(logical_device_, static_cast<uint32_t>(writes.size()),
                         writes.data(), 0, nullptr);

  object->allocated_ = true;
  return object;
}

std::shared_ptr<DescriptorSet> Renderer::CreateBoneDescriptors(
    std::shared_ptr<UniformBuffer> bone_ubo) {
  auto desc = std::make_shared<DescriptorSet>();
  desc->SetLayout(GetDescriptorLayout("Bone"));
  desc->AddUniformBuffer(0, bone_ubo);
  desc->Bake();
  return desc;
}

std::shared_ptr<DescriptorSet> Renderer::CreateDescriptors(
    std::shared_ptr<AttachmentTexture> texture) {
  std::shared_ptr<DescriptorSet> object = std::make_shared<DescriptorSet>();

  VkDescriptorPoolSize pool_sizes[] = {
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}};

  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.poolSizeCount = std::size(pool_sizes);
  pool_info.pPoolSizes = pool_sizes;
  pool_info.maxSets = 1;

  // Allocate pool
  WIESEL_CHECK_VKRESULT(vkCreateDescriptorPool(
      logical_device_, &pool_info, nullptr, &object->descriptor_pool_));

  std::vector<VkDescriptorSetLayout> layouts{
      1, GetDescriptorLayout("Present")->layout_};
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = object->descriptor_pool_;
  allocInfo.descriptorSetCount = layouts.size();
  allocInfo.pSetLayouts = layouts.data();
  WIESEL_CHECK_VKRESULT(vkAllocateDescriptorSets(logical_device_, &allocInfo,
                                                 &object->descriptor_set_));

  std::vector<VkWriteDescriptorSet> writes{};

  VkDescriptorImageInfo image_info;
  image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  image_info.imageView = texture->image_views_[0]->handle_;
  image_info.sampler = texture->sampler_ ? texture->sampler_->handle_
                                         : default_linear_sampler_->handle_;
  VkWriteDescriptorSet set{};
  set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  set.dstSet = object->descriptor_set_;
  set.dstBinding = 0;
  set.dstArrayElement = 0;
  set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  set.descriptorCount = 1;
  set.pImageInfo = &image_info;
  set.pNext = nullptr;

  writes.push_back(set);

  vkUpdateDescriptorSets(logical_device_, static_cast<uint32_t>(writes.size()),
                         writes.data(), 0, nullptr);

  object->allocated_ = true;
  return object;
}

std::shared_ptr<DescriptorSet> Renderer::CreateSkyboxDescriptors(
    std::shared_ptr<Texture> texture) {
  std::shared_ptr<DescriptorSet> object = std::make_shared<DescriptorSet>();

  VkDescriptorPoolSize pool_sizes[] = {
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}};

  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.poolSizeCount = std::size(pool_sizes);
  pool_info.pPoolSizes = pool_sizes;
  pool_info.maxSets = 1;

  // Allocate pool
  WIESEL_CHECK_VKRESULT(vkCreateDescriptorPool(
      logical_device_, &pool_info, nullptr, &object->descriptor_pool_));

  std::vector<VkDescriptorSetLayout> layouts{
      1, GetDescriptorLayout("Present")->layout_};
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = object->descriptor_pool_;
  allocInfo.descriptorSetCount = layouts.size();
  allocInfo.pSetLayouts = layouts.data();
  WIESEL_CHECK_VKRESULT(vkAllocateDescriptorSets(logical_device_, &allocInfo,
                                                 &object->descriptor_set_));

  std::vector<VkWriteDescriptorSet> writes{};

  VkDescriptorImageInfo image_info;
  image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  image_info.imageView = texture->image_view_->handle_;
  image_info.sampler = texture->sampler_ ? texture->sampler_->handle()
                                         : default_linear_sampler_->handle();

  VkWriteDescriptorSet set{};
  set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  set.dstSet = object->descriptor_set_;
  set.dstBinding = 0;
  set.dstArrayElement = 0;
  set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  set.descriptorCount = 1;
  set.pImageInfo = &image_info;
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
  LOG_DEBUG("Cleaning up the renderer");
  WaitForGPU();

  camera_ = nullptr;
  quad_index_buffer_ = nullptr;
  quad_vertex_buffer_ = nullptr;

  CleanupGlobalUniformBuffers();
  blank_texture_ = nullptr;
  ssao_noise_ = nullptr;
  pick_entity_id_image_ = nullptr;

  if (pick_staging_buffer_ != VK_NULL_HANDLE) {
    vmaDestroyBuffer(vma_allocator_, pick_staging_buffer_, pick_staging_alloc_);
    pick_staging_buffer_ = VK_NULL_HANDLE;
    pick_staging_alloc_ = VK_NULL_HANDLE;
  }

  LOG_DEBUG("Destroying graphics");
  CleanupPresentGraphics();

  LOG_DEBUG("Destroying descriptor set layout");
  CleanupDescriptorLayouts();

  LOG_DEBUG("Destroying semaphores and fences");
  for (uint32_t i = 0; i < kMaxFramesInFlight; i++) {
    vkDestroySemaphore(logical_device_, image_available_semaphores_[i],
                       nullptr);
    vkDestroySemaphore(logical_device_, render_order_semaphores_[i], nullptr);
    vkDestroyFence(logical_device_, fences_[i], nullptr);
  }
  for (VkSemaphore sem : render_finished_semaphores_) {
    vkDestroySemaphore(logical_device_, sem, nullptr);
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
  deletion_queue_.FlushAll();

  LOG_DEBUG("Destroying VMA allocator (textures alive: {}, deletion queue: {})",
            Texture::GetTotalTextureCount(), deletion_queue_.Size());

  // Dump all live VMA allocations for leak debugging
  {
    char* stats_json = nullptr;
    vmaBuildStatsString(vma_allocator_, &stats_json, VK_TRUE);
    if (stats_json) {
      // Only log if there are actual allocations remaining
      VmaTotalStatistics vma_stats;
      vmaCalculateStatistics(vma_allocator_, &vma_stats);
      if (vma_stats.total.statistics.allocationCount > 0) {
        LOG_WARN("VMA leak: {} allocations, {} bytes still alive",
                 vma_stats.total.statistics.allocationCount,
                 vma_stats.total.statistics.allocationBytes);
        // Write full dump to file for inspection
        std::ofstream dump("vma_leak_dump.json");
        if (dump.is_open()) {
          dump << stats_json;
          LOG_WARN("VMA allocation dump written to vma_leak_dump.json");
        }
      }
      std::cout << std::flush;
      vmaFreeStatsString(vma_allocator_, stats_json);
    }
  }

  vmaDestroyAllocator(vma_allocator_);
  vma_allocator_ = nullptr;

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
  spirv::Cleanup();
  initialized_ = false;
}

void Renderer::CreateVulkanInstance() {
#ifdef VULKAN_VALIDATION
  if (!CheckValidationLayerSupport()) {
    throw std::runtime_error("validation layers requested, but not available!");
  }
#endif

  VkApplicationInfo app_info{};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pApplicationName = "Wiesel";
  app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.pEngineName = "Wiesel Engine";
  app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.apiVersion = VK_API_VERSION_1_4;

  VkInstanceCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create_info.pApplicationInfo = &app_info;

  std::vector<const char*> extensions = GetRequiredExtensions();
  extensions.emplace_back(
      VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#ifdef __APPLE__
  extensions.emplace_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
  create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

  create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
  create_info.ppEnabledExtensionNames = extensions.data();

#ifdef VULKAN_VALIDATION
  VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};
  create_info.enabledLayerCount =
      static_cast<uint32_t>(validation_layers_.size());
  create_info.ppEnabledLayerNames = validation_layers_.data();

  PopulateDebugMessengerCreateInfo(debug_create_info);
  create_info.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debug_create_info;
#else
  create_info.enabledLayerCount = 0;
  create_info.pNext = nullptr;
#endif

  WIESEL_CHECK_VKRESULT(vkCreateInstance(&create_info, nullptr, &instance_));
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

  for (const VkPhysicalDevice& device : devices) {
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

  std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
  std::set<uint32_t> unique_queue_families = {GetGraphicsQueueFamilyIndex(),
                                              GetPresentQueueFamilyIndex()};

  float queuePriority = 1.0f;
  for (uint32_t queue_family : unique_queue_families) {
    VkDeviceQueueCreateInfo queue_create_info{};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = queue_family;
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = &queuePriority;
    queue_create_infos.push_back(queue_create_info);
  }

  VkPhysicalDeviceFeatures2 device_features2{};
  device_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  device_features2.features.fillModeNonSolid = true;
  device_features2.features.samplerAnisotropy = VK_TRUE;
  device_features2.features.wideLines = VK_TRUE;
  device_features2.features.independentBlend = VK_TRUE;

  VkPhysicalDeviceVulkan11Features vulkan11_features{};
  vulkan11_features.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
  device_features2.pNext = &vulkan11_features;

  VkPhysicalDeviceVulkan12Features vulkan12_features{};
  vulkan12_features.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
  vulkan12_features.bufferDeviceAddress = VK_TRUE;
  vulkan11_features.pNext = &vulkan12_features;

  // 1.3 brings dynamic rendering + synchronization2 in core. We use dynamic
  // rendering across every pass in the graph.
  VkPhysicalDeviceVulkan13Features vulkan13_features{};
  vulkan13_features.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  vulkan13_features.dynamicRendering = VK_TRUE;
  vulkan13_features.synchronization2 = VK_TRUE;
  vulkan11_features.pNext = &vulkan13_features;

  // 1.4 promotes dynamic-rendering-local-read to core (lets passes sample
  // attachments still bound as color/depth, without explicit barriers).
  VkPhysicalDeviceVulkan14Features vulkan14_features{};
  vulkan14_features.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;
  vulkan14_features.dynamicRenderingLocalRead = VK_TRUE;
  vulkan13_features.pNext = &vulkan14_features;

  // RT feature structs (chained only when RT is supported)
  VkPhysicalDeviceAccelerationStructureFeaturesKHR as_features{};
  as_features.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
  as_features.accelerationStructure = VK_TRUE;

  VkPhysicalDeviceRayTracingPipelineFeaturesKHR rt_pipeline_features{};
  rt_pipeline_features.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
  rt_pipeline_features.rayTracingPipeline = VK_TRUE;

  if (rt_supported_) {
    vulkan14_features.pNext = &as_features;
    as_features.pNext = &rt_pipeline_features;

    // Query RT properties for SBT alignment
    rt_pipeline_properties_.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
    rt_as_properties_.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
    rt_pipeline_properties_.pNext = &rt_as_properties_;

    VkPhysicalDeviceProperties2 props2{};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &rt_pipeline_properties_;
    vkGetPhysicalDeviceProperties2(physical_device_, &props2);
  }

  VkDeviceCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  create_info.queueCreateInfoCount =
      static_cast<uint32_t>(queue_create_infos.size());
  create_info.pQueueCreateInfos = queue_create_infos.data();
  create_info.pEnabledFeatures = nullptr;  // Using pNext chain instead
  create_info.pNext = &device_features2;
  create_info.enabledExtensionCount =
      static_cast<uint32_t>(device_extensions_.size());
  create_info.ppEnabledExtensionNames = device_extensions_.data();
  create_info.enabledLayerCount = 0;

  if (vkCreateDevice(physical_device_, &create_info, nullptr,
                     &logical_device_) != VK_SUCCESS) {
    throw std::runtime_error("failed to create logical device!");
  }

  vkGetDeviceQueue(logical_device_, GetPresentQueueFamilyIndex(), 0,
                   &present_queue_);
  vkGetDeviceQueue(logical_device_, GetGraphicsQueueFamilyIndex(), 0,
                   &graphics_queue_);
}

void Renderer::InitMemoryAllocator() {
  VmaAllocatorCreateInfo vma_info{};
  vma_info.physicalDevice = physical_device_;
  vma_info.device = logical_device_;
  vma_info.instance = instance_;
  vma_info.vulkanApiVersion = VK_API_VERSION_1_4;
  if (rt_supported_) {
    vma_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
  }
  WIESEL_CHECK_VKRESULT(vmaCreateAllocator(&vma_info, &vma_allocator_));
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
    pfn_vkCmdTraceRaysKHR_ = (PFN_vkCmdTraceRaysKHR)vkGetDeviceProcAddr(
        logical_device_, "vkCmdTraceRaysKHR");
    LOG_INFO("Ray tracing function pointers loaded");
  }
}

bool Renderer::CheckRayTracingSupport(VkPhysicalDevice device) {
  uint32_t extensionCount;
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
                                       nullptr);
  std::vector<VkExtensionProperties> extensions(extensionCount);
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
                                       extensions.data());

  bool has_as = false, has_rt_pipeline = false, has_deferred = false;
  for (const auto& ext : extensions) {
    if (strcmp(ext.extensionName,
               VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) == 0) {
      has_as = true;
    }
    if (strcmp(ext.extensionName, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME) ==
        0) {
      has_rt_pipeline = true;
    }
    if (strcmp(ext.extensionName,
               VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME) == 0) {
      has_deferred = true;
    }
  }
  return has_as && has_rt_pipeline && has_deferred;
}

std::shared_ptr<DescriptorSetLayout> Renderer::GetDescriptorLayout(
    const std::string& name) const {
  auto it = descriptor_layouts_.find(name);
  if (it != descriptor_layouts_.end()) {
    return it->second;
  }
  return nullptr;
}

void Renderer::RegisterDescriptorLayout(
    const std::string& name, std::shared_ptr<DescriptorSetLayout> layout) {
  descriptor_layouts_[name] = std::move(layout);
}

void Renderer::CreateDescriptorLayouts() {
  {
    auto layout = std::make_shared<DescriptorSetLayout>();
    // Per-instance data SSBO: shaders index instances[gl_InstanceIndex].
    layout->AddBinding(
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    for (int i = 0; i < kMaterialTextureCount; i++) {
      layout->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                         VK_SHADER_STAGE_FRAGMENT_BIT);
    }
    layout->Bake();
    RegisterDescriptorLayout("GeometryMesh", std::move(layout));
  }

  {
    auto layout = std::make_shared<DescriptorSetLayout>();
    layout->AddBinding(
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    for (int i = 0; i < kMaterialTextureCount; i++) {
      layout->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                         VK_SHADER_STAGE_FRAGMENT_BIT);
    }
    layout->Bake();
    RegisterDescriptorLayout("ShadowMesh", std::move(layout));
  }

  {
    auto layout = std::make_shared<DescriptorSetLayout>();
    layout->AddBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                       VK_SHADER_STAGE_VERTEX_BIT);
    layout->Bake();
    RegisterDescriptorLayout("GlobalShadow", std::move(layout));
  }

  {
    auto layout = std::make_shared<DescriptorSetLayout>();
    layout->AddBinding(
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    layout->AddBinding(
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    layout->AddBinding(
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    layout->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                       VK_SHADER_STAGE_FRAGMENT_BIT);
    layout->Bake();
    RegisterDescriptorLayout("Global", std::move(layout));
  }

  {
    auto layout = std::make_shared<DescriptorSetLayout>();
    layout->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                       VK_SHADER_STAGE_FRAGMENT_BIT);
    layout->Bake();
    RegisterDescriptorLayout("Present", std::move(layout));
  }

  {
    auto layout = std::make_shared<DescriptorSetLayout>();
    layout->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                       VK_SHADER_STAGE_FRAGMENT_BIT);
    layout->Bake();
    RegisterDescriptorLayout("Skybox", std::move(layout));
  }

  {
    auto layout = std::make_shared<DescriptorSetLayout>();
    layout->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                       VK_SHADER_STAGE_FRAGMENT_BIT);  // irradiance
    layout->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                       VK_SHADER_STAGE_FRAGMENT_BIT);  // prefilter
    layout->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                       VK_SHADER_STAGE_FRAGMENT_BIT);  // brdfLUT
    layout->Bake();
    RegisterDescriptorLayout("IBL", std::move(layout));
  }

  {
    auto layout = std::make_shared<DescriptorSetLayout>();
    layout->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                       VK_SHADER_STAGE_FRAGMENT_BIT);
    layout->Bake();
    RegisterDescriptorLayout("CubemapSampler", std::move(layout));
  }

  {
    auto layout = std::make_shared<DescriptorSetLayout>();
    layout->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                       VK_SHADER_STAGE_FRAGMENT_BIT);
    layout->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                       VK_SHADER_STAGE_FRAGMENT_BIT);
    layout->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                       VK_SHADER_STAGE_FRAGMENT_BIT);
    layout->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                       VK_SHADER_STAGE_FRAGMENT_BIT);
    layout->AddBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                       VK_SHADER_STAGE_FRAGMENT_BIT);
    layout->Bake();
    RegisterDescriptorLayout("SSAOGen", std::move(layout));
  }

  {
    auto layout = std::make_shared<DescriptorSetLayout>();
    layout->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                       VK_SHADER_STAGE_FRAGMENT_BIT);  // samplerSSAO
    layout->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                       VK_SHADER_STAGE_FRAGMENT_BIT);  // samplerDepth
    layout->Bake();
    RegisterDescriptorLayout("SSAOOutput", std::move(layout));
  }

  {
    auto layout = std::make_shared<DescriptorSetLayout>();
    layout->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                       VK_SHADER_STAGE_FRAGMENT_BIT);  // samplerSSAO
    layout->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                       VK_SHADER_STAGE_FRAGMENT_BIT);  // samplerDepth
    layout->Bake();
    RegisterDescriptorLayout("SSAOBlur", std::move(layout));
  }

  {
    auto layout = std::make_shared<DescriptorSetLayout>();
    layout->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                       VK_SHADER_STAGE_FRAGMENT_BIT);
    layout->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                       VK_SHADER_STAGE_FRAGMENT_BIT);
    layout->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                       VK_SHADER_STAGE_FRAGMENT_BIT);
    layout->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                       VK_SHADER_STAGE_FRAGMENT_BIT);
    layout->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                       VK_SHADER_STAGE_FRAGMENT_BIT);
    layout->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                       VK_SHADER_STAGE_FRAGMENT_BIT);
    layout->Bake();
    RegisterDescriptorLayout("GeometryOutput", std::move(layout));
  }

  {
    auto layout = std::make_shared<DescriptorSetLayout>();
    layout->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                       VK_SHADER_STAGE_FRAGMENT_BIT);
    layout->AddBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                       VK_SHADER_STAGE_VERTEX_BIT);
    layout->Bake();
    RegisterDescriptorLayout("SpriteDraw", std::move(layout));
  }

  // 2-sampler layout for bloom composite and motion blur
  {
    auto layout = std::make_shared<DescriptorSetLayout>();
    layout->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                       VK_SHADER_STAGE_FRAGMENT_BIT);
    layout->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                       VK_SHADER_STAGE_FRAGMENT_BIT);
    layout->Bake();
    RegisterDescriptorLayout("Postprocess2Input", std::move(layout));
  }

  {
    auto layout = std::make_shared<DescriptorSetLayout>();
    layout->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                       VK_SHADER_STAGE_FRAGMENT_BIT);
    layout->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                       VK_SHADER_STAGE_FRAGMENT_BIT);
    layout->AddBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                       VK_SHADER_STAGE_FRAGMENT_BIT);
    layout->Bake();
    RegisterDescriptorLayout("TAA", std::move(layout));
  }

  // Bone matrices UBO layout (set 2 for geometry and shadow passes)
  {
    auto layout = std::make_shared<DescriptorSetLayout>();
    layout->AddBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                       VK_SHADER_STAGE_VERTEX_BIT);
    layout->Bake();
    RegisterDescriptorLayout("Bone", std::move(layout));
  }
}

void Renderer::CreateSwapChain() {
  LOG_DEBUG("Creating swap chain");
  swap_chain_details_ = QuerySwapChainSupport(physical_device_);

  VkSurfaceFormatKHR surface_format =
      ChooseSwapSurfaceFormat(swap_chain_details_.formats);
  VkPresentModeKHR present_mode =
      ChooseSwapPresentMode(swap_chain_details_.present_modes);
  extent_ = ChooseSwapExtent(swap_chain_details_.capabilities);

  uint32_t image_count = swap_chain_details_.capabilities.minImageCount + 1;

  if (swap_chain_details_.capabilities.maxImageCount > 0 &&
      image_count > swap_chain_details_.capabilities.maxImageCount) {
    image_count = swap_chain_details_.capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  create_info.surface = surface_;
  create_info.minImageCount = image_count;
  create_info.imageFormat = surface_format.format;
  create_info.imageColorSpace = surface_format.colorSpace;
  create_info.imageExtent = extent_;
  create_info.imageArrayLayers = 1;
  create_info.imageUsage =
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

  uint32_t graphics_queue_family_index = GetGraphicsQueueFamilyIndex();
  uint32_t present_queue_family_index = GetPresentQueueFamilyIndex();

  uint32_t queue_family_indices[] = {graphics_queue_family_index,
                                     present_queue_family_index};

  if (graphics_queue_family_index != present_queue_family_index) {
    create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    create_info.queueFamilyIndexCount = 2;
    create_info.pQueueFamilyIndices = queue_family_indices;
  } else {
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.queueFamilyIndexCount = 0;      // Optional
    create_info.pQueueFamilyIndices = nullptr;  // Optional
  }
  create_info.preTransform = swap_chain_details_.capabilities.currentTransform;
  // The compositeAlpha field specifies if the alpha channel should be used for blending with other windows in the window system.
  // You'll almost always want to simply ignore the alpha channel, hence VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR.
  create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  create_info.presentMode = present_mode;
  // If it's clipped, obscured pixels will be ignored hence increasing the performance.
  create_info.clipped = VK_TRUE;
  create_info.oldSwapchain = VK_NULL_HANDLE;

  if (vkCreateSwapchainKHR(logical_device_, &create_info, nullptr,
                           &swap_chain_) != VK_SUCCESS) {
    throw std::runtime_error("failed to create swap chain!");
  }

  std::vector<VkImage> swap_chain_images;
  vkGetSwapchainImagesKHR(logical_device_, swap_chain_, &image_count, nullptr);
  swap_chain_images.resize(image_count);
  vkGetSwapchainImagesKHR(logical_device_, swap_chain_, &image_count,
                          swap_chain_images.data());
  swap_chain_image_format_ = surface_format.format;
  LOG_INFO("Swap chain surface: format={} colorSpace={}",
           static_cast<int>(surface_format.format),
           static_cast<int>(surface_format.colorSpace));

  aspect_ratio_ = extent_.width / (float)extent_.height;
  window_size_.width = extent_.width;
  window_size_.height = extent_.height;
  recreate_swap_chain_ = false;
  swap_chain_created_ = true;
  stats_.swap_chain_images = image_count;
  stats_.frames_in_flight = kMaxFramesInFlight;

  std::shared_ptr<AttachmentTexture> texture =
      std::make_shared<AttachmentTexture>();
  texture->format_ = surface_format.format;
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

  if (options_.msaa_mode > SamplingMode::DISABLED) {
    // With MSAA, render to MSAA color attachment and resolve to swapchain
    present_color_image_ = CreateAttachmentTexture(
        {extent_.width, extent_.height, AttachmentTextureType::Color,
         static_cast<uint32_t>(swap_chain_images.size()),
         swap_chain_image_format_, options_.msaa_mode});
  } else {
    // Without MSAA, render directly to swapchain
    present_color_image_ = swap_chain_texture_;
  }
}

void Renderer::CreatePresentGraphicsPipelines() {
  std::shared_ptr<Shader> present_vertex_shader = CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "engine://shaders/fullscreen_shader.vert"});
  std::shared_ptr<Shader> present_fragment_shader =
      CreateShader({ShaderTypeFragment, ShaderLangGLSL, "main",
                    ShaderSourceSource, "engine://shaders/quad_shader.frag"});
  present_pipeline_ = std::make_shared<Pipeline>(
      PipelineProperties{options_.msaa_mode, CullModeNone, false, true});
  present_pipeline_->AddColorAttachment(swap_chain_image_format_);
  present_pipeline_->AddInputLayout(GetDescriptorLayout("Present"));
  present_pipeline_->AddShader(present_vertex_shader);
  present_pipeline_->AddShader(present_fragment_shader);
  present_pipeline_->Bake();
}

void Renderer::RecreatePipeline(std::shared_ptr<Pipeline> pipeline) {
  pipeline->Bake();
}

std::shared_ptr<Shader> Renderer::CreateShader(ShaderProperties properties) {
  for (const std::string& item : shader_features_) {
    properties.defines.push_back(item);
  }
  return std::make_shared<Shader>(properties);
}

void Renderer::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                            VkMemoryPropertyFlags properties, VkBuffer& buffer,
                            VmaAllocation& allocation, VkDeviceSize alignment) {
  PROFILE_ZONE_SCOPED();
  VkBufferCreateInfo buffer_info{};
  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.size = size;
  buffer_info.usage = usage;
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo alloc_info{};
  alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
  if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
    alloc_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
  }

  if (alignment > 0) {
    WIESEL_CHECK_VKRESULT(
        vmaCreateBufferWithAlignment(vma_allocator_, &buffer_info, &alloc_info,
                                     alignment, &buffer, &allocation, nullptr));
  } else {
    WIESEL_CHECK_VKRESULT(vmaCreateBuffer(vma_allocator_, &buffer_info,
                                          &alloc_info, &buffer, &allocation,
                                          nullptr));
  }
}

void Renderer::CopyBuffer(VkBuffer src_buffer, VkBuffer dst_buffer,
                          VkDeviceSize size) {
  PROFILE_ZONE_SCOPED();
  VkCommandBuffer command_buffer = BeginSingleTimeCommands();
  CopyBuffer(command_buffer, src_buffer, dst_buffer, size);
  EndSingleTimeCommands(command_buffer);
}

void Renderer::CopyBuffer(VkCommandBuffer cmd, VkBuffer src_buffer,
                          VkBuffer dst_buffer, VkDeviceSize size) {
  VkBufferCopy copy_region{};
  copy_region.size = size;
  vkCmdCopyBuffer(cmd, src_buffer, dst_buffer, 1, &copy_region);
}

void Renderer::CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width,
                                 uint32_t height, VkDeviceSize base_offset,
                                 uint32_t layer, uint32_t layer_count) {
  PROFILE_ZONE_SCOPED();
  VkCommandBuffer command_buffer = BeginSingleTimeCommands();
  CopyBufferToImage(command_buffer, buffer, image, width, height, base_offset,
                    layer, layer_count);
  EndSingleTimeCommands(command_buffer);
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
                                     VkImageLayout old_layout,
                                     VkImageLayout new_layout,
                                     uint32_t mip_levels, uint32_t base_layer,
                                     uint32_t layer_count) {
  PROFILE_ZONE_SCOPED();
  VkCommandBuffer command_buffer = BeginSingleTimeCommands();
  TransitionImageLayout(command_buffer, image, format, old_layout, new_layout,
                        mip_levels, base_layer, layer_count);
  EndSingleTimeCommands(command_buffer);
}

void Renderer::TransitionImageLayout(VkCommandBuffer command_buffer,
                                     VkImage image, VkFormat format,
                                     VkImageLayout old_layout,
                                     VkImageLayout new_layout,
                                     uint32_t mip_levels, uint32_t base_layer,
                                     uint32_t layer_count) {
  PROFILE_ZONE_SCOPED();
  VkImageMemoryBarrier2 barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  barrier.oldLayout = old_layout;
  barrier.newLayout = new_layout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  if (new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ||
      old_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ||
      new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL ||
      old_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL) {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (HasStencilComponent(format)) {
      barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
  } else {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  }
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = mip_levels;
  barrier.subresourceRange.baseArrayLayer = base_layer;
  barrier.subresourceRange.layerCount = layer_count;

  switch (old_layout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
      barrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
      barrier.srcAccessMask = VK_ACCESS_2_NONE;
      break;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
      barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
      break;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      barrier.srcStageMask =
          VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
          VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
      barrier.srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      break;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
      barrier.srcStageMask =
          VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
          VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT |
          VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
      barrier.srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
      break;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      barrier.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT |
                             VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
      barrier.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
      break;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      barrier.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
      barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
      break;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      barrier.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
      barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
      break;
    case VK_IMAGE_LAYOUT_GENERAL:
      barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
      barrier.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT |
                              VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
      break;
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
      barrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
      barrier.srcAccessMask = VK_ACCESS_2_NONE;
      break;
    default:
      barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
      barrier.srcAccessMask = VK_ACCESS_2_NONE;
      break;
  }

  switch (new_layout) {
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      barrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
      barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT |
                              VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
      break;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      barrier.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                             VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
      barrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                              VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      break;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
      barrier.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                             VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT |
                             VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
      barrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                              VK_ACCESS_2_SHADER_READ_BIT;
      break;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT |
                             VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
      barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
      break;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      barrier.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
      barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
      break;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      barrier.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
      barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
      break;
    case VK_IMAGE_LAYOUT_GENERAL:
      barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
      barrier.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT |
                              VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
      break;
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
      barrier.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
      barrier.dstAccessMask = VK_ACCESS_2_NONE;
      break;
    default:
      barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
      barrier.dstAccessMask = VK_ACCESS_2_NONE;
      break;
  }

  VkDependencyInfo dep{};
  dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dep.imageMemoryBarrierCount = 1;
  dep.pImageMemoryBarriers = &barrier;
  vkCmdPipelineBarrier2(command_buffer, &dep);
}

void Renderer::CreateCommandPools() {
  command_pool_ = std::make_shared<CommandPool>();
}

void Renderer::CreateCommandBuffers() {
  command_buffers_.resize(kMaxFramesInFlight);
  for (uint32_t i = 0; i < kMaxFramesInFlight; i++) {
    command_buffers_[i] = command_pool_->CreateBuffer();
  }
}

void Renderer::CreatePermanentResources() {
  blank_texture_ = CreateBlankTexture("Renderer::blank_texture_");

  std::vector<Index> quad_indices = {0, 2, 1, 0, 3, 2};
  std::vector<Vertex2DNoColor> quad_vertices = {
      {{-1.0f, -1.0f}, {0.0f, 0.0f}},
      {{1.0f, -1.0f}, {1.0f, 0.0f}},
      {{1.0f, 1.0f}, {1.0f, 1.0f}},
      {{-1.0f, 1.0f}, {0.0f, 1.0f}},
  };

  quad_index_buffer_ = Engine::renderer()->CreateIndexBuffer(
      "Renderer::quad_index_buffer_", quad_indices);
  quad_vertex_buffer_ = Engine::renderer()->CreateVertexBuffer(
      "Renderer::quad_vertex_buffer_", quad_vertices);

  default_linear_sampler_ = std::make_shared<Sampler>(1, SamplerProps{});
  default_nearest_sampler_ = std::make_shared<Sampler>(
      1, SamplerProps{VK_FILTER_NEAREST, VK_FILTER_NEAREST, -1.0f});
  shadow_sampler_ = std::make_shared<Sampler>(
      1, SamplerProps{VK_FILTER_LINEAR, VK_FILTER_LINEAR, -1.0f,
                      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                      VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE, VK_TRUE,
                      VK_COMPARE_OP_LESS_OR_EQUAL});

  // SSAO
  ssao_kernel_uniform_buffer_ = CreateUniformBuffer(
      "Renderer::ssao_kernel_uniform_buffer_", sizeof(SSAOKernelUniformData));
  std::default_random_engine rnd_engine((unsigned)time(nullptr));
  std::uniform_real_distribution<float> rnd_dist(0.0f, 1.0f);

  // Sample kernel
  for (uint32_t i = 0; i < WIESEL_SSAO_KERNEL_SIZE; ++i) {
    glm::vec3 sample(rnd_dist(rnd_engine) * 2.0 - 1.0,
                     rnd_dist(rnd_engine) * 2.0 - 1.0, rnd_dist(rnd_engine));
    sample = glm::normalize(sample);
    sample *= rnd_dist(rnd_engine);
    float scale = float(i) / float(WIESEL_SSAO_KERNEL_SIZE);
    scale = std::lerp(0.1f, 1.0f, scale * scale);
    ssao_kernel_uniform_data_.samples[i] = glm::vec4(sample * scale, 0.0f);
  }
  memcpy(ssao_kernel_uniform_buffer_->data_, &ssao_kernel_uniform_data_,
         sizeof(ssao_kernel_uniform_data_));

  // Random noise
  std::vector<glm::vec4> noiseValues(WIESEL_SSAO_NOISE_DIM *
                                     WIESEL_SSAO_NOISE_DIM);
  for (uint32_t i = 0; i < static_cast<uint32_t>(noiseValues.size()); i++) {
    noiseValues[i] = glm::vec4(rnd_dist(rnd_engine) * 2.0f - 1.0f,
                               rnd_dist(rnd_engine) * 2.0f - 1.0f, 0.0f, 0.0f);
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
  CreateBuffer(sizeof(float), VK_BUFFER_USAGE_TRANSFER_DST_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               pick_staging_buffer_, pick_staging_alloc_);

  // Identity bone UBO (shared by all static / non-animated models)
  identity_bone_ubo_ = CreateUniformBuffer("Renderer::identity_bone_ubo_",
                                           sizeof(BoneMatricesUniformData));
  {
    BoneMatricesUniformData identity{};
    for (auto& m : identity.bone_matrices) {
      m = glm::mat4(1.0f);
    }
    memcpy(identity_bone_ubo_->data_, &identity,
           sizeof(BoneMatricesUniformData));
  }
  identity_bone_descriptor_ = CreateBoneDescriptors(identity_bone_ubo_);
}

void Renderer::CreateImage(uint32_t width, uint32_t height, uint32_t mip_levels,
                           SamplingMode sampling_mode, VkFormat format,
                           VkImageTiling tiling, VkImageUsageFlags usage,
                           VkMemoryPropertyFlags properties, VkImage& image,
                           VmaAllocation& allocation, VkImageCreateFlags flags,
                           uint32_t array_layers) {
  PROFILE_ZONE_SCOPED();
  VkImageCreateInfo image_info{};
  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.extent.width = width;
  image_info.extent.height = height;
  image_info.extent.depth = 1;
  image_info.mipLevels = mip_levels;
  image_info.arrayLayers = array_layers;
  image_info.format = format;
  /*
     * VK_IMAGE_TILING_LINEAR: Texels are laid out in row-major order like our pixels array
     * VK_IMAGE_TILING_OPTIMAL: Texels are laid out in an implementation defined order for optimal access
     */
  image_info.tiling = tiling;
  /*
     * VK_IMAGE_LAYOUT_UNDEFINED: Not usable by the GPU and the very first transition will discard the texels.
     * VK_IMAGE_LAYOUT_PREINITIALIZED: Not usable by the GPU, but the first transition will preserve the texels.
     */
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image_info.usage = usage;
  image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_info.samples = ToVkSampleCountFlagBits(sampling_mode);
  image_info.flags = flags;

  VmaAllocationCreateInfo alloc_info{};
  alloc_info.usage = VMA_MEMORY_USAGE_AUTO;

  WIESEL_CHECK_VKRESULT(vmaCreateImage(vma_allocator_, &image_info, &alloc_info,
                                       &image, &allocation, nullptr));
}

std::shared_ptr<ImageView> Renderer::CreateImageView(
    VkImage image, VkFormat format, VkImageAspectFlags aspect_flags,
    uint32_t mip_levels, VkImageViewType view_type, uint32_t layer,
    uint32_t layer_count) {
  PROFILE_ZONE_SCOPED();
  std::shared_ptr<ImageView> view = std::make_shared<ImageView>();
  view->layer_ = layer;
  view->layer_count_ = layer_count;

  VkImageViewCreateInfo view_info{};
  view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view_info.image = image;
  view_info.viewType = view_type;
  view_info.format = format;
  view_info.subresourceRange.aspectMask = aspect_flags;
  view_info.subresourceRange.baseMipLevel = 0;
  view_info.subresourceRange.levelCount = mip_levels;
  view_info.subresourceRange.baseArrayLayer = layer;
  view_info.subresourceRange.layerCount = layer_count;

  WIESEL_CHECK_VKRESULT(
      vkCreateImageView(logical_device_, &view_info, nullptr, &view->handle_));

  return view;
}

void Renderer::SetObjectName(VkObjectType type, uint64_t handle,
                             const char* name) {
  if (!pfn_set_debug_utils_object_name_ext_) {
    return;  // Silently skip if not available
  }
  VkDebugUtilsObjectNameInfoEXT name_info = {};
  name_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
  name_info.objectType = type;
  name_info.objectHandle = handle;
  name_info.pObjectName = name;
  pfn_set_debug_utils_object_name_ext_(logical_device_, &name_info);
}

std::shared_ptr<ImageView> Renderer::CreateImageViewMip(
    VkImage image, VkFormat format, VkImageAspectFlags aspect_flags,
    uint32_t base_mip_level, uint32_t level_count, VkImageViewType view_type,
    uint32_t layer, uint32_t layer_count) {
  PROFILE_ZONE_SCOPED();
  std::shared_ptr<ImageView> view = std::make_shared<ImageView>();
  view->layer_ = layer;
  view->layer_count_ = layer_count;

  VkImageViewCreateInfo view_info{};
  view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view_info.image = image;
  view_info.viewType = view_type;
  view_info.format = format;
  view_info.subresourceRange.aspectMask = aspect_flags;
  view_info.subresourceRange.baseMipLevel = base_mip_level;
  view_info.subresourceRange.levelCount = level_count;
  view_info.subresourceRange.baseArrayLayer = layer;
  view_info.subresourceRange.layerCount = layer_count;

  WIESEL_CHECK_VKRESULT(
      vkCreateImageView(logical_device_, &view_info, nullptr, &view->handle_));

  return view;
}

std::shared_ptr<ImageView> Renderer::CreateImageView(
    std::shared_ptr<AttachmentTexture> image, VkImageViewType view_type,
    uint32_t layer, uint32_t layer_count) {
  return CreateImageView(image->images_[0], image->format_,
                         image->aspect_flags_, image->mip_levels_, view_type,
                         layer, layer_count);
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

VkFormat Renderer::FindDepthStencilFormat() {
  return FindSupportedFormat(
      {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
      VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

bool Renderer::HasStencilComponent(VkFormat format) {
  return format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
         format == VK_FORMAT_D24_UNORM_S8_UINT ||
         format == VK_FORMAT_D16_UNORM_S8_UINT ||
         format == VK_FORMAT_S8_UINT;
}

bool Renderer::IsIntegerColorFormat(VkFormat format) {
  switch (format) {
    case VK_FORMAT_R32_UINT:
    case VK_FORMAT_R32_SINT:
    case VK_FORMAT_R32G32_UINT:
    case VK_FORMAT_R32G32_SINT:
    case VK_FORMAT_R32G32B32_UINT:
    case VK_FORMAT_R32G32B32_SINT:
    case VK_FORMAT_R32G32B32A32_UINT:
    case VK_FORMAT_R32G32B32A32_SINT:
    case VK_FORMAT_R16_UINT:
    case VK_FORMAT_R16_SINT:
    case VK_FORMAT_R8_UINT:
    case VK_FORMAT_R8_SINT:
      return true;
    default:
      return false;
  }
}

void Renderer::GenerateMipmaps(VkImage image, VkFormat image_format,
                               int32_t tex_width, int32_t tex_height,
                               uint32_t mip_levels) {
  VkCommandBuffer cmd = BeginSingleTimeCommands();
  GenerateMipmaps(cmd, image, image_format, tex_width, tex_height, mip_levels);
  EndSingleTimeCommands(cmd);
}

void Renderer::GenerateMipmaps(VkCommandBuffer cmd, VkImage image,
                               VkFormat image_format, int32_t tex_width,
                               int32_t tex_height, uint32_t mip_levels) {
  PROFILE_ZONE_SCOPED();
  VkFormatProperties format_properties;
  vkGetPhysicalDeviceFormatProperties(physical_device_, image_format,
                                      &format_properties);
  if (!(format_properties.optimalTilingFeatures &
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
    throw std::runtime_error(
        "texture image format does not support linear blitting!");
  }

  VkImageMemoryBarrier2 barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  barrier.image = image;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.subresourceRange.levelCount = 1;

  VkDependencyInfo dep{};
  dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dep.imageMemoryBarrierCount = 1;
  dep.pImageMemoryBarriers = &barrier;

  int32_t mip_width = tex_width;
  int32_t mip_height = tex_height;

  for (uint32_t i = 1; i < mip_levels; i++) {
    barrier.subresourceRange.baseMipLevel = i - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier2(cmd, &dep);

    VkImageBlit blit{};
    blit.srcOffsets[0] = {0, 0, 0};
    blit.srcOffsets[1] = {mip_width, mip_height, 1};
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.mipLevel = i - 1;
    blit.srcSubresource.baseArrayLayer = 0;
    blit.srcSubresource.layerCount = 1;
    blit.dstOffsets[0] = {0, 0, 0};
    blit.dstOffsets[1] = {mip_width > 1 ? mip_width / 2 : 1,
                          mip_height > 1 ? mip_height / 2 : 1, 1};
    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.mipLevel = i;
    blit.dstSubresource.baseArrayLayer = 0;
    blit.dstSubresource.layerCount = 1;

    vkCmdBlitImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                   VK_FILTER_LINEAR);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT |
                           VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    vkCmdPipelineBarrier2(cmd, &dep);

    if (mip_width > 1) {
      mip_width /= 2;
    }
    if (mip_height > 1) {
      mip_height /= 2;
    }
  }

  barrier.subresourceRange.baseMipLevel = mip_levels - 1;
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
  barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
  barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT |
                         VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
  vkCmdPipelineBarrier2(cmd, &dep);
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
  VkSemaphoreCreateInfo semaphore_info{};
  semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fence_info{};
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  image_available_semaphores_.resize(kMaxFramesInFlight);
  render_order_semaphores_.resize(kMaxFramesInFlight);
  fences_.resize(kMaxFramesInFlight);

  // render_finished is indexed by swapchain image, not by frame-in-flight,
  // because the presentation engine holds the semaphore until the image is
  // re-acquired. With N images > kMaxFramesInFlight the FIF-indexed
  // semaphore would be reused before the present completes.
  uint32_t sc_images = stats_.swap_chain_images;
  render_finished_semaphores_.resize(sc_images);

  for (uint32_t i = 0; i < kMaxFramesInFlight; i++) {
    WIESEL_CHECK_VKRESULT(vkCreateSemaphore(logical_device_, &semaphore_info,
                                            nullptr,
                                            &image_available_semaphores_[i]));
    WIESEL_CHECK_VKRESULT(vkCreateSemaphore(logical_device_, &semaphore_info,
                                            nullptr,
                                            &render_order_semaphores_[i]));
    WIESEL_CHECK_VKRESULT(
        vkCreateFence(logical_device_, &fence_info, nullptr, &fences_[i]));
  }
  for (uint32_t i = 0; i < sc_images; i++) {
    WIESEL_CHECK_VKRESULT(vkCreateSemaphore(logical_device_, &semaphore_info,
                                            nullptr,
                                            &render_finished_semaphores_[i]));
  }
}

void Renderer::CleanupDescriptorLayouts() {
  descriptor_layouts_.clear();
}

void Renderer::CleanupPresentGraphics() {
  present_pipeline_ = nullptr;
  present_color_image_ = nullptr;
  present_depth_stencil_ = nullptr;
  swap_chain_texture_ = nullptr;
  vkDestroySwapchainKHR(logical_device_, swap_chain_, nullptr);
}

void Renderer::CreateGlobalUniformBuffers() {
  PROFILE_ZONE_SCOPED();
  lights_uniform_buffer_ = CreateUniformBuffer(
      "Renderer::lights_uniform_buffer_", sizeof(LightsUniformData));
  camera_uniform_buffer_ = CreateUniformBuffer(
      "Renderer::camera_uniform_buffer_", sizeof(CameraUniformData));
  shadow_camera_uniform_buffer_ =
      CreateUniformBuffer("Renderer::shadow_camera_uniform_buffer_",
                          sizeof(ShadowMapMatricesUniformData));

  // Two slices (one per frame-in-flight) × starting capacity. Sized to absorb
  // a scene-worth of entities across shadow cascades + geometry + transparency
  // without needing to grow (growing invalidates live descriptors).
  instance_slice_capacity_ = 65536;
  VkDeviceSize instance_size = static_cast<VkDeviceSize>(kMaxFramesInFlight) *
                               instance_slice_capacity_ *
                               sizeof(MatricesUniformData);
  instance_storage_buffer_ =
      CreateStorageBuffer("Renderer::instance_storage_buffer_", instance_size);
  instance_next_index_ = 0;
}

void Renderer::CleanupGlobalUniformBuffers() {
  lights_uniform_buffer_ = nullptr;
  camera_uniform_buffer_ = nullptr;
  shadow_camera_uniform_buffer_ = nullptr;
  ssao_kernel_uniform_buffer_ = nullptr;
  instance_storage_buffer_ = nullptr;
  identity_bone_ubo_ = nullptr;
  identity_bone_descriptor_ = nullptr;
  default_linear_sampler_ = nullptr;
  default_nearest_sampler_ = nullptr;
  shadow_sampler_ = nullptr;
}

uint32_t Renderer::ReserveInstanceRange(uint32_t count,
                                        MatricesUniformData*& out_ptr) {
  // Grow the buffer if the slice is about to overflow. All draws submitted so
  // far this frame reference the old buffer by handle; since we allocate a new
  // one and update descriptors lazily, callers must reserve before they start
  // batching. This grow path is rare.
  if (instance_next_index_ + count > instance_slice_capacity_) {
    uint32_t new_cap = instance_slice_capacity_;
    while (instance_next_index_ + count > new_cap) {
      new_cap *= 2;
    }
    LOG_WARN(
        "Instance SSBO exhausted ({}/{}), growing to {}. Update descriptors.",
        instance_next_index_ + count, instance_slice_capacity_, new_cap);
    VkDeviceSize instance_size = static_cast<VkDeviceSize>(kMaxFramesInFlight) *
                                 new_cap * sizeof(MatricesUniformData);
    // Queue the old buffer for deletion after frames-in-flight drain.
    auto old = instance_storage_buffer_;
    GetDeletionQueue().Push([old]() { (void)old; });
    instance_storage_buffer_ = CreateStorageBuffer(
        "Renderer::instance_storage_buffer_", instance_size);
    instance_slice_capacity_ = new_cap;
    invalidate_model_descriptors_ = true;
  }

  uint32_t slice_base = current_frame_ * instance_slice_capacity_;
  uint32_t first_instance = slice_base + instance_next_index_;
  auto* base = static_cast<MatricesUniformData*>(instance_storage_buffer_->data_);
  out_ptr = base + first_instance;
  instance_next_index_ += count;
  return first_instance;
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

  // Recreate semaphores so the old swapchain's presentation engine doesn't
  // hold stale references to them
  for (uint32_t i = 0; i < kMaxFramesInFlight; i++) {
    vkDestroySemaphore(logical_device_, image_available_semaphores_[i],
                       nullptr);
  }
  for (auto sem : render_finished_semaphores_) {
    vkDestroySemaphore(logical_device_, sem, nullptr);
  }
  render_finished_semaphores_.clear();

  CleanupPresentGraphics();
  CreateSwapChain();
  CreatePresentGraphicsPipelines();

  // Recreate semaphores with the (possibly new) swapchain image count.
  VkSemaphoreCreateInfo semaphore_info{};
  semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  for (uint32_t i = 0; i < kMaxFramesInFlight; i++) {
    WIESEL_CHECK_VKRESULT(vkCreateSemaphore(logical_device_, &semaphore_info,
                                            nullptr,
                                            &image_available_semaphores_[i]));
  }
  render_finished_semaphores_.resize(stats_.swap_chain_images);
  for (uint32_t i = 0; i < stats_.swap_chain_images; i++) {
    WIESEL_CHECK_VKRESULT(vkCreateSemaphore(logical_device_, &semaphore_info,
                                            nullptr,
                                            &render_finished_semaphores_[i]));
  }

  // Notify that pipelines were recreated so cameras can recreate their resources
  PipelineRecreatedEvent event{};
  Engine::window()->GetEventHandler()(event);
}

void Renderer::SetViewport(VkExtent2D extent, VkCommandBuffer cmd) {
  PROFILE_ZONE_SCOPED();
  VkCommandBuffer cb = ResolveCmd(cmd);
  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = static_cast<float>(extent.width);
  viewport.height = static_cast<float>(extent.height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(cb, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = extent;
  vkCmdSetScissor(cb, 0, 1, &scissor);
}

void Renderer::SetViewport(glm::vec2 extent, VkCommandBuffer cmd) {
  PROFILE_ZONE_SCOPED();
  VkCommandBuffer cb = ResolveCmd(cmd);
  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = extent.x;
  viewport.height = extent.y;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(cb, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent.width = extent.x;
  scissor.extent.height = extent.y;
  vkCmdSetScissor(cb, 0, 1, &scissor);
}

void Renderer::SetScissor(int x, int y, int width, int height,
                          VkCommandBuffer cmd) {
  VkCommandBuffer cb = ResolveCmd(cmd);
  VkRect2D scissor{};
  scissor.offset = {std::max(x, 0), std::max(y, 0)};
  scissor.extent = {static_cast<uint32_t>(std::max(width, 0)),
                    static_cast<uint32_t>(std::max(height, 0))};
  vkCmdSetScissor(cb, 0, 1, &scissor);
}

void Renderer::BeginRender() {
  PROFILE_ZONE_SCOPED();
  stats_.Reset();
  slice_pool_used_[current_frame_] = 0;
  instance_next_index_ = 0;
  transient_resource_pool_->BeginFrame();

  // Wait for this frame slot's previous work to complete
  WIESEL_CHECK_VKRESULT(vkWaitForFences(
      logical_device_, 1, &fences_[current_frame_], VK_TRUE, UINT64_MAX));
  WIESEL_CHECK_VKRESULT(
      vkResetFences(logical_device_, 1, &fences_[current_frame_]));

  command_buffers_[current_frame_]->Reset();
  command_buffers_[current_frame_]->Begin();

  // Reloading stuff
  if (recreate_swap_chain_) {
    PROFILE_ZONE_SCOPED_N("Renderer::BeginRender: Recreate swap chain");
    RecreateSwapChain();
    recreate_swap_chain_ = false;
    recreate_pipeline_ = false;  // Already handled in RecreateSwapChain
    recreate_resources_ =
        true;  // Features need resource rebuild for new MSAA/format
  }
  if (recreate_pipeline_) {
    PROFILE_ZONE_SCOPED_N("Renderer::BeginRender: Recreate Pipeline");
    recreate_pipeline_ = false;
    recreate_resources_ = true;
    PipelineRecreatedEvent event{};
    Engine::window()->GetEventHandler()(event);
  }
}

bool Renderer::BeginPresent() {
  PROFILE_ZONE_SCOPED();
  VkResult result =
      vkAcquireNextImageKHR(logical_device_, swap_chain_, UINT64_MAX,
                            image_available_semaphores_[current_frame_],
                            VK_NULL_HANDLE, &image_index_);
  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    LOG_INFO(
        "Received VK_ERROR_OUT_OF_DATE_KHR, trying to recreate swap chain.");
    recreate_swap_chain_ = true;
    // End the command buffer and submit work so the fence and render_order
    // semaphore get signaled (prevents next frame deadlock).  We only signal
    // render_order here -- render_finished is skipped because the
    // presentation engine may still reference it from a prior present, and
    // signaling it again would violate the Vulkan spec.
    command_buffers_[current_frame_]->End();
    VkCommandBufferSubmitInfo cmd_info{};
    cmd_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    cmd_info.commandBuffer = command_buffers_[current_frame_]->handle_;
    VkSemaphoreSubmitInfo signal_info{};
    signal_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signal_info.semaphore = render_order_semaphores_[current_frame_];
    signal_info.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    VkSubmitInfo2 empty_submit{};
    empty_submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    empty_submit.commandBufferInfoCount = 1;
    empty_submit.pCommandBufferInfos = &cmd_info;
    empty_submit.signalSemaphoreInfoCount = 1;
    empty_submit.pSignalSemaphoreInfos = &signal_info;
    {
      std::lock_guard<std::mutex> lock(queue_submit_mutex_);
      WIESEL_CHECK_VKRESULT(vkQueueSubmit2(graphics_queue_, 1, &empty_submit,
                                           fences_[current_frame_]));
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
      TransitionImageLayout(command_buffers_[current_frame_]->handle_,
                            final_image->images_[0], final_image->format_,
                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 0, 1);
    }
  }

  VkCommandBuffer cmd = command_buffers_[current_frame_]->handle_;
  const bool msaa = options_.msaa_mode > SamplingMode::DISABLED;

  // Transition the swap-chain image from PRESENT_SRC_KHR to
  // COLOR_ATTACHMENT_OPTIMAL. When MSAA is on the swapchain is a resolve
  // target; without MSAA it's the direct color target. Either way it
  // starts in PRESENT_SRC_KHR after acquire (or UNDEFINED on first frame,
  // which TransitionImageLayout handles).
  TransitionImageLayout(cmd, swap_chain_texture_->images_[image_index_],
                        swap_chain_image_format_,
                        swap_chain_texture_->current_layout_,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1, 0, 1);
  swap_chain_texture_->current_layout_ =
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkRenderingAttachmentInfo color_info{};
  color_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  color_info.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  color_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_info.clearValue.color = {
      {clear_color_.red, clear_color_.green, clear_color_.blue,
       clear_color_.alpha}};

  if (msaa) {
    color_info.imageView =
        present_color_image_->image_views_[image_index_]->handle_;
    color_info.resolveImageView =
        swap_chain_texture_->image_views_[image_index_]->handle_;
    color_info.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_info.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
  } else {
    color_info.imageView =
        swap_chain_texture_->image_views_[image_index_]->handle_;
  }

  VkRenderingInfo ri{};
  ri.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  ri.renderArea.offset = {0, 0};
  ri.renderArea.extent = extent_;
  ri.layerCount = 1;
  ri.colorAttachmentCount = 1;
  ri.pColorAttachments = &color_info;

  vkCmdBeginRendering(cmd, &ri);
  present_pipeline_->Bind();
  SetViewport(extent_);
  return true;
}

void Renderer::EndPresent() {
  PROFILE_ZONE_SCOPED();
  VkCommandBuffer cmd = command_buffers_[current_frame_]->handle_;
  vkCmdEndRendering(cmd);

  // Transition the swapchain image back to PRESENT_SRC_KHR for vkQueuePresent.
  TransitionImageLayout(cmd, swap_chain_texture_->images_[image_index_],
                        swap_chain_image_format_,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 1, 0, 1);
  swap_chain_texture_->current_layout_ = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  if (camera_) {
    auto final_image = GetFinalOutputImage();
    if (final_image) {
      TransitionImageLayout(command_buffers_[current_frame_]->handle_,
                            final_image->images_[0], final_image->format_,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1, 0, 1);
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
  VkCommandBufferSubmitInfo cmd_info{};
  cmd_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
  cmd_info.commandBuffer = command_buffers_[current_frame_]->handle_;

  // Wait on image availability AND the previous frame's render order semaphore.
  // The latter serializes GPU execution across frames so shared intermediate
  // render targets don't have layout conflicts.
  uint32_t prev_frame =
      (current_frame_ + kMaxFramesInFlight - 1) % kMaxFramesInFlight;
  VkSemaphoreSubmitInfo wait_infos[2] = {};
  wait_infos[0].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
  wait_infos[0].semaphore = image_available_semaphores_[current_frame_];
  wait_infos[0].stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  wait_infos[1].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
  wait_infos[1].semaphore = render_order_semaphores_[prev_frame];
  wait_infos[1].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  // On the very first frame, no previous frame has signaled the order
  // semaphore yet. Skip the cross-frame wait in that case.
  uint32_t wait_count = frame_counter_ > 0 ? 2 : 1;

  // Signal both render_finished (for present) and render_order (for next frame).
  // render_finished is indexed by swapchain image so the presentation engine
  // never sees the same semaphore reused before the image is re-acquired.
  VkSemaphoreSubmitInfo signal_infos[2] = {};
  signal_infos[0].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
  signal_infos[0].semaphore = render_finished_semaphores_[image_index_];
  signal_infos[0].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  signal_infos[1].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
  signal_infos[1].semaphore = render_order_semaphores_[current_frame_];
  signal_infos[1].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

  VkSubmitInfo2 submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
  submitInfo.waitSemaphoreInfoCount = wait_count;
  submitInfo.pWaitSemaphoreInfos = wait_infos;
  submitInfo.commandBufferInfoCount = 1;
  submitInfo.pCommandBufferInfos = &cmd_info;
  submitInfo.signalSemaphoreInfoCount = 2;
  submitInfo.pSignalSemaphoreInfos = signal_infos;

  VkResult result;
  {
    std::lock_guard<std::mutex> lock(queue_submit_mutex_);
    WIESEL_CHECK_VKRESULT(vkQueueSubmit2(graphics_queue_, 1, &submitInfo,
                                         fences_[current_frame_]));
  }

  VkSemaphore present_wait[1] = {render_finished_semaphores_[image_index_]};
  VkPresentInfoKHR presentInfo{};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = present_wait;

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
  stats_.deletion_queue_pending = static_cast<uint32_t>(deletion_queue_.Size());

  // Query GPU memory usage from VMA
  VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
  vmaGetHeapBudgets(vma_allocator_, budgets);
  VkPhysicalDeviceMemoryProperties mem_props;
  vkGetPhysicalDeviceMemoryProperties(physical_device_, &mem_props);
  uint64_t used = 0;
  uint64_t budget = 0;
  for (uint32_t i = 0; i < mem_props.memoryHeapCount; i++) {
    if (mem_props.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
      used += budgets[i].usage;
      budget += budgets[i].budget;
    }
  }
  stats_.gpu_memory_used = used;
  stats_.gpu_memory_budget = budget;

  VmaTotalStatistics vma_stats;
  vmaCalculateStatistics(vma_allocator_, &vma_stats);
  uint32_t current_alloc_count = vma_stats.total.statistics.allocationCount;
  stats_.gpu_allocation_count = current_alloc_count;
  stats_.texture_memory = Texture::GetTotalTextureMemory();
  stats_.texture_count = Texture::GetTotalTextureCount();

  // Track allocations per second
  uint32_t new_allocs = current_alloc_count > prev_alloc_count_
                            ? current_alloc_count - prev_alloc_count_
                            : 0;
  prev_alloc_count_ = current_alloc_count;
  alloc_rate_accumulator_ += new_allocs;
  alloc_rate_timer_ += stats_.frame_time_ms / 1000.0f;
  if (alloc_rate_timer_ >= 1.0f) {
    stats_.gpu_allocations_per_second = alloc_rate_accumulator_;
    alloc_rate_accumulator_ = 0;
    alloc_rate_timer_ = 0.0f;
  }

  invalidate_model_descriptors_ = false;
  frame_counter_++;
  current_frame_ = (current_frame_ + 1) % kMaxFramesInFlight;
  // After this point the pipeline will be unavailable and that is intentional.
  // Since CameraData references to the component, if Scene gets deleted after EndPresent
  // it causes a crash next frame
  camera_ = nullptr;
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
  VkMemoryBarrier2 barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
  barrier.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
  barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
  barrier.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT |
                         VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
  barrier.dstAccessMask = VK_ACCESS_2_UNIFORM_READ_BIT;
  VkDependencyInfo dep{};
  dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dep.memoryBarrierCount = 1;
  dep.pMemoryBarriers = &barrier;
  vkCmdPipelineBarrier2(cmd, &dep);
}

// Templated helpers for MeshRendererComponent and SkinnedMeshRendererComponent
// which share the same field layout but are separate types (entt requirement).

template <typename T>
static void AllocateMeshRendererGpu(Renderer* renderer, T& mr,
                                    const std::shared_ptr<Material>& material) {
  PROFILE_ZONE_SCOPED();
  if (!mr.material_instance) {
    mr.material_instance = std::make_shared<MaterialInstance>();
    if (mr.material_handle.IsValid()) {
      mr.material_instance->base_material_handle = mr.material_handle;
    }
  }
  if (!mr.geometry_descriptor && material) {
    mr.geometry_descriptor = renderer->CreateMeshDescriptors(material);
  }
  if (!mr.shadow_descriptor && material) {
    mr.shadow_descriptor = renderer->CreateShadowMeshDescriptors(material);
  }
  mr.gpu_allocated = true;
}

template <typename T>
static std::shared_ptr<Material> ResolveMeshMaterial(
    T& mr, const std::shared_ptr<Mesh>& mesh) {
  PROFILE_ZONE_SCOPED();
  if (mr.material_handle.IsValid()) {
    auto mat = Engine::asset_manager().Get<Material>(mr.material_handle);
    if (mat) {
      return mat;
    }
  }
  return mesh->mat;
}

template <typename T>
static void CheckMeshRendererTextureChanges(
    Renderer* renderer, T& mr, const std::shared_ptr<Material>& material,
    uint64_t frame_counter) {
  PROFILE_ZONE_SCOPED();
  if (!material) {
    return;
  }
  // Material texture contents are frame-stable; skip the 8-slot resolve check
  // on repeat visits within the same frame (shadow cascades × phases).
  if (mr.last_texture_check_frame == frame_counter) {
    return;
  }
  mr.last_texture_check_frame = frame_counter;
  bool any_changed = false;
  TextureSlot* slots[] = {
      &material->base_texture, &material->normal_map,  &material->specular_map,
      &material->height_map,   &material->albedo_map,  &material->roughness_map,
      &material->metallic_map, &material->opacity_map,
  };
  for (auto* slot : slots) {
    std::shared_ptr<Texture> tex;
    if (slot->Resolve(tex)) {
      any_changed = true;
    }
  }
  if (any_changed) {
    if (mr.geometry_descriptor) {
      auto old = mr.geometry_descriptor;
      renderer->GetDeletionQueue().Push([old]() { (void)old; });
    }
    if (mr.shadow_descriptor) {
      auto old = mr.shadow_descriptor;
      renderer->GetDeletionQueue().Push([old]() { (void)old; });
    }
    mr.geometry_descriptor = renderer->CreateMeshDescriptors(material);
    mr.shadow_descriptor = renderer->CreateShadowMeshDescriptors(material);
  }
}

template <typename T>
static MatricesUniformData BuildMatricesData(
    T& mr, const TransformComponent& transform, entt::entity entity_handle,
    uint32_t scene_index) {
  PROFILE_ZONE_SCOPED();
  MatricesUniformData m{};
  m.model_matrix = transform.GetTransformMatrix();
  m.normal_matrix = transform.GetNormalMatrix();
  if (entity_handle != entt::null) {
    m.entity_id = (static_cast<uint32_t>(scene_index) << 24) |
                  (static_cast<uint32_t>(entity_handle) + 1);
  }
  if (mr.material_instance) {
    m.color_tint = mr.material_instance->GetColorTint();
    float alpha_cutoff =
        mr.material_instance->GetEffectiveFloat("alpha_cutoff");
    if (alpha_cutoff <= 0.0f) {
      alpha_cutoff = 0.5f;
    }
    m.material_params =
        glm::vec4(mr.material_instance->GetRoughness(),
                  mr.material_instance->GetMetallic(),
                  mr.material_instance->GetSpecular(), alpha_cutoff);
  }
  return m;
}

void Renderer::DrawMeshRenderer(MeshRendererComponent& mr,
                                const TransformComponent& transform,
                                bool shadow_pass, bool transparent_pass,
                                entt::entity entity_handle,
                                std::shared_ptr<DescriptorSet> ibl_descriptor) {
  PROFILE_ZONE_SCOPED();
  auto model_data = Engine::asset_manager().GetOrLoad<Model>(mr.model_handle);
  if (!model_data || mr.mesh_index < 0 ||
      mr.mesh_index >= static_cast<int32_t>(model_data->meshes.size())) {
    return;
  }

  auto& mesh = model_data->meshes[mr.mesh_index];
  // In geometry pass, skip transparent meshes. In transparent pass, skip opaque.
  if (!shadow_pass) {
    if (transparent_pass && !mesh->has_transparency) {
      return;
    }
    if (!transparent_pass && mesh->has_transparency) {
      return;
    }
  }

  auto material = ResolveMeshMaterial(mr, mesh);
  if (!mr.gpu_allocated || invalidate_model_descriptors_) {
    AllocateMeshRendererGpu(this, mr, material);
  }

  CheckMeshRendererTextureChanges(this, mr, material, frame_counter_);

  MatricesUniformData* dst = nullptr;
  uint32_t first_instance = ReserveInstanceRange(1, dst);
  *dst = BuildMatricesData(mr, transform, entity_handle, current_scene_index_);

  VkCommandBuffer cmd = command_buffers_[current_frame_]->handle_;
  auto global_desc =
      shadow_pass
          ? camera_->resource_pool->GetDescriptor("ShadowGlobalDescriptor")
          : camera_->resource_pool->GetDescriptor("GlobalDescriptor");
  auto descriptors =
      shadow_pass ? mr.shadow_descriptor : mr.geometry_descriptor;

  DrawMeshCmd(cmd, mesh, descriptors, identity_bone_descriptor_, global_desc,
              ibl_descriptor, first_instance);
  stats_.models++;
}

void Renderer::DrawSkinnedMeshRenderer(
    SkinnedMeshRendererComponent& mr, const TransformComponent& transform,
    const SkeletalAnimRuntime* skel, bool shadow_pass, bool transparent_pass,
    entt::entity entity_handle, std::shared_ptr<DescriptorSet> ibl_descriptor) {
  PROFILE_ZONE_SCOPED();
  auto model_data = Engine::asset_manager().GetOrLoad<Model>(mr.model_handle);
  if (!model_data || mr.mesh_index < 0 ||
      mr.mesh_index >= static_cast<int32_t>(model_data->meshes.size())) {
    return;
  }

  auto& mesh = model_data->meshes[mr.mesh_index];
  if (!shadow_pass) {
    if (transparent_pass && !mesh->has_transparency) {
      return;
    }
    if (!transparent_pass && mesh->has_transparency) {
      return;
    }
  }

  auto material = ResolveMeshMaterial(mr, mesh);
  if (!mr.gpu_allocated || invalidate_model_descriptors_) {
    AllocateMeshRendererGpu(this, mr, material);
  }

  CheckMeshRendererTextureChanges(this, mr, material, frame_counter_);

  MatricesUniformData* dst = nullptr;
  uint32_t first_instance = ReserveInstanceRange(1, dst);
  *dst = BuildMatricesData(mr, transform, entity_handle, current_scene_index_);

  std::shared_ptr<DescriptorSet> bone_desc = identity_bone_descriptor_;
  if (skel && skel->bone_descriptor) {
    bone_desc = skel->bone_descriptor;
  }

  VkCommandBuffer cmd = command_buffers_[current_frame_]->handle_;
  auto global_desc =
      shadow_pass
          ? camera_->resource_pool->GetDescriptor("ShadowGlobalDescriptor")
          : camera_->resource_pool->GetDescriptor("GlobalDescriptor");
  auto descriptors =
      shadow_pass ? mr.shadow_descriptor : mr.geometry_descriptor;

  DrawMeshCmd(cmd, mesh, descriptors, bone_desc, global_desc, ibl_descriptor,
              first_instance);
  stats_.models++;
}

bool Renderer::PrepareMesh(MeshRendererComponent& mr, MeshDrawPrep& out) {
  PROFILE_ZONE_SCOPED();
  auto model_data = Engine::asset_manager().GetOrLoad<Model>(mr.model_handle);
  if (!model_data || mr.mesh_index < 0 ||
      mr.mesh_index >= static_cast<int32_t>(model_data->meshes.size())) {
    return false;
  }
  out.mesh = model_data->meshes[mr.mesh_index];
  out.material = ResolveMeshMaterial(mr, out.mesh);
  if (!mr.gpu_allocated || invalidate_model_descriptors_) {
    AllocateMeshRendererGpu(this, mr, out.material);
  }
  CheckMeshRendererTextureChanges(this, mr, out.material, frame_counter_);
  out.geometry_descriptor = mr.geometry_descriptor;
  out.shadow_descriptor = mr.shadow_descriptor;
  return true;
}

bool Renderer::PrepareMesh(SkinnedMeshRendererComponent& mr,
                           MeshDrawPrep& out) {
  PROFILE_ZONE_SCOPED();
  auto model_data = Engine::asset_manager().GetOrLoad<Model>(mr.model_handle);
  if (!model_data || mr.mesh_index < 0 ||
      mr.mesh_index >= static_cast<int32_t>(model_data->meshes.size())) {
    return false;
  }
  out.mesh = model_data->meshes[mr.mesh_index];
  out.material = ResolveMeshMaterial(mr, out.mesh);
  if (!mr.gpu_allocated || invalidate_model_descriptors_) {
    AllocateMeshRendererGpu(this, mr, out.material);
  }
  CheckMeshRendererTextureChanges(this, mr, out.material, frame_counter_);
  out.geometry_descriptor = mr.geometry_descriptor;
  out.shadow_descriptor = mr.shadow_descriptor;
  return true;
}

void Renderer::UpdateDrawStats(const std::shared_ptr<Mesh>& mesh,
                               uint32_t instance_count) {
  PROFILE_ZONE_SCOPED();
  stats_.draw_calls++;
  if (instance_count > 1) {
    stats_.instanced_draw_calls++;
    // Each instance past the first is a draw call the CPU did not have to
    // record or submit - that is exactly what batching buys us.
    stats_.saved_by_batching += instance_count - 1;
  } else {
    stats_.single_draw_calls++;
  }
  stats_.total_instances += instance_count;
  stats_.meshes += instance_count;
  stats_.vertices +=
      static_cast<uint32_t>(mesh->vertices.size()) * instance_count;
  stats_.triangles +=
      (static_cast<uint32_t>(mesh->indices.size()) / 3) * instance_count;
}

void Renderer::DrawMeshSimple(VkCommandBuffer cmd, std::shared_ptr<Mesh> mesh,
                              std::shared_ptr<DescriptorSet> bone_descriptor) {
  PROFILE_ZONE_SCOPED();
  if (!mesh->allocated_) {
    return;
  }
  VkBuffer vb[] = {mesh->vertex_buffer->buffer_handle_};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(cmd, 0, 1, vb, offsets);
  vkCmdBindIndexBuffer(cmd, mesh->index_buffer->buffer_handle_, 0,
                       mesh->index_buffer->index_type_);
  if (bone_descriptor) {
    bound_pipeline_->BindDescriptorSets(cmd, {bone_descriptor});
  }
  uint32_t index_count = static_cast<uint32_t>(mesh->indices.size());
  vkCmdDrawIndexed(cmd, index_count, 1, 0, 0, 0);
}

void Renderer::DrawMeshCmd(VkCommandBuffer cmd, std::shared_ptr<Mesh> mesh,
                           std::shared_ptr<DescriptorSet> mesh_descriptors,
                           std::shared_ptr<DescriptorSet> bone_descriptors,
                           std::shared_ptr<DescriptorSet> global_descriptors,
                           std::shared_ptr<DescriptorSet> ibl_descriptors,
                           uint32_t first_instance) {
  PROFILE_ZONE_SCOPED();
  if (!mesh->allocated_) {
    return;
  }

  VkBuffer vertexBuffers[] = {mesh->vertex_buffer->buffer_handle_};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
  vkCmdBindIndexBuffer(cmd, mesh->index_buffer->buffer_handle_, 0,
                       mesh->index_buffer->index_type_);

  if (ibl_descriptors) {
    VkDescriptorSet sets[4] = {
        mesh_descriptors->descriptor_set_, global_descriptors->descriptor_set_,
        bone_descriptors->descriptor_set_, ibl_descriptors->descriptor_set_};
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            bound_pipeline_->layout_, 0, 4, sets, 0, nullptr);
  } else {
    VkDescriptorSet sets[3] = {mesh_descriptors->descriptor_set_,
                               global_descriptors->descriptor_set_,
                               bone_descriptors->descriptor_set_};
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            bound_pipeline_->layout_, 0, 3, sets, 0, nullptr);
  }

  uint32_t index_count = static_cast<uint32_t>(mesh->indices.size());
  vkCmdDrawIndexed(cmd, index_count, 1, 0, 0, first_instance);
  UpdateDrawStats(mesh, 1);
}

void Renderer::RequestEntityPick(
    uint32_t x, uint32_t y,
    std::shared_ptr<AttachmentTexture> entity_id_texture,
    std::shared_ptr<AttachmentTexture> fallback_entity_id_texture) {
  PROFILE_ZONE_SCOPED();
  pick_x_ = x;
  pick_y_ = y;
  pick_entity_id_image_ = entity_id_texture;
  pick_fallback_image_ = fallback_entity_id_texture;
  pick_pending_ = true;
}

bool Renderer::ExecuteEntityPick(entt::entity& out_entity,
                                 uint8_t& out_scene_index) {
  PROFILE_ZONE_SCOPED();
  out_entity = entt::null;
  out_scene_index = 0;
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
  // Use the texture's actual tracked layout for the barrier.
  // Entity ID textures are typically in COLOR_ATTACHMENT_OPTIMAL after
  // render passes, not SHADER_READ_ONLY_OPTIMAL.  Using the wrong
  // oldLayout is undefined behavior and breaks on strict drivers (RADV).
  VkImageLayout src_layout = pick_entity_id_image_->current_layout_;
  if (src_layout == VK_IMAGE_LAYOUT_UNDEFINED) {
    // Fallback: texture not tracked by render graph, assume post-renderpass state
    src_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  }

  VkCommandBuffer cmd = BeginSingleTimeCommands();

  TransitionImageLayout(cmd, src_image, pick_entity_id_image_->format_,
                        src_layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 1, 0,
                        1);

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

  TransitionImageLayout(cmd, src_image, pick_entity_id_image_->format_,
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, src_layout, 1, 0,
                        1);

  EndSingleTimeCommands(cmd);

  // Read back the value
  void* data;
  WIESEL_CHECK_VKRESULT(
      vmaMapMemory(vma_allocator_, pick_staging_alloc_, &data));
  uint32_t value = *static_cast<uint32_t*>(data);
  vmaUnmapMemory(vma_allocator_, pick_staging_alloc_);

  pick_entity_id_image_ = nullptr;

  // If primary texture had no hit, try fallback (canvas entity IDs)
  if (value == 0 && pick_fallback_image_) {
    auto fallback = pick_fallback_image_;
    pick_fallback_image_ = nullptr;

    VkImage fb_image = fallback->images_[0];
    VkImageLayout fb_layout = fallback->current_layout_;
    if (fb_layout == VK_IMAGE_LAYOUT_UNDEFINED) {
      fb_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    VkCommandBuffer cmd2 = BeginSingleTimeCommands();

    TransitionImageLayout(cmd2, fb_image, fallback->format_, fb_layout,
                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 1, 0, 1);

    VkBufferImageCopy fb_region{};
    fb_region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    fb_region.imageOffset = {static_cast<int32_t>(pick_x_),
                             static_cast<int32_t>(pick_y_), 0};
    fb_region.imageExtent = {1, 1, 1};
    vkCmdCopyImageToBuffer(cmd2, fb_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           pick_staging_buffer_, 1, &fb_region);

    TransitionImageLayout(cmd2, fb_image, fallback->format_,
                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, fb_layout, 1, 0,
                          1);

    EndSingleTimeCommands(cmd2);

    WIESEL_CHECK_VKRESULT(
        vmaMapMemory(vma_allocator_, pick_staging_alloc_, &data));
    value = *static_cast<uint32_t*>(data);
    vmaUnmapMemory(vma_allocator_, pick_staging_alloc_);
  } else {
    pick_fallback_image_ = nullptr;
  }

  if (value != 0) {
    out_scene_index = static_cast<uint8_t>(value >> 24);
    uint32_t id = (value & 0x00FFFFFFu) - 1;
    out_entity = static_cast<entt::entity>(id);
  }
  return true;
}

void Renderer::DrawSprite(SpriteRendererComponent& sprite,
                          const TransformComponent& transform) {
  PROFILE_ZONE_SCOPED();
  if (!sprite.sprite_handle_.IsValid()) {
    return;
  }

  // Lazily resolve GPU data from asset manager
  if (!sprite.gpu_data_ || sprite.sprite_handle_ != sprite.bound_sprite_) {
    sprite.gpu_data_ =
        Engine::asset_manager().Get<SpriteGpuData>(sprite.sprite_handle_);
    if (!sprite.gpu_data_) {
      return;
    }

    // Allocate per-instance UBO if needed
    if (!sprite.ubo_) {
      sprite.ubo_ =
          CreateUniformBuffer("Sprite UBO", sizeof(SpriteUniformData));
    }

    // Rebuild descriptor set for the new sprite
    sprite.descriptor_ = std::make_shared<DescriptorSet>();
    sprite.descriptor_->SetLayout(GetDescriptorLayout("SpriteDraw"));
    sprite.descriptor_->AddCombinedImageSampler(0, sprite.gpu_data_->view,
                                                sprite.gpu_data_->sampler);
    sprite.descriptor_->AddUniformBuffer(1, sprite.ubo_);
    sprite.descriptor_->Bake();
    sprite.bound_sprite_ = sprite.sprite_handle_;
  }

  if (!sprite.gpu_data_ || !sprite.descriptor_) {
    return;
  }

  // Update UBO
  SpriteUniformData data{};
  data.model_matrix = transform.GetTransformMatrix();
  data.tint = sprite.tint_;
  data.flip_x = sprite.flip_x_ ? 1 : 0;
  data.flip_y = sprite.flip_y_ ? 1 : 0;
  memcpy(sprite.ubo_->data_, &data, sizeof(SpriteUniformData));

  // Bind and draw
  VkBuffer buffers[] = {sprite.gpu_data_->vertex_buffer->buffer_handle_};
  VkDeviceSize offsets[] = {0};
  static_assert(std::size(buffers) == std::size(offsets));
  vkCmdBindVertexBuffers(command_buffers_[current_frame_]->handle_, 0,
                         std::size(buffers), buffers, offsets);

  VkDescriptorSet sets[] = {
      sprite.descriptor_->descriptor_set_,
      camera_->resource_pool->GetDescriptor("GlobalDescriptor")
          ->descriptor_set_};

  vkCmdBindDescriptorSets(command_buffers_[current_frame_]->handle_,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          bound_pipeline_->layout_, 0, std::size(sets), sets, 0,
                          nullptr);

  vkCmdDraw(command_buffers_[current_frame_]->handle_, 6, 1, 0, 0);
  stats_.draw_calls++;
}

Renderer::SliceDrawResource& Renderer::AcquireSliceResource(
    std::shared_ptr<Texture> texture,
    std::shared_ptr<DescriptorSetLayout> layout) {
  auto& pool = slice_pool_[current_frame_];
  uint32_t idx = slice_pool_used_[current_frame_]++;
  if (idx >= pool.size()) {
    pool.emplace_back();
  }
  auto& res = pool[idx];
  if (!res.ubo) {
    res.ubo =
        CreateUniformBuffer("Slice UBO", sizeof(CanvasElementUniformData));
  }
  VkImageView view = texture->image_view_->handle_;
  VkSampler sampler =
      texture->sampler_ ? texture->sampler_->handle() : VK_NULL_HANDLE;
  if (!res.descriptor || res.bound_texture != view ||
      res.bound_sampler != sampler) {
    res.descriptor = std::make_shared<DescriptorSet>();
    res.descriptor->SetLayout(layout);
    res.descriptor->AddUniformBuffer(0, res.ubo);
    res.descriptor->AddCombinedImageSampler(1, texture->image_view_,
                                            texture->sampler_);
    res.descriptor->Bake();
    res.bound_texture = view;
    res.bound_sampler = sampler;
  }
  return res;
}

void Renderer::DrawTexturedRect(glm::vec2 position, glm::vec2 size,
                                std::shared_ptr<Texture> texture,
                                glm::vec4 tint, glm::vec4 uv_rect,
                                std::shared_ptr<DescriptorSetLayout> layout,
                                uint32_t entity_id) {
  if (!texture || !texture->is_allocated_) {
    return;
  }

  // Look up 9-slice borders from texture asset properties
  glm::vec4 slice_border{0};
  if (!texture->path_.empty()) {
    auto handle = Engine::asset_manager().FindBySourcePath(texture->path_);
    if (handle.IsValid()) {
      const auto* meta = Engine::asset_manager().GetMetadata(handle);
      if (meta) {
        const auto* props = meta->GetProperties<TextureAssetProperties>();
        if (props) {
          slice_border = props->slice_border;
        }
      }
    }
  }

  bool sliced = slice_border.x > 0 || slice_border.y > 0 ||
                slice_border.z > 0 || slice_border.w > 0;

  if (!sliced) {
    // Single quad
    auto& res = AcquireSliceResource(texture, layout);
    CanvasElementUniformData data{};
    data.position = position;
    data.size = size;
    data.color = tint;
    data.uv_rect = uv_rect;
    data.entity_id = entity_id;
    memcpy(res.ubo->data_, &data, sizeof(CanvasElementUniformData));

    VkDescriptorSet sets[] = {res.descriptor->descriptor_set_};
    vkCmdBindDescriptorSets(command_buffers_[current_frame_]->handle_,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            bound_pipeline_->layout_, 0, 1, sets, 0, nullptr);
    vkCmdDraw(command_buffers_[current_frame_]->handle_, 6, 1, 0, 0);
    stats_.draw_calls++;
  } else {
    // 9-slice rendering
    float bL = slice_border.x;
    float bT = slice_border.y;
    float bR = slice_border.z;
    float bB = slice_border.w;

    float tw = static_cast<float>(texture->width_);
    float th = static_cast<float>(texture->height_);

    // UV borders (normalized)
    float uL = bL / tw;
    float uR = 1.0f - bR / tw;
    float vT = bT / th;
    float vB = 1.0f - bB / th;

    // Screen positions
    float x0 = position.x;
    float y0 = position.y;
    float x3 = x0 + size.x;
    float y3 = y0 + size.y;
    float x1 = x0 + bL;
    float x2 = x3 - bR;
    float y1 = y0 + bT;
    float y2 = y3 - bB;

    // 9 regions: position, size, uv_rect(startU, startV, endU, endV)
    struct SliceRegion {
      glm::vec2 pos;
      glm::vec2 size;
      glm::vec4 uv;
    };

    SliceRegion regions[9] = {
        // Top row
        {{x0, y0}, {bL, bT}, {0, 0, uL, vT}},
        {{x1, y0}, {x2 - x1, bT}, {uL, 0, uR, vT}},
        {{x2, y0}, {bR, bT}, {uR, 0, 1, vT}},
        // Middle row
        {{x0, y1}, {bL, y2 - y1}, {0, vT, uL, vB}},
        {{x1, y1}, {x2 - x1, y2 - y1}, {uL, vT, uR, vB}},
        {{x2, y1}, {bR, y2 - y1}, {uR, vT, 1, vB}},
        // Bottom row
        {{x0, y2}, {bL, bB}, {0, vB, uL, 1}},
        {{x1, y2}, {x2 - x1, bB}, {uL, vB, uR, 1}},
        {{x2, y2}, {bR, bB}, {uR, vB, 1, 1}},
    };

    for (int i = 0; i < 9; i++) {
      auto& r = regions[i];
      if (r.size.x <= 0 || r.size.y <= 0) {
        continue;
      }

      auto& res = AcquireSliceResource(texture, layout);
      CanvasElementUniformData data{};
      data.position = r.pos;
      data.size = r.size;
      data.color = tint;
      data.uv_rect = r.uv;
      data.entity_id = entity_id;
      memcpy(res.ubo->data_, &data, sizeof(CanvasElementUniformData));

      VkDescriptorSet sets[] = {res.descriptor->descriptor_set_};
      vkCmdBindDescriptorSets(command_buffers_[current_frame_]->handle_,
                              VK_PIPELINE_BIND_POINT_GRAPHICS,
                              bound_pipeline_->layout_, 0, 1, sets, 0, nullptr);
      vkCmdDraw(command_buffers_[current_frame_]->handle_, 6, 1, 0, 0);
      stats_.draw_calls++;
    }
  }
}

void Renderer::DrawCanvasDescriptor(
    glm::vec2 position, glm::vec2 size,
    std::shared_ptr<DescriptorSet> descriptor,
    std::shared_ptr<DescriptorSetLayout> /*layout*/, uint32_t entity_id) {
  if (!descriptor) {
    return;
  }

  VkDescriptorSet sets[] = {descriptor->descriptor_set_};
  vkCmdBindDescriptorSets(command_buffers_[current_frame_]->handle_,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          bound_pipeline_->layout_, 0, 1, sets, 0, nullptr);
  vkCmdDraw(command_buffers_[current_frame_]->handle_, 6, 1, 0, 0);
  stats_.draw_calls++;
}

void Renderer::DrawSkybox(std::shared_ptr<Skybox> skybox) {
  std::array<VkDescriptorSet, 2> sets{
      skybox->descriptors_->descriptor_set_,
      camera_->resource_pool->GetDescriptor("GlobalDescriptor")
          ->descriptor_set_};

  vkCmdBindDescriptorSets(command_buffers_[current_frame_]->handle_,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          bound_pipeline_->layout_, 0, 2, sets.data(), 0,
                          nullptr);

  // draw cube via gl_VertexIndex (no vertex/index buffer needed)
  vkCmdDraw(command_buffers_[current_frame_]->handle_, 36, 1, 0, 0);
}

void Renderer::DrawFullscreen(
    std::shared_ptr<Pipeline> pipeline,
    std::initializer_list<std::shared_ptr<DescriptorSet>> descriptors,
    VkCommandBuffer cmd) {
  VkCommandBuffer cb = ResolveCmd(cmd);
  std::vector<VkDescriptorSet> sets;
  for (const auto& item : descriptors) {
    if (!item) {
      continue;
    }
    sets.push_back(item->descriptor_set_);
  }
  if (sets.empty()) {
    LOG_DEBUG("DrawFullscreen called with no valid descriptors, skipping");
    return;
  }

  vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipeline->layout_, 0, sets.size(), sets.data(), 0,
                          nullptr);

  vkCmdDraw(cb, 3, 1, 0, 0);
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

void Renderer::SetCameraData(std::shared_ptr<CameraData> camera_data) {
  camera_ = camera_data;
  viewport_size_ = camera_data->viewport_size;
  camera_uniform_data_.position = camera_data->position;
  camera_uniform_data_.view_matrix = camera_data->view_matrix;
  camera_uniform_data_.projection = camera_data->projection;
  camera_uniform_data_.inv_projection = camera_data->inv_projection;
  camera_uniform_data_.near_plane = camera_data->near_plane;
  camera_uniform_data_.far_plane = camera_data->far_plane;
  shadow_camera_uniform_data_.enable_shadows = camera_data->does_shadow_pass;
  for (int i = 0; i < WIESEL_SHADOW_CASCADE_COUNT; ++i) {
    shadow_camera_uniform_data_.view_projection_matrix[i] =
        camera_data->shadow_map_cascades[i].ViewProjMatrix;
    camera_uniform_data_.cascade_splits[i] =
        camera_data->shadow_map_cascades[i].SplitDepth;
  }
  camera_uniform_data_.enable_ssao = options_.ssao_enabled;
  camera_uniform_data_.debug_cascades = options_.debug_cascades;

  // TAA jitter
  if (options_.aa_mode == AntiAliasingMode::TAA) {
    camera_uniform_data_.prev_view_projection = prev_jittered_vp_;

    int idx = static_cast<int>((taa_frame_index_ % 16) + 1);
    float jitter_x = Halton(idx, 2) - 0.5f;
    float jitter_y = Halton(idx, 3) - 0.5f;
    camera_uniform_data_.projection[2][0] += jitter_x * 2.0f / viewport_size_.x;
    camera_uniform_data_.projection[2][1] += jitter_y * 2.0f / viewport_size_.y;
    camera_uniform_data_.taa_jitter_offset =
        glm::vec2(jitter_x / viewport_size_.x, jitter_y / viewport_size_.y);

    prev_jittered_vp_ =
        camera_uniform_data_.projection * camera_uniform_data_.view_matrix;
    taa_frame_index_++;
  } else {
    camera_uniform_data_.prev_view_projection =
        camera_data->prev_view_projection;
    camera_uniform_data_.taa_jitter_offset = glm::vec2(0.0f);
  }

  camera_uniform_data_.ambient =
      glm::vec4(options_.ambient_color, options_.ambient_intensity);
}

std::shared_ptr<DescriptorSet> Renderer::GetFinalOutputDescriptor() const {
  if (!camera_ || !camera_->resource_pool) {
    return nullptr;
  }
  return camera_->resource_pool->GetDescriptor("PipelineOutputDescriptor");
}

std::shared_ptr<AttachmentTexture> Renderer::GetFinalOutputImage() const {
  if (!camera_ || !camera_->resource_pool) {
    return nullptr;
  }
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
  PROFILE_ZONE_SCOPED();
  VkCommandPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
  poolInfo.queueFamilyIndex = queue_family_indices_.graphics_family.value();
  VkCommandPool pool;
  WIESEL_CHECK_VKRESULT(
      vkCreateCommandPool(logical_device_, &poolInfo, nullptr, &pool));
  return pool;
}

thread_local VkCommandPool Renderer::tl_command_pool_ = VK_NULL_HANDLE;
thread_local VkCommandBuffer Renderer::tl_batch_cmd_ = VK_NULL_HANDLE;
thread_local bool Renderer::tl_batch_active_ = false;
thread_local std::vector<Renderer::StagingResource>
    Renderer::tl_deferred_staging_;

void Renderer::SetThreadCommandPool(VkCommandPool pool) {
  tl_command_pool_ = pool;
}

void Renderer::BeginBatchUpload() {
  if (tl_batch_active_) {
    return;
  }
  tl_batch_active_ = true;
  tl_batch_cmd_ = BeginSingleTimeCommands();
}

void Renderer::EndBatchUpload() {
  if (!tl_batch_active_) {
    return;
  }
  tl_batch_active_ = false;
  VkCommandBuffer cmd = tl_batch_cmd_;
  tl_batch_cmd_ = VK_NULL_HANDLE;

  // Submit all recorded commands in one go
  VkCommandPool pool = tl_command_pool_ != VK_NULL_HANDLE
                           ? tl_command_pool_
                           : command_pool_->handle_;
  EndSingleTimeCommands(cmd, pool);

  // Now that GPU is done, free all deferred staging buffers
  for (auto& staging : tl_deferred_staging_) {
    vmaDestroyBuffer(vma_allocator_, staging.buffer, staging.allocation);
  }
  tl_deferred_staging_.clear();
}

void Renderer::DeferStagingCleanup(VkBuffer buffer, VmaAllocation allocation) {
  tl_deferred_staging_.push_back({buffer, allocation});
}

VkCommandBuffer Renderer::BeginSingleTimeCommands() {
  // In batch mode, return the shared command buffer
  if (tl_batch_active_ && tl_batch_cmd_ != VK_NULL_HANDLE) {
    return tl_batch_cmd_;
  }
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
  WIESEL_CHECK_VKRESULT(
      vkAllocateCommandBuffers(logical_device_, &allocInfo, &commandBuffer));

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  WIESEL_CHECK_VKRESULT(vkBeginCommandBuffer(commandBuffer, &beginInfo));

  return commandBuffer;
}

void Renderer::EndSingleTimeCommands(VkCommandBuffer commandBuffer) {
  // In batch mode, don't submit - the batch will be flushed by EndBatchUpload
  if (tl_batch_active_ && commandBuffer == tl_batch_cmd_) {
    return;
  }
  VkCommandPool pool = tl_command_pool_ != VK_NULL_HANDLE
                           ? tl_command_pool_
                           : command_pool_->handle_;
  EndSingleTimeCommands(commandBuffer, pool);
}

void Renderer::EndSingleTimeCommands(VkCommandBuffer commandBuffer,
                                     VkCommandPool pool) {
  WIESEL_CHECK_VKRESULT(vkEndCommandBuffer(commandBuffer));

  VkCommandBufferSubmitInfo cmd_info{};
  cmd_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
  cmd_info.commandBuffer = commandBuffer;

  VkSubmitInfo2 submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
  submitInfo.commandBufferInfoCount = 1;
  submitInfo.pCommandBufferInfos = &cmd_info;

  VkFenceCreateInfo fenceInfo{};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  VkFence fence;
  WIESEL_CHECK_VKRESULT(
      vkCreateFence(logical_device_, &fenceInfo, nullptr, &fence));

  {
    std::lock_guard<std::mutex> lock(queue_submit_mutex_);
    WIESEL_CHECK_VKRESULT(
        vkQueueSubmit2(graphics_queue_, 1, &submitInfo, fence));
  }

  WIESEL_CHECK_VKRESULT(
      vkWaitForFences(logical_device_, 1, &fence, VK_TRUE, UINT64_MAX));
  vkDestroyFence(logical_device_, fence, nullptr);

  vkFreeCommandBuffers(logical_device_, pool, 1, &commandBuffer);
}

bool Renderer::IsDeviceSuitable(VkPhysicalDevice device) {
  VkPhysicalDeviceProperties props;
  vkGetPhysicalDeviceProperties(device, &props);
  if (props.apiVersion < VK_API_VERSION_1_4) {
    return false;
  }

  QueueFamilyIndices indices = FindQueueFamilies(device);

  bool extensionsSupported = CheckDeviceExtensionSupport(device);
  bool swapChainAdequate = false;
  if (extensionsSupported) {
    SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(device);
    swapChainAdequate = !swapChainSupport.formats.empty() &&
                        !swapChainSupport.present_modes.empty();
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
    // Prefer IMMEDIATE (no vsync), fall back to MAILBOX (low-latency triple buffer)
    for (VkPresentModeKHR mode : available_present_modes) {
      if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
        return mode;
      }
    }
    for (VkPresentModeKHR mode : available_present_modes) {
      if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
        return mode;
      }
    }
  } else {
    // Prefer MAILBOX (triple buffered vsync), fall back to FIFO (standard vsync)
    for (VkPresentModeKHR mode : available_present_modes) {
      if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
        return mode;
      }
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
      indices.graphics_family = i;
    }

    VkBool32 presentSupport = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &presentSupport);

    if (presentSupport) {
      indices.present_family = i;
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
    details.present_modes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        device, surface_, &presentModeCount, details.present_modes.data());
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

}  // namespace wiesel