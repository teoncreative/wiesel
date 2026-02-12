
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
#include <vk_mem_alloc.h>

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
  int cascade_index;
};

struct BloomPushConstants {
  float threshold;
  float intensity;
};

struct MotionBlurPushConstants {
  float strength;
  int num_samples;
};

template<typename T>
class Setting {
public:
  Setting(T v) : value(v), change_hook(nullptr) {}

  void SetHook(bool* ptr) {
    change_hook = ptr;
  }

  Setting& operator=(const T& new_val) {
    SetValue(new_val);
    return *this;
  }

  operator T() const {
    return value;
  }

  struct Proxy {
    Setting* owner;
    T old_value;

    operator T*() {
      return &owner->value;
    }

    ~Proxy() {
      if (owner->change_hook && old_value != owner->value) {
        *owner->change_hook = true;
      }
    }
  };

  Proxy operator&() {
    return Proxy{this, value};
  }

private:
  void SetValue(const T& new_val) {
    if (value == new_val) {
      return;
    }

    value = new_val;

    if (change_hook)
      *change_hook = true;
  }

  T value;
  bool* change_hook;
};

struct RendererOptions {
  // Pass enable/disable - no pipeline recreation needed
  Setting<bool> ssao_enabled = true;
  Setting<bool> bloom_enabled = false;
  Setting<bool> motion_blur_enabled = false;
  Setting<bool> only_ssao = false;
  Setting<bool> debug_cascades = false;

  // Requires pipeline recreation
  Setting<bool> wireframe_enabled = false;
  Setting<VkSampleCountFlagBits> msaa_samples = VK_SAMPLE_COUNT_1_BIT;
  // Requires swap-chain recreation
  Setting<bool> vsync = false;

  // Effect parameters - push constants, updated every frame
  Setting<float> bloom_threshold = 0.7f;
  Setting<float> bloom_intensity = 0.6f;
  Setting<float> motion_blur_strength = 1.0f;
  Setting<int> motion_blur_samples = 8;

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
  Ref<Texture> CreateBlankTexture(const TextureProps& texture_props,
                                  const SamplerProps& sampler_props);
  Ref<Texture> CreateTexture(const std::string& path,
                             const TextureProps& texture_props,
                             const SamplerProps& sampler_props);
  Ref<Texture> CreateTexture(void* buffer,
                             size_t size_per_pixel,
                             const TextureProps& texture_props,
                             const SamplerProps& sampler_props);
  Ref<Texture> CreateCubemapTexture(const std::array<std::string, 6>& paths,
                                    const TextureProps& texture_props,
                                    const SamplerProps& sampler_props);
  Ref<Texture> CreateCubemapTextureFromSingle(const std::string& path,
                                    const TextureProps& texture_props,
                                    const SamplerProps& sampler_props);
  void DestroyTexture(Texture& texture);
  VkSampler CreateTextureSampler(uint32_t mip_levels, const SamplerProps& props);

  Ref<AttachmentTexture> CreateAttachmentTexture(
      const AttachmentTextureProps& props);

  void SetAttachmentTextureBuffer(Ref<AttachmentTexture> texture, void* buffer,
                                  size_t size_per_pixel);

  void DestroyAttachmentTexture(AttachmentTexture& texture);

