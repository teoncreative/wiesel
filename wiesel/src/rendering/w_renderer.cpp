
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

Renderer::Renderer(Ref<AppWindow> window) : m_Window(window) {
  Spirv::Init();
#ifdef VULKAN_VALIDATION
  validationLayers.push_back("VK_LAYER_KHRONOS_validation");
#endif

  m_DeviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
#ifdef __APPLE__
  m_DeviceExtensions.push_back("VK_KHR_portability_subset");
#endif

  m_RecreatePipeline = false;
  m_EnableWireframe = false;
  m_EnableSSAO = true;
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
}

void Renderer::Initialize(const RendererProperties&& properties) {
  CreateVulkanInstance();
#ifdef VULKAN_VALIDATION
  SetupDebugMessenger();
#endif
  PerfMarker::Init(m_Instance);
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
  m_Initialized = true;
}

VkDevice Renderer::GetLogicalDevice() {
  return m_LogicalDevice;
}

template <typename T>
Ref<MemoryBuffer> Renderer::CreateVertexBuffer(std::vector<T> vertices) {
  Ref<MemoryBuffer> memoryBuffer =
      CreateReference<MemoryBuffer>(MemoryTypeVertexBuffer);

  memoryBuffer->m_Size = vertices.size();

  VkDeviceSize bufferSize = sizeof(T) * vertices.size();
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               stagingBuffer, stagingBufferMemory);

  void* data;
  vkMapMemory(m_LogicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
  memcpy(data, vertices.data(), bufferSize);
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

template Ref<MemoryBuffer> Renderer::CreateVertexBuffer<Vertex3D>(
    std::vector<Vertex3D>);

template Ref<MemoryBuffer> Renderer::CreateVertexBuffer<Vertex2DNoColor>(
    std::vector<Vertex2DNoColor>);

template Ref<MemoryBuffer> Renderer::CreateVertexBuffer<VertexSprite>(
    std::vector<VertexSprite>);

void Renderer::DestroyVertexBuffer(MemoryBuffer& buffer) {
  vkDestroyBuffer(m_LogicalDevice, buffer.m_Buffer, nullptr);
  vkFreeMemory(m_LogicalDevice, buffer.m_BufferMemory, nullptr);
}

Ref<IndexBuffer> Renderer::CreateIndexBuffer(std::vector<Index> indices) {
  Ref<IndexBuffer> memoryBuffer = CreateReference<IndexBuffer>();

  static_assert(sizeof(Index) == sizeof(uint32_t));
  memoryBuffer->m_IndexType = VK_INDEX_TYPE_UINT32;
  memoryBuffer->m_Size = indices.size();
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

  uniformBuffer->m_Data = malloc(size);
  uniformBuffer->m_Size = size;
  // TODO not use host coherent memory, use staging buffer and copy when it changes
  // like how I did in GlistEngine
  // This is slow af
  CreateBuffer(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               uniformBuffer->m_Buffer, uniformBuffer->m_BufferMemory);

  WIESEL_CHECK_VKRESULT(vkMapMemory(m_LogicalDevice,
                                    uniformBuffer->m_BufferMemory, 0, size, 0,
                                    &uniformBuffer->m_Data));

  memset(uniformBuffer->m_Data, 0, size);

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

void Renderer::SetupCameraComponent(CameraComponent& component) {
  component.AspectRatio = Engine::GetRenderer()->GetAspectRatio();
  VkExtent2D extent = Engine::GetRenderer()->GetExtent();
  component.ViewportSize.x = extent.width;
  component.ViewportSize.y = extent.height;

  component.SSAOColorImage = CreateAttachmentTexture(
      {extent.width / 2, extent.height / 2, AttachmentTextureType::Offscreen, 1,
       VK_FORMAT_R8_UNORM, VK_SAMPLE_COUNT_1_BIT, true});
  component.SSAOBlurHorzColorImage = CreateAttachmentTexture(
      {extent.width, extent.height, AttachmentTextureType::Offscreen, 1,
       VK_FORMAT_R8_UNORM, VK_SAMPLE_COUNT_1_BIT, true});
  component.SSAOBlurVertColorImage = CreateAttachmentTexture(
      {extent.width, extent.height, AttachmentTextureType::Offscreen, 1,
       VK_FORMAT_R8_UNORM, VK_SAMPLE_COUNT_1_BIT, true});
  component.SSAOGenFramebuffer = m_SSAOGenRenderPass->CreateFramebuffer(
      0, {component.SSAOColorImage->m_ImageViews[0]},
      {extent.width / 2, extent.height / 2});
  component.SSAOBlurHorzFramebuffer = m_SSAOBlurHorzRenderPass->CreateFramebuffer(
      0, {component.SSAOBlurHorzColorImage->m_ImageViews[0]},
      {extent.width, extent.height});
  component.SSAOBlurVertFramebuffer = m_SSAOBlurVertRenderPass->CreateFramebuffer(
      0, {component.SSAOBlurVertColorImage->m_ImageViews[0]},
      {extent.width, extent.height});

  component.GeometryViewPosImage = CreateAttachmentTexture(
      {extent.width, extent.height, AttachmentTextureType::Offscreen, 1,
       VK_FORMAT_R32G32B32A32_SFLOAT, m_MsaaSamples, true});
  component.GeometryWorldPosImage = CreateAttachmentTexture(
      {extent.width, extent.height, AttachmentTextureType::Offscreen, 1,
       VK_FORMAT_R32G32B32A32_SFLOAT, m_MsaaSamples, true});
  component.GeometryDepthImage = CreateAttachmentTexture(
      {extent.width, extent.height, AttachmentTextureType::Offscreen, 1,
       VK_FORMAT_R32_SFLOAT, m_MsaaSamples, true});
  component.GeometryNormalImage = CreateAttachmentTexture(
      {extent.width, extent.height, AttachmentTextureType::Offscreen, 1,
       VK_FORMAT_R8G8B8A8_UNORM, m_MsaaSamples, true});
  component.GeometryAlbedoImage = CreateAttachmentTexture(
      {extent.width, extent.height, AttachmentTextureType::Offscreen, 1,
       VK_FORMAT_R8G8B8A8_UNORM, m_MsaaSamples, true});
  component.GeometryMaterialImage = CreateAttachmentTexture(
      {extent.width, extent.height, AttachmentTextureType::Offscreen, 1,
       VK_FORMAT_R16G16B16A16_SFLOAT, m_MsaaSamples, true});
  component.GeometryDepthStencil = CreateAttachmentTexture(
      {extent.width, extent.height, AttachmentTextureType::DepthStencil, 1,
       FindDepthFormat(), m_MsaaSamples, true});

  component.ShadowDepthStencil = CreateAttachmentTexture(
      {WIESEL_SHADOWMAP_DIM, WIESEL_SHADOWMAP_DIM,
       AttachmentTextureType::DepthStencil, 1, FindDepthFormat(),
       VK_SAMPLE_COUNT_1_BIT, true, WIESEL_SHADOW_CASCADE_COUNT});
  component.ShadowDepthViewArray =
      CreateImageView(component.ShadowDepthStencil, VK_IMAGE_VIEW_TYPE_2D_ARRAY,
                      0, WIESEL_SHADOW_CASCADE_COUNT);
  for (int i = 0; i < WIESEL_SHADOW_CASCADE_COUNT; ++i) {
    component.ShadowDepthViews[i] =
        CreateImageView(component.ShadowDepthStencil, VK_IMAGE_VIEW_TYPE_2D, i);
    std::array<ImageView*, 1> textures = {
        component.ShadowDepthViews[i].get(),
    };
    component.ShadowFramebuffers[i] = m_ShadowRenderPass->CreateFramebuffer(
        0, textures, {WIESEL_SHADOWMAP_DIM, WIESEL_SHADOWMAP_DIM});
  }

  if (m_MsaaSamples > VK_SAMPLE_COUNT_1_BIT) {
    component.GeometryViewPosResolveImage = CreateAttachmentTexture(
        {extent.width, extent.height, AttachmentTextureType::Resolve, 1,
         VK_FORMAT_R32G32B32A32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, true});
    component.GeometryWorldPosResolveImage = CreateAttachmentTexture(
        {extent.width, extent.height, AttachmentTextureType::Resolve, 1,
         VK_FORMAT_R32G32B32A32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, true});
    component.GeometryDepthResolveImage = CreateAttachmentTexture(
        {extent.width, extent.height, AttachmentTextureType::Resolve, 1,
         VK_FORMAT_R32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, true});
    component.GeometryNormalResolveImage = CreateAttachmentTexture(
        {extent.width, extent.height, AttachmentTextureType::Resolve, 1,
         VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT, true});
    component.GeometryAlbedoResolveImage = CreateAttachmentTexture(
        {extent.width, extent.height, AttachmentTextureType::Resolve, 1,
         VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT, true});
    component.GeometryMaterialResolveImage = CreateAttachmentTexture(
        {extent.width, extent.height, AttachmentTextureType::Resolve, 1,
         VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT, true});
    std::array<AttachmentTexture*, 13> textures = {
        component.GeometryViewPosImage.get(),
        component.GeometryWorldPosImage.get(),
        component.GeometryDepthImage.get(),
        component.GeometryNormalImage.get(),
        component.GeometryAlbedoImage.get(),
        component.GeometryMaterialImage.get(),
        component.GeometryDepthStencil.get(),
        component.GeometryViewPosResolveImage.get(),
        component.GeometryWorldPosResolveImage.get(),
        component.GeometryDepthResolveImage.get(),
        component.GeometryNormalResolveImage.get(),
        component.GeometryAlbedoResolveImage.get(),
        component.GeometryMaterialResolveImage.get(),
    };
    component.GeometryFramebuffer = m_GeometryRenderPass->CreateFramebuffer(
        0, textures, component.ViewportSize);
  } else {
    component.GeometryViewPosResolveImage = component.GeometryViewPosImage;
    component.GeometryWorldPosResolveImage = component.GeometryWorldPosImage;
    component.GeometryDepthResolveImage = component.GeometryDepthImage;
    component.GeometryNormalResolveImage = component.GeometryNormalImage;
    component.GeometryAlbedoResolveImage = component.GeometryAlbedoImage;
    component.GeometryMaterialResolveImage = component.GeometryMaterialImage;
    std::array<AttachmentTexture*, 7> textures = {
        component.GeometryViewPosImage.get(),
        component.GeometryWorldPosImage.get(),
        component.GeometryDepthImage.get(),
        component.GeometryNormalImage.get(),
        component.GeometryAlbedoImage.get(),
        component.GeometryDepthStencil.get(),
        component.GeometryMaterialImage.get(),
    };
    component.GeometryFramebuffer = m_GeometryRenderPass->CreateFramebuffer(
        0, textures, component.ViewportSize);
  }

  component.LightingColorImage = CreateAttachmentTexture(
      {extent.width, extent.height, AttachmentTextureType::Offscreen, 1,
       m_SwapChainImageFormat, m_MsaaSamples});
  if (m_MsaaSamples > VK_SAMPLE_COUNT_1_BIT) {
    component.LightingColorResolveImage = CreateAttachmentTexture(
        {extent.width, extent.height, AttachmentTextureType::Resolve, 1,
         m_SwapChainImageFormat, VK_SAMPLE_COUNT_1_BIT, true});

    std::array<AttachmentTexture*, 2> textures{
        component.LightingColorImage.get(),
        component.LightingColorResolveImage.get()};
    component.LightingFramebuffer = m_LightingRenderPass->CreateFramebuffer(
        0, textures, {extent.width, extent.height});
  } else {
    component.LightingColorResolveImage = component.LightingColorImage;
    std::array<AttachmentTexture*, 1> textures{
        component.LightingColorImage.get()};
    component.LightingFramebuffer = m_LightingRenderPass->CreateFramebuffer(
        0, textures, {extent.width, extent.height});
  }

  component.SpriteColorImage = CreateAttachmentTexture(
      {extent.width, extent.height, AttachmentTextureType::Offscreen, 1,
       m_SwapChainImageFormat, VK_SAMPLE_COUNT_1_BIT, true});

  std::array<AttachmentTexture*, 1> textures{component.SpriteColorImage.get()};
  component.SpriteFramebuffer = m_SpriteRenderPass->CreateFramebuffer(
      0, textures, {extent.width, extent.height});

  component.CompositeColorImage = CreateAttachmentTexture(
      {extent.width, extent.height, AttachmentTextureType::Offscreen, 1,
       m_SwapChainImageFormat, m_MsaaSamples});
  if (m_MsaaSamples > VK_SAMPLE_COUNT_1_BIT) {
    component.CompositeColorResolveImage = CreateAttachmentTexture(
        {extent.width, extent.height, AttachmentTextureType::Resolve, 1,
         m_SwapChainImageFormat, VK_SAMPLE_COUNT_1_BIT, true});

    std::array<AttachmentTexture*, 2> textures{
        component.CompositeColorImage.get(),
        component.CompositeColorResolveImage.get()};
    component.CompositeFramebuffer = m_LightingRenderPass->CreateFramebuffer(
        0, textures, {extent.width, extent.height});
  } else {
    component.CompositeColorResolveImage = component.LightingColorImage;
    std::array<AttachmentTexture*, 1> textures{
        component.CompositeColorImage.get()};
    component.CompositeFramebuffer = m_LightingRenderPass->CreateFramebuffer(
        0, textures, {extent.width, extent.height});
  }

  component.GlobalDescriptor = CreateGlobalDescriptors(component);
  component.ShadowDescriptor = CreateShadowGlobalDescriptors(component);
  component.GeometryOutputDescriptor = CreateReference<DescriptorSet>();
  component.GeometryOutputDescriptor->SetLayout(
      m_GeometryOutputDescriptorLayout);
  component.GeometryOutputDescriptor->AddCombinedImageSampler(
      0, component.GeometryViewPosResolveImage->m_ImageViews[0],
      m_DefaultNearestSampler);
  component.GeometryOutputDescriptor->AddCombinedImageSampler(
      1, component.GeometryWorldPosResolveImage->m_ImageViews[0],
      m_DefaultNearestSampler);
  component.GeometryOutputDescriptor->AddCombinedImageSampler(
      2, component.GeometryDepthResolveImage->m_ImageViews[0],
      m_DefaultNearestSampler);
  component.GeometryOutputDescriptor->AddCombinedImageSampler(
      3, component.GeometryNormalResolveImage->m_ImageViews[0],
      m_DefaultNearestSampler);
  component.GeometryOutputDescriptor->AddCombinedImageSampler(
      4, component.GeometryAlbedoResolveImage->m_ImageViews[0],
      m_DefaultNearestSampler);
  component.GeometryOutputDescriptor->AddCombinedImageSampler(
      5, component.GeometryMaterialResolveImage->m_ImageViews[0],
      m_DefaultNearestSampler);
  component.GeometryOutputDescriptor->Bake();

  component.LightingOutputDescriptor = CreateReference<DescriptorSet>();
  component.LightingOutputDescriptor->SetLayout(m_PresentDescriptorLayout);
  component.LightingOutputDescriptor->AddCombinedImageSampler(
      0, component.LightingColorResolveImage->m_ImageViews[0],
      m_DefaultLinearSampler);
  component.LightingOutputDescriptor->Bake();

  component.SpriteOutputDescriptor = CreateReference<DescriptorSet>();
  component.SpriteOutputDescriptor->SetLayout(m_PresentDescriptorLayout);
  component.SpriteOutputDescriptor->AddCombinedImageSampler(
      0, component.SpriteColorImage->m_ImageViews[0],
      m_DefaultLinearSampler);
  component.SpriteOutputDescriptor->Bake();

  component.CompositeOutputDescriptor = CreateReference<DescriptorSet>();
  component.CompositeOutputDescriptor->SetLayout(m_PresentDescriptorLayout);
  component.CompositeOutputDescriptor->AddCombinedImageSampler(
      0, component.CompositeColorResolveImage->m_ImageViews[0],
      m_DefaultLinearSampler);
  component.CompositeOutputDescriptor->Bake();

  component.SSAOGenDescriptor = CreateReference<DescriptorSet>();
  component.SSAOGenDescriptor->SetLayout(m_SSAOGenDescriptorLayout);
  component.SSAOGenDescriptor->AddCombinedImageSampler(
      0, component.GeometryViewPosResolveImage->m_ImageViews[0],
      m_DefaultNearestSampler);
  component.SSAOGenDescriptor->AddCombinedImageSampler(
      1, component.GeometryNormalResolveImage->m_ImageViews[0],
      m_DefaultNearestSampler);
  component.SSAOGenDescriptor->AddCombinedImageSampler(
      2, component.GeometryDepthResolveImage->m_ImageViews[0],
      m_DefaultNearestSampler);
  component.SSAOGenDescriptor->AddCombinedImageSampler(
      3, m_SSAONoise->m_ImageViews[0], m_DefaultLinearSampler);
  component.SSAOGenDescriptor->AddUniformBuffer(4, m_SSAOKernelUniformBuffer);
  component.SSAOGenDescriptor->Bake();

  component.SSAOOutputDescriptor = CreateReference<DescriptorSet>();
  component.SSAOOutputDescriptor->SetLayout(m_SSAOOutputDescriptorLayout);
  component.SSAOOutputDescriptor->AddCombinedImageSampler(
      0, component.SSAOColorImage->m_ImageViews[0], m_DefaultNearestSampler);
  component.SSAOOutputDescriptor->AddCombinedImageSampler(
      1, component.GeometryDepthResolveImage->m_ImageViews[0],m_DefaultNearestSampler);
  component.SSAOOutputDescriptor->Bake();

  component.SSAOBlurHorzOutputDescriptor = CreateReference<DescriptorSet>();
  component.SSAOBlurHorzOutputDescriptor->SetLayout(m_SSAOBlurDescriptorLayout);
  component.SSAOBlurHorzOutputDescriptor->AddCombinedImageSampler(
      0, component.SSAOBlurHorzColorImage->m_ImageViews[0], m_DefaultLinearSampler);
  component.SSAOBlurHorzOutputDescriptor->AddCombinedImageSampler(
      1, component.GeometryDepthResolveImage->m_ImageViews[0],m_DefaultNearestSampler);
  component.SSAOBlurHorzOutputDescriptor->Bake();

  component.SSAOBlurVertOutputDescriptor = CreateReference<DescriptorSet>();
  component.SSAOBlurVertOutputDescriptor->SetLayout(m_SSAOBlurDescriptorLayout);
  component.SSAOBlurVertOutputDescriptor->AddCombinedImageSampler(
      0, component.SSAOBlurVertColorImage->m_ImageViews[0], m_DefaultLinearSampler);
  component.SSAOBlurHorzOutputDescriptor->AddCombinedImageSampler(
      1, component.GeometryDepthResolveImage->m_ImageViews[0],m_DefaultNearestSampler);
  component.SSAOBlurVertOutputDescriptor->Bake();

  component.IsViewChanged = true;
  component.IsPosChanged = true;
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

  texture->m_Format = format;
  texture->m_Sampler = CreateTextureSampler(texture->m_MipLevels, {});
  texture->m_ImageView =
      CreateImageView(texture->m_Image, format, VK_IMAGE_ASPECT_COLOR_BIT,
                      texture->m_MipLevels);

  texture->m_IsAllocated = true;
  return texture;
}

Ref<Texture> Renderer::CreateBlankTexture(const TextureProps& textureProps,
                                          const SamplerProps& samplerProps) {
  Ref<Texture> texture = CreateReference<Texture>(TextureTypeDiffuse, "");

  texture->m_Width = textureProps.Width;
  texture->m_Height = textureProps.Height;
  texture->m_Size = texture->m_Width * texture->m_Height * STBI_rgb_alpha;
  stbi_uc* pixels = new stbi_uc[texture->m_Size];
  std::memset(pixels, 0, texture->m_Size);  // full black
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

  texture->m_Sampler = CreateTextureSampler(1, samplerProps);
  texture->m_ImageView =
      CreateImageView(texture->m_Image, format, VK_IMAGE_ASPECT_COLOR_BIT,
                      texture->m_MipLevels);

  texture->m_IsAllocated = true;
  return texture;
}

Ref<Texture> Renderer::CreateTexture(const std::string& path,
                                     const TextureProps& textureProps,
                                     const SamplerProps& samplerProps) {
  Ref<Texture> texture = CreateReference<Texture>(textureProps.Type, path);

  stbi_uc* pixels =
      stbi_load(path.c_str(), reinterpret_cast<int*>(&texture->m_Width),
                reinterpret_cast<int*>(&texture->m_Height),
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

Ref<Texture> Renderer::CreateTexture(void* buffer, size_t sizePerPixel,
                                     const TextureProps& textureProps,
                                     const SamplerProps& samplerProps) {
  Ref<Texture> texture = CreateReference<Texture>(textureProps.Type, "");
  texture->m_Width = textureProps.Width;
  texture->m_Height = textureProps.Height;
  texture->m_Size = texture->m_Width * texture->m_Height * sizePerPixel;

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
  memcpy(data, buffer, static_cast<size_t>(texture->m_Size));
  vkUnmapMemory(m_LogicalDevice, stagingBufferMemory);

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

Ref<Texture> Renderer::CreateCubemapTexture(
    const std::array<std::string, 6>& paths,
    const Wiesel::TextureProps& textureProps,
    const Wiesel::SamplerProps& samplerProps) {
  Ref<Texture> texture = CreateReference<Texture>(textureProps.Type, "");
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
      texture->m_Width = w;
      texture->m_Height = h;
      texture->m_Size = texture->m_Width * texture->m_Height * STBI_rgb_alpha;
      texture->m_MipLevels = 1;
      totalSize = texture->m_Size * 6;
      allPixels = new stbi_uc[totalSize];
    }

    if (w != texture->m_Width || h != texture->m_Height) {
      throw std::runtime_error("cubemap face size mismatch!");
    }

    memcpy(allPixels + i * texture->m_Size, pixels, texture->m_Size);
    stbi_image_free(pixels);
  }
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  CreateBuffer(totalSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               stagingBuffer, stagingBufferMemory);

  void* data;
  vkMapMemory(m_LogicalDevice, stagingBufferMemory, 0, totalSize, 0, &data);
  memcpy(data, allPixels, static_cast<size_t>(totalSize));
  vkUnmapMemory(m_LogicalDevice, stagingBufferMemory);

  CreateImage(texture->m_Width, texture->m_Height, texture->m_MipLevels,
              VK_SAMPLE_COUNT_1_BIT, textureProps.ImageFormat,
              VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                  VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture->m_Image,
              texture->m_DeviceMemory, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, 6);

  for (uint32_t layer = 0; layer < 6; layer++) {
    TransitionImageLayout(
        texture->m_Image, textureProps.ImageFormat, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, texture->m_MipLevels, layer, 1);

    CopyBufferToImage(stagingBuffer, texture->m_Image,
                      static_cast<uint32_t>(texture->m_Width),
                      static_cast<uint32_t>(texture->m_Height),
                      texture->m_Size * layer, layer);
  }

  vkDestroyBuffer(m_LogicalDevice, stagingBuffer, nullptr);
  vkFreeMemory(m_LogicalDevice, stagingBufferMemory, nullptr);
  delete[] allPixels;

  texture->m_Sampler = CreateTextureSampler(texture->m_MipLevels, samplerProps);
  texture->m_ImageView = CreateImageView(
      texture->m_Image, textureProps.ImageFormat, VK_IMAGE_ASPECT_COLOR_BIT,
      texture->m_MipLevels, VK_IMAGE_VIEW_TYPE_CUBE, 0, 6);
  for (uint32_t layer = 0; layer < 6; layer++) {
    TransitionImageLayout(texture->m_Image, textureProps.ImageFormat,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          texture->m_MipLevels, layer, 1);
  }
  texture->m_IsAllocated = true;
  return texture;
}

Ref<AttachmentTexture> Renderer::CreateAttachmentTexture(
    const AttachmentTextureProps& props) {
  if (props.Type == AttachmentTextureType::SwapChain) {
    throw new std::runtime_error(
        "AttachmentTextureType::SwapChain cannot be created!");
  }
  Ref<AttachmentTexture> texture = CreateReference<AttachmentTexture>();
  texture->m_Type = props.Type;
  texture->m_Format = props.ImageFormat;
  texture->m_Width = props.Width;
  texture->m_Height = props.Height;
  texture->m_MsaaSamples = props.MsaaSamples;
  int flags;
  if (props.Type == AttachmentTextureType::DepthStencil) {
    flags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  } else {
    flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  }

  if (props.Sampled) {
    flags |= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  } else {
    flags |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
  }
  if (props.TransferDest) {
    flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  }

  int aspectFlags;
  if (props.Type == AttachmentTextureType::DepthStencil) {
    aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
  } else {
    aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
  }
  texture->m_AspectFlags = aspectFlags;
  texture->m_MipLevels = 1;
  texture->m_Images.resize(props.ImageCount);
  texture->m_DeviceMemories.resize(props.ImageCount);
  texture->m_ImageViews.resize(props.ImageCount);

  for (uint32_t i = 0; i < props.ImageCount; i++) {
    CreateImage(props.Width, props.Height, 1, props.MsaaSamples,
                props.ImageFormat, VK_IMAGE_TILING_OPTIMAL, flags,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture->m_Images[i],
                texture->m_DeviceMemories[i], 0, props.LayerCount);

    if (props.LayerCount != 1)
      texture->m_ImageViews[i] =
          CreateImageView(texture->m_Images[i], props.ImageFormat, aspectFlags,
                          1, VK_IMAGE_VIEW_TYPE_2D_ARRAY, 0, props.LayerCount);
    else {
      texture->m_ImageViews[i] =
          CreateImageView(texture->m_Images[i], props.ImageFormat, aspectFlags,
                          1, VK_IMAGE_VIEW_TYPE_2D, 0, 1);
    }

    if (props.Type == AttachmentTextureType::DepthStencil) {
      TransitionImageLayout(texture->m_Images[i], props.ImageFormat,
                            VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1,
                            0, props.LayerCount);
    } else if (props.Type == AttachmentTextureType::Color ||
               props.Type == AttachmentTextureType::Resolve ||
               props.Type == AttachmentTextureType::Offscreen) {
      TransitionImageLayout(
          texture->m_Images[i], props.ImageFormat, VK_IMAGE_LAYOUT_UNDEFINED,
          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1, 0, props.LayerCount);
    } else if (props.Type == AttachmentTextureType::SwapChain) {
      TransitionImageLayout(
          texture->m_Images[i], props.ImageFormat, VK_IMAGE_LAYOUT_UNDEFINED,
          VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 1, 0, props.LayerCount);
    }
  }

  texture->m_IsAllocated = true;
  return texture;
}

void Renderer::SetAttachmentTextureBuffer(Ref<AttachmentTexture> texture,
                                          void* buffer, size_t sizePerPixel) {
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  size_t size = texture->m_Width * texture->m_Height * sizePerPixel;
  CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               stagingBuffer, stagingBufferMemory);

  void* data;
  vkMapMemory(m_LogicalDevice, stagingBufferMemory, 0, size, 0, &data);
  memcpy(data, buffer, static_cast<size_t>(size));
  vkUnmapMemory(m_LogicalDevice, stagingBufferMemory);

  TransitionImageLayout(texture->m_Images[0], texture->m_Format,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1);

  CopyBufferToImage(stagingBuffer, texture->m_Images[0],
                    static_cast<uint32_t>(texture->m_Width),
                    static_cast<uint32_t>(texture->m_Height));

  TransitionImageLayout(texture->m_Images[0], texture->m_Format,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1);

  vkDestroyBuffer(m_LogicalDevice, stagingBuffer, nullptr);
  vkFreeMemory(m_LogicalDevice, stagingBufferMemory, nullptr);
}

void Renderer::DestroyTexture(Texture& texture) {
  if (!texture.m_IsAllocated) {
    return;
  }

  texture.m_ImageView = nullptr;
  vkDeviceWaitIdle(m_LogicalDevice);
  vkDestroySampler(m_LogicalDevice, texture.m_Sampler, nullptr);
  vkDestroyImage(m_LogicalDevice, texture.m_Image, nullptr);
  vkFreeMemory(m_LogicalDevice, texture.m_DeviceMemory, nullptr);

  texture.m_IsAllocated = false;
}

VkSampler Renderer::CreateTextureSampler(uint32_t mipLevels,
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
                 m_PhysicalDeviceProperties.limits.maxSamplerAnisotropy);
  }
  samplerInfo.borderColor = props.BorderColor;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = VK_FALSE;
  samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerInfo.maxLod = static_cast<float>(mipLevels);

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
  texture.m_ImageViews.clear();
  if (texture.m_Type != AttachmentTextureType::SwapChain) {
    for (VkImage& image : texture.m_Images) {
      vkDestroyImage(m_LogicalDevice, image, nullptr);
    }
    for (VkDeviceMemory& memory : texture.m_DeviceMemories) {
      vkFreeMemory(m_LogicalDevice, memory, nullptr);
    }
  }
  texture.m_IsAllocated = false;
}

