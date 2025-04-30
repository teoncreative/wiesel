
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "w_pch.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#include <stb_image.h>

#include "rendering/w_buffer.hpp"
#include "rendering/w_camera.hpp"
#include "rendering/w_command.hpp"
#include "rendering/w_descriptor.hpp"
#include "rendering/w_framebuffer.hpp"
#include "rendering/w_mesh.hpp"
#include "rendering/w_texture.hpp"
#include "rendering/w_sprite.hpp"
#include "scene/w_components.hpp"
#include "scene/w_lights.hpp"
#include "util/w_color.hpp"
#include "util/w_utils.hpp"
#include "w_pipeline.hpp"
#include "w_renderpass.hpp"
#include "w_sampler.hpp"
#include "w_shader.hpp"
#include "w_skybox.hpp"
#include "window/w_window.hpp"

namespace Wiesel {

struct ShadowPipelinePushConstant {
  int CascadeIndex;
};

struct RendererProperties {};

class Renderer {
 public:
  explicit Renderer(Ref<AppWindow> window);
  ~Renderer();

  void Initialize(const RendererProperties&& props);

  template<typename T>
  Ref<MemoryBuffer> CreateVertexBuffer(std::vector<T> vertices);

  void DestroyVertexBuffer(MemoryBuffer& buffer);

  Ref<IndexBuffer> CreateIndexBuffer(std::vector<Index> indices);
  void DestroyIndexBuffer(MemoryBuffer& buffer);

  Ref<UniformBuffer> CreateUniformBuffer(VkDeviceSize size);
  void DestroyUniformBuffer(UniformBuffer& buffer);

  void SetupCameraComponent(CameraComponent& component);

  Ref<Texture> CreateBlankTexture();
  Ref<Texture> CreateBlankTexture(const TextureProps& textureProps,
                                  const SamplerProps& samplerProps);
  Ref<Texture> CreateTexture(const std::string& path,
                             const TextureProps& textureProps,
                             const SamplerProps& samplerProps);
  Ref<Texture> CreateTexture(void* buffer,
                             size_t sizePerPixel,
                             const TextureProps& textureProps,
                             const SamplerProps& samplerProps);
  Ref<Texture> CreateCubemapTexture(const std::array<std::string, 6>& paths,
                                    const TextureProps& textureProps,
                                    const SamplerProps& samplerProps);
  void DestroyTexture(Texture& texture);
  VkSampler CreateTextureSampler(uint32_t mipLevels, const SamplerProps& props);

  Ref<AttachmentTexture> CreateAttachmentTexture(
      const AttachmentTextureProps& props);

  void SetAttachmentTextureBuffer(Ref<AttachmentTexture> texture, void* buffer,
                                  size_t sizePerPixel);

  void DestroyAttachmentTexture(AttachmentTexture& texture);

  Ref<DescriptorSet> CreateMeshDescriptors(Ref<UniformBuffer> uniformBuffer,
                                            Ref<Material> material);

  Ref<DescriptorSet> CreateShadowMeshDescriptors(
      Ref<UniformBuffer> uniformBuffer, Ref<Material> material);

  Ref<DescriptorSet> CreateGlobalDescriptors(CameraComponent& camera);
  Ref<DescriptorSet> CreateShadowGlobalDescriptors(CameraComponent& camera);

  Ref<DescriptorSet> CreateDescriptors(Ref<AttachmentTexture> texture);
  Ref<DescriptorSet> CreateSkyboxDescriptors(Ref<Texture> texture);

  void DestroyDescriptorLayout(DescriptorSetLayout& layout);

  void RecreatePipeline(Ref<Pipeline> pipeline);

  Ref<Shader> CreateShader(ShaderProperties properties);

  void SetClearColor(float r, float g, float b, float a = 1.0f);
  void SetClearColor(const Colorf& color);
  WIESEL_GETTER_FN Colorf& GetClearColor();

  void SetMsaaSamples(VkSampleCountFlagBits samples);
  WIESEL_GETTER_FN VkSampleCountFlagBits GetMsaaSamples();

  void SetVsync(bool vsync);
  WIESEL_GETTER_FN bool IsVsync();

  void SetWireframeEnabled(bool value);
  WIESEL_GETTER_FN bool IsWireframeEnabled();
  WIESEL_GETTER_FN bool* IsWireframeEnabledPtr();

  void SetSSAOEnabled(bool value);
  WIESEL_GETTER_FN bool IsSSAOEnabled();
  WIESEL_GETTER_FN bool* IsSSAOEnabledPtr();

  void SetRecreatePipeline(bool value);
  WIESEL_GETTER_FN bool IsRecreatePipeline();

