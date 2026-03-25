
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "w_pch.h"

#include <stb_image.h>

#include "rendering/w_buffer.h"
#include "rendering/w_camera.h"
#include "rendering/w_command.h"
#include "rendering/w_deletion_queue.h"
#include "rendering/w_descriptor.h"
#include "rendering/w_framebuffer.h"
#include "rendering/w_mesh.h"
#include "rendering/w_sprite.h"
#include "rendering/w_texture.h"
#include "scene/w_components.h"
#include "scene/w_lights.h"
#include "util/w_color.h"
#include "util/w_utils.h"
#include "w_pipeline.h"
#include "w_renderpass.h"
#include "w_sampler.h"
#include "w_shader.h"
#include "w_skybox.h"
#include "window/w_window.h"

namespace Wiesel {

class AccelerationStructureManager;

struct ShadowPipelinePushConstant {
  int cascade_index;
};

template <typename T>
class Setting {
 public:
  Setting(T v) : value(v), change_hook(nullptr) {}

  void SetHook(bool* ptr) { change_hook = ptr; }

  T Get() { return value; }

  Setting& operator=(const T& new_val) {
    SetValue(new_val);
    return *this;
  }

  operator T() const { return value; }

  struct Proxy {
    Setting* owner;
    T old_value;

    operator T*() { return &owner->value; }

    ~Proxy() {
      if (owner->change_hook && old_value != owner->value) {
        *owner->change_hook = true;
      }
    }
  };

  Proxy operator&() { return Proxy{this, value}; }

 private:
  void SetValue(const T& new_val) {
    if (value == new_val) {
      return;
    }

    value = new_val;

    if (change_hook) {
      *change_hook = true;
    }
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
  Setting<int> debug_cascades = 0;  // 0=off, 1=cascades, 2=material
  Setting<bool> show_colliders = false;
  Setting<bool> show_triggers = false;
  Setting<bool> show_reverb_zones = false;
  Setting<bool> show_cameras = true;
  Setting<bool> shadows_enabled = true;
  Setting<bool> rt_shadows_enabled = true;

  // Requires pipeline recreation
  Setting<bool> wireframe_enabled = false;
  Setting<SamplingMode> msaa_mode = SamplingMode::DISABLED;
  // Requires swap-chain recreation
  Setting<bool> vsync = false;

  // Anti-aliasing mode (mutually exclusive: None, FXAA, TAA)
  Setting<AntiAliasingMode> aa_mode = AntiAliasingMode::None;

  // Effect parameters - push constants, updated every frame
  Setting<float> bloom_threshold = 0.7f;
  Setting<float> bloom_intensity = 0.6f;
  Setting<float> motion_blur_strength = 1.0f;
  Setting<int> motion_blur_samples = 8;

  // Scene ambient
  glm::vec3 ambient_color = {1.0f, 1.0f, 1.0f};
  float ambient_intensity = 0.3f;

  // IBL
  Setting<bool> ibl_enabled = true;
};

struct RendererProperties {};

struct RenderStats {
  uint32_t draw_calls = 0;
  uint32_t vertices = 0;
  uint32_t triangles = 0;
  uint32_t meshes = 0;
  uint32_t models = 0;
  float frame_time_ms = 0.0f;
  uint32_t swap_chain_images = 0;
  uint32_t frames_in_flight = 0;

  void Reset() {
    draw_calls = 0;
    vertices = 0;
    triangles = 0;
    meshes = 0;
    models = 0;
  }
};

class Renderer {
 public:
  explicit Renderer(std::shared_ptr<AppWindow> window);
  ~Renderer();

  void Initialize(const RendererProperties&& props);

  template <typename T>
  std::shared_ptr<MemoryBuffer> CreateVertexBuffer(std::vector<T> vertices);

  std::shared_ptr<IndexBuffer> CreateIndexBuffer(std::vector<Index> indices);

  std::shared_ptr<UniformBuffer> CreateUniformBuffer(VkDeviceSize size);
  std::shared_ptr<UniformBuffer> CreateStorageBuffer(VkDeviceSize size);

  void SetupCameraComponent(CameraComponent& component);