Ref<DescriptorSet> Renderer::CreateMeshDescriptors(
    Ref<UniformBuffer> uniformBuffer, Ref<Material> material) {
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
      m_LogicalDevice, &poolInfo, nullptr, &object->m_DescriptorPool));

  std::vector<VkDescriptorSetLayout> layouts{
      1, m_GeometryMeshDescriptorLayout->m_Layout};
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = object->m_DescriptorPool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = layouts.data();
  WIESEL_CHECK_VKRESULT(vkAllocateDescriptorSets(m_LogicalDevice, &allocInfo,
                                                 &object->m_DescriptorSet));

  std::vector<VkWriteDescriptorSet> writes;
  writes.reserve(8);
  std::vector<VkDescriptorBufferInfo> bufferInfos;
  bufferInfos.reserve(1);
  std::vector<VkDescriptorImageInfo> imageInfos;
  imageInfos.reserve(7);

  {
    bufferInfos.push_back({
        .buffer = uniformBuffer->m_Buffer,
        .offset = 0,
        .range = sizeof(MatricesUniformData),
    });
    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = object->m_DescriptorSet;
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
    if (material->BaseTexture == nullptr) {
      imageInfo.imageView = m_BlankTexture->m_ImageView->m_Handle;
      imageInfo.sampler = m_BlankTexture->m_Sampler;
    } else {
      imageInfo.imageView = material->BaseTexture->m_ImageView->m_Handle;
      imageInfo.sampler = material->BaseTexture->m_Sampler;
    }
    imageInfos.emplace_back(imageInfo);

    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = object->m_DescriptorSet;
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
    if (material->NormalMap == nullptr) {
      imageInfo.imageView = m_BlankTexture->m_ImageView->m_Handle;
      imageInfo.sampler = m_BlankTexture->m_Sampler;
    } else {
      imageInfo.imageView = material->NormalMap->m_ImageView->m_Handle;
      imageInfo.sampler = material->NormalMap->m_Sampler;
    }
    imageInfos.emplace_back(imageInfo);

    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = object->m_DescriptorSet;
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
    if (material->SpecularMap == nullptr) {
      imageInfo.imageView = m_BlankTexture->m_ImageView->m_Handle;
      imageInfo.sampler = m_BlankTexture->m_Sampler;
    } else {
      imageInfo.imageView = material->SpecularMap->m_ImageView->m_Handle;
      imageInfo.sampler = material->SpecularMap->m_Sampler;
    }
    imageInfos.emplace_back(imageInfo);

    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = object->m_DescriptorSet;
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
    if (material->HeightMap == nullptr) {
      imageInfo.imageView = m_BlankTexture->m_ImageView->m_Handle;
      imageInfo.sampler = m_BlankTexture->m_Sampler;
    } else {
      imageInfo.imageView = material->HeightMap->m_ImageView->m_Handle;
      imageInfo.sampler = material->HeightMap->m_Sampler;
    }
    imageInfos.emplace_back(imageInfo);

    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = object->m_DescriptorSet;
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
    if (material->AlbedoMap == nullptr) {
      imageInfo.imageView = m_BlankTexture->m_ImageView->m_Handle;
      imageInfo.sampler = m_BlankTexture->m_Sampler;
    } else {
      imageInfo.imageView = material->AlbedoMap->m_ImageView->m_Handle;
      imageInfo.sampler = material->AlbedoMap->m_Sampler;
    }
    imageInfos.emplace_back(imageInfo);

    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = object->m_DescriptorSet;
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
    if (material->RoughnessMap == nullptr) {
      imageInfo.imageView = m_BlankTexture->m_ImageView->m_Handle;
      imageInfo.sampler = m_BlankTexture->m_Sampler;
    } else {
      imageInfo.imageView = material->RoughnessMap->m_ImageView->m_Handle;
      imageInfo.sampler = material->RoughnessMap->m_Sampler;
    }
    imageInfos.emplace_back(imageInfo);

    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = object->m_DescriptorSet;
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
    if (material->MetallicMap == nullptr) {
      imageInfo.imageView = m_BlankTexture->m_ImageView->m_Handle;
      imageInfo.sampler = m_BlankTexture->m_Sampler;
    } else {
      imageInfo.imageView = material->MetallicMap->m_ImageView->m_Handle;
      imageInfo.sampler = material->MetallicMap->m_Sampler;
    }
    imageInfos.emplace_back(imageInfo);

    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = object->m_DescriptorSet;
    set.dstBinding = 7;
    set.dstArrayElement = 0;
    set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    set.descriptorCount = 1;
    set.pImageInfo = &imageInfos.back();
    set.pNext = nullptr;
    writes.emplace_back(set);
  }

  vkUpdateDescriptorSets(m_LogicalDevice, static_cast<uint32_t>(writes.size()),
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
      m_LogicalDevice, &poolInfo, nullptr, &object->m_DescriptorPool));

  std::vector<VkDescriptorSetLayout> layouts{
      1, m_ShadowMeshDescriptorLayout->m_Layout};
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = object->m_DescriptorPool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = layouts.data();
  WIESEL_CHECK_VKRESULT(vkAllocateDescriptorSets(m_LogicalDevice, &allocInfo,
                                                 &object->m_DescriptorSet));

  std::vector<VkWriteDescriptorSet> writes;
  writes.reserve(2);
  std::vector<VkDescriptorBufferInfo> bufferInfos;
  bufferInfos.reserve(1);
  std::vector<VkDescriptorImageInfo> imageInfos;
  imageInfos.reserve(1);

  {
    bufferInfos.push_back({
        .buffer = uniformBuffer->m_Buffer,
        .offset = 0,
        .range = sizeof(MatricesUniformData),
    });
    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = object->m_DescriptorSet;
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
    if (material->BaseTexture == nullptr) {
      imageInfo.imageView = m_BlankTexture->m_ImageView->m_Handle;
      imageInfo.sampler = m_BlankTexture->m_Sampler;
    } else {
      imageInfo.imageView = material->BaseTexture->m_ImageView->m_Handle;
      imageInfo.sampler = material->BaseTexture->m_Sampler;
    }
    imageInfos.emplace_back(imageInfo);

    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = object->m_DescriptorSet;
    set.dstBinding = 1;
    set.dstArrayElement = 0;
    set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    set.descriptorCount = 1;
    set.pImageInfo = &imageInfos.back();
    set.pNext = nullptr;
    writes.emplace_back(set);
  }

  vkUpdateDescriptorSets(m_LogicalDevice, static_cast<uint32_t>(writes.size()),
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
      m_LogicalDevice, &poolInfo, nullptr, &object->m_DescriptorPool));

  std::vector<VkDescriptorSetLayout> layouts{
      1, m_GlobalDescriptorLayout->m_Layout};
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = object->m_DescriptorPool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = layouts.data();
  WIESEL_CHECK_VKRESULT(vkAllocateDescriptorSets(m_LogicalDevice, &allocInfo,
                                                 &object->m_DescriptorSet));

  std::vector<VkWriteDescriptorSet> writes;
  writes.reserve(4);
  std::vector<VkDescriptorBufferInfo> bufferInfos;
  bufferInfos.reserve(3);
  std::vector<VkDescriptorImageInfo> imageInfos;
  imageInfos.reserve(1);

  {
    VkDescriptorBufferInfo bufferInfo;
    bufferInfo.buffer = m_LightsUniformBuffer->m_Buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(LightsUniformData);
    bufferInfos.emplace_back(bufferInfo);

    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = object->m_DescriptorSet;
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
    bufferInfo.buffer = m_CameraUniformBuffer->m_Buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(CameraUniformData);
    bufferInfos.emplace_back(bufferInfo);

    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = object->m_DescriptorSet;
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
    bufferInfo.buffer = m_ShadowCameraUniformBuffer->m_Buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(ShadowMapMatricesUniformData);
    bufferInfos.emplace_back(bufferInfo);

    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = object->m_DescriptorSet;
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
    if (camera.ShadowDepthViewArray == nullptr) {
      imageInfo.imageView = m_BlankTexture->m_ImageView->m_Handle;
      imageInfo.sampler = m_BlankTexture->m_Sampler;
    } else {
      imageInfo.imageView = camera.ShadowDepthViewArray->m_Handle;
      imageInfo.sampler = m_DefaultLinearSampler->m_Sampler;
    }
    imageInfos.emplace_back(imageInfo);

    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = object->m_DescriptorSet;
    set.dstBinding = 3;
    set.dstArrayElement = 0;
    set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    set.descriptorCount = 1;
    set.pImageInfo = &imageInfos.back();
    set.pNext = nullptr;
    writes.emplace_back(set);
  }

  vkUpdateDescriptorSets(m_LogicalDevice, static_cast<uint32_t>(writes.size()),
                         writes.data(), 0, nullptr);

  object->m_Allocated = true;

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
      m_LogicalDevice, &poolInfo, nullptr, &object->m_DescriptorPool));

  std::vector<VkDescriptorSetLayout> layouts{
      1, m_GlobalShadowDescriptorLayout->m_Layout};
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = object->m_DescriptorPool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = layouts.data();
  WIESEL_CHECK_VKRESULT(vkAllocateDescriptorSets(m_LogicalDevice, &allocInfo,
                                                 &object->m_DescriptorSet));

  std::vector<VkWriteDescriptorSet> writes;
  writes.reserve(1);
  std::vector<VkDescriptorBufferInfo> bufferInfos;
  bufferInfos.reserve(1);

  {
    VkDescriptorBufferInfo bufferInfo;
    bufferInfo.buffer = m_ShadowCameraUniformBuffer->m_Buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(ShadowMapMatricesUniformData);
    bufferInfos.emplace_back(bufferInfo);

    VkWriteDescriptorSet set{};
    set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    set.dstSet = object->m_DescriptorSet;
    set.dstBinding = 0;
    set.dstArrayElement = 0;
    set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    set.descriptorCount = 1;
    set.pBufferInfo = &bufferInfos[bufferInfos.size() - 1];
    set.pNext = nullptr;

    writes.emplace_back(set);
  }

  vkUpdateDescriptorSets(m_LogicalDevice, static_cast<uint32_t>(writes.size()),
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
      m_LogicalDevice, &poolInfo, nullptr, &object->m_DescriptorPool));

  std::vector<VkDescriptorSetLayout> layouts{
      1, m_PresentDescriptorLayout->m_Layout};
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = object->m_DescriptorPool;
  allocInfo.descriptorSetCount = layouts.size();
  allocInfo.pSetLayouts = layouts.data();
  WIESEL_CHECK_VKRESULT(vkAllocateDescriptorSets(m_LogicalDevice, &allocInfo,
                                                 &object->m_DescriptorSet));

  std::vector<VkWriteDescriptorSet> writes{};

  VkDescriptorImageInfo imageInfo;
  imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  imageInfo.imageView = texture->m_ImageViews[0]->m_Handle;
  imageInfo.sampler = texture->m_Samplers.empty()
                          ? m_DefaultLinearSampler->m_Sampler
                          : texture->m_Samplers[0]->m_Sampler;
  VkWriteDescriptorSet set{};
  set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  set.dstSet = object->m_DescriptorSet;
  set.dstBinding = 0;
  set.dstArrayElement = 0;
  set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  set.descriptorCount = 1;
  set.pImageInfo = &imageInfo;
  set.pNext = nullptr;

  writes.push_back(set);

  vkUpdateDescriptorSets(m_LogicalDevice, static_cast<uint32_t>(writes.size()),
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
      m_LogicalDevice, &poolInfo, nullptr, &object->m_DescriptorPool));

  std::vector<VkDescriptorSetLayout> layouts{
      1, m_PresentDescriptorLayout->m_Layout};
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = object->m_DescriptorPool;
  allocInfo.descriptorSetCount = layouts.size();
  allocInfo.pSetLayouts = layouts.data();
  WIESEL_CHECK_VKRESULT(vkAllocateDescriptorSets(m_LogicalDevice, &allocInfo,
                                                 &object->m_DescriptorSet));

  std::vector<VkWriteDescriptorSet> writes{};

  VkDescriptorImageInfo imageInfo;
  imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  imageInfo.imageView = texture->m_ImageView->m_Handle;
  imageInfo.sampler = texture->m_Sampler ? texture->m_Sampler
                                         : m_DefaultLinearSampler->m_Sampler;

  VkWriteDescriptorSet set{};
  set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  set.dstSet = object->m_DescriptorSet;
  set.dstBinding = 0;
  set.dstArrayElement = 0;
  set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  set.descriptorCount = 1;
  set.pImageInfo = &imageInfo;
  set.pNext = nullptr;

  writes.push_back(set);

  vkUpdateDescriptorSets(m_LogicalDevice, static_cast<uint32_t>(writes.size()),
                         writes.data(), 0, nullptr);

  return object;
}