  WIESEL_GETTER_FN VkDevice GetLogicalDevice();
  WIESEL_GETTER_FN float GetAspectRatio() const;
  WIESEL_GETTER_FN const WindowSize& GetWindowSize() const;

  WIESEL_GETTER_FN const VkExtent2D& GetExtent() const { return m_Extent; };

  WIESEL_GETTER_FN const uint32_t GetGraphicsQueueFamilyIndex() const {
    return m_QueueFamilyIndices.graphicsFamily.value();
  }

  WIESEL_GETTER_FN const uint32_t GetPresentQueueFamilyIndex() const {
    return m_QueueFamilyIndices.presentFamily.value();
  }

  WIESEL_GETTER_FN const CommandBuffer& GetCommandBuffer() const {
    return *m_CommandBuffer;
  }

  WIESEL_GETTER_FN const VkFormat GetSwapChainImageFormat() const {
    return m_SwapChainImageFormat;
  }

  WIESEL_GETTER_FN const Ref<CameraData> GetCameraData()
      const {
    return m_Camera;
  }

  WIESEL_GETTER_FN const VkPhysicalDeviceProperties GetPhysicalDeviceProperties() const {
    return m_PhysicalDeviceProperties;
  }

  WIESEL_GETTER_FN const VkPhysicalDeviceFeatures GetPhysicalDeviceFeatures() const {
    return m_PhysicalDeviceFeatures;
  }

  WIESEL_GETTER_FN const Ref<Pipeline> GetSkyboxPipeline() const {
    return m_SkyboxPipeline;
  }

  WIESEL_GETTER_FN const Ref<Pipeline> GetSSAOGenPipeline() const {
    return m_SSAOGenPipeline;
  }

  WIESEL_GETTER_FN const Ref<Pipeline> GetSSAOBlurPipeline() const {
    return m_SSAOBlurPipeline;
  }

  WIESEL_GETTER_FN const Ref<Pipeline> GetLightingPipeline() const {
    return m_LightingPipeline;
  }

  WIESEL_GETTER_FN const Ref<Pipeline> GetSpritePipeline() const {
    return m_SpritePipeline;
  }

  WIESEL_GETTER_FN const Ref<Pipeline> GetCompositePipeline() const {
    return m_CompositePipeline;
  }

  WIESEL_GETTER_FN const Ref<Sampler> GetDefaultLinearSampler() const {
    return m_DefaultLinearSampler;
  }

  WIESEL_GETTER_FN const Ref<Sampler> GetDefaultNearestSampler() const {
    return m_DefaultNearestSampler;
  }

  WIESEL_GETTER_FN const Ref<MemoryBuffer> GetQuadIndexBuffer() const {
    return m_QuadIndexBuffer;
  }
  WIESEL_GETTER_FN const Ref<MemoryBuffer> GetQuadVertexBuffer() const {
    return m_QuadVertexBuffer;
  }

  WIESEL_GETTER_FN const Ref<DescriptorSetLayout> GetSpriteDrawDescriptorLayout() const {
    return m_SpriteDrawDescriptorLayout;
  }

  void SetViewport(VkExtent2D extent);
  void SetViewport(glm::vec2 extent);

  void DrawModel(ModelComponent& model, TransformComponent& transform,
                 bool shadowPass);
  void DrawMesh(Ref<Mesh> mesh, TransformComponent& transform, bool shadowPass);
  void DrawSprite(SpriteComponent& sprite, TransformComponent& transform);
  void DrawSkybox(Ref<Skybox> skybox);
  void DrawFullscreen(Ref<Pipeline> pipeline, std::initializer_list<Ref<DescriptorSet>> descriptors);

  void BeginRender();
  void BeginFrame();
  void BeginShadowPass(uint32_t cascade);
  void EndShadowPass();
  void BeginGeometryPass();
  void EndGeometryPass();
  void BeginSSAOGenPass();
  void EndSSAOGenPass();
  void BeginSSAOBlurPass();
  void EndSSAOBlurPass();
  void BeginLightingPass();
  void EndLightingPass();
  void BeginSpritePass();
  void EndSpritePass();
  void BeginCompositePass();
  void EndCompositePass();
  void EndFrame();

  bool BeginPresent();
  void EndPresent();

  void SetCameraData(Ref<CameraData> camera);

  void RecreateSwapChain();
  void Cleanup();

  void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                    VkMemoryPropertyFlags properties, VkBuffer& buffer,
                    VkDeviceMemory& bufferMemory);

