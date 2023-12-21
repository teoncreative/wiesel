
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

#include "util/imgui/imgui_spectrum.hpp"
#include "util/w_spirv.hpp"
#include "util/w_vectors.hpp"
#include "w_engine.hpp"

namespace Wiesel {

Renderer::Renderer(Ref<AppWindow> window) : m_Window(window) {
  Spirv::Init();
#ifdef VULKAN_VALIDATION
  validationLayers.push_back("VK_LAYER_KHRONOS_validation");
#endif

  m_DeviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
#ifdef __APPLE__
  m_DeviceExtensions.push_back("VK_KHR_portability_subset");
#endif

  m_RecreateGraphicsPipeline = false;
  m_RecreateShaders = false;
  m_EnableWireframe = false;
  m_RecreateSwapChain = false;
  m_SwapChainCreated = false;
  m_Vsync = true;
  m_ImageIndex = 0;
  m_MsaaSamples = VK_SAMPLE_COUNT_1_BIT;
  m_PreviousMsaaSamples = VK_SAMPLE_COUNT_1_BIT;
  m_ClearColor = {0.1f, 0.1f, 0.2f, 1.0f};
}

Renderer::~Renderer() {
    Cleanup();
};

void Renderer::Initialize() {
    CreateVulkanInstance();
#ifdef VULKAN_VALIDATION
    SetupDebugMessenger();
#endif
    CreateSurface();
    PickPhysicalDevice();
    CreateLogicalDevice();
    CreateGlobalUniformBuffers();
    // ---
    CreateSwapChain();
    CreateImageViews();
    CreateDefaultRenderPass();
    CreateDefaultDescriptorSetLayout();
    CreateDefaultGraphicsPipeline();
    CreateCommandPools();
    CreateCommandBuffers();
    CreatePermanentResources();
    CreateColorResources();
    CreateDepthResources();
    CreateFramebuffers();
    CreateSyncObjects();
    m_Initialized = true;
}

VkDevice Renderer::GetLogicalDevice() {
  return m_LogicalDevice;
}

Ref<MemoryBuffer> Renderer::CreateVertexBuffer(std::vector<Vertex> vertices) {
  Ref<MemoryBuffer> memoryBuffer =
      CreateReference<MemoryBuffer>(MemoryTypeVertexBuffer);

  VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               stagingBuffer, stagingBufferMemory);

  void* data;
  vkMapMemory(m_LogicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
  memcpy(data, vertices.data(), (size_t)bufferSize);
  vkUnmapMemory(m_LogicalDevice, stagingBufferMemory);

  CreateBuffer(
      bufferSize,
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memoryBuffer->m_Buffer,
      memoryBuffer->m_BufferMemory);

  CopyBuffer(stagingBuffer, memoryBuffer->m_Buffer, bufferSize);

  vkDestroyBuffer(m_LogicalDevice, stagingBuffer, nullptr);
  vkFreeMemory(m_LogicalDevice, stagingBufferMemory, nullptr);
  return memoryBuffer;
}

void Renderer::DestroyVertexBuffer(MemoryBuffer& buffer) {
  vkDestroyBuffer(m_LogicalDevice, buffer.m_Buffer, nullptr);
  vkFreeMemory(m_LogicalDevice, buffer.m_BufferMemory, nullptr);
}

Ref<MemoryBuffer> Renderer::CreateIndexBuffer(
    std::vector<Index> indices) {
  Ref<MemoryBuffer> memoryBuffer =
      CreateReference<MemoryBuffer>(MemoryTypeIndexBuffer);

  VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               stagingBuffer, stagingBufferMemory);