void Renderer::DestroyDescriptorLayout(DescriptorSetLayout& layout) {
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
  m_RecreatePipeline = true;
}

bool Renderer::IsWireframeEnabled() {
  return m_EnableWireframe;
}

bool* Renderer::IsWireframeEnabledPtr() {
  return &m_EnableWireframe;
}

void Renderer::SetSSAOEnabled(bool value) {
  m_EnableSSAO = value;
}

bool Renderer::IsSSAOEnabled() {
  return m_EnableSSAO;
}

bool* Renderer::IsSSAOEnabledPtr() {
  return &m_EnableSSAO;
}

void Renderer::SetRecreatePipeline(bool value) {
  m_RecreatePipeline = value;
}

bool Renderer::IsRecreatePipeline() {
  return m_RecreatePipeline;
}

float Renderer::GetAspectRatio() const {
  return m_AspectRatio;
}

const WindowSize& Renderer::GetWindowSize() const {
  return m_WindowSize;
}

void Renderer::Cleanup() {
  if (!m_Initialized) {
    return;
  }

  vkDeviceWaitIdle(m_LogicalDevice);
  LOG_DEBUG("Destroying Renderer");

  m_Camera = nullptr;
  m_QuadIndexBuffer = nullptr;
  m_QuadVertexBuffer = nullptr;

  CleanupGlobalUniformBuffers();
  m_BlankTexture = nullptr;

  LOG_DEBUG("Destroying graphics");
  CleanupGeometryGraphics();
  CleanupPresentGraphics();

  LOG_DEBUG("Destroying descriptor set layout");
  CleanupDescriptorLayouts();

  LOG_DEBUG("Destroying semaphores and fences");
  vkDestroySemaphore(m_LogicalDevice, m_RenderFinishedSemaphore, nullptr);
  vkDestroySemaphore(m_LogicalDevice, m_ImageAvailableSemaphore, nullptr);
  vkDestroyFence(m_LogicalDevice, m_Fence, nullptr);

  LOG_DEBUG("Destroying command pool");
  m_CommandBuffer = nullptr;
  m_CommandPool = nullptr;

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
    vkGetPhysicalDeviceProperties(m_PhysicalDevice,
                                  &m_PhysicalDeviceProperties);
    vkGetPhysicalDeviceFeatures(m_PhysicalDevice, &m_PhysicalDeviceFeatures);
    m_MsaaSamples = GetMaxUsableSampleCount();
    m_PreviousMsaaSamples = m_MsaaSamples;
    if (m_PhysicalDeviceFeatures.shaderImageGatherExtended) {
      m_ShaderFeatures.push_back("USE_GATHER");
    }
  } else {
    throw std::runtime_error("failed to find a suitable GPU!");
  }
}