  Ref<DescriptorSet> CreateMeshDescriptors(Ref<UniformBuffer> uniform_buffer,
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

  WIESEL_GETTER_FN RendererOptions& options() { return options_; }
  void SetRecreatePipeline(bool value) { recreate_pipeline_ = value; }
  WIESEL_GETTER_FN bool IsRecreatePipeline() const { return recreate_pipeline_; }

  Ref<DescriptorSet> GetFinalOutputDescriptor() const;
  Ref<AttachmentTexture> GetFinalOutputImage() const;

  WIESEL_GETTER_FN VkDevice GetLogicalDevice();
  WIESEL_GETTER_FN float GetAspectRatio() const { return aspect_ratio_; }
  WIESEL_GETTER_FN const WindowSize& GetWindowSize() const { return window_size_; }
  WIESEL_GETTER_FN const VkExtent2D& GetExtent() const { return extent_; }

  WIESEL_GETTER_FN const uint32_t GetGraphicsQueueFamilyIndex() const {
    return queue_family_indices_.graphicsFamily.value();
  }

  WIESEL_GETTER_FN const uint32_t GetPresentQueueFamilyIndex() const {
    return queue_family_indices_.presentFamily.value();
  }

  WIESEL_GETTER_FN const CommandBuffer& GetCommandBuffer() const {
    return *command_buffer_;
  }

  WIESEL_GETTER_FN const VkFormat GetSwapChainImageFormat() const {
    return swap_chain_image_format_;
  }

  WIESEL_GETTER_FN const Ref<CameraData> GetCameraData()
      const {
    return camera_;
  }

  WIESEL_GETTER_FN const VkPhysicalDeviceProperties GetPhysicalDeviceProperties() const {
    return physical_device_properties_;
  }

  WIESEL_GETTER_FN const VkPhysicalDeviceFeatures GetPhysicalDeviceFeatures() const {
    return physical_device_features_;
  }

  WIESEL_GETTER_FN const Ref<Pipeline> GetSkyboxPipeline() const {
    return skybox_pipeline_;
  }

  WIESEL_GETTER_FN const Ref<Pipeline> GetSSAOGenPipeline() const {
    return ssao_gen_pipeline_;
  }

  WIESEL_GETTER_FN const Ref<Pipeline> GetSSAOBlurHorzPipeline() const {
    return ssao_blur_horz_pipeline_;
  }

  WIESEL_GETTER_FN const Ref<Pipeline> GetSSAOBlurVertPipeline() const {
    return ssao_blur_vert_pipeline_;
  }

  WIESEL_GETTER_FN const Ref<Pipeline> GetLightingPipeline() const {
    return lighting_pipeline_;
  }

  WIESEL_GETTER_FN const Ref<Pipeline> GetSpritePipeline() const {
    return sprite_pipeline_;
  }

  WIESEL_GETTER_FN const Ref<Pipeline> GetCompositePipeline() const {
    return composite_pipeline_;
  }

  WIESEL_GETTER_FN const Ref<Pipeline> GetPresentPipeline() const {
    return present_pipeline_;
  }

  WIESEL_GETTER_FN const Ref<Pipeline> GetBloomExtractPipeline() const {
    return bloom_extract_pipeline_;
  }

  WIESEL_GETTER_FN const Ref<Pipeline> GetBloomBlurHPipeline() const {
    return bloom_blur_h_pipeline_;
  }

  WIESEL_GETTER_FN const Ref<Pipeline> GetBloomBlurVPipeline() const {
    return bloom_blur_v_pipeline_;
  }

  WIESEL_GETTER_FN const Ref<Pipeline> GetBloomCompositePipeline() const {
    return bloom_composite_pipeline_;
  }

  WIESEL_GETTER_FN const Ref<Pipeline> GetMotionBlurPipeline() const {
    return motion_blur_pipeline_;
  }

  WIESEL_GETTER_FN const Ref<Sampler> GetDefaultLinearSampler() const {
    return default_linear_sampler_;
  }

  WIESEL_GETTER_FN const Ref<Sampler> GetDefaultNearestSampler() const {
    return default_nearest_sampler_;
  }

  WIESEL_GETTER_FN const Ref<MemoryBuffer> GetQuadIndexBuffer() const {
    return quad_index_buffer_;
  }

  WIESEL_GETTER_FN const Ref<MemoryBuffer> GetQuadVertexBuffer() const {
    return quad_vertex_buffer_;
  }

  WIESEL_GETTER_FN const Ref<DescriptorSetLayout> GetSpriteDrawDescriptorLayout() const {
    return sprite_draw_descriptor_layout_;
  }

  void SetViewport(VkExtent2D extent);
  void SetViewport(glm::vec2 extent);

  void DrawModel(ModelComponent& model, const TransformComponent& transform,
                 bool shadowPass);
  void DrawMesh(Ref<Mesh> mesh, Ref<DescriptorSet> mesh_descriptors, bool shadowPass);
  void AllocateModelRenderData(ModelComponent& model, const Model& model_data);
  void DrawSprite(SpriteComponent& sprite, const TransformComponent& transform);
  void DrawSkybox(std::shared_ptr<Skybox> skybox);
  void DrawFullscreen(std::shared_ptr<Pipeline> pipeline, std::initializer_list<std::shared_ptr<DescriptorSet>> descriptors);

  void BeginRender();
  void UpdateUniformData();

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
                         uint32_t height, VkDeviceSize base_offset = 0,
                         uint32_t layer = 0,
                         uint32_t layer_count = 1);

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