  std::shared_ptr<Texture> CreateBlankTexture();
  std::shared_ptr<Texture> CreateBlankTexture(
      const TextureProps& texture_props, const SamplerProps& sampler_props);
  std::shared_ptr<Texture> CreateTexture(const std::string& path,
                                         const TextureProps& texture_props,
                                         const SamplerProps& sampler_props);
  std::shared_ptr<Texture> CreateTexture(void* buffer, size_t size_per_pixel,
                                         const TextureProps& texture_props,
                                         const SamplerProps& sampler_props);
  std::shared_ptr<Texture> CreateCubemapTexture(
      const std::array<std::string, 6>& paths,
      const TextureProps& texture_props, const SamplerProps& sampler_props);
  std::shared_ptr<Texture> CreateCubemapTextureFromSingle(
      const std::string& virtual_path, const TextureProps& texture_props,
      const SamplerProps& sampler_props);
  VkSampler CreateTextureSampler(uint32_t mip_levels,
                                 const SamplerProps& props);

  std::shared_ptr<AttachmentTexture> CreateAttachmentTexture(
      const AttachmentTextureProps& props);

  void SetAttachmentTextureBuffer(std::shared_ptr<AttachmentTexture> texture,
                                  void* buffer, size_t size_per_pixel);

  std::shared_ptr<DescriptorSet> CreateMeshDescriptors(
      std::shared_ptr<UniformBuffer> uniform_buffer,
      std::shared_ptr<Material> material);

  std::shared_ptr<DescriptorSet> CreateShadowMeshDescriptors(
      std::shared_ptr<UniformBuffer> uniform_buffer,
      std::shared_ptr<Material> material);

  std::shared_ptr<DescriptorSet> CreateGlobalDescriptors(
      CameraComponent& camera);
  std::shared_ptr<DescriptorSet> CreateShadowGlobalDescriptors(
      CameraComponent& camera);
  std::shared_ptr<DescriptorSet> CreateBoneDescriptors(
      std::shared_ptr<UniformBuffer> bone_ubo);

  std::shared_ptr<DescriptorSet> CreateDescriptors(
      std::shared_ptr<AttachmentTexture> texture);
  std::shared_ptr<DescriptorSet> CreateSkyboxDescriptors(
      std::shared_ptr<Texture> texture);

  void DestroyDescriptorLayout(DescriptorSetLayout& layout);

  void RecreatePipeline(std::shared_ptr<Pipeline> pipeline);

  std::shared_ptr<Shader> CreateShader(ShaderProperties properties);

  void SetClearColor(float r, float g, float b, float a = 1.0f);
  void SetClearColor(const Colorf& color);
  WIESEL_GETTER_FN Colorf& GetClearColor();

  WIESEL_GETTER_FN RendererOptions& options() { return options_; }

  void SetRecreatePipeline(bool value) { recreate_pipeline_ = value; }

  WIESEL_GETTER_FN bool IsRecreatePipeline() const {
    return recreate_pipeline_;
  }

  WIESEL_GETTER_FN bool NeedsRecreateResources() const {
    return recreate_resources_;
  }

  void SetRecreateResources(bool value) { recreate_resources_ = value; }

  void ClearRecreateResources() { recreate_resources_ = false; }

  std::shared_ptr<DescriptorSet> GetFinalOutputDescriptor() const;
  std::shared_ptr<AttachmentTexture> GetFinalOutputImage() const;

  WIESEL_GETTER_FN VkDevice GetLogicalDevice();

  WIESEL_GETTER_FN float GetAspectRatio() const { return aspect_ratio_; }

  WIESEL_GETTER_FN const WindowSize& GetWindowSize() const {
    return window_size_;
  }

  WIESEL_GETTER_FN const VkExtent2D& GetExtent() const { return extent_; }

  WIESEL_GETTER_FN const uint32_t GetGraphicsQueueFamilyIndex() const {
    return queue_family_indices_.graphics_family.value();
  }

  WIESEL_GETTER_FN const uint32_t GetPresentQueueFamilyIndex() const {
    return queue_family_indices_.present_family.value();
  }

  WIESEL_GETTER_FN const CommandBuffer& GetCommandBuffer() const {
    return *command_buffers_[current_frame_];
  }

