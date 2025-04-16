
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
#include "rendering/w_descriptor.hpp"
#include "rendering/w_mesh.hpp"
#include "rendering/w_texture.hpp"
#include "rendering/w_command.hpp"
#include "rendering/w_framebuffer.hpp"
#include "scene/w_components.hpp"
#include "scene/w_lights.hpp"
#include "util/w_color.hpp"
#include "util/w_utils.hpp"
#include "w_renderpass.hpp"
#include "w_renderpipeline.hpp"
#include "w_shader.hpp"
#include "window/w_window.hpp"

#define VULKAN_VALIDATION
#ifdef DEBUG
#endif

namespace Wiesel {

class Renderer {
 public:
  explicit Renderer(Ref<AppWindow> window);
  ~Renderer();

  void Initialize();

  Ref<MemoryBuffer> CreateVertexBuffer(std::vector<Vertex3D> vertices);
  Ref<MemoryBuffer> CreateVertexBuffer(std::vector<Vertex2D> vertices);
  void DestroyVertexBuffer(MemoryBuffer& buffer);

  Ref<MemoryBuffer> CreateIndexBuffer(std::vector<Index> indices);
  void DestroyIndexBuffer(MemoryBuffer& buffer);

  Ref<UniformBuffer> CreateUniformBuffer(VkDeviceSize size);
  void DestroyUniformBuffer(UniformBuffer& buffer);

  Ref<Texture> CreateBlankTexture();
  Ref<Texture> CreateBlankTexture(const TextureProps& textureProps,
                                  const SamplerProps& samplerProps);
  Ref<Texture> CreateTexture(const std::string& path,
                                   const TextureProps& textureProps,
                                   const SamplerProps& samplerProps);
  void DestroyTexture(Texture& texture);
  VkSampler CreateTextureSampler(uint32_t mipLevels, SamplerProps samplerProps);

  Ref<AttachmentTexture> CreateAttachmentTexture(const AttachmentTextureProps& props);

  void DestroyAttachmentTexture(AttachmentTexture& texture);

  // optimization for this function is disabled because compiler does something weird
  Ref<DescriptorData> CreateDescriptors(
      Ref<UniformBuffer> uniformBuffer,
      Ref<Material> material) __attribute__((optnone));

  Ref<DescriptorData> CreateDescriptors(Ref<AttachmentTexture> texture);

  void DestroyDescriptors(DescriptorData& descriptorPool);

  void DestroyDescriptorLayout(DescriptorLayout& layout);

  Ref<GraphicsPipeline> CreateGraphicsPipeline(
      PipelineProperties properties);
  void AllocateGraphicsPipeline(PipelineProperties properties,
                                Ref<GraphicsPipeline> pipeline);
  void DestroyGraphicsPipeline(GraphicsPipeline& pipeline);
  void RecreateGraphicsPipeline(Ref<GraphicsPipeline> pipeline);
  void RecreateShader(Ref<Shader> shader);

  Ref<Shader> CreateShader(ShaderProperties properties);
  Ref<Shader> CreateShader(const std::vector<uint32_t>& code,
                                 ShaderProperties properties);
  void DestroyShader(Shader& shader);

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

  void SetRecreateGraphicsPipeline(bool value);
  WIESEL_GETTER_FN bool IsRecreateGraphicsPipeline();

  void SetRecreateShaders(bool value);
  WIESEL_GETTER_FN bool IsRecreateShaders();

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
    return m_GeometryColorResolveImage;
  }

  void SetViewport(VkExtent2D extent);

  void DrawModel(ModelComponent& model, TransformComponent& transform);
  void DrawMesh(Ref<Mesh> mesh, TransformComponent& transform);
  void BlitImageToSwapChain(Ref<AttachmentTexture> texture);
  void DrawImageToSwapChain(Ref<AttachmentTexture> texture);
  void BeginRender();
  void BeginPresent();
  void EndPresent();

  bool BeginFrame();
  void EndFrame();

  void SetCameraData(Ref<CameraData> camera);

  void RecreateSwapChain();
  void Cleanup();

 private:
  void CreateVulkanInstance();
  void CreateSurface();
  void PickPhysicalDevice();
  void CreateLogicalDevice();
  void CreateSwapChain();
  void CreateRenderPass();
  void CreateDefaultGraphicsPipeline();
  void CreateCommandPools();
  void CreateCommandBuffers();
  void CreatePermanentResources();
  void CreateSyncObjects();
  void CreateGlobalUniformBuffers();
  void CleanupSwapChain();
  void CleanupRenderPass();
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
                         uint32_t height);

  void TransitionImageLayout(VkImage image, VkFormat format,
                             VkImageLayout oldLayout, VkImageLayout newLayout,
                             uint32_t mipLevels);
  void TransitionImageLayout(VkImage image, VkFormat format,
                             VkImageLayout oldLayout, VkImageLayout newLayout,
                             uint32_t mipLevels, VkCommandBuffer commandBuffer);
  void CreateImage(uint32_t width, uint32_t height, uint32_t mipLevels,
                   VkSampleCountFlagBits numSamples, VkFormat format,
                   VkImageTiling tiling, VkImageUsageFlags usage,
                   VkMemoryPropertyFlags properties, VkImage& image,
                   VkDeviceMemory& imageMemory);
  VkImageView CreateImageView(VkImage image, VkFormat format,
                              VkImageAspectFlags aspectFlags,
                              uint32_t mipLevels);
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
  Ref<DescriptorLayout> m_GeometryDescriptorLayout;
  Ref<DescriptorLayout> m_PresentDescriptorLayout;
  Ref<AttachmentTexture> m_GeometryDepthStencil;
  Ref<AttachmentTexture> m_GeometryColorImage;
  Ref<AttachmentTexture> m_GeometryColorResolveImage;
  Ref<AttachmentTexture> m_PresentColorImage;
  Ref<AttachmentTexture> m_PresentDepthStencil;
  Ref<Texture> m_BlankTexture;

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
  bool m_RecreateSwapChain;
  Ref<UniformBuffer> m_LightsUniformBuffer;
  LightsUniformData m_LightsUniformData;
  Ref<UniformBuffer> m_CameraUniformBuffer;
  CameraUniformData m_CameraUniformData;
  bool m_EnableWireframe;
  bool m_RecreateGraphicsPipeline;
  bool m_RecreateShaders;
  Ref<GraphicsPipeline> m_GeometryGraphicsPipeline;
  Ref<GraphicsPipeline> m_PresentGraphicsPipeline;
  Ref<RenderPass> m_GeometryRenderPass;
  Ref<RenderPass> m_PresentRenderPass;
  std::vector<Ref<Framebuffer>> m_PresentFramebuffers;
  Ref<Framebuffer> m_GeometryFramebuffer;
  Ref<MemoryBuffer> m_PresentVertexBuffer;
  Ref<MemoryBuffer> m_PresentIndexBuffer;
  Ref<CameraData> m_CameraData;

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