void Renderer::CreateLogicalDevice() {
  LOG_DEBUG("Creating logical device");
  m_QueueFamilyIndices = FindQueueFamilies(m_PhysicalDevice);

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
      static_cast<uint32_t>(m_DeviceExtensions.size());
  createInfo.ppEnabledExtensionNames = m_DeviceExtensions.data();
  createInfo.enabledLayerCount = 0;

  if (vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr,
                     &m_LogicalDevice) != VK_SUCCESS) {
    throw std::runtime_error("failed to create logical device!");
  }

  vkGetDeviceQueue(m_LogicalDevice, GetPresentQueueFamilyIndex(), 0,
                   &m_PresentQueue);
  vkGetDeviceQueue(m_LogicalDevice, GetGraphicsQueueFamilyIndex(), 0,
                   &m_GraphicsQueue);
}

void Renderer::CreateDescriptorLayouts() {
  m_GeometryMeshDescriptorLayout = CreateReference<DescriptorSetLayout>();
  m_GeometryMeshDescriptorLayout->AddBinding(
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

  for (int i = 0; i < kMaterialTextureCount; i++) {
    m_GeometryMeshDescriptorLayout->AddBinding(
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        VK_SHADER_STAGE_FRAGMENT_BIT);
  }
  m_GeometryMeshDescriptorLayout->Bake();

  m_ShadowMeshDescriptorLayout = CreateReference<DescriptorSetLayout>();
  m_ShadowMeshDescriptorLayout->AddBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                           VK_SHADER_STAGE_VERTEX_BIT);
  m_ShadowMeshDescriptorLayout->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
  m_ShadowMeshDescriptorLayout->Bake();

  m_GlobalShadowDescriptorLayout = CreateReference<DescriptorSetLayout>();
  m_GlobalShadowDescriptorLayout->AddBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                             VK_SHADER_STAGE_VERTEX_BIT);
  m_GlobalShadowDescriptorLayout->Bake();

  m_GlobalDescriptorLayout = CreateReference<DescriptorSetLayout>();
  m_GlobalDescriptorLayout->AddBinding(
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
  m_GlobalDescriptorLayout->AddBinding(
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
  m_GlobalDescriptorLayout->AddBinding(
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
  m_GlobalDescriptorLayout->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
  m_GlobalDescriptorLayout->Bake();

  m_PresentDescriptorLayout = CreateReference<DescriptorSetLayout>();
  m_PresentDescriptorLayout->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
  m_PresentDescriptorLayout->Bake();

  m_SkyboxDescriptorLayout = CreateReference<DescriptorSetLayout>();
  m_SkyboxDescriptorLayout->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
  m_SkyboxDescriptorLayout->Bake();

  m_SSAOGenDescriptorLayout = CreateReference<DescriptorSetLayout>();
  m_SSAOGenDescriptorLayout->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
  m_SSAOGenDescriptorLayout->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
  m_SSAOGenDescriptorLayout->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
  m_SSAOGenDescriptorLayout->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
  m_SSAOGenDescriptorLayout->AddBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                        VK_SHADER_STAGE_FRAGMENT_BIT);
  m_SSAOGenDescriptorLayout->Bake();

  m_SSAOOutputDescriptorLayout = CreateReference<DescriptorSetLayout>();
  m_SSAOOutputDescriptorLayout->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT); // samplerSSAO
  m_SSAOOutputDescriptorLayout->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT); // samplerDepth
  m_SSAOOutputDescriptorLayout->Bake();

  m_SSAOBlurDescriptorLayout = CreateReference<DescriptorSetLayout>();
  m_SSAOBlurDescriptorLayout->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT); // samplerSSAO
  m_SSAOBlurDescriptorLayout->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT); // samplerDepth
  m_SSAOBlurDescriptorLayout->Bake();

  m_GeometryOutputDescriptorLayout = CreateReference<DescriptorSetLayout>();
  m_GeometryOutputDescriptorLayout->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
  m_GeometryOutputDescriptorLayout->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
  m_GeometryOutputDescriptorLayout->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
  m_GeometryOutputDescriptorLayout->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
  m_GeometryOutputDescriptorLayout->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
  m_GeometryOutputDescriptorLayout->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
  m_GeometryOutputDescriptorLayout->Bake();

  m_SpriteDrawDescriptorLayout = CreateReference<DescriptorSetLayout>();
  m_SpriteDrawDescriptorLayout->AddBinding(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
  m_SpriteDrawDescriptorLayout->AddBinding(
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
  m_SpriteDrawDescriptorLayout->Bake();
}

void Renderer::CreateSwapChain() {
  LOG_DEBUG("Creating swap chain");
  m_SwapChainDetails = QuerySwapChainSupport(m_PhysicalDevice);

  VkSurfaceFormatKHR surfaceFormat =
      ChooseSwapSurfaceFormat(m_SwapChainDetails.formats);
  VkPresentModeKHR presentMode =
      ChooseSwapPresentMode(m_SwapChainDetails.presentModes);
  m_Extent = ChooseSwapExtent(m_SwapChainDetails.capabilities);

  uint32_t imageCount = m_SwapChainDetails.capabilities.minImageCount + 1;

  if (m_SwapChainDetails.capabilities.maxImageCount > 0 &&
      imageCount > m_SwapChainDetails.capabilities.maxImageCount) {
    imageCount = m_SwapChainDetails.capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  createInfo.surface = m_Surface;
  createInfo.minImageCount = imageCount;
  createInfo.imageFormat = surfaceFormat.format;
  createInfo.imageColorSpace = surfaceFormat.colorSpace;
  createInfo.imageExtent = m_Extent;
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
  createInfo.preTransform = m_SwapChainDetails.capabilities.currentTransform;
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

  std::vector<VkImage> swapChainImages;
  vkGetSwapchainImagesKHR(m_LogicalDevice, m_SwapChain, &imageCount, nullptr);
  swapChainImages.resize(imageCount);
  vkGetSwapchainImagesKHR(m_LogicalDevice, m_SwapChain, &imageCount,
                          swapChainImages.data());
  m_SwapChainImageFormat = surfaceFormat.format;

  m_AspectRatio = m_Extent.width / (float)m_Extent.height;
  m_WindowSize.Width = m_Extent.width;
  m_WindowSize.Height = m_Extent.height;
  m_RecreateSwapChain = false;
  m_SwapChainCreated = true;

  Ref<AttachmentTexture> texture = CreateReference<AttachmentTexture>();
  texture->m_Format = surfaceFormat.format;
  texture->m_Width = m_Extent.width;
  texture->m_Height = m_Extent.height;
  texture->m_Type = AttachmentTextureType::SwapChain;
  texture->m_IsAllocated = true;
  texture->m_MsaaSamples = m_MsaaSamples;
  for (VkImage& image : swapChainImages) {
    TransitionImageLayout(image, m_SwapChainImageFormat,
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1);

    TransitionImageLayout(image, m_SwapChainImageFormat,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 1);
    texture->m_Images.push_back(image);
    texture->m_ImageViews.push_back(CreateImageView(
        image, m_SwapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1));
  }
  m_SwapChainTexture = texture;

  m_PresentDepthStencil = CreateAttachmentTexture(
      {m_Extent.width, m_Extent.height, AttachmentTextureType::DepthStencil,
       static_cast<uint32_t>(swapChainImages.size()), FindDepthFormat(),
       m_MsaaSamples});

  m_PresentColorImage = CreateAttachmentTexture(
      {m_Extent.width, m_Extent.height, AttachmentTextureType::Color,
       static_cast<uint32_t>(swapChainImages.size()), m_SwapChainImageFormat,
       m_MsaaSamples});

  m_PresentRenderPass = CreateReference<RenderPass>(PassType::Present);
  m_PresentRenderPass->AttachOutput(m_PresentColorImage);
  m_PresentRenderPass->AttachOutput(m_PresentDepthStencil);
  m_PresentRenderPass->AttachOutput(m_SwapChainTexture);
  m_PresentRenderPass->Bake();
  m_PresentFramebuffers.resize(swapChainImages.size());
  std::array<AttachmentTexture*, 3> textures{m_PresentColorImage.get(),
                                             m_PresentDepthStencil.get(),
                                             m_SwapChainTexture.get()};
  for (uint32_t i = 0; i < swapChainImages.size(); i++) {
    m_PresentFramebuffers[i] = m_PresentRenderPass->CreateFramebuffer(
        i, textures, {m_Extent.width, m_Extent.height});
  }
}

void Renderer::CreateGeometryRenderPass() {
  LOG_DEBUG("Creating render pass");

  m_GeometryRenderPass = CreateReference<RenderPass>(PassType::Geometry);
  m_GeometryRenderPass->AttachOutput({.Type = AttachmentTextureType::Offscreen,
                                      .Format = VK_FORMAT_R32G32B32A32_SFLOAT,
                                      .MsaaSamples = m_MsaaSamples});
  m_GeometryRenderPass->AttachOutput({.Type = AttachmentTextureType::Offscreen,
                                      .Format = VK_FORMAT_R32G32B32A32_SFLOAT,
                                      .MsaaSamples = m_MsaaSamples});
  m_GeometryRenderPass->AttachOutput({.Type = AttachmentTextureType::Offscreen,
                                      .Format = VK_FORMAT_R32_SFLOAT,
                                      .MsaaSamples = m_MsaaSamples});
  m_GeometryRenderPass->AttachOutput({.Type = AttachmentTextureType::Offscreen,
                                      .Format = VK_FORMAT_R8G8B8A8_UNORM,
                                      .MsaaSamples = m_MsaaSamples});
  m_GeometryRenderPass->AttachOutput({.Type = AttachmentTextureType::Offscreen,
                                      .Format = VK_FORMAT_R8G8B8A8_UNORM,
                                      .MsaaSamples = m_MsaaSamples});
  m_GeometryRenderPass->AttachOutput({.Type = AttachmentTextureType::Offscreen,
                                      .Format = VK_FORMAT_R16G16B16A16_SFLOAT,
                                      .MsaaSamples = m_MsaaSamples});
  m_GeometryRenderPass->AttachOutput(
      {.Type = AttachmentTextureType::DepthStencil,
       .Format = FindDepthFormat(),
       .MsaaSamples = m_MsaaSamples});
  if (m_MsaaSamples > VK_SAMPLE_COUNT_1_BIT) {
    m_GeometryRenderPass->AttachOutput({.Type = AttachmentTextureType::Resolve,
                                        .Format = VK_FORMAT_R32G32B32A32_SFLOAT,
                                        .MsaaSamples = VK_SAMPLE_COUNT_1_BIT});
    m_GeometryRenderPass->AttachOutput({.Type = AttachmentTextureType::Resolve,
                                        .Format = VK_FORMAT_R32G32B32A32_SFLOAT,
                                        .MsaaSamples = VK_SAMPLE_COUNT_1_BIT});
    m_GeometryRenderPass->AttachOutput({.Type = AttachmentTextureType::Resolve,
                                        .Format = VK_FORMAT_R32_SFLOAT,
                                        .MsaaSamples = VK_SAMPLE_COUNT_1_BIT});
    m_GeometryRenderPass->AttachOutput({.Type = AttachmentTextureType::Resolve,
                                        .Format = VK_FORMAT_R8G8B8A8_UNORM,
                                        .MsaaSamples = VK_SAMPLE_COUNT_1_BIT});
    m_GeometryRenderPass->AttachOutput({.Type = AttachmentTextureType::Resolve,
                                        .Format = VK_FORMAT_R8G8B8A8_UNORM,
                                        .MsaaSamples = VK_SAMPLE_COUNT_1_BIT});
    m_GeometryRenderPass->AttachOutput({.Type = AttachmentTextureType::Resolve,
                                        .Format = VK_FORMAT_R16G16B16A16_SFLOAT,
                                        .MsaaSamples = VK_SAMPLE_COUNT_1_BIT});
  }
  m_GeometryRenderPass->Bake();

  m_LightingRenderPass = CreateReference<RenderPass>(PassType::Lighting);
  m_LightingRenderPass->AttachOutput({.Type = AttachmentTextureType::Offscreen,
                                      .Format = m_SwapChainImageFormat,
                                      .MsaaSamples = m_MsaaSamples});
  if (m_MsaaSamples > VK_SAMPLE_COUNT_1_BIT) {
    m_LightingRenderPass->AttachOutput({.Type = AttachmentTextureType::Resolve,
                                        .Format = m_SwapChainImageFormat,
                                        .MsaaSamples = VK_SAMPLE_COUNT_1_BIT});
  }
  m_LightingRenderPass->Bake();

  m_CompositeRenderPass = CreateReference<RenderPass>(PassType::PostProcess);
  m_CompositeRenderPass->AttachOutput({.Type = AttachmentTextureType::Offscreen,
                                       .Format = m_SwapChainImageFormat,
                                       .MsaaSamples = m_MsaaSamples});
  if (m_MsaaSamples > VK_SAMPLE_COUNT_1_BIT) {
    m_CompositeRenderPass->AttachOutput({.Type = AttachmentTextureType::Resolve,
                                         .Format = m_SwapChainImageFormat,
                                         .MsaaSamples = VK_SAMPLE_COUNT_1_BIT});
  }
  m_CompositeRenderPass->Bake();

  m_SpriteRenderPass = CreateReference<RenderPass>(PassType::PostProcess);
  m_SpriteRenderPass->AttachOutput({.Type = AttachmentTextureType::Offscreen,
                                    .Format = m_SwapChainImageFormat,
                                    .MsaaSamples = VK_SAMPLE_COUNT_1_BIT});
  m_SpriteRenderPass->Bake();

  m_SSAOGenRenderPass = CreateReference<RenderPass>(PassType::PostProcess);
  m_SSAOGenRenderPass->AttachOutput({.Type = AttachmentTextureType::Offscreen,
                                     .Format = VK_FORMAT_R8_UNORM,
                                     .MsaaSamples = VK_SAMPLE_COUNT_1_BIT});
  m_SSAOGenRenderPass->Bake();

  m_SSAOBlurHorzRenderPass = CreateReference<RenderPass>(PassType::PostProcess);
  m_SSAOBlurHorzRenderPass->AttachOutput({.Type = AttachmentTextureType::Offscreen,
                                      .Format = VK_FORMAT_R8_UNORM,
                                      .MsaaSamples = VK_SAMPLE_COUNT_1_BIT});
  m_SSAOBlurHorzRenderPass->Bake();

  m_SSAOBlurVertRenderPass = CreateReference<RenderPass>(PassType::PostProcess);
  m_SSAOBlurVertRenderPass->AttachOutput({.Type = AttachmentTextureType::Offscreen,
                                      .Format = VK_FORMAT_R8_UNORM,
                                      .MsaaSamples = VK_SAMPLE_COUNT_1_BIT});
  m_SSAOBlurVertRenderPass->Bake();

  m_ShadowRenderPass = CreateReference<RenderPass>(PassType::Shadow);
  m_ShadowRenderPass->AttachOutput({.Type = AttachmentTextureType::DepthStencil,
                                    .Format = FindDepthFormat(),
                                    .MsaaSamples = VK_SAMPLE_COUNT_1_BIT});
  m_ShadowRenderPass->Bake();
}

void Renderer::CreateGeometryGraphicsPipelines() {
  LOG_DEBUG("Creating graphics pipeline");
  auto geometryVertexShader =
      CreateShader({ShaderTypeVertex, ShaderLangGLSL, "main",
                    ShaderSourceSource, "assets/internal_shaders/geometry_shader.vert"});
  auto geometryFragmentShader =
      CreateShader({ShaderTypeFragment, ShaderLangGLSL, "main",
                    ShaderSourceSource, "assets/internal_shaders/geometry_shader.frag"});
  m_GeometryPipeline = CreateReference<Pipeline>(PipelineProperties{
      m_MsaaSamples, CullModeBack, m_EnableWireframe, false});
  m_GeometryPipeline->SetVertexData(Vertex3D::GetBindingDescription(),
                                    Vertex3D::GetAttributeDescriptions());
  m_GeometryPipeline->SetRenderPass(m_GeometryRenderPass);
  m_GeometryPipeline->AddInputLayout(m_GeometryMeshDescriptorLayout);
  m_GeometryPipeline->AddInputLayout(m_GlobalDescriptorLayout);
  m_GeometryPipeline->AddShader(geometryVertexShader);
  m_GeometryPipeline->AddShader(geometryFragmentShader);
  m_GeometryPipeline->Bake();

  auto skyboxVertexShader =
      CreateShader({ShaderTypeVertex, ShaderLangGLSL, "main",
                    ShaderSourceSource, "assets/internal_shaders/skybox_shader.vert"});
  auto skyboxFragmentShader =
      CreateShader({ShaderTypeFragment, ShaderLangGLSL, "main",
                    ShaderSourceSource, "assets/internal_shaders/skybox_shader.frag"});
  m_SkyboxPipeline = CreateReference<Pipeline>(PipelineProperties{
      m_MsaaSamples, CullModeFront, false, false, true, false});
  m_SkyboxPipeline->SetRenderPass(m_LightingRenderPass);
  m_SkyboxPipeline->AddInputLayout(m_SkyboxDescriptorLayout);
  m_SkyboxPipeline->AddInputLayout(m_GlobalDescriptorLayout);
  m_SkyboxPipeline->AddShader(skyboxVertexShader);
  m_SkyboxPipeline->AddShader(skyboxFragmentShader);
  m_SkyboxPipeline->Bake();

  auto fullscreenVertexShader = CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "assets/internal_shaders/fullscreen_shader.vert"});
  auto lightingFragmentShader =
      CreateShader({ShaderTypeFragment, ShaderLangGLSL, "main",
                    ShaderSourceSource, "assets/internal_shaders/lighting_shader.frag"});

  m_LightingPipeline = CreateReference<Pipeline>(PipelineProperties{
      m_MsaaSamples, CullModeFront, false, true, true, false});
  m_LightingPipeline->SetRenderPass(m_LightingRenderPass);
  m_LightingPipeline->AddInputLayout(m_GeometryOutputDescriptorLayout);
  m_LightingPipeline->AddInputLayout(m_SSAOOutputDescriptorLayout);
  m_LightingPipeline->AddInputLayout(m_GlobalDescriptorLayout);
  m_LightingPipeline->AddInputLayout(m_SkyboxDescriptorLayout);
  m_LightingPipeline->AddShader(fullscreenVertexShader);
  m_LightingPipeline->AddShader(lightingFragmentShader);
  m_LightingPipeline->Bake();

  auto shadowVertexShader =
      CreateShader({ShaderTypeVertex, ShaderLangGLSL, "main",
                    ShaderSourceSource, "assets/internal_shaders/shadow_shader.vert"});
  auto shadowFragmentShader =
      CreateShader({ShaderTypeFragment, ShaderLangGLSL, "main",
                    ShaderSourceSource, "assets/internal_shaders/shadow_shader.frag"});
  m_ShadowPipeline = CreateReference<Pipeline>(PipelineProperties{
      VK_SAMPLE_COUNT_1_BIT, CullModeFront, false, false, true, true});
  m_ShadowPipeline->SetRenderPass(m_ShadowRenderPass);
  m_ShadowPipeline->SetVertexData(Vertex3D::GetBindingDescription(),
                                  Vertex3D::GetAttributeDescriptions());
  m_ShadowPipeline->AddPushConstant(m_ShadowPipelinePushConstant,
                                    VK_SHADER_STAGE_VERTEX_BIT);
  m_ShadowPipeline->AddInputLayout(m_ShadowMeshDescriptorLayout);
  m_ShadowPipeline->AddInputLayout(m_GlobalShadowDescriptorLayout);
  m_ShadowPipeline->AddShader(shadowVertexShader);
  m_ShadowPipeline->AddShader(shadowFragmentShader);
  m_ShadowPipeline->Bake();

  auto ssaoFragmentShader =
      CreateShader({ShaderTypeFragment, ShaderLangGLSL, "main",
                    ShaderSourceSource, "assets/internal_shaders/ssao_gen_shader.frag"});

  m_SSAOGenPipeline = CreateReference<Pipeline>(PipelineProperties{
      VK_SAMPLE_COUNT_1_BIT, CullModeFront, false, false, false, false});
  m_SSAOGenPipeline->SetRenderPass(m_SSAOGenRenderPass);
  m_SSAOGenPipeline->AddInputLayout(m_SSAOGenDescriptorLayout);
  m_SSAOGenPipeline->AddInputLayout(m_GlobalDescriptorLayout);
  m_SSAOGenPipeline->AddShader(fullscreenVertexShader);
  m_SSAOGenPipeline->AddShader(ssaoFragmentShader);
  m_SSAOGenPipeline->Bake();

  auto ssaoBlurHorzFragmentShader = CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "assets/internal_shaders/ssao_blur_shader.frag"});

  m_SSAOBlurHorzPipeline = CreateReference<Pipeline>(PipelineProperties{
      VK_SAMPLE_COUNT_1_BIT, CullModeFront, false, false, false, false});
  m_SSAOBlurHorzPipeline->SetRenderPass(m_SSAOBlurHorzRenderPass);
  m_SSAOBlurHorzPipeline->AddInputLayout(m_SSAOBlurDescriptorLayout);
  m_SSAOBlurHorzPipeline->AddShader(fullscreenVertexShader);
  m_SSAOBlurHorzPipeline->AddShader(ssaoBlurHorzFragmentShader);
  m_SSAOBlurHorzPipeline->Bake();

  auto ssaoBlurVertFragmentShader = CreateShader(
      {ShaderTypeFragment, ShaderLangGLSL, "main", ShaderSourceSource,
       "assets/internal_shaders/ssao_blur_shader.frag", {"BLUR_VERTICAL"}});

  m_SSAOBlurVertPipeline = CreateReference<Pipeline>(PipelineProperties{
      VK_SAMPLE_COUNT_1_BIT, CullModeFront, false, false, false, false});
  m_SSAOBlurVertPipeline->SetRenderPass(m_SSAOBlurVertRenderPass);
  m_SSAOBlurVertPipeline->AddInputLayout(m_SSAOBlurDescriptorLayout);
  m_SSAOBlurVertPipeline->AddShader(fullscreenVertexShader);
  m_SSAOBlurVertPipeline->AddShader(ssaoBlurVertFragmentShader);
  m_SSAOBlurVertPipeline->Bake();

  auto spriteVertexShader =
      CreateShader({ShaderTypeVertex, ShaderLangGLSL, "main",
                    ShaderSourceSource, "assets/internal_shaders/sprite_shader.vert"});
  auto spriteFragmentShader =
      CreateShader({ShaderTypeFragment, ShaderLangGLSL, "main",
                    ShaderSourceSource, "assets/internal_shaders/sprite_shader.frag"});

  m_SpritePipeline = CreateReference<Pipeline>(PipelineProperties{
      VK_SAMPLE_COUNT_1_BIT, CullModeNone, false, true, false, false});
  m_SpritePipeline->SetVertexData(VertexSprite::GetBindingDescriptions(),
                                  VertexSprite::GetAttributeDescriptions());
  m_SpritePipeline->SetRenderPass(m_SpriteRenderPass);
  m_SpritePipeline->AddInputLayout(m_SpriteDrawDescriptorLayout);
  m_SpritePipeline->AddInputLayout(m_GlobalDescriptorLayout);
  m_SpritePipeline->AddShader(spriteVertexShader);
  m_SpritePipeline->AddShader(spriteFragmentShader);
  m_SpritePipeline->Bake();

  auto compositeFragmentShader =
      CreateShader({ShaderTypeFragment, ShaderLangGLSL, "main",
                    ShaderSourceSource, "assets/internal_shaders/quad_shader.frag"});

  m_CompositePipeline = CreateReference<Pipeline>(PipelineProperties{
      m_MsaaSamples, CullModeFront, false, true, true, false});
  m_CompositePipeline->SetRenderPass(m_CompositeRenderPass);
  m_CompositePipeline->AddInputLayout(m_SkyboxDescriptorLayout);
  m_CompositePipeline->AddShader(fullscreenVertexShader);
  m_CompositePipeline->AddShader(compositeFragmentShader);
  m_CompositePipeline->Bake();
}