  WIESEL_GETTER_FN const VkFormat GetSwapChainImageFormat() const {
    return swap_chain_image_format_;
  }

  WIESEL_GETTER_FN const std::shared_ptr<CameraData> GetCameraData() const {
    return camera_;
  }

  WIESEL_GETTER_FN const VkPhysicalDeviceProperties
  GetPhysicalDeviceProperties() const {
    return physical_device_properties_;
  }

  WIESEL_GETTER_FN const VkPhysicalDeviceFeatures
  GetPhysicalDeviceFeatures() const {
    return physical_device_features_;
  }

  WIESEL_GETTER_FN bool IsRayTracingSupported() const { return rt_supported_; }

  WIESEL_GETTER_FN const VkPhysicalDeviceRayTracingPipelinePropertiesKHR&
  GetRTProperties() const {
    return rt_pipeline_properties_;
  }

  WIESEL_GETTER_FN const VkPhysicalDeviceAccelerationStructurePropertiesKHR&
  GetASProperties() const {
    return rt_as_properties_;
  }

  // RT function pointer accessors
  PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR()
      const {
    return pfn_vkCreateAccelerationStructureKHR_;
  }

  PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR()
      const {
    return pfn_vkDestroyAccelerationStructureKHR_;
  }

  PFN_vkGetAccelerationStructureBuildSizesKHR
  vkGetAccelerationStructureBuildSizesKHR() const {
    return pfn_vkGetAccelerationStructureBuildSizesKHR_;
  }

  PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR()
      const {
    return pfn_vkCmdBuildAccelerationStructuresKHR_;
  }

  PFN_vkGetAccelerationStructureDeviceAddressKHR
  vkGetAccelerationStructureDeviceAddressKHR() const {
    return pfn_vkGetAccelerationStructureDeviceAddressKHR_;
  }

  PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR() const {
    return pfn_vkCreateRayTracingPipelinesKHR_;
  }

  PFN_vkGetRayTracingShaderGroupHandlesKHR
  vkGetRayTracingShaderGroupHandlesKHR() const {
    return pfn_vkGetRayTracingShaderGroupHandlesKHR_;
  }

  PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR() const {
    return pfn_vkCmdTraceRaysKHR_;
  }

  WIESEL_GETTER_FN std::shared_ptr<AccelerationStructureManager> GetASManager()
      const {
    return as_manager_;
  }

  WIESEL_GETTER_FN const std::shared_ptr<Pipeline> GetPresentPipeline() const {
    return present_pipeline_;
  }

  WIESEL_GETTER_FN const std::shared_ptr<Sampler> GetDefaultLinearSampler()
      const {
    return default_linear_sampler_;
  }

  WIESEL_GETTER_FN const std::shared_ptr<Sampler> GetDefaultNearestSampler()
      const {
    return default_nearest_sampler_;
  }

  WIESEL_GETTER_FN const std::shared_ptr<MemoryBuffer> GetQuadIndexBuffer()
      const {
    return quad_index_buffer_;
  }

  WIESEL_GETTER_FN const std::shared_ptr<MemoryBuffer> GetQuadVertexBuffer()
      const {
    return quad_vertex_buffer_;
  }

  // Descriptor layout registry (used by RenderFeatures)

  std::shared_ptr<DescriptorSetLayout> GetDescriptorLayout(
      const std::string& name) const;
  void RegisterDescriptorLayout(const std::string& name,
                                std::shared_ptr<DescriptorSetLayout> layout);

  WIESEL_GETTER_FN std::shared_ptr<DescriptorSet> GetIdentityBoneDescriptor()
      const {
    return identity_bone_descriptor_;
  }

  // Shared resource getters (used by RenderFeatures)

  WIESEL_GETTER_FN std::shared_ptr<AttachmentTexture> GetSSAONoise() const {
    return ssao_noise_;
  }

  WIESEL_GETTER_FN std::shared_ptr<UniformBuffer> GetSSAOKernelUniformBuffer()
      const {
    return ssao_kernel_uniform_buffer_;
  }

  WIESEL_GETTER_FN std::shared_ptr<UniformBuffer> GetCameraUniformBuffer()
      const {
    return camera_uniform_buffer_;
  }

