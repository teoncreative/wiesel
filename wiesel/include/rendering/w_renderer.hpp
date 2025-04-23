
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
#include "scene/w_components.hpp"
#include "scene/w_lights.hpp"
#include "util/w_color.hpp"
#include "util/w_utils.hpp"
#include "w_pipeline.hpp"
#include "w_renderpass.hpp"
#include "w_shader.hpp"
#include "w_skybox.hpp"
#include "window/w_window.hpp"

namespace Wiesel {

struct ShadowPipelinePushConstant {
  int CascadeIndex;
};

struct RendererProperties {

};

class Renderer {
 public:
  explicit Renderer(Ref<AppWindow> window);
  ~Renderer();

  void Initialize(const RendererProperties&& props);

  Ref<MemoryBuffer> CreateVertexBuffer(std::vector<Vertex3D> vertices);
  Ref<MemoryBuffer> CreateVertexBuffer(std::vector<Vertex2DNoColor> vertices);
  void DestroyVertexBuffer(MemoryBuffer& buffer);

  Ref<MemoryBuffer> CreateIndexBuffer(std::vector<Index> indices);
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
  Ref<Texture> CreateCubemapTexture(const std::array<std::string, 6>& paths,
                             const TextureProps& textureProps,
                             const SamplerProps& samplerProps);
  void DestroyTexture(Texture& texture);
  VkSampler CreateTextureSampler(uint32_t mipLevels, SamplerProps samplerProps);
  VkSampler CreateDepthSampler();

  Ref<AttachmentTexture> CreateAttachmentTexture(const AttachmentTextureProps& props);

  void DestroyAttachmentTexture(AttachmentTexture& texture);


  Ref<DescriptorData> CreateMeshDescriptors(
      Ref<UniformBuffer> uniformBuffer,
      Ref<Material> material);

  Ref<DescriptorData> CreateShadowMeshDescriptors(
      Ref<UniformBuffer> uniformBuffer,
      Ref<Material> material);

  Ref<DescriptorData> CreateGlobalDescriptors(CameraComponent& camera);
  Ref<DescriptorData> CreateShadowGlobalDescriptors(CameraComponent& camera);

  Ref<DescriptorData> CreateDescriptors(Ref<AttachmentTexture> texture);
  Ref<DescriptorData> CreateSkyboxDescriptors(Ref<Texture> texture);

  void DestroyDescriptors(DescriptorData& descriptorPool);

  void DestroyDescriptorLayout(DescriptorLayout& layout);

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

  WIESEL_GETTER_FN const Ref<AttachmentTexture> GetGeometryColorResolveImage() const {
    return m_Camera->GeometryColorResolveImage;
  }
  WIESEL_GETTER_FN const Ref<AttachmentTexture> GetLightingColorResolveImage() const {
    return m_Camera->LightingColorResolveImage;
  }
  WIESEL_GETTER_FN const Ref<AttachmentTexture> GetCompositeColorResolveImage() const {
    return m_Camera->CompositeColorResolveImage;
  }
  WIESEL_GETTER_FN const Ref<Pipeline> GetSkyboxPipeline() const {
    return m_SkyboxPipeline;
  }
  WIESEL_GETTER_FN const Ref<Pipeline> GetLightingPipeline() const {
    return m_LightingPipeline;
  }
  WIESEL_GETTER_FN const Ref<Pipeline> GetCompositePipeline() const {
    return m_LightingPipeline;
  }


  void SetViewport(VkExtent2D extent);
  void SetViewport(glm::vec2 extent);

  void DrawModel(ModelComponent& model, TransformComponent& transform, bool shadowPass);
  void DrawMesh(Ref<Mesh> mesh, TransformComponent& transform, bool shadowPass);
  void DrawSkybox(Ref<Skybox> skybox);
  void DrawQuad(Ref<AttachmentTexture> texture,
                Ref<Pipeline> pipeline);

  void BeginRender();
  void BeginFrame();
  void BeginShadowPass(uint32_t cascade);
  void EndShadowPass();
  void BeginGeometryPass();
  void EndGeometryPass();
  void BeginLightingPass();
  void EndLightingPass();
  void BeginCompositePass();
  void EndCompositePass();
  void EndFrame();