  void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
  void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width,
                         uint32_t height, VkDeviceSize baseOffset = 0,
                         uint32_t layer = 0);

  void TransitionImageLayout(VkImage image, VkFormat format,
                             VkImageLayout oldLayout, VkImageLayout newLayout,
                             uint32_t mipLevels, uint32_t baseLayer = 0,
                             uint32_t layerCount = 1);

  void TransitionImageLayout(VkImage image, VkFormat format,
                             VkImageLayout oldLayout, VkImageLayout newLayout,
                             uint32_t mipLevels, VkCommandBuffer commandBuffer,
                             uint32_t baseLayer, uint32_t layerCount);

  void CreateImage(uint32_t width, uint32_t height, uint32_t mipLevels,
                   VkSampleCountFlagBits numSamples, VkFormat format,
                   VkImageTiling tiling, VkImageUsageFlags usage,
                   VkMemoryPropertyFlags properties, VkImage& image,
                   VkDeviceMemory& imageMemory, VkImageCreateFlags flags = 0,
                   uint32_t arrayLayers = 1);

  Ref<ImageView> CreateImageView(
      VkImage image, VkFormat format, VkImageAspectFlags aspectFlags,
      uint32_t mipLevels, VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D,
      uint32_t layer = 0, uint32_t layerCount = 1);

  Ref<ImageView> CreateImageView(
      Ref<AttachmentTexture> image,
      VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D, uint32_t layer = 0,
      uint32_t layerCount = 1);

 private:
  void CreateVulkanInstance();
  void CreateSurface();
  void PickPhysicalDevice();
  void CreateLogicalDevice();
  void CreateDescriptorLayouts();
  void CreateSwapChain();
  void CreateGeometryRenderPass();
  void CreateGeometryGraphicsPipelines();
  void CreatePresentGraphicsPipelines();
  void CreateCommandPools();
  void CreateCommandBuffers();
  void CreatePermanentResources();
  void CreateSyncObjects();
  void CreateGlobalUniformBuffers();
  void CleanupGeometryGraphics();
  void CleanupPresentGraphics();
  void CleanupDescriptorLayouts();
  void CleanupGlobalUniformBuffers();
  int32_t RateDeviceSuitability(VkPhysicalDevice device);
  bool IsDeviceSuitable(VkPhysicalDevice device);
  VkSurfaceFormatKHR ChooseSwapSurfaceFormat(
      const std::vector<VkSurfaceFormatKHR>& availableFormats);
  VkPresentModeKHR ChooseSwapPresentMode(
      const std::vector<VkPresentModeKHR>& availablePresentModes);
  VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
  bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
  SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);
  VkCommandBuffer BeginSingleTimeCommands();
  void EndSingleTimeCommands(VkCommandBuffer commandBuffer);
  uint32_t FindMemoryType(uint32_t typeFilter,
                          VkMemoryPropertyFlags properties);

  std::vector<const char*> GetRequiredExtensions();
  QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);

  VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates,
                               VkImageTiling tiling,
                               VkFormatFeatureFlags features);
  VkFormat FindDepthFormat();
  bool HasStencilComponent(VkFormat format);
  void GenerateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth,
                       int32_t texHeight, uint32_t mipLevels);
  VkSampleCountFlagBits GetMaxUsableSampleCount();
#ifdef VULKAN_VALIDATION
  bool CheckValidationLayerSupport();
  void SetupDebugMessenger();
  VkResult CreateDebugUtilsMessengerEXT(
      VkInstance instance,
      const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
      const VkAllocationCallbacks* pAllocator,
      VkDebugUtilsMessengerEXT* pDebugMessenger);
  void PopulateDebugMessengerCreateInfo(
      VkDebugUtilsMessengerCreateInfoEXT& createInfo);
  void DestroyDebugUtilsMessengerEXT(VkInstance instance,
                                     VkDebugUtilsMessengerEXT debugMessenger,
                                     const VkAllocationCallbacks* pAllocator);
#endif

 private:
  friend class ImGuiLayer;
  friend class RenderPass;
  friend class Mesh;
  friend class Scene;

  static Ref<Renderer> s_Renderer;

#ifdef VULKAN_VALIDATION
  std::vector<const char*> validationLayers;
  VkDebugUtilsMessengerEXT m_DebugMessenger{};