  WIESEL_GETTER_FN std::shared_ptr<UniformBuffer> GetLightsUniformBuffer()
      const {
    return lights_uniform_buffer_;
  }

  WIESEL_GETTER_FN std::shared_ptr<UniformBuffer> GetShadowCameraUniformBuffer()
      const {
    return shadow_camera_uniform_buffer_;
  }

  ShadowMapMatricesUniformData& GetShadowCameraUniformData() {
    return shadow_camera_uniform_data_;
  }

  VkFormat FindDepthFormat();

  void SetViewport(VkExtent2D extent, VkCommandBuffer cmd = VK_NULL_HANDLE);
  void SetViewport(glm::vec2 extent, VkCommandBuffer cmd = VK_NULL_HANDLE);

  void DrawModel(ModelComponent& model, const TransformComponent& transform,
                 bool shadow_pass, entt::entity entity_handle = entt::null);
  void DrawModelTransparent(ModelComponent& model,
                            const TransformComponent& transform,
                            entt::entity entity_handle = entt::null);
  void DrawMeshCmd(VkCommandBuffer cmd, std::shared_ptr<Mesh> mesh,
                   std::shared_ptr<DescriptorSet> mesh_descriptors,
                   std::shared_ptr<DescriptorSet> bone_descriptors,
                   std::shared_ptr<DescriptorSet> global_descriptors);
  void AllocateModelRenderData(ModelComponent& model, const Model& model_data);
  void DrawSprite(SpriteComponent& sprite, const TransformComponent& transform);
  void DrawCanvasRect(const RectangleTransformComponent& rt,
                      CanvasRectComponent& rect,
                      std::shared_ptr<DescriptorSetLayout> layout,
                      float entity_id = 0);
  void DrawTexturedRect(glm::vec2 position, glm::vec2 size,
                        std::shared_ptr<Texture> texture, glm::vec4 tint,
                        glm::vec4 uv_rect,
                        std::shared_ptr<DescriptorSetLayout> layout,
                        float entity_id = 0);
  void DrawCanvasText(const RectangleTransformComponent& rt,
                      TextComponent& text,
                      std::shared_ptr<DescriptorSetLayout> layout,
                      float entity_id = 0);
  void DrawSkybox(std::shared_ptr<Skybox> skybox);
  void DrawFullscreen(
      std::shared_ptr<Pipeline> pipeline,
      std::initializer_list<std::shared_ptr<DescriptorSet>> descriptors,
      VkCommandBuffer cmd = VK_NULL_HANDLE);
  void RequestEntityPick(
      uint32_t x, uint32_t y,
      std::shared_ptr<AttachmentTexture> entity_id_texture,
      std::shared_ptr<AttachmentTexture> fallback_entity_id_texture = nullptr);
  bool ExecuteEntityPick(entt::entity& out_entity);

  void SetBoundPipeline(Pipeline* p) { bound_pipeline_ = p; }

  Pipeline* GetBoundPipeline() const { return bound_pipeline_; }

  const RenderStats& GetStats() const { return stats_; }

  DeletionQueue& GetDeletionQueue() { return deletion_queue_; }

  void BeginRender();
  void UpdateUniformData();

  bool BeginPresent();
  void EndPresent();

  void SetCameraData(std::shared_ptr<CameraData> camera);

  void RecreateSwapChain();

  // Wait for all GPU work to finish and flush the deletion queue.
  // Call before destroying resources that may still be in flight.
  void WaitForGPU();

  void Cleanup();

  void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                    VkMemoryPropertyFlags properties, VkBuffer& buffer,
                    VkDeviceMemory& buffer_memory);