void Renderer::CreatePresentGraphicsPipelines() {
  auto presentVertexShader = CreateShader(
      {ShaderTypeVertex, ShaderLangGLSL, "main", ShaderSourceSource,
       "assets/internal_shaders/fullscreen_shader.vert"});
  auto presentFragmentShader =
      CreateShader({ShaderTypeFragment, ShaderLangGLSL, "main",
                    ShaderSourceSource, "assets/internal_shaders/quad_shader.frag"});
  m_PresentPipeline = CreateReference<Pipeline>(
      PipelineProperties{m_MsaaSamples, CullModeNone, false, true});
  m_PresentPipeline->SetRenderPass(m_PresentRenderPass);
  m_PresentPipeline->AddInputLayout(m_PresentDescriptorLayout);
  m_PresentPipeline->AddShader(presentVertexShader);
  m_PresentPipeline->AddShader(presentFragmentShader);
  m_PresentPipeline->Bake();
}

void Renderer::RecreatePipeline(Ref<Pipeline> pipeline) {
  pipeline->Bake();
}

Ref<Shader> Renderer::CreateShader(ShaderProperties properties) {
  for (const auto& item : m_ShaderFeatures) {
    properties.Defines.push_back(item);
  }
  return CreateReference<Shader>(properties);
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
                                 uint32_t height, VkDeviceSize baseOffset,
                                 uint32_t layer) {
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
  m_CommandPool = CreateReference<CommandPool>();
}