  void* data;
  vkMapMemory(m_LogicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
  memcpy(data, indices.data(), (size_t)bufferSize);
  vkUnmapMemory(m_LogicalDevice, stagingBufferMemory);

  CreateBuffer(
      bufferSize,
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memoryBuffer->m_Buffer,
      memoryBuffer->m_BufferMemory);

  CopyBuffer(stagingBuffer, memoryBuffer->m_Buffer, bufferSize);

  vkDestroyBuffer(m_LogicalDevice, stagingBuffer, nullptr);
  vkFreeMemory(m_LogicalDevice, stagingBufferMemory, nullptr);

  return memoryBuffer;
}

Ref<UniformBuffer> Renderer::CreateUniformBuffer(VkDeviceSize size) {
  Ref<UniformBuffer> uniformBuffer = CreateReference<UniformBuffer>();

  CreateBuffer(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               uniformBuffer->m_Buffer, uniformBuffer->m_BufferMemory);

  WIESEL_CHECK_VKRESULT(vkMapMemory(m_LogicalDevice,
                                    uniformBuffer->m_BufferMemory, 0, size, 0,
                                    &uniformBuffer->m_Data));

  return uniformBuffer;
}

void Renderer::DestroyIndexBuffer(MemoryBuffer& buffer) {
  vkDeviceWaitIdle(m_LogicalDevice);
  vkDestroyBuffer(m_LogicalDevice, buffer.m_Buffer, nullptr);
  vkFreeMemory(m_LogicalDevice, buffer.m_BufferMemory, nullptr);
}

void Renderer::DestroyUniformBuffer(UniformBuffer& buffer) {
  vkDeviceWaitIdle(m_LogicalDevice);
  vkDestroyBuffer(m_LogicalDevice, buffer.m_Buffer, nullptr);
  vkFreeMemory(m_LogicalDevice, buffer.m_BufferMemory, nullptr);
}

Ref<Texture> Renderer::CreateBlankTexture() {
  Ref<Texture> texture = CreateReference<Texture>(TextureTypeDiffuse, "");

  stbi_uc pixels[] = {255, 255, 255, 255};  // full white
  texture->m_Width = 1;
  texture->m_Height = 1;
  texture->m_Size = texture->m_Width * texture->m_Height * STBI_rgb_alpha;
  texture->m_MipLevels = static_cast<uint32_t>(std::floor(std::log2(
                             std::max(texture->m_Width, texture->m_Height)))) +
                         1;

  VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  CreateBuffer(texture->m_Size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               stagingBuffer, stagingBufferMemory);

  void* data;
  vkMapMemory(m_LogicalDevice, stagingBufferMemory, 0, texture->m_Size, 0,
              &data);
  memcpy(data, pixels, static_cast<size_t>(texture->m_Size));
  vkUnmapMemory(m_LogicalDevice, stagingBufferMemory);

  CreateImage(texture->m_Width, texture->m_Height, texture->m_MipLevels,
              VK_SAMPLE_COUNT_1_BIT, format, VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                  VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture->m_Image,
              texture->m_DeviceMemory);

  TransitionImageLayout(texture->m_Image, format, VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        texture->m_MipLevels);
  CopyBufferToImage(stagingBuffer, texture->m_Image,
                    static_cast<uint32_t>(texture->m_Width),
                    static_cast<uint32_t>(texture->m_Height));

  vkDestroyBuffer(m_LogicalDevice, stagingBuffer, nullptr);
  vkFreeMemory(m_LogicalDevice, stagingBufferMemory, nullptr);

  // todo loading pregenerated mipmaps
  GenerateMipmaps(texture->m_Image, VK_FORMAT_R8G8B8A8_UNORM, texture->m_Width,
                  texture->m_Height, texture->m_MipLevels);

  texture->m_Sampler = CreateTextureSampler(texture->m_MipLevels, {});
  texture->m_ImageView =
      CreateImageView(texture->m_Image, format, VK_IMAGE_ASPECT_COLOR_BIT,
                      texture->m_MipLevels);

  texture->m_IsAllocated = true;
  return texture;
}

Ref<Texture> Renderer::CreateBlankTexture(int width, int height, TextureProps textureProps,
                                SamplerProps samplerProps) {
  Ref<Texture> texture = CreateReference<Texture>(TextureTypeDiffuse, "");

  texture->m_Width = width;
  texture->m_Height = height;
  texture->m_Size = texture->m_Width * texture->m_Height * STBI_rgb_alpha;
  stbi_uc* pixels = new stbi_uc[texture->m_Size];
  std::memset(pixels, 0, texture->m_Size); // full black
  texture->m_MipLevels = static_cast<uint32_t>(std::floor(std::log2(
                             std::max(texture->m_Width, texture->m_Height)))) +
                         1;

  VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  CreateBuffer(texture->m_Size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               stagingBuffer, stagingBufferMemory);

  void* data;
  vkMapMemory(m_LogicalDevice, stagingBufferMemory, 0, texture->m_Size, 0,
              &data);
  memcpy(data, pixels, static_cast<size_t>(texture->m_Size));
  vkUnmapMemory(m_LogicalDevice, stagingBufferMemory);

  CreateImage(texture->m_Width, texture->m_Height, texture->m_MipLevels,
              VK_SAMPLE_COUNT_1_BIT, format, VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                  VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture->m_Image,
              texture->m_DeviceMemory);

  TransitionImageLayout(texture->m_Image, format, VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        texture->m_MipLevels);
  CopyBufferToImage(stagingBuffer, texture->m_Image,
                    static_cast<uint32_t>(texture->m_Width),
                    static_cast<uint32_t>(texture->m_Height));

  vkDestroyBuffer(m_LogicalDevice, stagingBuffer, nullptr);
  vkFreeMemory(m_LogicalDevice, stagingBufferMemory, nullptr);

  // todo loading pregenerated mipmaps
  GenerateMipmaps(texture->m_Image, VK_FORMAT_R8G8B8A8_UNORM, texture->m_Width,
                  texture->m_Height, texture->m_MipLevels);

  texture->m_Sampler = CreateTextureSampler(texture->m_MipLevels, {});
  texture->m_ImageView =
      CreateImageView(texture->m_Image, format, VK_IMAGE_ASPECT_COLOR_BIT,
                      texture->m_MipLevels);

  texture->m_IsAllocated = true;
  return texture;
}

Ref<Texture> Renderer::CreateTexture(const std::string& path,
                                           TextureProps textureProps,
                                           SamplerProps samplerProps) {
  Ref<Texture> texture =
      CreateReference<Texture>(textureProps.Type, path);

  stbi_uc* pixels =
      stbi_load(path.c_str(), &texture->m_Width, &texture->m_Height,
                &texture->m_Channels, STBI_rgb_alpha);

  if (!pixels) {
    throw std::runtime_error("failed to load texture image: " + path);
  }
  texture->m_Size = texture->m_Width * texture->m_Height * STBI_rgb_alpha;
  if (textureProps.GenerateMipmaps) {
    texture->m_MipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(
                               texture->m_Width, texture->m_Height)))) +
                           1;
  } else {
    texture->m_MipLevels = 1;
  }

  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  CreateBuffer(texture->m_Size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               stagingBuffer, stagingBufferMemory);

  void* data;
  vkMapMemory(m_LogicalDevice, stagingBufferMemory, 0, texture->m_Size, 0,
              &data);
  memcpy(data, pixels, static_cast<size_t>(texture->m_Size));
  vkUnmapMemory(m_LogicalDevice, stagingBufferMemory);

  stbi_image_free(pixels);

  CreateImage(texture->m_Width, texture->m_Height, texture->m_MipLevels,
              VK_SAMPLE_COUNT_1_BIT, textureProps.ImageFormat,
              VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                  VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture->m_Image,
              texture->m_DeviceMemory);

  TransitionImageLayout(
      texture->m_Image, textureProps.ImageFormat, VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, texture->m_MipLevels);
  CopyBufferToImage(stagingBuffer, texture->m_Image,
                    static_cast<uint32_t>(texture->m_Width),
                    static_cast<uint32_t>(texture->m_Height));

  vkDestroyBuffer(m_LogicalDevice, stagingBuffer, nullptr);
  vkFreeMemory(m_LogicalDevice, stagingBufferMemory, nullptr);

  // todo loading pregenerated mipmaps
  GenerateMipmaps(texture->m_Image, textureProps.ImageFormat, texture->m_Width,
                  texture->m_Height, texture->m_MipLevels);
  texture->m_Sampler = CreateTextureSampler(texture->m_MipLevels, samplerProps);
  texture->m_ImageView =
      CreateImageView(texture->m_Image, textureProps.ImageFormat,
                      VK_IMAGE_ASPECT_COLOR_BIT, texture->m_MipLevels);

  texture->m_IsAllocated = true;
  return texture;
}

Ref<AttachmentTexture> Renderer::CreateDepthStencil() {
  Ref<AttachmentTexture> texture = CreateReference<AttachmentTexture>();

  VkFormat depthFormat = FindDepthFormat();

  CreateImage(m_Extent.width, m_Extent.height, 1,
              m_MsaaSamples, depthFormat, VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture->m_Image,
              texture->m_DeviceMemory);

  texture->m_ImageView = CreateImageView(texture->m_Image, depthFormat,
                                         VK_IMAGE_ASPECT_DEPTH_BIT, 1);

  TransitionImageLayout(texture->m_Image, depthFormat,
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1);

  texture->m_IsAllocated = true;
  return texture;
}

Ref<AttachmentTexture> Renderer::CreateColorImage() {
  Ref<AttachmentTexture> texture = CreateReference<AttachmentTexture>();

  VkFormat colorFormat = m_SwapChainImageFormat;

  CreateImage(m_Extent.width, m_Extent.height, 1,
              m_MsaaSamples, colorFormat, VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture->m_Image,
              texture->m_DeviceMemory);

  texture->m_ImageView = CreateImageView(texture->m_Image, colorFormat,
                                         VK_IMAGE_ASPECT_COLOR_BIT, 1);

  texture->m_IsAllocated = true;
  return texture;
}

void Renderer::DestroyTexture(Texture& texture) {
  if (!texture.m_IsAllocated) {
    return;
  }

  vkDeviceWaitIdle(m_LogicalDevice);
  vkDestroySampler(m_LogicalDevice, texture.m_Sampler, nullptr);
  vkDestroyImageView(m_LogicalDevice, texture.m_ImageView, nullptr);
  vkDestroyImage(m_LogicalDevice, texture.m_Image, nullptr);
  vkFreeMemory(m_LogicalDevice, texture.m_DeviceMemory, nullptr);

  texture.m_IsAllocated = false;
}

// todo reuse samplers
VkSampler Renderer::CreateTextureSampler(uint32_t mipLevels,
                                         SamplerProps samplerProps) {
  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = samplerProps.MagFilter;
  samplerInfo.minFilter = samplerProps.MinFilter;
  // * VK_SAMPLER_ADDRESS_MODE_REPEAT: Repeat the texture when going beyond the image dimensions.
  // * VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT: Like repeat, but inverts the coordinates to mirror the image when going beyond the dimensions.
  // * VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE: Take the color of the edge closest to the coordinate beyond the image dimensions.
  // * VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE: Like clamp to edge, but instead uses the edge opposite to the closest edge.
  // * VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER: Return a solid color when sampling beyond the dimensions of the image.
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(m_PhysicalDevice, &properties);

  if (samplerProps.MaxAnisotropy <= 0) {
    samplerInfo.anisotropyEnable = VK_FALSE;
  } else {
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = std::min(
        samplerProps.MaxAnisotropy, properties.limits.maxSamplerAnisotropy);
  }
  samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = VK_FALSE;
  samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerInfo.minLod = 0.0f;  // Optional
  samplerInfo.maxLod = static_cast<float>(mipLevels);
  samplerInfo.mipLodBias = 0.0f;  // Optional

  VkSampler sampler;
  WIESEL_CHECK_VKRESULT(
      vkCreateSampler(m_LogicalDevice, &samplerInfo, nullptr, &sampler));
  return sampler;
}