  void CopyBuffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size);
  void CopyBuffer(VkCommandBuffer cmd, VkBuffer src_buffer, VkBuffer dst_buffer,
                  VkDeviceSize size);
  void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width,
                         uint32_t height, VkDeviceSize base_offset = 0,
                         uint32_t layer = 0, uint32_t layer_count = 1);
  void CopyBufferToImage(VkCommandBuffer cmd, VkBuffer buffer, VkImage image,
                         uint32_t width, uint32_t height,
                         VkDeviceSize base_offset = 0, uint32_t layer = 0,
                         uint32_t layer_count = 1);

  void TransitionImageLayout(VkImage image, VkFormat format,
                             VkImageLayout oldLayout, VkImageLayout newLayout,
                             uint32_t mipLevels, uint32_t baseLayer = 0,
                             uint32_t layerCount = 1);

  void TransitionImageLayout(VkImage image, VkFormat format,
                             VkImageLayout oldLayout, VkImageLayout newLayout,
                             uint32_t mipLevels, VkCommandBuffer command_buffer,
                             uint32_t baseLayer, uint32_t layerCount);

  void CreateImage(uint32_t width, uint32_t height, uint32_t mipLevels,
                   SamplingMode msaa_mode, VkFormat format,
                   VkImageTiling tiling, VkImageUsageFlags usage,
                   VkMemoryPropertyFlags properties, VkImage& image,
                   VkDeviceMemory& imageMemory, VkImageCreateFlags flags = 0,
                   uint32_t arrayLayers = 1);

  std::shared_ptr<ImageView> CreateImageView(
      VkImage image, VkFormat format, VkImageAspectFlags aspectFlags,
      uint32_t mipLevels, VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D,
      uint32_t layer = 0, uint32_t layerCount = 1);

  // Create an image view for a specific mip level (used for per-mip rendering)
  std::shared_ptr<ImageView> CreateImageViewMip(
      VkImage image, VkFormat format, VkImageAspectFlags aspectFlags,
      uint32_t baseMipLevel, uint32_t levelCount = 1,
      VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D, uint32_t layer = 0,
      uint32_t layerCount = 1);

  std::shared_ptr<ImageView> CreateImageView(
      std::shared_ptr<AttachmentTexture> image,
      VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D, uint32_t layer = 0,
      uint32_t layer_count = 1);

  void SetObjectName(VkObjectType type, uint64_t handle, const char* name);

  SamplingMode GetHighestSamplingMode() const { return max_sampling_mode_; }

  const std::vector<SamplingMode>& GetSupportedSamplingModes() const {
    return supported_sampling_modes_;
  }

  // Create a transient command pool for background thread uploads.
  // Set it as active with SetThreadCommandPool() before doing GPU uploads
  // from a non-main thread, then clear it when done.
  VkCommandPool CreateTransientCommandPool();
  static void SetThreadCommandPool(VkCommandPool pool);

  // Batch upload mode: all single-time commands are recorded into one command
  // buffer and submitted together when EndBatchUpload is called.
  // This avoids per-texture/per-mesh GPU sync during model loading.
  // Staging buffers are deferred until the batch is flushed via DeferStagingCleanup.
  void BeginBatchUpload();
  void EndBatchUpload();
  void DeferStagingCleanup(VkBuffer buffer, VkDeviceMemory memory);

  VkCommandBuffer BeginSingleTimeCommands();
  VkCommandBuffer BeginSingleTimeCommands(VkCommandPool pool);

  // Resolve command buffer: returns cmd if valid, otherwise current frame's buffer
  VkCommandBuffer ResolveCmd(VkCommandBuffer cmd = VK_NULL_HANDLE) {
    if (cmd != VK_NULL_HANDLE) {
      return cmd;
    }
    return command_buffers_[current_frame_]->handle_;
  }

  void EndSingleTimeCommands(VkCommandBuffer commandBuffer);
  void EndSingleTimeCommands(VkCommandBuffer commandBuffer, VkCommandPool pool);

 private:
  void CreateVulkanInstance();
  void LoadInstanceExtensions();
  void CreateSurface();
  void PickPhysicalDevice();
  void CreateLogicalDevice();
  void LoadDeviceExtensions();
  void CreateDescriptorLayouts();
  void CreateSwapChain();
  void CreatePresentGraphicsPipelines();
  void CreateCommandPools();
  void CreateCommandBuffers();
  void CreatePermanentResources();
  void CreateSyncObjects();
  void CreateGlobalUniformBuffers();
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

  struct StagingResource {
    VkBuffer buffer;
    VkDeviceMemory memory;
  };

  static thread_local VkCommandPool tl_command_pool_;
  static thread_local VkCommandBuffer tl_batch_cmd_;
  static thread_local bool tl_batch_active_;
  static thread_local std::vector<StagingResource> tl_deferred_staging_;
  uint32_t FindMemoryType(uint32_t typeFilter,
                          VkMemoryPropertyFlags properties);

  std::vector<const char*> GetRequiredExtensions();
  QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);

  VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates,
                               VkImageTiling tiling,
                               VkFormatFeatureFlags features);
  bool HasStencilComponent(VkFormat format);
  void GenerateMipmaps(VkImage image, VkFormat image_format, int32_t tex_width,
                       int32_t tex_height, uint32_t mip_levels);
  void GenerateMipmaps(VkCommandBuffer cmd, VkImage image,
                       VkFormat image_format, int32_t tex_width,
                       int32_t tex_height, uint32_t mip_levels);

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

  TracyVkCtx GetTracyCtx() const { return tracy_ctx_; }

 private:
  friend class ImGuiLayer;
  friend class RenderPass;
  friend class RenderGraph;
  friend class Mesh;
  friend class Scene;
  friend class CommandBuffer;
  friend class AccelerationStructureManager;
  friend class Application;