void Renderer::CreateCommandBuffers() {
  m_CommandBuffer = m_CommandPool->CreateBuffer();
}

void Renderer::CreatePermanentResources() {
  m_ShadowPipelinePushConstant = CreateReference<ShadowPipelinePushConstant>();

  m_BlankTexture = CreateBlankTexture();

  std::vector<Index> quadIndices = {0, 1, 2, 2, 3, 0};
  std::vector<Vertex2DNoColor> quadVertices = {
      {{-1.0f, -1.0f}, {0.0f, 0.0f}},
      {{1.0f, -1.0f}, {1.0f, 0.0f}},
      {{1.0f, 1.0f}, {1.0f, 1.0f}},
      {{-1.0f, 1.0f}, {0.0f, 1.0f}},
  };

  m_QuadIndexBuffer = Engine::GetRenderer()->CreateIndexBuffer(quadIndices);

  m_QuadVertexBuffer = Engine::GetRenderer()->CreateVertexBuffer(quadVertices);

  m_DefaultLinearSampler = CreateReference<Sampler>(1, SamplerProps{});
  m_DefaultNearestSampler = CreateReference<Sampler>(
      1, SamplerProps{VK_FILTER_NEAREST, VK_FILTER_NEAREST, -1.0f});

  // SSAO
  m_SSAOKernelUniformBuffer =
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
    m_SSAOKernelUniformData.Samples[i] = glm::vec4(sample * scale, 0.0f);
  }
  memcpy(m_SSAOKernelUniformBuffer->m_Data, &m_SSAOKernelUniformData,
         sizeof(m_SSAOKernelUniformData));

  // Random noise
  std::vector<glm::vec4> noiseValues(WIESEL_SSAO_NOISE_DIM *
                                     WIESEL_SSAO_NOISE_DIM);
  for (uint32_t i = 0; i < static_cast<uint32_t>(noiseValues.size()); i++) {
    noiseValues[i] = glm::vec4(rndDist(rndEngine) * 2.0f - 1.0f,
                               rndDist(rndEngine) * 2.0f - 1.0f, 0.0f, 0.0f);
  }
  // Upload as texture
  m_SSAONoise = CreateAttachmentTexture(
      AttachmentTextureProps{.Width = WIESEL_SSAO_NOISE_DIM,
                             .Height = WIESEL_SSAO_NOISE_DIM,
                             .Type = AttachmentTextureType::Offscreen,
                             .ImageFormat = VK_FORMAT_R32G32B32A32_SFLOAT,
                             .Sampled = true,
                             .TransferDest = true});
  SetAttachmentTextureBuffer(m_SSAONoise, noiseValues.data(),
                             sizeof(glm::vec4));
}

void Renderer::CreateImage(uint32_t width, uint32_t height, uint32_t mipLevels,
                           VkSampleCountFlagBits numSamples, VkFormat format,
                           VkImageTiling tiling, VkImageUsageFlags usage,
                           VkMemoryPropertyFlags properties, VkImage& image,
                           VkDeviceMemory& imageMemory,
                           VkImageCreateFlags flags, uint32_t arrayLayers) {
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

Ref<ImageView> Renderer::CreateImageView(VkImage image, VkFormat format,
                                         VkImageAspectFlags aspectFlags,
                                         uint32_t mipLevels,
                                         VkImageViewType viewType,
                                         uint32_t layer, uint32_t layerCount) {
  Ref<ImageView> view = CreateReference<ImageView>();
  view->m_Layer = layer;
  view->m_LayerCount = layerCount;

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
      vkCreateImageView(m_LogicalDevice, &viewInfo, nullptr, &view->m_Handle));

  return view;
}