void Renderer::DestroyAttachmentTexture(AttachmentTexture& texture) {
  if (!texture.m_IsAllocated) {
    return;
  }
  vkDeviceWaitIdle(m_LogicalDevice);
  vkDestroyImageView(m_LogicalDevice, texture.m_ImageView, nullptr);
  vkDestroyImage(m_LogicalDevice, texture.m_Image, nullptr);
  vkFreeMemory(m_LogicalDevice, texture.m_DeviceMemory, nullptr);

  texture.m_IsAllocated = false;
}

Ref<DescriptorData> Renderer::CreateDescriptors(
    Ref<UniformBuffer> uniformBuffer, Ref<Material> material) {
  Ref<DescriptorData> object = CreateReference<DescriptorData>();

  VkDescriptorPoolSize poolSizes[] = {
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
       k_MaterialTextureCount}};

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = std::size(poolSizes);
  poolInfo.pPoolSizes = poolSizes;
  poolInfo.maxSets = 1;
  // Allocate pool
  WIESEL_CHECK_VKRESULT(vkCreateDescriptorPool(
      m_LogicalDevice, &poolInfo, nullptr, &object->m_DescriptorPool));

  std::vector<VkDescriptorSetLayout> layouts{
      1, m_DefaultDescriptorLayout->m_Layout};
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = object->m_DescriptorPool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = layouts.data();
  WIESEL_CHECK_VKRESULT(vkAllocateDescriptorSets(m_LogicalDevice,
      &allocInfo,&object->m_DescriptorSet));

  std::vector<VkWriteDescriptorSet> writes{};
    {
      VkDescriptorBufferInfo bufferInfo{};
      bufferInfo.buffer = uniformBuffer->m_Buffer;
      bufferInfo.offset = 0;
      bufferInfo.range = sizeof(UniformBufferObject);

      VkWriteDescriptorSet set{};
      set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      set.dstSet = object->m_DescriptorSet;
      set.dstBinding = 0;
      set.dstArrayElement = 0;
      set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      set.descriptorCount = 1;
      set.pBufferInfo = &bufferInfo;
      set.pNext = nullptr;

      writes.push_back(set);
    }

    {  // move this to another set
      VkDescriptorBufferInfo bufferInfo{};
      bufferInfo.buffer = m_LightsGlobalUbo->m_Buffer;
      bufferInfo.offset = 0;
      bufferInfo.range = sizeof(UniformBufferObject);

      VkWriteDescriptorSet set{};
      set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      set.dstSet = object->m_DescriptorSet;
      set.dstBinding = 1;
      set.dstArrayElement = 0;
      set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      set.descriptorCount = 1;
      set.pBufferInfo = &bufferInfo;
      set.pNext = nullptr;

      writes.push_back(set);
    }

    {  // base texture
      VkDescriptorImageInfo imageInfo{};
      imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      if (material->BaseTexture == nullptr) {
        imageInfo.imageView = m_BlankTexture->m_ImageView;
        imageInfo.sampler = m_BlankTexture->m_Sampler;
      } else {
        imageInfo.imageView = material->BaseTexture->m_ImageView;
        imageInfo.sampler = material->BaseTexture->m_Sampler;
      }

      VkWriteDescriptorSet set{};
      set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      set.dstSet = object->m_DescriptorSet;
      set.dstBinding = 2;
      set.dstArrayElement = 0;
      set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      set.descriptorCount = 1;
      set.pImageInfo = &imageInfo;
      set.pNext = nullptr;

      writes.push_back(set);
    }

    {  // normal texture
      VkDescriptorImageInfo imageInfo{};
      imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      if (material->NormalMap == nullptr) {
        imageInfo.imageView = m_BlankTexture->m_ImageView;
        imageInfo.sampler = m_BlankTexture->m_Sampler;
      } else {
        imageInfo.imageView = material->NormalMap->m_ImageView;
        imageInfo.sampler = material->NormalMap->m_Sampler;
      }

      VkWriteDescriptorSet set{};
      set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      set.dstSet = object->m_DescriptorSet;
      set.dstBinding = 3;
      set.dstArrayElement = 0;
      set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      set.descriptorCount = 1;
      set.pImageInfo = &imageInfo;
      set.pNext = nullptr;

      writes.push_back(set);
    }

    {  // specular texture
      VkDescriptorImageInfo imageInfo{};
      imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      if (material->SpecularMap == nullptr) {
        imageInfo.imageView = m_BlankTexture->m_ImageView;
        imageInfo.sampler = m_BlankTexture->m_Sampler;
      } else {
        imageInfo.imageView = material->SpecularMap->m_ImageView;
        imageInfo.sampler = material->SpecularMap->m_Sampler;
      }

      VkWriteDescriptorSet set{};
      set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      set.dstSet = object->m_DescriptorSet;
      set.dstBinding = 4;
      set.dstArrayElement = 0;
      set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      set.descriptorCount = 1;
      set.pImageInfo = &imageInfo;
      set.pNext = nullptr;

      writes.push_back(set);
    }

    {  // height texture
      VkDescriptorImageInfo imageInfo{};
      imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      if (material->HeightMap == nullptr) {
        imageInfo.imageView = m_BlankTexture->m_ImageView;
        imageInfo.sampler = m_BlankTexture->m_Sampler;
      } else {
        imageInfo.imageView = material->HeightMap->m_ImageView;
        imageInfo.sampler = material->HeightMap->m_Sampler;
      }

      VkWriteDescriptorSet set{};
      set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      set.dstSet = object->m_DescriptorSet;
      set.dstBinding = 5;
      set.dstArrayElement = 0;
      set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      set.descriptorCount = 1;
      set.pImageInfo = &imageInfo;
      set.pNext = nullptr;

      writes.push_back(set);
    }

    {  // albedo texture
      VkDescriptorImageInfo imageInfo{};
      imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      if (material->AlbedoMap == nullptr) {
        imageInfo.imageView = m_BlankTexture->m_ImageView;
        imageInfo.sampler = m_BlankTexture->m_Sampler;
      } else {
        imageInfo.imageView = material->AlbedoMap->m_ImageView;
        imageInfo.sampler = material->AlbedoMap->m_Sampler;
      }

      VkWriteDescriptorSet set{};
      set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      set.dstSet = object->m_DescriptorSet;
      set.dstBinding = 6;
      set.dstArrayElement = 0;
      set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      set.descriptorCount = 1;
      set.pImageInfo = &imageInfo;
      set.pNext = nullptr;

      writes.push_back(set);
    }

    {  // roughness texture
      VkDescriptorImageInfo imageInfo{};
      imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      if (material->RoughnessMap == nullptr) {
        imageInfo.imageView = m_BlankTexture->m_ImageView;
        imageInfo.sampler = m_BlankTexture->m_Sampler;
      } else {
        imageInfo.imageView = material->RoughnessMap->m_ImageView;
        imageInfo.sampler = material->RoughnessMap->m_Sampler;
      }

      VkWriteDescriptorSet set{};
      set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      set.dstSet = object->m_DescriptorSet;
      set.dstBinding = 7;
      set.dstArrayElement = 0;
      set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      set.descriptorCount = 1;
      set.pImageInfo = &imageInfo;
      set.pNext = nullptr;

      writes.push_back(set);
    }

    {  // metallic texture
      VkDescriptorImageInfo imageInfo{};
      imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      if (material->MetallicMap == nullptr) {
        imageInfo.imageView = m_BlankTexture->m_ImageView;
        imageInfo.sampler = m_BlankTexture->m_Sampler;
      } else {
        imageInfo.imageView = material->MetallicMap->m_ImageView;
        imageInfo.sampler = material->MetallicMap->m_Sampler;
      }

      VkWriteDescriptorSet set{};
      set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      set.dstSet = object->m_DescriptorSet;
      set.dstBinding = 8;
      set.dstArrayElement = 0;
      set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      set.descriptorCount = 1;
      set.pImageInfo = &imageInfo;
      set.pNext = nullptr;

      writes.push_back(set);
    }
  vkUpdateDescriptorSets(m_LogicalDevice, static_cast<uint32_t>(writes.size()),
                         writes.data(), 0, nullptr);

  return object;
}