  void SetObjectName(VkObjectType type, uint64_t handle, const char* name);

  VkSampleCountFlagBits GetMaxMsaaSamples() const {
    return max_msaa_samples_;
  }

  VkSampleCountFlags GetSupportedMsaaFlags() const {
    return possible_msaa_flags;
  }

 private:
  void CreateVulkanInstance();
  void LoadInstanceExtensions();
  void CreateSurface();
  void PickPhysicalDevice();
  void CreateLogicalDevice();
  void LoadDeviceExtensions();
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
      const std::vector<VkSurfaceFormatKHR>& available_formats);
  VkPresentModeKHR ChooseSwapPresentMode(
      const std::vector<VkPresentModeKHR>& available_present_modes);
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

  VkSampleCountFlagBits FindMaxUsableSampleCount(VkSampleCountFlags flags);

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

  void CreateTracy();
  TracyVkCtx GetTracyCtx() const {
    return tracy_ctx_;
  }

 private:
  friend class ImGuiLayer;
  friend class RenderPass;
  friend class RenderGraph;
  friend class Mesh;
  friend class Scene;
  friend class CommandBuffer;

  static Ref<Renderer> renderer_;

#ifdef VULKAN_VALIDATION
  std::vector<const char*> validation_layers_;
  VkDebugUtilsMessengerEXT debug_messenger_{};
#endif
  std::vector<const char*> device_extensions_;

  bool initialized_;
  Ref<AppWindow> window_;
  VkInstance instance_{};
  VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
  VkDevice logical_device_{};
  VkSurfaceKHR surface_{};
  VkQueue graphics_queue_{};
  VkQueue present_queue_{};
  VkSwapchainKHR swap_chain_{};
  bool swap_chain_created_;

  uint32_t image_index_;
  VkFormat swap_chain_image_format_;
  Ref<AttachmentTexture> swap_chain_texture_;

  VkExtent2D extent_{};

  Ref<CommandPool> command_pool_;
  Ref<CommandBuffer> command_buffer_;

  VkSemaphore image_available_semaphore_;
  VkSemaphore render_finished_semaphore_;
  VkFence fence_;

  float_t aspect_ratio_;
  WindowSize window_size_;
  Colorf clear_color_;
  Ref<UniformBuffer> lights_uniform_buffer_;
  LightsUniformData lights_uniform_data_;
  Ref<UniformBuffer> camera_uniform_buffer_;
  Ref<UniformBuffer> shadow_camera_uniform_buffer_;
  Ref<UniformBuffer> ssao_kernel_uniform_buffer_;
  CameraUniformData camera_uniform_data_;
  ShadowMapMatricesUniformData shadow_camera_uniform_data_;
  SSAOKernelUniformData ssao_kernel_uniform_data_;
  RendererOptions options_;
  bool recreate_pipeline_;
  bool recreate_swap_chain_;

  Ref<CameraData> camera_;
  glm::vec2 viewport_size_;

  Ref<DescriptorSetLayout> geometry_mesh_descriptor_layout_;
  Ref<DescriptorSetLayout> shadow_mesh_descriptor_layout_;
  Ref<DescriptorSetLayout> global_descriptor_layout_;
  Ref<DescriptorSetLayout> global_shadow_descriptor_layout_;
  Ref<DescriptorSetLayout> ssao_gen_descriptor_layout_;
  Ref<DescriptorSetLayout> ssao_blur_descriptor_layout_;
  Ref<DescriptorSetLayout> ssao_output_descriptor_layout_;
  Ref<DescriptorSetLayout> geometry_output_descriptor_layout_;
  Ref<DescriptorSetLayout> sprite_draw_descriptor_layout_;