Ref<ImageView> Renderer::CreateImageView(Ref<AttachmentTexture> image,
                                         VkImageViewType viewType,
                                         uint32_t layer, uint32_t layerCount) {
  return CreateImageView(image->m_Images[0], image->m_Format,
                         image->m_AspectFlags, image->m_MipLevels, viewType,
                         layer, layerCount);
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
  VkSampleCountFlags counts =
      m_PhysicalDeviceProperties.limits.framebufferColorSampleCounts &
      m_PhysicalDeviceProperties.limits.framebufferDepthSampleCounts;
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
                                          nullptr, &m_ImageAvailableSemaphore));
  WIESEL_CHECK_VKRESULT(vkCreateSemaphore(m_LogicalDevice, &semaphoreInfo,
                                          nullptr, &m_RenderFinishedSemaphore));
  WIESEL_CHECK_VKRESULT(
      vkCreateFence(m_LogicalDevice, &fenceInfo, nullptr, &m_Fence));
}

void Renderer::CleanupDescriptorLayouts() {
  m_GeometryMeshDescriptorLayout = nullptr;
  m_PresentDescriptorLayout = nullptr;
}

void Renderer::CleanupGeometryGraphics() {
  m_GeometryPipeline = nullptr;
  m_GeometryRenderPass = nullptr;
}

void Renderer::CleanupPresentGraphics() {
  m_PresentPipeline = nullptr;
  m_PresentColorImage = nullptr;
  m_PresentDepthStencil = nullptr;
  m_SwapChainTexture = nullptr;
  m_PresentRenderPass = nullptr;
  m_PresentFramebuffers.clear();
  m_PresentFramebuffers.clear();
  vkDestroySwapchainKHR(m_LogicalDevice, m_SwapChain, nullptr);
}

void Renderer::CreateGlobalUniformBuffers() {
  m_LightsUniformBuffer = CreateUniformBuffer(sizeof(LightsUniformData));
  m_CameraUniformBuffer = CreateUniformBuffer(sizeof(CameraUniformData));
  m_ShadowCameraUniformBuffer =
      CreateUniformBuffer(sizeof(ShadowMapMatricesUniformData));
}

void Renderer::CleanupGlobalUniformBuffers() {
  m_LightsUniformBuffer = nullptr;
  m_CameraUniformBuffer = nullptr;
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

  CleanupPresentGraphics();
  CleanupGeometryGraphics();
  CreateSwapChain();
  CreatePresentGraphicsPipelines();
  CreateGeometryRenderPass();
  CreateGeometryGraphicsPipelines();
}