void Renderer::DestroyDescriptors(DescriptorData& descriptorPool) {
  // Destroying the pool is enough to destroy all descriptor set objects.
  vkDestroyDescriptorPool(m_LogicalDevice, descriptorPool.m_DescriptorPool,
                          nullptr);
}

Ref<DescriptorLayout> Renderer::CreateDescriptorLayout() {
  auto layout = CreateReference<DescriptorLayout>();
  std::vector<VkDescriptorSetLayoutBinding> bindings{};
  bindings.reserve(2 + k_MaterialTextureCount);

  VkDescriptorSetLayoutBinding uboLayoutBinding{
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      .pImmutableSamplers = nullptr};
  bindings.push_back(uboLayoutBinding);

  VkDescriptorSetLayoutBinding lightGlobalUboLayoutBinding{
      .binding = static_cast<uint32_t>(bindings.size()),
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      .pImmutableSamplers = nullptr};
  bindings.push_back(lightGlobalUboLayoutBinding);

  for (int i = 0; i < k_MaterialTextureCount; i++) {
    VkDescriptorSetLayoutBinding samplerLayoutBinding{
        .binding = static_cast<uint32_t>(bindings.size()),
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = nullptr};
    bindings.push_back(samplerLayoutBinding);
  }

  VkDescriptorSetLayoutCreateInfo layoutInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = static_cast<uint32_t>(bindings.size()),
      .pBindings = bindings.data()};

  WIESEL_CHECK_VKRESULT(vkCreateDescriptorSetLayout(
      m_LogicalDevice, &layoutInfo, nullptr, &layout->m_Layout));
  layout->m_Allocated = true;
  return layout;
}

void Renderer::DestroyDescriptorLayout(DescriptorLayout& layout) {
  if (!layout.m_Allocated) {
    return;
  }
  layout.m_Allocated = false;
  vkDestroyDescriptorSetLayout(m_LogicalDevice, layout.m_Layout, nullptr);
}

void Renderer::SetClearColor(float r, float g, float b, float a) {
  m_ClearColor.Red = r;
  m_ClearColor.Green = g;
  m_ClearColor.Blue = b;
  m_ClearColor.Alpha = a;
}

void Renderer::SetClearColor(const Colorf& color) {
  m_ClearColor = color;
}

Colorf& Renderer::GetClearColor() {
  return m_ClearColor;
}

void Renderer::SetMsaaSamples(VkSampleCountFlagBits samples) {
  m_MsaaSamples = samples;
}

VkSampleCountFlagBits Renderer::GetMsaaSamples() {
  return m_MsaaSamples;
}

void Renderer::SetVsync(bool vsync) {
  if (vsync == m_Vsync) {
    return;
  }

  m_Vsync = vsync;
  if (m_SwapChainCreated) {
    m_RecreateSwapChain = true;
  }
}

WIESEL_GETTER_FN bool Renderer::IsVsync() {
  return m_Vsync;
}

void Renderer::SetWireframeEnabled(bool value) {
  m_EnableWireframe = value;
  m_RecreateGraphicsPipeline = true;
}

bool Renderer::IsWireframeEnabled() {
  return m_EnableWireframe;
}

bool* Renderer::IsWireframeEnabledPtr() {
  return &m_EnableWireframe;
}

void Renderer::SetRecreateGraphicsPipeline(bool value) {
  m_RecreateGraphicsPipeline = value;
}

bool Renderer::IsRecreateGraphicsPipeline() {
  return m_RecreateGraphicsPipeline;
}

void Renderer::SetRecreateShaders(bool value) {
  m_RecreateShaders = value;
}

bool Renderer::IsRecreateShaders() {
  return m_RecreateGraphicsPipeline;
}

float Renderer::GetAspectRatio() const {
  return m_AspectRatio;
}

const WindowSize& Renderer::GetWindowSize() const {
  return m_WindowSize;
}

LightsUniformBufferObject& Renderer::GetLightsBufferObject() {
  return m_LightsBufferObject;
}

void Renderer::Cleanup() {
  if (!m_Initialized) {
    return;
  }

  vkDeviceWaitIdle(m_LogicalDevice);
  LOG_DEBUG("Destroying Renderer");

  CleanupGlobalUniformBuffers();
  m_BlankTexture = nullptr;

  CleanupSwapChain();

  LOG_DEBUG("Destroying descriptor set layout");
  m_DefaultDescriptorLayout = nullptr;

  LOG_DEBUG("Destroying default graphics pipeline");
  m_DefaultGraphicsPipeline = nullptr;

  LOG_DEBUG("Destroying default render pass");
  CleanupDefaultRenderPass();

  LOG_DEBUG("Destroying semaphores and fences");
  vkDestroySemaphore(m_LogicalDevice, m_RenderFinishedSemaphore, nullptr);
  vkDestroySemaphore(m_LogicalDevice, m_ImageAvailableSemaphore, nullptr);
  vkDestroyFence(m_LogicalDevice, m_Fence, nullptr);

  LOG_DEBUG("Destroying command pool");
  vkDestroyCommandPool(m_LogicalDevice, m_CommandPool, nullptr);

  LOG_DEBUG("Destroying device");
  vkDestroyDevice(m_LogicalDevice, nullptr);

#ifdef VULKAN_VALIDATION
  LOG_DEBUG("Destroying debug messanger");
  DestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
#endif

  LOG_DEBUG("Destroying surface khr");
  vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
  LOG_DEBUG("Destroying vulkan instance");
  vkDestroyInstance(m_Instance, nullptr);

  LOG_DEBUG("Renderer destroyed");
  Spirv::Cleanup();
  m_Initialized = false;
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
  createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
  createInfo.ppEnabledLayerNames = validationLayers.data();

  PopulateDebugMessengerCreateInfo(debugCreateInfo);
  createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
#else
  createInfo.enabledLayerCount = 0;
  createInfo.pNext = nullptr;
#endif

  WIESEL_CHECK_VKRESULT(vkCreateInstance(&createInfo, nullptr, &m_Instance));
}

void Renderer::CreateSurface() {
  m_Window->CreateWindowSurface(m_Instance, &m_Surface);
}

void Renderer::PickPhysicalDevice() {
  uint32_t deviceCount = 0;
  vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);
  LOG_DEBUG("{} devices found!", deviceCount);
  if (deviceCount == 0) {
    throw std::runtime_error("failed to find GPUs with Vulkan support!");
  }

  std::vector<VkPhysicalDevice> devices(deviceCount);
  vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

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
    m_PhysicalDevice = candidates.rbegin()->second;
    m_MsaaSamples = GetMaxUsableSampleCount();
    m_PreviousMsaaSamples = GetMaxUsableSampleCount();
  } else {
    throw std::runtime_error("failed to find a suitable GPU!");
  }
}