#ifdef ID_BUFFER_PASS
  Ref<RenderPass> id_render_pass_;
  Ref<Pipeline> id_pipeline_;
#endif

  Ref<RenderPass> geometry_render_pass_;
  Ref<Pipeline> geometry_pipeline_;

  Ref<RenderPass> shadow_render_pass_;
  Ref<Pipeline> shadow_pipeline_;
  Ref<ShadowPipelinePushConstant> shadow_pipeline_push_constant_;

  Ref<RenderPass> lighting_render_pass_;
  Ref<DescriptorSetLayout> skybox_descriptor_layout_;
  Ref<Pipeline> skybox_pipeline_;
  Ref<Pipeline> lighting_pipeline_;

  Ref<RenderPass> ssao_gen_render_pass_;
  Ref<Pipeline> ssao_gen_pipeline_;

  Ref<RenderPass> ssao_blur_horz_render_pass_;
  Ref<Pipeline> ssao_blur_horz_pipeline_;
  Ref<RenderPass> ssao_blur_vert_render_pass_;
  Ref<Pipeline> ssao_blur_vert_pipeline_;

  Ref<RenderPass> sprite_render_pass_;
  Ref<Pipeline> sprite_pipeline_;

  Ref<RenderPass> composite_render_pass_;
  Ref<Pipeline> composite_pipeline_;

  Ref<RenderPass> postprocess_render_pass_;
  Ref<DescriptorSetLayout> postprocess_2input_descriptor_layout_;
  Ref<Pipeline> bloom_extract_pipeline_;
  Ref<Pipeline> bloom_blur_h_pipeline_;
  Ref<Pipeline> bloom_blur_v_pipeline_;
  Ref<Pipeline> bloom_composite_pipeline_;
  Ref<Pipeline> motion_blur_pipeline_;
  Ref<BloomPushConstants> bloom_push_constants_;
  Ref<MotionBlurPushConstants> motion_blur_push_constants_;

  Ref<RenderPass> present_render_pass_;
  Ref<DescriptorSetLayout> present_descriptor_layout_;
  Ref<Pipeline> present_pipeline_;
  Ref<AttachmentTexture> present_color_image_;
  Ref<AttachmentTexture> present_depth_stencil_;
  std::vector<Ref<Framebuffer>> present_framebuffers_;

  Ref<Sampler> default_linear_sampler_;
  Ref<Sampler> default_nearest_sampler_;
  Ref<Texture> blank_texture_;
  Ref<MemoryBuffer> quad_vertex_buffer_;
  Ref<IndexBuffer> quad_index_buffer_;
  Ref<AttachmentTexture> ssao_noise_;

  QueueFamilyIndices queue_family_indices_;
  SwapChainSupportDetails swap_chain_details_;
  VkPhysicalDeviceProperties physical_device_properties_;
  VkPhysicalDeviceFeatures physical_device_features_;
  std::vector<std::string> shader_features_;

  VkSampleCountFlagBits max_msaa_samples_;
  VkSampleCountFlags possible_msaa_flags;

  TracyVkCtx tracy_ctx_;
  PFN_vkSetDebugUtilsObjectNameEXT pfn_set_debug_utils_object_name_ext_ = nullptr;
  PFN_vkCreateDebugUtilsMessengerEXT pfn_create_debug_utils_messenger_ext_ = nullptr;
  PFN_vkDestroyDebugUtilsMessengerEXT pfn_destroy_debug_utils_messenger_ext_ = nullptr;
};

#ifdef VULKAN_VALIDATION
static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data);
#endif

}  // namespace Wiesel