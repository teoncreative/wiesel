
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
#include "scene/w_components.hpp"
#include "scene/w_lights.hpp"
#include "util/w_color.hpp"
#include "util/w_utils.hpp"
#include "w_pipeline.hpp"
#include "w_renderpass.hpp"
#include "w_shader.hpp"
#include "window/w_window.hpp"

#ifdef DEBUG
#define VULKAN_VALIDATION
#endif

namespace Wiesel {
const uint32_t k_MaxFramesInFlight = 2;

class Renderer {
 public:
  explicit Renderer(Ref<AppWindow> window);
  ~Renderer();

  Ref<MemoryBuffer> CreateVertexBuffer(std::vector<Vertex> vertices);
  void DestroyVertexBuffer(MemoryBuffer& buffer);

  Ref<MemoryBuffer> CreateIndexBuffer(std::vector<Index> indices);
  void DestroyIndexBuffer(MemoryBuffer& buffer);

  Ref<UniformBuffer> CreateUniformBuffer(VkDeviceSize size);
  void DestroyUniformBuffer(UniformBuffer& buffer);

  Ref<UniformBufferSet> CreateUniformBufferSet(uint32_t frames,
                                                     VkDeviceSize size);
  void DestroyUniformBufferSet(UniformBufferSet& bufferSet);

  Ref<Texture> CreateBlankTexture();
  Ref<Texture> CreateTexture(const std::string& path,
                                   TextureProps textureProps,
                                   SamplerProps samplerProps);
  void DestroyTexture(Texture& texture);
  VkSampler CreateTextureSampler(uint32_t mipLevels, SamplerProps samplerProps);

  Ref<DepthStencil> CreateDepthStencil();
  void DestroyDepthStencil(DepthStencil& texture);

  Ref<ColorImage> CreateColorImage();
  void DestroyColorImage(ColorImage& texture);

  Ref<DescriptorData> CreateDescriptors(Ref<UniformBufferSet> uniformBufferSet,
                                        Ref<Material> material)
      __attribute__((
          optnone));  // optimization for this function is disabled because compiler does something weird
  void DestroyDescriptors(DescriptorData& descriptorPool);

  // todo properties
  Ref<DescriptorLayout> CreateDescriptorLayout();
  void DestroyDescriptorLayout(DescriptorLayout& layout);

  Ref<GraphicsPipeline> CreateGraphicsPipeline(
      PipelineProperties properties);
  void AllocateGraphicsPipeline(PipelineProperties properties,
                                Ref<GraphicsPipeline> pipeline);
  void DestroyGraphicsPipeline(GraphicsPipeline& pipeline);
  void RecreateGraphicsPipeline(Ref<GraphicsPipeline> pipeline);
  void RecreateShader(Ref<Shader> shader);

  Ref<RenderPass> CreateRenderPass(RenderPassProperties properties);
  void AllocateRenderPass(Ref<RenderPass> renderPass);
  void DestroyRenderPass(RenderPass& renderPass);
  void RecreateRenderPass(Ref<RenderPass> renderPass);

  Ref<Shader> CreateShader(ShaderProperties properties);
  Ref<Shader> CreateShader(const std::vector<uint32_t>& code,
                                 ShaderProperties properties);
  void DestroyShader(Shader& shader);

  WIESEL_GETTER_FN Ref<CameraData> GetCameraData();

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
  WIESEL_GETTER_FN uint32_t GetCurrentFrame() const;
  WIESEL_GETTER_FN const WindowSize& GetWindowSize() const;
  WIESEL_GETTER_FN LightsUniformBufferObject& GetLightsBufferObject();

  bool BeginFrame(Ref<CameraData> data);
  void DrawModel(ModelComponent& model, TransformComponent& transform);
  void DrawMesh(Ref<Mesh> mesh, TransformComponent& transform);
  void EndFrame();

  void RecreateSwapChain();
  void Cleanup();

 private:
  void CreateVulkanInstance();
  void CreateSurface();
  void CreateImageViews();
  void PickPhysicalDevice();
  void CreateLogicalDevice();
  void CreateSwapChain();
  void CreateDefaultRenderPass();
  void CreateDefaultDescriptorSetLayout();
  void CreateDefaultGraphicsPipeline();
  void CreateDepthResources();
  void CreateColorResources();
  void CreateFramebuffers();
  void CreateCommandPools();
  void CreateCommandBuffers();
  void CreatePermanentResources();
  void CreateSyncObjects();
  void CreateGlobalUniformBuffers();
  void CleanupSwapChain();
  void CleanupDefaultRenderPass();
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

  static Ref<Renderer> s_Renderer;

#ifdef VULKAN_VALIDATION
  std::vector<const char*> validationLayers;
  VkDebugUtilsMessengerEXT m_DebugMessenger{};
#endif
  std::vector<const char*> m_DeviceExtensions;

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
  // make this Wiesel Texture
  std::vector<VkImage> m_SwapChainImages;
  std::vector<VkImageView> m_SwapChainImageViews;
  VkFormat m_SwapChainImageFormat;

  VkExtent2D m_SwapChainExtent{};
  Ref<DescriptorLayout> m_DefaultDescriptorLayout{};
  Ref<DepthStencil> m_DepthStencil;
  Ref<ColorImage> m_ColorImage;
  Ref<Texture> m_BlankTexture;

  std::vector<VkFramebuffer> m_SwapChainFramebuffers;
  VkCommandPool m_CommandPool{};

  std::vector<VkCommandBuffer> m_CommandBuffers;
  std::vector<VkSemaphore> m_ImageAvailableSemaphores;
  std::vector<VkSemaphore> renderFinishedSemaphores;
  std::vector<VkFence> m_InFlightFences;

  uint32_t m_CurrentFrame = 0;
  float_t m_AspectRatio;
  WindowSize m_WindowSize;
  VkSampleCountFlagBits m_MsaaSamples;
  VkSampleCountFlagBits m_PreviousMsaaSamples;
  Colorf m_ClearColor;
  bool m_Vsync;
  bool m_RecreateSwapChain;
  Ref<UniformBufferSet> m_LightsGlobalUboSet;
  LightsUniformBufferObject m_LightsGlobalUbo;
  bool m_EnableWireframe;
  bool m_RecreateGraphicsPipeline;
  bool m_RecreateShaders;
  Ref<GraphicsPipeline> m_DefaultGraphicsPipeline;
  Ref<GraphicsPipeline> m_CurrentGraphicsPipeline;
  Ref<RenderPass> m_DefaultRenderPass;
  Ref<CameraData> m_CameraData;
};

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

}  // namespace Wiesel