void Renderer::CreateLogicalDevice() {
  LOG_DEBUG("Creating logical device");
  QueueFamilyIndices indices = FindQueueFamilies(m_PhysicalDevice);

  std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
  std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(),
                                            indices.presentFamily.value()};

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
  //deviceFeatures.fillModeNonSolid = true;
  deviceFeatures.samplerAnisotropy = VK_TRUE;

  VkDeviceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  createInfo.queueCreateInfoCount =
      static_cast<uint32_t>(queueCreateInfos.size());
  createInfo.pQueueCreateInfos = queueCreateInfos.data();
  createInfo.pEnabledFeatures = &deviceFeatures;
  createInfo.enabledExtensionCount =
      static_cast<uint32_t>(m_DeviceExtensions.size());
  createInfo.ppEnabledExtensionNames = m_DeviceExtensions.data();
  createInfo.enabledLayerCount = 0;

  if (vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr,
                     &m_LogicalDevice) != VK_SUCCESS) {
    throw std::runtime_error("failed to create logical device!");
  }

  vkGetDeviceQueue(m_LogicalDevice, indices.presentFamily.value(), 0,
                   &m_PresentQueue);
  vkGetDeviceQueue(m_LogicalDevice, indices.graphicsFamily.value(), 0,
                   &m_GraphicsQueue);
}

void Renderer::CreateSwapChain() {
  LOG_DEBUG("Creating swap chain");
  SwapChainSupportDetails swapChainSupport =
      QuerySwapChainSupport(m_PhysicalDevice);

  VkSurfaceFormatKHR surfaceFormat =
      ChooseSwapSurfaceFormat(swapChainSupport.formats);
  VkPresentModeKHR presentMode =
      ChooseSwapPresentMode(swapChainSupport.presentModes);
  VkExtent2D extent = ChooseSwapExtent(swapChainSupport.capabilities);

  uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

  if (swapChainSupport.capabilities.maxImageCount > 0 &&
      imageCount > swapChainSupport.capabilities.maxImageCount) {
    imageCount = swapChainSupport.capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  createInfo.surface = m_Surface;
  createInfo.minImageCount = imageCount;
  createInfo.imageFormat = surfaceFormat.format;
  createInfo.imageColorSpace = surfaceFormat.colorSpace;
  createInfo.imageExtent = extent;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  QueueFamilyIndices indices = FindQueueFamilies(m_PhysicalDevice);
  uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(),
                                   indices.presentFamily.value()};

  if (indices.graphicsFamily != indices.presentFamily) {
    createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices = queueFamilyIndices;
  } else {
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;      // Optional
    createInfo.pQueueFamilyIndices = nullptr;  // Optional
  }
  createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
  // The compositeAlpha field specifies if the alpha channel should be used for blending with other windows in the window system.
  // You'll almost always want to simply ignore the alpha channel, hence VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR.
  createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  createInfo.presentMode = presentMode;
  // If it's clipped, obscured pixels will be ignored hence increasing the performance.
  createInfo.clipped = VK_TRUE;
  createInfo.oldSwapchain = VK_NULL_HANDLE;

  if (vkCreateSwapchainKHR(m_LogicalDevice, &createInfo, nullptr,
                           &m_SwapChain) != VK_SUCCESS) {
    throw std::runtime_error("failed to create swap chain!");
  }

  vkGetSwapchainImagesKHR(m_LogicalDevice, m_SwapChain, &imageCount, nullptr);
  m_SwapChainImages.resize(imageCount);
  vkGetSwapchainImagesKHR(m_LogicalDevice, m_SwapChain, &imageCount,
                          m_SwapChainImages.data());
  m_SwapChainImageFormat = surfaceFormat.format;
  m_Extent = extent;

  m_AspectRatio = m_Extent.width / (float)m_Extent.height;
  m_WindowSize.Width = m_Extent.width;
  m_WindowSize.Height = m_Extent.height;
  m_RecreateSwapChain = false;
  m_SwapChainCreated = true;
}

void Renderer::CreateImageViews() {
  m_SwapChainImageViews.resize(m_SwapChainImages.size());

  for (uint32_t i = 0; i < m_SwapChainImages.size(); i++) {
    m_SwapChainImageViews[i] =
        CreateImageView(m_SwapChainImages[i], m_SwapChainImageFormat,
                        VK_IMAGE_ASPECT_COLOR_BIT, 1);
  }
}

void Renderer::CreateDefaultRenderPass() {
  LOG_DEBUG("Creating default render pass");
  RenderPassSpecification specification{
      PassType::Geometry,
      FindDepthFormat(),
      m_SwapChainImageFormat,
      m_MsaaSamples
  };
  m_RenderPass = CreateReference<RenderPass>(specification);
  m_RenderPass->Bake();
}

void Renderer::CreateDefaultDescriptorSetLayout() {
  m_DefaultDescriptorLayout = CreateDescriptorLayout();
}

void Renderer::CreateDefaultGraphicsPipeline() {
  LOG_DEBUG("Creating default graphics pipeline");
  auto vertexShader = CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourcePrecompiled,
       "assets/shaders/standard_shader.vert.spv"});
  auto fragmentShader = CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourcePrecompiled,
       "assets/shaders/standard_shader.frag.spv"});
  m_DefaultGraphicsPipeline = CreateGraphicsPipeline(
      {CullModeBack, m_EnableWireframe, m_RenderPass,
       m_DefaultDescriptorLayout, vertexShader, fragmentShader, false,
       m_Extent.width, m_Extent.height});
}

Ref<GraphicsPipeline> Renderer::CreateGraphicsPipeline(
    PipelineProperties properties) {
  Ref<GraphicsPipeline> pipeline =
      CreateReference<GraphicsPipeline>(properties);
  AllocateGraphicsPipeline(properties, pipeline);
  return pipeline;
}