void Renderer::SetViewport(VkExtent2D extent) {
  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = static_cast<float>(extent.width);
  viewport.height = static_cast<float>(extent.height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(m_CommandBuffer->m_Handle, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = extent;
  vkCmdSetScissor(m_CommandBuffer->m_Handle, 0, 1, &scissor);
}

void Renderer::SetViewport(glm::vec2 extent) {
  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = extent.x;
  viewport.height = extent.y;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(m_CommandBuffer->m_Handle, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent.width = extent.x;
  scissor.extent.height = extent.y;
  vkCmdSetScissor(m_CommandBuffer->m_Handle, 0, 1, &scissor);
}

void Renderer::BeginRender() {
  vkResetFences(m_LogicalDevice, 1, &m_Fence);
  m_CommandBuffer->Reset();
  m_CommandBuffer->Begin();
  if (m_PreviousMsaaSamples != m_MsaaSamples) {
    LOG_INFO("Msaa samples changed to {} from {}!",
             std::to_string(m_MsaaSamples),
             std::to_string(m_PreviousMsaaSamples));
    m_PreviousMsaaSamples = m_MsaaSamples;
  }

  // Reloading stuff
  if (m_RecreateSwapChain) {
    RecreateSwapChain();
    m_RecreateSwapChain = false;
    m_RecreatePipeline = false;
  }
  if (m_RecreatePipeline) {
    vkDeviceWaitIdle(m_LogicalDevice);
    LOG_INFO("Recreating graphics pipeline...");
    m_GeometryPipeline->m_Properties.m_EnableWireframe =
        m_EnableWireframe;  // Update wireframe mode
    RecreatePipeline(m_GeometryPipeline);
    m_RecreatePipeline = false;
  }
}

bool Renderer::BeginPresent() {
  VkResult result = vkAcquireNextImageKHR(m_LogicalDevice, m_SwapChain,
                                          UINT64_MAX, m_ImageAvailableSemaphore,
                                          VK_NULL_HANDLE, &m_ImageIndex);
  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    m_RecreateSwapChain = true;
    return false;
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    throw std::runtime_error("failed to acquire swap chain image!");
  }
  // Setup
  vkResetFences(m_LogicalDevice, 1, &m_Fence);

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

  TransitionImageLayout(m_Camera->CompositeColorResolveImage->m_Images[0],
                        m_Camera->CompositeColorResolveImage->m_Format,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                        m_CommandBuffer->m_Handle, 0, 1);

  m_PresentPipeline->Bind(PipelineBindPointGraphics);
  m_PresentRenderPass->Begin(m_PresentFramebuffers[m_ImageIndex], m_ClearColor);
  SetViewport(m_Extent);
  return true;
}

void Renderer::EndPresent() {
  m_PresentRenderPass->End();
  // This was done here to prevent some errors caused by doing it inside the pass
  // I'm not sure if this is a correct solution, find out and move this to the present image if not required
  TransitionImageLayout(m_Camera->CompositeColorResolveImage->m_Images[0],
                        m_Camera->CompositeColorResolveImage->m_Format,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1,
                        m_CommandBuffer->m_Handle, 0, 1);
  /*
  for (const auto& item : textures) {
    TransitionImageLayout(item->m_Images[0], item->m_Format,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1,
                          m_CommandBuffer->m_Handle);
  }*/

  m_CommandBuffer->End();

  // Presentation
  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &m_CommandBuffer->m_Handle;

  VkSemaphore waitSemaphores[] = {m_ImageAvailableSemaphore};
  VkPipelineStageFlags waitStages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = waitSemaphores;
  submitInfo.pWaitDstStageMask = waitStages;

  VkSemaphore signalSemaphores[] = {m_RenderFinishedSemaphore};
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = signalSemaphores;

  WIESEL_CHECK_VKRESULT(
      vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, m_Fence));

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

  vkWaitForFences(m_LogicalDevice, 1, &m_Fence, VK_TRUE, UINT64_MAX);
}

void Renderer::UpdateUniformData() {
  memcpy(m_LightsUniformBuffer->m_Data, &m_LightsUniformData,
         sizeof(m_LightsUniformData));
  memcpy(m_CameraUniformBuffer->m_Data, &m_CameraUniformData,
         sizeof(m_CameraUniformData));
}

void Renderer::BeginShadowPass(uint32_t cascade) {
  memcpy(m_ShadowCameraUniformBuffer->m_Data, &m_ShadowCameraUniformData,
         sizeof(m_ShadowCameraUniformData));
  m_ShadowPipelinePushConstant->CascadeIndex = cascade;

  m_ShadowPipeline->Bind(PipelineBindPointGraphics);
  m_ShadowRenderPass->Begin(m_Camera->ShadowFramebuffers[cascade],
                            {0, 0, 0, 1});
  SetViewport(glm::vec2{WIESEL_SHADOWMAP_DIM, WIESEL_SHADOWMAP_DIM});
}

void Renderer::EndShadowPass() {
  m_ShadowRenderPass->End();
}

void Renderer::BeginGeometryPass() {
  m_GeometryPipeline->Bind(PipelineBindPointGraphics);
  m_GeometryRenderPass->Begin(m_Camera->GeometryFramebuffer, {0, 0, 0, 0});
  SetViewport(m_ViewportSize);
}

void Renderer::EndGeometryPass() {
  m_GeometryRenderPass->End();
}

void Renderer::DrawModel(ModelComponent& model, const TransformComponent& transform, bool shadowPass) {
  for (int i = 0; i < model.Data.Meshes.size(); i++) {
    const auto& mesh = model.Data.Meshes[i];
    DrawMesh(mesh, transform, shadowPass);
  }
}

void Renderer::DrawMesh(Ref<Mesh> mesh, const TransformComponent& transform, bool shadowPass) {
  if (!mesh->IsAllocated) {
    return;
  }
  mesh->UpdateTransform(transform.TransformMatrix, transform.NormalMatrix);

  VkBuffer vertexBuffers[] = {mesh->VertexBuffer->m_Buffer};
  VkDeviceSize offsets[] = {0};
  static_assert(std::size(vertexBuffers) == std::size(offsets));
  vkCmdBindVertexBuffers(m_CommandBuffer->m_Handle, 0, std::size(vertexBuffers),
                         vertexBuffers, offsets);
  // Todo get the index type from index buffer instead of hardcoding it.
  vkCmdBindIndexBuffer(m_CommandBuffer->m_Handle, mesh->IndexBuffer->m_Buffer,
                       0, mesh->IndexBuffer->m_IndexType);

  VkPipelineLayout layout =
      shadowPass ? m_ShadowPipeline->m_Layout : m_GeometryPipeline->m_Layout;

  VkDescriptorSet sets[2] = {
      shadowPass ? mesh->ShadowDescriptors->m_DescriptorSet
                 : mesh->GeometryDescriptors->m_DescriptorSet,
      shadowPass ? m_Camera->ShadowDescriptor->m_DescriptorSet
                 : m_Camera->GlobalDescriptor->m_DescriptorSet};

  vkCmdBindDescriptorSets(m_CommandBuffer->m_Handle,
                          VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 2, sets,
                          0, nullptr);

  vkCmdDrawIndexed(m_CommandBuffer->m_Handle,
                   static_cast<uint32_t>(mesh->Indices.size()), 1, 0, 0, 0);
}

void Renderer::DrawSprite(SpriteComponent& sprite, const TransformComponent& transform) {
  if (!sprite.m_AssetHandle->m_IsAllocated) {
    return;
  }
  sprite.m_AssetHandle->UpdateTransform(transform.TransformMatrix);
  // TODO: In the feature, we can use instanced sprites for atlas sprites
  const SpriteAsset::Frame& frame =
      sprite.m_AssetHandle->m_Frames[sprite.m_CurrentFrame];

  Ref<MemoryBuffer> vertexBuffer = Engine::GetRenderer()->GetQuadVertexBuffer();
  VkBuffer buffers[] = {frame.VertexBuffer->m_Buffer};
  VkDeviceSize offsets[] = {0};
  static_assert(std::size(buffers) == std::size(offsets));
  vkCmdBindVertexBuffers(m_CommandBuffer->m_Handle, 0, std::size(buffers),
                         buffers, offsets);

  VkDescriptorSet sets[] = {frame.Descriptor->m_DescriptorSet,
                             m_Camera->GlobalDescriptor->m_DescriptorSet};

  vkCmdBindDescriptorSets(
      m_CommandBuffer->m_Handle, VK_PIPELINE_BIND_POINT_GRAPHICS,
      m_SpritePipeline->m_Layout, 0, std::size(sets), sets, 0, nullptr);

  vkCmdDraw(m_CommandBuffer->m_Handle, 6, 1, 0, 0);
}

void Renderer::BeginSSAOGenPass() {
  TransitionImageLayout(m_Camera->GeometryViewPosResolveImage->m_Images[0],
                        m_Camera->GeometryViewPosResolveImage->m_Format,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                        m_CommandBuffer->m_Handle, 0, 1);
  TransitionImageLayout(m_Camera->GeometryWorldPosResolveImage->m_Images[0],
                        m_Camera->GeometryWorldPosResolveImage->m_Format,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                        m_CommandBuffer->m_Handle, 0, 1);
  TransitionImageLayout(m_Camera->GeometryDepthResolveImage->m_Images[0],
                        m_Camera->GeometryDepthResolveImage->m_Format,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                        m_CommandBuffer->m_Handle, 0, 1);
  TransitionImageLayout(m_Camera->GeometryNormalResolveImage->m_Images[0],
                        m_Camera->GeometryNormalResolveImage->m_Format,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                        m_CommandBuffer->m_Handle, 0, 1);
  TransitionImageLayout(m_SSAONoise->m_Images[0], m_SSAONoise->m_Format,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                        m_CommandBuffer->m_Handle, 0, 1);
  m_SSAOGenRenderPass->Begin(m_Camera->SSAOGenFramebuffer, {0, 0, 0, 0});
  SetViewport(glm::vec2{m_ViewportSize.x / 2, m_ViewportSize.y / 2});
}

void Renderer::EndSSAOGenPass() {
  m_SSAOGenRenderPass->End();
  TransitionImageLayout(m_Camera->GeometryViewPosResolveImage->m_Images[0],
                        m_Camera->GeometryViewPosResolveImage->m_Format,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1,
                        m_CommandBuffer->m_Handle, 0, 1);
  TransitionImageLayout(m_Camera->GeometryWorldPosResolveImage->m_Images[0],
                        m_Camera->GeometryWorldPosResolveImage->m_Format,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1,
                        m_CommandBuffer->m_Handle, 0, 1);
  TransitionImageLayout(m_Camera->GeometryDepthResolveImage->m_Images[0],
                        m_Camera->GeometryDepthResolveImage->m_Format,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1,
                        m_CommandBuffer->m_Handle, 0, 1);
  TransitionImageLayout(m_Camera->GeometryNormalResolveImage->m_Images[0],
                        m_Camera->GeometryNormalResolveImage->m_Format,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1,
                        m_CommandBuffer->m_Handle, 0, 1);
  TransitionImageLayout(m_SSAONoise->m_Images[0], m_SSAONoise->m_Format,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1,
                        m_CommandBuffer->m_Handle, 0, 1);
}

void Renderer::BeginSSAOBlurHorzPass() {
  TransitionImageLayout(m_Camera->SSAOColorImage->m_Images[0],
                        m_Camera->SSAOColorImage->m_Format,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                        m_CommandBuffer->m_Handle, 0, 1);
  TransitionImageLayout(m_Camera->GeometryDepthResolveImage->m_Images[0],
                        m_Camera->GeometryDepthResolveImage->m_Format,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                        m_CommandBuffer->m_Handle, 0, 1);
  m_SSAOBlurHorzRenderPass->Begin(m_Camera->SSAOBlurHorzFramebuffer, {0, 0, 0, 0});
  SetViewport(m_ViewportSize);
}

void Renderer::EndSSAOBlurHorzPass() {
  m_SSAOBlurHorzRenderPass->End();
  TransitionImageLayout(m_Camera->SSAOColorImage->m_Images[0],
                        m_Camera->SSAOColorImage->m_Format,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1,
                        m_CommandBuffer->m_Handle, 0, 1);
  TransitionImageLayout(m_Camera->GeometryDepthResolveImage->m_Images[0],
                        m_Camera->GeometryDepthResolveImage->m_Format,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1,
                        m_CommandBuffer->m_Handle, 0, 1);
}

void Renderer::BeginSSAOBlurVertPass() {
  TransitionImageLayout(m_Camera->SSAOBlurHorzColorImage->m_Images[0],
                        m_Camera->SSAOBlurHorzColorImage->m_Format,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                        m_CommandBuffer->m_Handle, 0, 1);
  TransitionImageLayout(m_Camera->GeometryDepthResolveImage->m_Images[0],
                        m_Camera->GeometryDepthResolveImage->m_Format,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                        m_CommandBuffer->m_Handle, 0, 1);
  m_SSAOBlurVertRenderPass->Begin(m_Camera->SSAOBlurVertFramebuffer, {0, 0, 0, 0});
  SetViewport(m_ViewportSize);
}

void Renderer::EndSSAOBlurVertPass() {
  m_SSAOBlurVertRenderPass->End();
  TransitionImageLayout(m_Camera->SSAOBlurHorzColorImage->m_Images[0],
                        m_Camera->SSAOBlurHorzColorImage->m_Format,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1,
                        m_CommandBuffer->m_Handle, 0, 1);
  TransitionImageLayout(m_Camera->GeometryDepthResolveImage->m_Images[0],
                        m_Camera->GeometryDepthResolveImage->m_Format,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1,
                        m_CommandBuffer->m_Handle, 0, 1);
}

void Renderer::BeginLightingPass() {
  TransitionImageLayout(m_Camera->GeometryViewPosResolveImage->m_Images[0],
                        m_Camera->GeometryViewPosResolveImage->m_Format,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                        m_CommandBuffer->m_Handle, 0, 1);
  TransitionImageLayout(m_Camera->GeometryWorldPosResolveImage->m_Images[0],
                        m_Camera->GeometryWorldPosResolveImage->m_Format,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                        m_CommandBuffer->m_Handle, 0, 1);
  TransitionImageLayout(m_Camera->GeometryNormalResolveImage->m_Images[0],
                        m_Camera->GeometryNormalResolveImage->m_Format,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                        m_CommandBuffer->m_Handle, 0, 1);
  TransitionImageLayout(m_Camera->GeometryAlbedoResolveImage->m_Images[0],
                        m_Camera->GeometryAlbedoResolveImage->m_Format,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                        m_CommandBuffer->m_Handle, 0, 1);
  TransitionImageLayout(m_Camera->GeometryMaterialResolveImage->m_Images[0],
                        m_Camera->GeometryMaterialResolveImage->m_Format,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                        m_CommandBuffer->m_Handle, 0, 1);
  TransitionImageLayout(m_Camera->SSAOBlurVertColorImage->m_Images[0],
                        m_Camera->SSAOBlurVertColorImage->m_Format,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                        m_CommandBuffer->m_Handle, 0, 1);
  if (m_Camera->ShadowDepthStencil) {
    TransitionImageLayout(m_Camera->ShadowDepthStencil->m_Images[0],
                          m_Camera->ShadowDepthStencil->m_Format,
                          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                          m_CommandBuffer->m_Handle, 0,
                          WIESEL_SHADOW_CASCADE_COUNT);
  }
  m_LightingRenderPass->Begin(m_Camera->LightingFramebuffer, m_ClearColor);
}

void Renderer::EndLightingPass() {
  m_LightingRenderPass->End();
  TransitionImageLayout(m_Camera->GeometryViewPosResolveImage->m_Images[0],
                        m_Camera->GeometryViewPosResolveImage->m_Format,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1,
                        m_CommandBuffer->m_Handle, 0, 1);
  TransitionImageLayout(m_Camera->GeometryWorldPosResolveImage->m_Images[0],
                        m_Camera->GeometryWorldPosResolveImage->m_Format,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1,
                        m_CommandBuffer->m_Handle, 0, 1);
  TransitionImageLayout(m_Camera->GeometryNormalResolveImage->m_Images[0],
                        m_Camera->GeometryNormalResolveImage->m_Format,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1,
                        m_CommandBuffer->m_Handle, 0, 1);
  TransitionImageLayout(m_Camera->GeometryAlbedoResolveImage->m_Images[0],
                        m_Camera->GeometryAlbedoResolveImage->m_Format,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1,
                        m_CommandBuffer->m_Handle, 0, 1);
  TransitionImageLayout(m_Camera->GeometryMaterialResolveImage->m_Images[0],
                        m_Camera->GeometryMaterialResolveImage->m_Format,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1,
                        m_CommandBuffer->m_Handle, 0, 1);
  TransitionImageLayout(m_Camera->SSAOBlurVertColorImage->m_Images[0],
                        m_Camera->SSAOBlurVertColorImage->m_Format,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1,
                        m_CommandBuffer->m_Handle, 0, 1);
  if (m_Camera->ShadowDepthStencil) {
    TransitionImageLayout(m_Camera->ShadowDepthStencil->m_Images[0],
                          m_Camera->ShadowDepthStencil->m_Format,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1,
                          m_CommandBuffer->m_Handle, 0,
                          WIESEL_SHADOW_CASCADE_COUNT);
  }
}

void Renderer::BeginSpritePass() {
  m_SpriteRenderPass->Begin(m_Camera->SpriteFramebuffer, {0, 0, 0, 0});
}

void Renderer::EndSpritePass() {
  m_SpriteRenderPass->End();
}

void Renderer::BeginCompositePass() {
  TransitionImageLayout(m_Camera->LightingColorResolveImage->m_Images[0],
                        m_Camera->LightingColorResolveImage->m_Format,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                        m_CommandBuffer->m_Handle, 0, 1);
  TransitionImageLayout(m_Camera->SpriteColorImage->m_Images[0],
                        m_Camera->SpriteColorImage->m_Format,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                        m_CommandBuffer->m_Handle, 0, 1);
  m_CompositeRenderPass->Begin(m_Camera->CompositeFramebuffer, m_ClearColor);
}

void Renderer::EndCompositePass() {
  m_CompositeRenderPass->End();
  TransitionImageLayout(m_Camera->LightingColorResolveImage->m_Images[0],
                        m_Camera->LightingColorResolveImage->m_Format,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1,
                        m_CommandBuffer->m_Handle, 0, 1);
  TransitionImageLayout(m_Camera->SpriteColorImage->m_Images[0],
                        m_Camera->SpriteColorImage->m_Format,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1,
                        m_CommandBuffer->m_Handle, 0, 1);
}

void Renderer::DrawSkybox(Ref<Skybox> skybox) {
  std::array<VkDescriptorSet, 2> sets{
      skybox->m_Descriptors->m_DescriptorSet,
      m_Camera->GlobalDescriptor->m_DescriptorSet};

  vkCmdBindDescriptorSets(
      m_CommandBuffer->m_Handle, VK_PIPELINE_BIND_POINT_GRAPHICS,
      m_SkyboxPipeline->m_Layout, 0, 2, sets.data(), 0, nullptr);

  // draw cube via gl_VertexIndex (no vertex/index buffer needed)
  vkCmdDraw(m_CommandBuffer->m_Handle, 36, 1, 0, 0);
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
    sets.push_back(item->m_DescriptorSet);
  }
  vkCmdBindDescriptorSets(m_CommandBuffer->m_Handle,
                          VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->m_Layout,
                          0, sets.size(), sets.data(), 0, nullptr);

  // Draw the quad.
  vkCmdDraw(m_CommandBuffer->m_Handle, 3, 1, 0, 0);
}

void Renderer::SetCameraData(Ref<CameraData> cameraData) {
  m_Camera = cameraData;
  m_ViewportSize = cameraData->ViewportSize;
  m_CameraUniformData.Position = cameraData->Position;
  m_CameraUniformData.ViewMatrix = cameraData->ViewMatrix;
  m_CameraUniformData.Projection = cameraData->Projection;
  m_CameraUniformData.InvProjection = cameraData->InvProjection;
  m_CameraUniformData.NearPlane = cameraData->NearPlane;
  m_CameraUniformData.FarPlane = cameraData->FarPlane;
  m_ShadowCameraUniformData.EnableShadows = cameraData->DoesShadowPass;
  for (int i = 0; i < WIESEL_SHADOW_CASCADE_COUNT; ++i) {
    m_ShadowCameraUniformData.ViewProjectionMatrix[i] =
        cameraData->ShadowMapCascades[i].ViewProjMatrix;
    m_CameraUniformData.CascadeSplits[i] =
        cameraData->ShadowMapCascades[i].SplitDepth;
  }
  // Todo move this to another ubo for options maybe
  m_CameraUniformData.EnableSSAO = m_EnableSSAO;
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
  allocInfo.commandPool = m_CommandPool->m_Handle;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(m_LogicalDevice, &allocInfo, &commandBuffer);

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

  vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(m_GraphicsQueue);

  vkFreeCommandBuffers(m_LogicalDevice, m_CommandPool->m_Handle, 1,
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
    std::cout << std::flush;
  } else if (messageSeverity ==
             VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    LOG_WARN("{}", std::string(pCallbackData->pMessage));
    std::cout << std::flush;
  } else if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    LOG_ERROR("{}", std::string(pCallbackData->pMessage));
    std::cout << std::flush;
  } else {
    LOG_INFO("{}", std::string(pCallbackData->pMessage));
    std::cout << std::flush;
  }

  return VK_FALSE;
}

#endif

}  // namespace Wiesel