#ifdef VULKAN_VALIDATION
  std::vector<const char*> validation_layers_;
  VkDebugUtilsMessengerEXT debug_messenger_{};
#endif
  std::vector<const char*> device_extensions_;

  bool initialized_;
  std::shared_ptr<AppWindow> window_;
  VkInstance instance_{};
  VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
  VkDevice logical_device_{};
  VkSurfaceKHR surface_{};
  VkQueue graphics_queue_{};
  VkQueue present_queue_{};
  std::mutex queue_submit_mutex_;
  VkSwapchainKHR swap_chain_{};
  bool swap_chain_created_;

  uint32_t image_index_;
  VkFormat swap_chain_image_format_;
  std::shared_ptr<AttachmentTexture> swap_chain_texture_;

  VkExtent2D extent_{};

  static constexpr uint32_t kMaxFramesInFlight = 2;
  uint32_t current_frame_ = 0;
  uint64_t frame_counter_ = 0;

  std::shared_ptr<CommandPool> command_pool_;
  std::vector<std::shared_ptr<CommandBuffer>> command_buffers_;

  std::vector<VkSemaphore> image_available_semaphores_;
  std::vector<VkSemaphore> render_finished_semaphores_;
  std::vector<VkSemaphore>
      render_order_semaphores_;  // Cross-frame GPU serialization
  std::vector<VkFence> fences_;

  float_t aspect_ratio_;
  WindowSize window_size_;
  Colorf clear_color_;
  std::shared_ptr<UniformBuffer> lights_uniform_buffer_;
  LightsUniformData lights_uniform_data_;
  std::shared_ptr<UniformBuffer> camera_uniform_buffer_;
  std::shared_ptr<UniformBuffer> shadow_camera_uniform_buffer_;
  std::shared_ptr<UniformBuffer> ssao_kernel_uniform_buffer_;
  CameraUniformData camera_uniform_data_;
  ShadowMapMatricesUniformData shadow_camera_uniform_data_;
  SSAOKernelUniformData ssao_kernel_uniform_data_;
  glm::mat4 prev_jittered_vp_{1.0f};
  uint32_t taa_frame_index_ = 0;
  RendererOptions options_;
  bool recreate_pipeline_;
  bool recreate_swap_chain_;
  bool recreate_resources_ = false;

  std::shared_ptr<CameraData> camera_;
  glm::vec2 viewport_size_;

  std::unordered_map<std::string, std::shared_ptr<DescriptorSetLayout>>
      descriptor_layouts_;

  // Entity picking readback
  VkBuffer pick_staging_buffer_ = VK_NULL_HANDLE;
  VkDeviceMemory pick_staging_memory_ = VK_NULL_HANDLE;
  bool pick_pending_ = false;

  RenderStats stats_;
  uint32_t pick_x_ = 0;
  uint32_t pick_y_ = 0;
  std::shared_ptr<AttachmentTexture> pick_entity_id_image_;
  std::shared_ptr<AttachmentTexture> pick_fallback_image_;

  std::shared_ptr<UniformBuffer> identity_bone_ubo_;
  std::shared_ptr<DescriptorSet> identity_bone_descriptor_;

  // Currently bound pipeline, set by Pipeline::Bind(), used by Draw*
  Pipeline* bound_pipeline_ = nullptr;

  std::shared_ptr<RenderPass> present_render_pass_;
  std::shared_ptr<Pipeline> present_pipeline_;
  std::shared_ptr<AttachmentTexture> present_color_image_;
  std::shared_ptr<AttachmentTexture> present_depth_stencil_;
  std::vector<std::shared_ptr<Framebuffer>> present_framebuffers_;

  std::shared_ptr<Sampler> default_linear_sampler_;
  std::shared_ptr<Sampler> default_nearest_sampler_;
  std::shared_ptr<Sampler> shadow_sampler_;
  std::shared_ptr<Texture> blank_texture_;
  std::shared_ptr<MemoryBuffer> quad_vertex_buffer_;
  std::shared_ptr<IndexBuffer> quad_index_buffer_;
  std::shared_ptr<AttachmentTexture> ssao_noise_;

  QueueFamilyIndices queue_family_indices_;
  SwapChainSupportDetails swap_chain_details_;
  VkPhysicalDeviceProperties physical_device_properties_;
  VkPhysicalDeviceFeatures physical_device_features_;
  std::vector<std::string> shader_features_;

  SamplingMode max_sampling_mode_;
  std::vector<SamplingMode> supported_sampling_modes_;

  TracyVkCtx tracy_ctx_;
  PFN_vkSetDebugUtilsObjectNameEXT pfn_set_debug_utils_object_name_ext_ =
      nullptr;
  PFN_vkCreateDebugUtilsMessengerEXT pfn_create_debug_utils_messenger_ext_ =
      nullptr;
  PFN_vkDestroyDebugUtilsMessengerEXT pfn_destroy_debug_utils_messenger_ext_ =
      nullptr;

  DeletionQueue deletion_queue_;

  // Transient UBO+descriptor pool for textured rect draws (double-buffered per FIF)
  struct SliceDrawResource {
    std::shared_ptr<UniformBuffer> ubo;
    std::shared_ptr<DescriptorSet> descriptor;
    VkImageView bound_texture = VK_NULL_HANDLE;
    VkSampler bound_sampler = VK_NULL_HANDLE;
  };

  std::vector<SliceDrawResource> slice_pool_[kMaxFramesInFlight];
  uint32_t slice_pool_used_[kMaxFramesInFlight] = {};
  SliceDrawResource& AcquireSliceResource(
      std::shared_ptr<Texture> texture,
      std::shared_ptr<DescriptorSetLayout> layout);

  // Ray tracing support
  std::shared_ptr<AccelerationStructureManager> as_manager_;
  bool rt_supported_ = false;
  VkPhysicalDeviceRayTracingPipelinePropertiesKHR rt_pipeline_properties_{};
  VkPhysicalDeviceAccelerationStructurePropertiesKHR rt_as_properties_{};
  bool CheckRayTracingSupport(VkPhysicalDevice device);

  // RT function pointers
  PFN_vkCreateAccelerationStructureKHR pfn_vkCreateAccelerationStructureKHR_ =
      nullptr;
  PFN_vkDestroyAccelerationStructureKHR pfn_vkDestroyAccelerationStructureKHR_ =
      nullptr;
  PFN_vkGetAccelerationStructureBuildSizesKHR
      pfn_vkGetAccelerationStructureBuildSizesKHR_ = nullptr;
  PFN_vkCmdBuildAccelerationStructuresKHR
      pfn_vkCmdBuildAccelerationStructuresKHR_ = nullptr;
  PFN_vkGetAccelerationStructureDeviceAddressKHR
      pfn_vkGetAccelerationStructureDeviceAddressKHR_ = nullptr;
  PFN_vkCreateRayTracingPipelinesKHR pfn_vkCreateRayTracingPipelinesKHR_ =
      nullptr;
  PFN_vkGetRayTracingShaderGroupHandlesKHR
      pfn_vkGetRayTracingShaderGroupHandlesKHR_ = nullptr;
  PFN_vkCmdTraceRaysKHR pfn_vkCmdTraceRaysKHR_ = nullptr;
};

#ifdef VULKAN_VALIDATION
static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data);
#endif

}  // namespace Wiesel