void Renderer::AllocateGraphicsPipeline(PipelineProperties properties,
                                        Ref<GraphicsPipeline> pipeline) {
  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = 1;
  pipelineLayoutInfo.pSetLayouts = &properties.m_DescriptorLayout->m_Layout;
  pipelineLayoutInfo.pushConstantRangeCount = 0;

  WIESEL_CHECK_VKRESULT(vkCreatePipelineLayout(
      m_LogicalDevice, &pipelineLayoutInfo, nullptr, &pipeline->m_Layout));

  VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
  vertShaderStageInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertShaderStageInfo.module = properties.m_VertexShader->ShaderModule;
  vertShaderStageInfo.pName =
      properties.m_VertexShader->Properties.Main.c_str();

  VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
  fragShaderStageInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragShaderStageInfo.module = properties.m_FragmentShader->ShaderModule;
  fragShaderStageInfo.pName =
      properties.m_FragmentShader->Properties.Main.c_str();
  VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo,
                                                    fragShaderStageInfo};

  std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                               VK_DYNAMIC_STATE_SCISSOR};

  VkPipelineDynamicStateCreateInfo dynamicState{};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
  dynamicState.pDynamicStates = dynamicStates.data();

  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  auto bindingDescription = Wiesel::Vertex::GetBindingDescription();
  auto attributeDescriptions = Wiesel::Vertex::GetAttributeDescriptions();

  vertexInputInfo.vertexBindingDescriptionCount = 1;
  vertexInputInfo.vertexAttributeDescriptionCount =
      static_cast<uint32_t>(attributeDescriptions.size());
  vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
  vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

  VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = (float)properties.m_ViewportWidth;
  viewport.height = (float)properties.m_ViewportHeight;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent.width = properties.m_ViewportWidth;
  scissor.extent.height = properties.m_ViewportHeight;

  VkPipelineViewportStateCreateInfo viewportState{};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.pViewports = &viewport;
  viewportState.scissorCount = 1;
  viewportState.pScissors = &scissor;

  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  /*
     * VK_POLYGON_MODE_FILL: fill the area of the polygon with fragments
     * VK_POLYGON_MODE_LINE: polygon edges are drawn as lines
     * VK_POLYGON_MODE_POINT: polygon vertices are drawn as points
     */
  if (properties.m_EnableWireframe) {
    rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
  } else {
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  }
  rasterizer.lineWidth = 1.0f;
  switch (properties.m_CullFace) {
    case CullModeNone:
      rasterizer.cullMode = VK_CULL_MODE_NONE;
      break;
    case CullModeFront:
      rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
      break;
    case CullModeBack:
      rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
      break;
    case CullModeBoth:
      rasterizer.cullMode = VK_CULL_MODE_FRONT_AND_BACK;
      break;
  }
  rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

  rasterizer.depthBiasEnable = VK_FALSE;
  rasterizer.depthBiasConstantFactor = 0.0f;  // Optional
  rasterizer.depthBiasClamp = 0.0f;           // Optional
  rasterizer.depthBiasSlopeFactor = 0.0f;     // Optional

  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = m_MsaaSamples;
  multisampling.minSampleShading = 1.0f;           // Optional
  multisampling.pSampleMask = nullptr;             // Optional
  multisampling.alphaToCoverageEnable = VK_FALSE;  // Optional
  multisampling.alphaToOneEnable = VK_FALSE;       // Optional

  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  if (properties.m_EnableAlphaBlending) {
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor =
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
  } else {
    colorBlendAttachment.blendEnable = VK_FALSE;
  }

  VkPipelineColorBlendStateCreateInfo colorBlending{};
  colorBlending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.logicOp = VK_LOGIC_OP_COPY;  // Optional
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;
  colorBlending.blendConstants[0] = 0.0f;  // Optional
  colorBlending.blendConstants[1] = 0.0f;  // Optional
  colorBlending.blendConstants[2] = 0.0f;  // Optional
  colorBlending.blendConstants[3] = 0.0f;  // Optional

  VkPipelineDepthStencilStateCreateInfo depthStencil{};
  depthStencil.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencil.depthTestEnable = VK_TRUE;
  depthStencil.depthWriteEnable = VK_TRUE;
  depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
  depthStencil.depthBoundsTestEnable = VK_FALSE;
  depthStencil.minDepthBounds = 0.0f;  // Optional
  depthStencil.maxDepthBounds = 1.0f;  // Optional
  depthStencil.stencilTestEnable = VK_FALSE;
  depthStencil.front = {};  // Optional
  depthStencil.back = {};   // Optional

  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = shaderStages;

  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pDepthStencilState = &depthStencil;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.pDynamicState = &dynamicState;
  pipelineInfo.layout = pipeline->m_Layout;
  pipelineInfo.renderPass = properties.m_RenderPass->GetVulkanHandle();
  pipelineInfo.subpass = 0;
  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;  // Optional
  pipelineInfo.basePipelineIndex = -1;               // Optional

  WIESEL_CHECK_VKRESULT(
      vkCreateGraphicsPipelines(m_LogicalDevice, VK_NULL_HANDLE, 1,
                                &pipelineInfo, nullptr, &pipeline->m_Pipeline));

  pipeline->m_IsAllocated = true;
}

void Renderer::DestroyGraphicsPipeline(GraphicsPipeline& pipeline) {
  vkDestroyPipeline(m_LogicalDevice, pipeline.m_Pipeline, nullptr);
  vkDestroyPipelineLayout(m_LogicalDevice, pipeline.m_Layout, nullptr);
  pipeline.m_IsAllocated = true;
}

void Renderer::RecreateGraphicsPipeline(Ref<GraphicsPipeline> pipeline) {
  DestroyGraphicsPipeline(*pipeline);
  AllocateGraphicsPipeline(pipeline->m_Properties, pipeline);
}

void Renderer::RecreateShader(Ref<Shader> shader) {
  if (shader->Properties.Source == ShaderSourceSource) {
    DestroyShader(*shader);
    std::vector<char> file = ReadFile(shader->Properties.Path);
    std::vector<uint32_t> code{};
    if (!Spirv::ShaderToSPV(shader->Properties.Type, file, code)) {
      throw std::runtime_error("Failed to compile shader!");
    }

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size() * 4;
    createInfo.pCode = code.data();
    WIESEL_CHECK_VKRESULT(vkCreateShaderModule(m_LogicalDevice, &createInfo,
                                               nullptr, &shader->ShaderModule));
  } else if (shader->Properties.Source == ShaderSourcePrecompiled) {
    DestroyShader(*shader);
    std::vector<uint32_t> code = ReadFileUint32(shader->Properties.Path);
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size() * 4;
    createInfo.pCode = code.data();
    WIESEL_CHECK_VKRESULT(vkCreateShaderModule(m_LogicalDevice, &createInfo,
                                               nullptr, &shader->ShaderModule));

  } else {
    throw std::runtime_error("Shader source not implemented!");
  }
}

Ref<Shader> Renderer::CreateShader(ShaderProperties properties) {
  if (properties.Source == ShaderSourceSource) {
    auto file = ReadFile(properties.Path);
    std::vector<uint32_t> code{};
    if (!Spirv::ShaderToSPV(properties.Type, file, code)) {
      throw std::runtime_error("Failed to compile shader!");
    }
    return CreateShader(code, properties);
  } else if (properties.Source == ShaderSourcePrecompiled) {
    auto code = ReadFileUint32(properties.Path);
    return CreateShader(code, properties);
  } else {
    throw std::runtime_error("Shader source not implemented!");
  }
}

Ref<Shader> Renderer::CreateShader(const std::vector<uint32_t>& code,
                                         ShaderProperties properties) {
  LOG_DEBUG("Creating shader with lang: {}, type: {}, source: {} {}, main: {}",
            std::to_string(properties.Lang), std::to_string(properties.Type),
            std::to_string(properties.Source), properties.Path,
            properties.Main);
  Ref<Shader> shader = CreateReference<Shader>(properties);
  VkShaderModuleCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = code.size() * 4;
  createInfo.pCode = code.data();
  WIESEL_CHECK_VKRESULT(vkCreateShaderModule(m_LogicalDevice, &createInfo,
                                             nullptr, &shader->ShaderModule));
  return shader;
}

void Renderer::DestroyShader(Shader& shader) {
  LOG_DEBUG("Destroying shader");
  vkDestroyShaderModule(m_LogicalDevice, shader.ShaderModule, nullptr);
}

void Renderer::CreateDepthResources() {
  LOG_DEBUG("Creating depth stencil");
  m_DepthStencils.resize(m_SwapChainImages.size());
  for (size_t i = 0; i < m_DepthStencils.size(); i++) {
    m_DepthStencils[i] = CreateDepthStencil();
  }
}

void Renderer::CreateColorResources() {
  LOG_DEBUG("Creating color image");

  m_ColorImages.resize(m_SwapChainImages.size());
  for (size_t i = 0; i < m_ColorImages.size(); i++) {
    m_ColorImages[i] = CreateColorImage();
  }
}

void Renderer::CreateFramebuffers() {
  m_Framebuffers.resize(m_SwapChainImages.size());
  for (size_t i = 0; i < m_Framebuffers.size(); i++) {
    std::array<VkImageView, 3> attachments = {
        m_ColorImages[i]->m_ImageView,
        m_DepthStencils[i]->m_ImageView,
        m_SwapChainImageViews[i],
    };

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = m_RenderPass->GetVulkanHandle();
    framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    framebufferInfo.pAttachments = attachments.data();
    framebufferInfo.width = m_Extent.width;
    framebufferInfo.height = m_Extent.height;
    framebufferInfo.layers = 1;

    WIESEL_CHECK_VKRESULT(vkCreateFramebuffer(m_LogicalDevice, &framebufferInfo,
                                              nullptr,
                                              &m_Framebuffers[i]));
  }
}