  bool BeginPresent();
  void EndPresent();

  void SetCameraData(Ref<CameraData> camera);

  void RecreateSwapChain();
  void Cleanup();

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
  void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                    VkMemoryPropertyFlags properties, VkBuffer& buffer,
                    VkDeviceMemory& bufferMemory);

  void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
  void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width,
                         uint32_t height,
                         VkDeviceSize baseOffset = 0,
                         uint32_t layer = 0);

  void TransitionImageLayout(VkImage image, VkFormat format,
                             VkImageLayout oldLayout, VkImageLayout newLayout,
                             uint32_t mipLevels,
                             uint32_t baseLayer = 0,
                             uint32_t layerCount = 1);

  void TransitionImageLayout(VkImage image, VkFormat format,
                             VkImageLayout oldLayout, VkImageLayout newLayout,
                             uint32_t mipLevels, VkCommandBuffer commandBuffer,
                             uint32_t baseLayer,
                             uint32_t layerCount);

  void CreateImage(uint32_t width, uint32_t height, uint32_t mipLevels,
                   VkSampleCountFlagBits numSamples, VkFormat format,
                   VkImageTiling tiling, VkImageUsageFlags usage,
                   VkMemoryPropertyFlags properties, VkImage& image,
                   VkDeviceMemory& imageMemory,
                   VkImageCreateFlags flags = 0,
                   uint32_t arrayLayers = 1);

  Ref<ImageView> CreateImageView(VkImage image, VkFormat format,
                              VkImageAspectFlags aspectFlags,
                              uint32_t mipLevels,
                              VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D,
                              uint32_t layer = 0,
                              uint32_t layerCount = 1);

  Ref<ImageView> CreateImageView(Ref<AttachmentTexture> image,
                                 VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D,
                                 uint32_t layer = 0,
                                 uint32_t layerCount = 1);

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
  CameraUniformData m_CameraUniformData;
  ShadowCameraUniformData m_ShadowCameraUniformData;
  bool m_EnableWireframe;
  bool m_RecreatePipeline;
  bool m_RecreateSwapChain;

  Ref<CameraData> m_Camera;
  glm::vec2 m_ViewportSize;

  Ref<DescriptorLayout> m_GeometryMeshDescriptorLayout;
  Ref<DescriptorLayout> m_ShadowMeshDescriptorLayout;
  Ref<DescriptorLayout> m_GlobalDescriptorLayout;
  Ref<DescriptorLayout> m_GlobalShadowDescriptorLayout;

  Ref<RenderPass> m_GeometryRenderPass;
  Ref<Pipeline> m_GeometryPipeline;

  Ref<RenderPass> m_ShadowRenderPass;
  Ref<Pipeline> m_ShadowPipeline;
  Ref<ShadowPipelinePushConstant> m_ShadowPipelinePushConstant;

  Ref<RenderPass> m_LightingRenderPass;
  Ref<DescriptorLayout> m_SkyboxDescriptorLayout;
  Ref<Pipeline> m_SkyboxPipeline;
  Ref<Pipeline> m_LightingPipeline;

  Ref<RenderPass> m_CompositeRenderPass;
  Ref<Pipeline> m_CompositePipeline;

  Ref<RenderPass> m_PresentRenderPass;
  Ref<DescriptorLayout> m_PresentDescriptorLayout;
  Ref<Pipeline> m_PresentPipeline;
  Ref<AttachmentTexture> m_PresentColorImage;
  Ref<AttachmentTexture> m_PresentDepthStencil;
  std::vector<Ref<Framebuffer>> m_PresentFramebuffers;

  VkSampler m_DefaultLinearSampler;
  VkSampler m_DepthSampler;
  Ref<Texture> m_BlankTexture;
  Ref<MemoryBuffer> m_FullscreenQuadVertexBuffer;
  Ref<MemoryBuffer> m_FullscreenQuadIndexBuffer;

  QueueFamilyIndices m_QueueFamilyIndices;
  SwapChainSupportDetails m_SwapChainDetails;
};

#ifdef VULKAN_VALIDATION
static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
#endif

}  // namespace Wiesel