#endif
  std::vector<const char*> m_DeviceExtensions;

  bool m_Initialized;
  Ref<AppWindow> m_Window;
  VkInstance m_Instance{};
  VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
  VkDevice m_LogicalDevice{};
  VkSurfaceKHR m_Surface{};
  VkQueue m_GraphicsQueue{};
  VkQueue m_PresentQueue{};
  VkSwapchainKHR m_SwapChain{};
  bool m_SwapChainCreated;

  uint32_t m_ImageIndex;
  VkFormat m_SwapChainImageFormat;
  Ref<AttachmentTexture> m_SwapChainTexture;

  VkExtent2D m_Extent{};

  Ref<CommandPool> m_CommandPool;
  Ref<CommandBuffer> m_CommandBuffer;

  VkSemaphore m_ImageAvailableSemaphore;
  VkSemaphore m_RenderFinishedSemaphore;
  VkFence m_Fence;

  float_t m_AspectRatio;
  WindowSize m_WindowSize;
  VkSampleCountFlagBits m_MsaaSamples;
  VkSampleCountFlagBits m_PreviousMsaaSamples;
  Colorf m_ClearColor;
  bool m_Vsync;
  Ref<UniformBuffer> m_LightsUniformBuffer;
  LightsUniformData m_LightsUniformData;
  Ref<UniformBuffer> m_CameraUniformBuffer;
  Ref<UniformBuffer> m_ShadowCameraUniformBuffer;
  Ref<UniformBuffer> m_SSAOKernelUniformBuffer;
  CameraUniformData m_CameraUniformData;
  ShadowMapMatricesUniformData m_ShadowCameraUniformData;
  SSAOKernelUniformData m_SSAOKernelUniformData;
  bool m_EnableWireframe;
  bool m_EnableSSAO;
  bool m_RecreatePipeline;
  bool m_RecreateSwapChain;

  Ref<CameraData> m_Camera;
  glm::vec2 m_ViewportSize;

  Ref<DescriptorSetLayout> m_GeometryMeshDescriptorLayout;
  Ref<DescriptorSetLayout> m_ShadowMeshDescriptorLayout;
  Ref<DescriptorSetLayout> m_GlobalDescriptorLayout;
  Ref<DescriptorSetLayout> m_GlobalShadowDescriptorLayout;
  Ref<DescriptorSetLayout> m_SSAOGenDescriptorLayout;
  Ref<DescriptorSetLayout> m_SSAOBlurDescriptorLayout;
  Ref<DescriptorSetLayout> m_SSAOOutputDescriptorLayout;
  Ref<DescriptorSetLayout> m_GeometryOutputDescriptorLayout;
  Ref<DescriptorSetLayout> m_SpriteDrawDescriptorLayout;

  Ref<RenderPass> m_GeometryRenderPass;
  Ref<Pipeline> m_GeometryPipeline;

  Ref<RenderPass> m_ShadowRenderPass;
  Ref<Pipeline> m_ShadowPipeline;
  Ref<ShadowPipelinePushConstant> m_ShadowPipelinePushConstant;

  Ref<RenderPass> m_LightingRenderPass;
  Ref<DescriptorSetLayout> m_SkyboxDescriptorLayout;
  Ref<Pipeline> m_SkyboxPipeline;
  Ref<Pipeline> m_LightingPipeline;

  Ref<RenderPass> m_SSAOGenRenderPass;
  Ref<Pipeline> m_SSAOGenPipeline;

  Ref<RenderPass> m_SSAOBlurRenderPass;
  Ref<Pipeline> m_SSAOBlurPipeline;

  Ref<RenderPass> m_SpriteRenderPass;
  Ref<Pipeline> m_SpritePipeline;

  Ref<RenderPass> m_CompositeRenderPass;
  Ref<Pipeline> m_CompositePipeline;

  Ref<RenderPass> m_PresentRenderPass;
  Ref<DescriptorSetLayout> m_PresentDescriptorLayout;
  Ref<Pipeline> m_PresentPipeline;
  Ref<AttachmentTexture> m_PresentColorImage;
  Ref<AttachmentTexture> m_PresentDepthStencil;
  std::vector<Ref<Framebuffer>> m_PresentFramebuffers;

  Ref<Sampler> m_DefaultLinearSampler;
  Ref<Sampler> m_DefaultNearestSampler;
  Ref<Texture> m_BlankTexture;
  Ref<MemoryBuffer> m_QuadVertexBuffer;
  Ref<IndexBuffer> m_QuadIndexBuffer;
  Ref<AttachmentTexture> m_SSAONoise;

  QueueFamilyIndices m_QueueFamilyIndices;
  SwapChainSupportDetails m_SwapChainDetails;
  VkPhysicalDeviceProperties m_PhysicalDeviceProperties;
  VkPhysicalDeviceFeatures m_PhysicalDeviceFeatures;
};

#ifdef VULKAN_VALIDATION
static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
#endif

}  // namespace Wiesel