void Renderer::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                            VkMemoryPropertyFlags properties, VkBuffer& buffer,
                            VkDeviceMemory& bufferMemory) {
  VkBufferCreateInfo bufferInfo{};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = size;
  bufferInfo.usage = usage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  WIESEL_CHECK_VKRESULT(
      vkCreateBuffer(m_LogicalDevice, &bufferInfo, nullptr, &buffer));

  VkMemoryRequirements memRequirements;
  vkGetBufferMemoryRequirements(m_LogicalDevice, buffer, &memRequirements);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex =
      FindMemoryType(memRequirements.memoryTypeBits, properties);

  WIESEL_CHECK_VKRESULT(
      vkAllocateMemory(m_LogicalDevice, &allocInfo, nullptr, &bufferMemory));
  WIESEL_CHECK_VKRESULT(
      vkBindBufferMemory(m_LogicalDevice, buffer, bufferMemory, 0));
}

void Renderer::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer,
                          VkDeviceSize size) {
  VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

  VkBufferCopy copyRegion{};
  copyRegion.size = size;
  vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

  EndSingleTimeCommands(commandBuffer);
}

void Renderer::CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width,
                                 uint32_t height) {
  VkCommandBuffer commandBuffer = BeginSingleTimeCommands();
  VkBufferImageCopy region{};
  region.bufferOffset = 0;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;

  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
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
                                     uint32_t mipLevels) {
  VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = oldLayout;
  barrier.newLayout = newLayout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

    if (HasStencilComponent(format)) {
      barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
  } else {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  }
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = mipLevels;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;

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
  } else {
    throw std::invalid_argument("unsupported layout transition!");
  }

  vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0,
                       nullptr, 0, nullptr, 1, &barrier);

  EndSingleTimeCommands(commandBuffer);
}

void Renderer::CreateCommandPools() {
  QueueFamilyIndices queueFamilyIndices = FindQueueFamilies(m_PhysicalDevice);

  VkCommandPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
  WIESEL_CHECK_VKRESULT(
      vkCreateCommandPool(m_LogicalDevice, &poolInfo, nullptr, &m_CommandPool));
}

void Renderer::CreateCommandBuffers() {
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = m_CommandPool;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = 1;

  WIESEL_CHECK_VKRESULT(vkAllocateCommandBuffers(m_LogicalDevice, &allocInfo,
                                                 &m_CommandBuffer));
}

void Renderer::CreatePermanentResources() {
  m_BlankTexture = CreateBlankTexture();
}

void Renderer::CreateImage(uint32_t width, uint32_t height, uint32_t mipLevels,
                           VkSampleCountFlagBits numSamples, VkFormat format,
                           VkImageTiling tiling, VkImageUsageFlags usage,
                           VkMemoryPropertyFlags properties, VkImage& image,
                           VkDeviceMemory& imageMemory) {
  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width = static_cast<uint32_t>(width);
  imageInfo.extent.height = static_cast<uint32_t>(height);
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = mipLevels;
  imageInfo.arrayLayers = 1;
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
  imageInfo.flags = 0;  // Optional
  WIESEL_CHECK_VKRESULT(
      vkCreateImage(m_LogicalDevice, &imageInfo, nullptr, &image));

  VkMemoryRequirements memRequirements;
  vkGetImageMemoryRequirements(m_LogicalDevice, image, &memRequirements);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex =
      FindMemoryType(memRequirements.memoryTypeBits, properties);

  WIESEL_CHECK_VKRESULT(
      vkAllocateMemory(m_LogicalDevice, &allocInfo, nullptr, &imageMemory));

  vkBindImageMemory(m_LogicalDevice, image, imageMemory, 0);
}

VkImageView Renderer::CreateImageView(VkImage image, VkFormat format,
                                      VkImageAspectFlags aspectFlags,
                                      uint32_t mipLevels) {
  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = image;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = format;
  viewInfo.subresourceRange.aspectMask = aspectFlags;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = mipLevels;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  VkImageView imageView;
  WIESEL_CHECK_VKRESULT(
      vkCreateImageView(m_LogicalDevice, &viewInfo, nullptr, &imageView));

  return imageView;
}

