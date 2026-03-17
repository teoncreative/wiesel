
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
#include "util/w_color.hpp"
#include "util/w_utils.hpp"
#include "w_texture.hpp"

namespace Wiesel {

class Renderer;
class RenderPass;
class Framebuffer;
struct Pipeline;

// Opaque handle to a resource in the render graph
struct RGResource {
  uint32_t index = UINT32_MAX;
  bool IsValid() const { return index != UINT32_MAX; }
  bool operator==(const RGResource& other) const { return index == other.index; }
  bool operator!=(const RGResource& other) const { return index != other.index; }
};

// How a pass accesses a resource
enum class RGAccess {
  ColorAttachmentWrite,       // VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
  DepthStencilWrite,          // VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
  DepthStencilReadOnly,       // VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
  ShaderRead,                 // VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
  StorageImageRead,           // VK_IMAGE_LAYOUT_GENERAL (imageLoad in shader)
  StorageImageWrite,          // VK_IMAGE_LAYOUT_GENERAL
  Present                     // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
};

VkImageLayout RGAccessToLayout(RGAccess access);

// Description for creating a transient resource
struct RGTextureDesc {
  std::string name;
  uint32_t width = 0;
  uint32_t height = 0;
  VkFormat format = VK_FORMAT_UNDEFINED;
  SamplingMode samples = SamplingMode::DISABLED;
  AttachmentTextureType type = AttachmentTextureType::Offscreen;
  uint32_t layer_count = 1;
  bool sampled = true;
};

// Internal: tracks a resource across the graph
struct RGResourceData {
  std::string name;
  bool is_transient = false;
  RGTextureDesc desc;
  std::shared_ptr<AttachmentTexture> texture;
  VkImageLayout current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
  VkImageLayout initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
};

// Internal: one read or write reference in a pass
struct RGResourceRef {
  RGResource resource;
  RGAccess access;
  bool skip_dependency = false;  // If true, don't create topological edge
};

// Execute callback - called each frame during graph execution
using RGExecuteFn = std::function<void(VkCommandBuffer cmd)>;

class RenderGraphPass {
  friend class RenderGraph;

  std::string name_;
  uint32_t index_ = 0;
  std::shared_ptr<RenderPass> render_pass_;
  std::shared_ptr<Framebuffer> framebuffer_;
  std::vector<RGResourceRef> inputs_;
  std::vector<RGResourceRef> outputs_;
  RGExecuteFn execute_fn_;
  glm::vec2 viewport_size_ = {0, 0};
  Colorf clear_color_ = {0, 0, 0, 1};
  bool enabled_ = true;
  bool manages_render_pass_ = true;  // If true, graph calls Begin/End on render pass

 public:
  const std::string& GetName() const { return name_; }
  bool IsEnabled() const { return enabled_; }
};

// Per-pass timing data
struct PassTimingResult {
  std::string name;
  float cpu_time_ms = 0.0f;
#ifdef WIESEL_GPU_PROFILING
  float gpu_time_ms = 0.0f;
#endif
};

class RenderGraph {
 public:
  explicit RenderGraph(Renderer& renderer);
  ~RenderGraph();

  // Build Phase

  // Import an external resource into the graph
  RGResource ImportTexture(const std::string& name,
                           std::shared_ptr<AttachmentTexture> texture,
                           VkImageLayout initial_layout = VK_IMAGE_LAYOUT_UNDEFINED);

  // Declare a transient resource (created by graph during Compile)
  RGResource DeclareTransient(const RGTextureDesc& desc);

  // Add a render pass. Returns the pass index for further configuration.
  uint32_t AddPass(const std::string& name,
                   std::shared_ptr<RenderPass> render_pass,
                   RGExecuteFn execute);

  // Configure pass inputs (resources the pass reads)
  void PassReadsTexture(uint32_t pass, RGResource resource);
  void PassReadsDepth(uint32_t pass, RGResource resource);
  void PassReadsStorageImage(uint32_t pass, RGResource resource);

  // Read a resource from a previous frame (barrier only, no dependency edge).
  // Use this for frame-to-frame resources like TAA history that would create
  // cycles if declared as normal reads.
  void PassReadsExternalTexture(uint32_t pass, RGResource resource);

  // Configure pass outputs (resources the pass writes)
  void PassWritesColor(uint32_t pass, RGResource resource);
  void PassWritesDepth(uint32_t pass, RGResource resource);
  void PassWritesStorageImage(uint32_t pass, RGResource resource);
  void PassPresents(uint32_t pass, RGResource resource);

  // Configure pass properties
  void SetPassFramebuffer(uint32_t pass, std::shared_ptr<Framebuffer> fb);
  void SetPassViewport(uint32_t pass, glm::vec2 size);
  void SetPassClearColor(uint32_t pass, const Colorf& color);
  void SetPassEnabled(uint32_t pass, bool enabled);
  void SetPassManagesRenderPass(uint32_t pass, bool manages);

  void Compile();
  void Execute(VkCommandBuffer cmd);

  // Clear all passes and resources for rebuilding, but keep GPU profiling state
  void Clear();

  // Management
  void MarkDirty();
  bool IsDirty() const { return dirty_; }
  bool IsCompiled() const { return compiled_; }

  // Access a resource's texture (valid after Compile)
  std::shared_ptr<AttachmentTexture> GetTexture(RGResource handle) const;

  // Get a pass by index
  RenderGraphPass& GetPass(uint32_t index);
  const RenderGraphPass& GetPass(uint32_t index) const;

  const std::vector<PassTimingResult>& GetPassTimings() const { return pass_timings_; }

#ifdef WIESEL_GPU_PROFILING
  void CreateQueryPool();
  void DestroyQueryPool();
#endif

 private:
  void TopologicalSort();
  void TransitionResource(VkCommandBuffer cmd, RGResourceData& resource, VkImageLayout required);
  void InsertBarriers(VkCommandBuffer cmd, const RenderGraphPass& pass);
  void UpdateOutputLayouts(const RenderGraphPass& pass);
  void CreateTransientResources();
  void DestroyTransientResources();

  Renderer& renderer_;
  bool dirty_ = true;
  bool compiled_ = false;
  std::vector<RGResourceData> resources_;
  std::vector<RenderGraphPass> passes_;
  std::vector<uint32_t> sorted_order_;
  std::vector<PassTimingResult> pass_timings_;

#ifdef WIESEL_GPU_PROFILING
  // Triple-buffered query pools so we always read from a frame guaranteed
  // complete by the fence (needs kMaxFramesInFlight + 1 pools).
  static constexpr uint32_t kTimingFrames = 3;
  VkQueryPool query_pools_[kTimingFrames] = {};
  uint32_t timing_frame_ = 0;
  uint32_t query_count_ = 0;  // Number of queries used in the current frame
  float timestamp_period_ = 0.0f;
  // Per-pool pass name lists so we read the correct names for each pool
  std::vector<std::string> query_pass_names_[kTimingFrames];
  bool query_pool_created_ = false;
#endif
};

}  // namespace Wiesel