VkFormat Renderer::FindSupportedFormat(const std::vector<VkFormat>& candidates,
                                       VkImageTiling tiling,
                                       VkFormatFeatureFlags features) {
  for (VkFormat format : candidates) {
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(m_PhysicalDevice, format, &props);
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
  VkFormatProperties formatProperties;
  vkGetPhysicalDeviceFormatProperties(m_PhysicalDevice, imageFormat,
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
  VkPhysicalDeviceProperties physicalDeviceProperties;
  vkGetPhysicalDeviceProperties(m_PhysicalDevice, &physicalDeviceProperties);

  VkSampleCountFlags counts =
      physicalDeviceProperties.limits.framebufferColorSampleCounts &
      physicalDeviceProperties.limits.framebufferDepthSampleCounts;
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

void Renderer::CreateSyncObjects() {
  VkSemaphoreCreateInfo semaphoreInfo{};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fenceInfo{};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  WIESEL_CHECK_VKRESULT(vkCreateSemaphore(m_LogicalDevice, &semaphoreInfo,
                                          nullptr,
                                          &m_ImageAvailableSemaphore));
  WIESEL_CHECK_VKRESULT(vkCreateSemaphore(m_LogicalDevice, &semaphoreInfo,
                                          nullptr,
                                          &m_RenderFinishedSemaphore));
  WIESEL_CHECK_VKRESULT(vkCreateFence(m_LogicalDevice, &fenceInfo, nullptr,
                                      &m_Fence));
}

void Renderer::CleanupSwapChain() {
  LOG_DEBUG("Cleanup swap chain");
  m_DepthStencils.clear();
  m_ColorImages.clear();

  LOG_DEBUG("Destroying swap chain framebuffers");
  for (size_t i = 0; i < m_Framebuffers.size(); i++) {
    vkDestroyFramebuffer(m_LogicalDevice, m_Framebuffers[i], nullptr);
  }

  LOG_DEBUG("Destroying swap chain imageviews");
  for (size_t i = 0; i < m_SwapChainImageViews.size(); i++) {
    vkDestroyImageView(m_LogicalDevice, m_SwapChainImageViews[i], nullptr);
  }

  LOG_DEBUG("Destroying swap chain");
  vkDestroySwapchainKHR(m_LogicalDevice, m_SwapChain, nullptr);
}

void Renderer::CleanupDefaultRenderPass() {
  m_RenderPass = nullptr;
}

void Renderer::CreateGlobalUniformBuffers() {
  m_LightsGlobalUbo = CreateUniformBuffer(sizeof(LightsUniformBufferObject) + 16);
}

void Renderer::CleanupGlobalUniformBuffers() {
  m_LightsGlobalUbo = nullptr;
}

void Renderer::RecreateSwapChain() {
  LOG_INFO("Recreating swap chains...");
  WindowSize size{};
  m_Window->GetWindowFramebufferSize(size);
  while (size.Width == 0 || size.Height == 0) {
    m_Window->GetWindowFramebufferSize(size);
    m_Window->OnUpdate();
  }

  vkDeviceWaitIdle(m_LogicalDevice);

  CleanupSwapChain();

  CreateSwapChain();
  CreateImageViews();
  CreateColorResources();
  CreateDepthResources();
  CreateFramebuffers();
}

bool Renderer::BeginFrame() {
  VkResult result =
      vkAcquireNextImageKHR(m_LogicalDevice, m_SwapChain, UINT64_MAX,
                            m_ImageAvailableSemaphore,
                            VK_NULL_HANDLE, &m_ImageIndex);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || m_RecreateSwapChain) {
    RecreateSwapChain();
    return false;
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    throw std::runtime_error("failed to acquire swap chain image!");
  }
  // Setup
  vkResetFences(m_LogicalDevice, 1, &m_Fence);
  vkResetCommandBuffer(m_CommandBuffer, 0);
  if (m_PreviousMsaaSamples != m_MsaaSamples) {
    LOG_INFO("Msaa samples changed to {} from {}!",
             std::to_string(m_MsaaSamples),
             std::to_string(m_PreviousMsaaSamples));
    m_PreviousMsaaSamples = m_MsaaSamples;
  }

  // Reloading stuff
  if (m_RecreateShaders) {
    RecreateShader(m_DefaultGraphicsPipeline->m_Properties.m_VertexShader);
    RecreateShader(m_DefaultGraphicsPipeline->m_Properties.m_FragmentShader);
    m_RecreateShaders = false;
    m_RecreateGraphicsPipeline = true;
  }

  if (m_RecreateGraphicsPipeline) {
    vkDeviceWaitIdle(m_LogicalDevice);
    LOG_INFO("Recreating graphics pipeline...");
    m_DefaultGraphicsPipeline->m_Properties.m_EnableWireframe =
        m_EnableWireframe;  // Update wireframe mode
    RecreateGraphicsPipeline(m_DefaultGraphicsPipeline);
    m_RecreateGraphicsPipeline = false;
  }

  m_CurrentGraphicsPipeline = m_DefaultGraphicsPipeline;

  // Actual drawing
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = 0;                   // Optional
  beginInfo.pInheritanceInfo = nullptr;  // Optional

  WIESEL_CHECK_VKRESULT(
      vkBeginCommandBuffer(m_CommandBuffer, &beginInfo));

  vkCmdBindPipeline(m_CommandBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_CurrentGraphicsPipeline->m_Pipeline);

  m_RenderPass->Bind();

  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = static_cast<float>(m_Extent.width);
  viewport.height = static_cast<float>(m_Extent.height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(m_CommandBuffer, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = m_Extent;
  vkCmdSetScissor(m_CommandBuffer, 0, 1, &scissor);

  auto& lights = GetLightsBufferObject();
  memcpy(m_LightsGlobalUbo->m_Buffer, &lights,
         sizeof(lights));

  return true;
}

void Renderer::DrawModel(ModelComponent& model, TransformComponent& transform) {
  for (int i = 0; i < model.Data.Meshes.size(); i++) {
    const auto& mesh = model.Data.Meshes[i];
    DrawMesh(mesh, transform);
  }
}

void Renderer::DrawMesh(Ref<Mesh> mesh, TransformComponent& transform) {
  if (!mesh->IsAllocated) {
    return;
  }
  mesh->UpdateUniformBuffer(transform);

  VkBuffer vertexBuffers[] = {mesh->VertexBuffer->m_Buffer};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(m_CommandBuffer, 0, 1, vertexBuffers,
                         offsets);
  vkCmdBindIndexBuffer(m_CommandBuffer,
                       mesh->IndexBuffer->m_Buffer, 0, VK_INDEX_TYPE_UINT32);

  vkCmdBindDescriptorSets(
      m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
      m_CurrentGraphicsPipeline->m_Layout, 0, 1,
      &mesh->Descriptors->m_DescriptorSet, 0, nullptr);
  vkCmdDrawIndexed(m_CommandBuffer,
                   static_cast<uint32_t>(mesh->Indices.size()), 1, 0, 0, 0);
}

void Renderer::EndFrame() {
  // Finish command buffer
  vkCmdEndRenderPass(m_CommandBuffer);
  WIESEL_CHECK_VKRESULT(vkEndCommandBuffer(m_CommandBuffer));

  // Presentation
  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &m_CommandBuffer;

  VkSemaphore waitSemaphores[] = {m_ImageAvailableSemaphore};
  VkPipelineStageFlags waitStages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = waitSemaphores;
  submitInfo.pWaitDstStageMask = waitStages;

  VkSemaphore signalSemaphores[] = {m_RenderFinishedSemaphore};
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = signalSemaphores;

  WIESEL_CHECK_VKRESULT(vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo,
                                      m_Fence));

  VkPresentInfoKHR presentInfo{};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = signalSemaphores;

  VkSwapchainKHR swapChains[] = {m_SwapChain};
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = swapChains;

  presentInfo.pImageIndices = &m_ImageIndex;
  presentInfo.pResults = nullptr;  // Optional

  VkResult result = vkQueuePresentKHR(m_PresentQueue, &presentInfo);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    RecreateSwapChain();
  } else if (result != VK_SUCCESS) {
    throw std::runtime_error("failed to present swap chain image!");
  }

  m_CurrentGraphicsPipeline = nullptr;

  vkWaitForFences(m_LogicalDevice, 1, &m_Fence,
                  VK_TRUE, UINT64_MAX);
}

void Renderer::SetCameraData(Ref<CameraData> cameraData) {
  m_CameraData = cameraData;
}

std::vector<const char*> Renderer::GetRequiredExtensions() {
  uint32_t extensionsCount = 0;
  const char** windowExtensions;
  windowExtensions = m_Window->GetRequiredInstanceExtensions(&extensionsCount);

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
  allocInfo.commandPool = m_CommandPool;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(m_LogicalDevice, &allocInfo, &commandBuffer);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  return commandBuffer;
}

void Renderer::EndSingleTimeCommands(VkCommandBuffer commandBuffer) {
  vkEndCommandBuffer(commandBuffer);

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(m_GraphicsQueue);

  vkFreeCommandBuffers(m_LogicalDevice, m_CommandPool, 1, &commandBuffer);
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
  if (!m_Vsync) {
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
    m_Window->GetWindowFramebufferSize(size);

    VkExtent2D actualExtent = {static_cast<uint32_t>(size.Width),
                               static_cast<uint32_t>(size.Height)};

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

  std::set<std::string> requiredExtensions(m_DeviceExtensions.begin(),
                                           m_DeviceExtensions.end());

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
    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_Surface, &presentSupport);

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
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_Surface,
                                            &details.capabilities);

  uint32_t formatCount;
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount,
                                       nullptr);

  if (formatCount != 0) {
    details.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount,
                                         details.formats.data());
  }

  uint32_t presentModeCount;
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface,
                                            &presentModeCount, nullptr);

  if (presentModeCount != 0) {
    details.presentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        device, m_Surface, &presentModeCount, details.presentModes.data());
  }

  return details;
}

uint32_t Renderer::FindMemoryType(uint32_t typeFilter,
                                  VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &memProperties);
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
      m_Instance, &createInfo, nullptr, &m_DebugMessenger));
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

  for (const char* layerName : validationLayers) {
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
DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
              VkDebugUtilsMessageTypeFlagsEXT messageType,
              const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
              void* pUserData) {
  if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
    LOG_DEBUG("{}", std::string(pCallbackData->pMessage));
  } else if (messageSeverity ==
             VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    LOG_WARN("{}", std::string(pCallbackData->pMessage));
  } else if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    LOG_ERROR("{}", std::string(pCallbackData->pMessage));
  } else {
    LOG_INFO("{}", std::string(pCallbackData->pMessage));
  }
  return VK_FALSE;
}

#endif

}  // namespace Wiesel