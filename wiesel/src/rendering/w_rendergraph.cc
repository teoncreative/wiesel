
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/w_rendergraph.h"
#include "rendering/w_deletion_queue.h"
#include "rendering/w_renderer.h"
#include "rendering/w_transient_resource_pool.h"

#include <algorithm>
#include <queue>

namespace wiesel {

VkImageLayout RGAccessToLayout(RGAccess access) {
  switch (access) {
    case RGAccess::ColorAttachmentWrite:
      return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    case RGAccess::DepthStencilWrite:
      return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    case RGAccess::DepthStencilReadOnly:
      return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    case RGAccess::ShaderRead:
      return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case RGAccess::StorageImageRead:
      return VK_IMAGE_LAYOUT_GENERAL;
    case RGAccess::StorageImageWrite:
      return VK_IMAGE_LAYOUT_GENERAL;
    default:
      return VK_IMAGE_LAYOUT_UNDEFINED;
  }
}

RenderGraph::RenderGraph(Renderer& renderer) : renderer_(renderer) {
#ifdef WIESEL_GPU_PROFILING
  timestamp_period_ =
      renderer_.GetPhysicalDeviceProperties().limits.timestampPeriod;
#endif
}

RenderGraph::~RenderGraph() {
  DestroyTransientResources();
#ifdef WIESEL_GPU_PROFILING
  DestroyQueryPool();
#endif
}

RGResource RenderGraph::ImportTexture(
    const std::string& name, std::shared_ptr<AttachmentTexture> texture,
    VkImageLayout initial_layout) {
  RGResource handle;
  handle.index = static_cast<uint32_t>(resources_.size());

  RGResourceData data;
  data.name = name;
  data.is_transient = false;
  data.texture = texture;
  // Use the texture's tracked layout if no explicit initial layout was given
  if (initial_layout == VK_IMAGE_LAYOUT_UNDEFINED && texture) {
    data.initial_layout = texture->current_layout_;
    data.current_layout = texture->current_layout_;
  } else {
    data.initial_layout = initial_layout;
    data.current_layout = initial_layout;
  }
  resources_.push_back(std::move(data));

  compiled_ = false;
  return handle;
}

RGResource RenderGraph::DeclareTransient(const RGTextureDesc& desc) {
  RGResource handle;
  handle.index = static_cast<uint32_t>(resources_.size());

  RGResourceData data;
  data.name = desc.name;
  data.is_transient = true;
  data.desc = desc;
  data.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
  data.current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
  resources_.push_back(std::move(data));

  compiled_ = false;
  return handle;
}

uint32_t RenderGraph::AddPass(const std::string& name, RGExecuteFn execute) {
  uint32_t index = static_cast<uint32_t>(passes_.size());

  RenderGraphPass pass;
  pass.name_ = name;
  pass.index_ = index;
  pass.execute_fn_ = std::move(execute);
  passes_.push_back(std::move(pass));

  compiled_ = false;
  return index;
}

void RenderGraph::PassReadsTexture(uint32_t pass, RGResource resource) {
  passes_[pass].inputs_.push_back({resource, RGAccess::ShaderRead});
  compiled_ = false;
}

void RenderGraph::PassReadsDepth(uint32_t pass, RGResource resource) {
  passes_[pass].inputs_.push_back({resource, RGAccess::DepthStencilReadOnly});
  compiled_ = false;
}

void RenderGraph::PassReadsStorageImage(uint32_t pass, RGResource resource) {
  passes_[pass].inputs_.push_back({resource, RGAccess::StorageImageRead});
  compiled_ = false;
}

void RenderGraph::PassReadsExternalTexture(uint32_t pass, RGResource resource) {
  passes_[pass].inputs_.push_back({resource, RGAccess::ShaderRead, true});
  compiled_ = false;
}

void RenderGraph::PassWritesColor(uint32_t pass, RGResource resource) {
  passes_[pass].outputs_.push_back({resource, RGAccess::ColorAttachmentWrite});
  compiled_ = false;
}

void RenderGraph::PassWritesColor(uint32_t pass, RGResource resource,
                                  RGResource resolve) {
  RGResourceRef ref{resource, RGAccess::ColorAttachmentWrite};
  ref.resolve = resolve;
  passes_[pass].outputs_.push_back(ref);
  if (resolve.IsValid()) {
    // Resolve target needs layout transitions too; declare it as a separate
    // output without a dependency edge of its own (no cycle risk since
    // resolve is only written by this pass).
    passes_[pass].outputs_.push_back(
        {resolve, RGAccess::ColorAttachmentWrite, true});
  }
  compiled_ = false;
}

void RenderGraph::PassWritesDepth(uint32_t pass, RGResource resource) {
  passes_[pass].outputs_.push_back({resource, RGAccess::DepthStencilWrite});
  compiled_ = false;
}

void RenderGraph::PassWritesDepthLoad(uint32_t pass, RGResource resource) {
  RGResourceRef ref{resource, RGAccess::DepthStencilWrite};
  ref.load = true;
  passes_[pass].outputs_.push_back(ref);
  compiled_ = false;
}

void RenderGraph::PassWritesStorageImage(uint32_t pass, RGResource resource) {
  passes_[pass].outputs_.push_back({resource, RGAccess::StorageImageWrite});
  compiled_ = false;
}

void RenderGraph::SetPassViewport(uint32_t pass, glm::vec2 size) {
  passes_[pass].viewport_size_ = size;
}

void RenderGraph::SetPassClearColor(uint32_t pass, const Colorf& color) {
  passes_[pass].clear_color_ = color;
}

void RenderGraph::SetPassEnabled(uint32_t pass, bool enabled) {
  passes_[pass].enabled_ = enabled;
}

void RenderGraph::SetPassAutoBeginRendering(uint32_t pass, bool enabled) {
  passes_[pass].auto_begin_rendering_ = enabled;
}

void RenderGraph::SetPassResolveFn(uint32_t pass, RGResolveFn fn) {
  passes_[pass].resolve_fn_ = std::move(fn);
}

void RenderGraph::Clear() {
  DestroyTransientResources();
  resources_.clear();

  // Defer destruction of old passes, their execute lambdas may capture
  // std::shared_ptr<> resources still referenced by in-flight command buffers.
  if (!passes_.empty()) {
    auto old_passes =
        std::make_shared<std::vector<RenderGraphPass>>(std::move(passes_));
    renderer_.GetDeletionQueue().Push([old_passes]() {
      // old_passes released here, destroying lambdas and their captures
    });
  }
  passes_.clear();

  sorted_order_.clear();
  compiled_ = false;
  dirty_ = true;
}

void RenderGraph::Compile() {
  PROFILE_ZONE_SCOPED();
  // Sort first so CreateTransientResources can use the execution order to
  // compute live ranges and alias non-overlapping transients into the same
  // backing image.
  TopologicalSort();
  CreateTransientResources();
  // After the pool has assigned textures, let each pass resolve anything
  // that depends on those textures (framebuffers, descriptors, etc).
  for (auto& pass : passes_) {
    if (pass.resolve_fn_) {
      pass.resolve_fn_(*this);
    }
  }
  compiled_ = true;
  dirty_ = false;
}

void RenderGraph::TopologicalSort() {
  PROFILE_ZONE_SCOPED();
  uint32_t pass_count = static_cast<uint32_t>(passes_.size());

  // Build adjacency list: for each resource, track which pass writes it
  // and which passes read it. Writers must execute before readers.
  std::unordered_map<uint32_t, uint32_t>
      resource_writer;  // resource index -> pass index
  std::vector<std::vector<uint32_t>> adj(pass_count);
  std::vector<uint32_t> in_degree(pass_count, 0);

  // First pass: find all writers
  for (uint32_t i = 0; i < pass_count; i++) {
    for (const auto& output : passes_[i].outputs_) {
      resource_writer[output.resource.index] = i;
    }
  }

  // Second pass: create edges from writers to readers
  for (uint32_t i = 0; i < pass_count; i++) {
    for (const auto& input : passes_[i].inputs_) {
      if (input.skip_dependency) {
        continue;  // External/cross-frame reads
      }
      auto it = resource_writer.find(input.resource.index);
      if (it != resource_writer.end() && it->second != i) {
        adj[it->second].push_back(i);
        in_degree[i]++;
      }
    }
  }

  // Kahn's algorithm
  std::queue<uint32_t> queue;
  for (uint32_t i = 0; i < pass_count; i++) {
    if (in_degree[i] == 0) {
      queue.push(i);
    }
  }

  sorted_order_.clear();
  sorted_order_.reserve(pass_count);

  while (!queue.empty()) {
    uint32_t current = queue.front();
    queue.pop();
    sorted_order_.push_back(current);

    for (uint32_t neighbor : adj[current]) {
      in_degree[neighbor]--;
      if (in_degree[neighbor] == 0) {
        queue.push(neighbor);
      }
    }
  }

  if (sorted_order_.size() != pass_count) {
    LOG_ERROR(
        "RenderGraph: Cycle detected in pass dependencies! "
        "Sorted {} of {} passes.",
        sorted_order_.size(), pass_count);
  }
}

void RenderGraph::CreateTransientResources() {
  PROFILE_ZONE_SCOPED();
  // Release anything we acquired on a previous compile. Each Compile
  // recomputes live ranges and may produce a different aliasing plan.
  DestroyTransientResources();

  auto to_props = [](const RGResourceData& resource) {
    AttachmentTextureProps props;
    props.width = resource.desc.width;
    props.height = resource.desc.height;
    props.type = resource.desc.type;
    props.image_count = 1;
    props.image_format = resource.desc.format;
    props.sampling_mode = resource.desc.samples;
    props.sampled = resource.desc.sampled;
    props.layer_count = resource.desc.layer_count;
    return props;
  };

  auto& pool = renderer_.GetTransientResourcePool();

  // If topo sort failed (cycle), don't try to alias - give every transient
  // its own entry so the pass still runs.
  const bool can_alias = sorted_order_.size() == passes_.size();
  if (!can_alias) {
    for (auto& resource : resources_) {
      if (!resource.is_transient) {
        continue;
      }
      resource.texture = pool.Acquire(to_props(resource));
      acquired_transients_.push_back(resource.texture);
    }
    return;
  }

  // Compute live ranges in sorted-order space. A transient is live from the
  // first pass that touches it to the last pass that touches it.
  std::vector<uint32_t> pass_to_order(passes_.size(), UINT32_MAX);
  for (uint32_t i = 0; i < sorted_order_.size(); ++i) {
    pass_to_order[sorted_order_[i]] = i;
  }

  struct Lifetime {
    uint32_t resource_index;
    uint32_t first_use;
    uint32_t last_use;
  };
  std::vector<Lifetime> lifetimes;
  lifetimes.reserve(resources_.size());

  for (uint32_t r = 0; r < resources_.size(); ++r) {
    if (!resources_[r].is_transient) {
      continue;
    }
    uint32_t first = UINT32_MAX;
    uint32_t last = 0;
    bool used = false;
    auto scan = [&](const std::vector<RGResourceRef>& refs, uint32_t order) {
      for (const auto& ref : refs) {
        if (ref.resource.index == r) {
          first = std::min(first, order);
          last = std::max(last, order);
          used = true;
        }
      }
    };
    for (uint32_t p = 0; p < passes_.size(); ++p) {
      uint32_t order = pass_to_order[p];
      if (order == UINT32_MAX) {
        continue;
      }
      scan(passes_[p].inputs_, order);
      scan(passes_[p].outputs_, order);
    }
    if (used) {
      lifetimes.push_back({r, first, last});
    } else {
      // Orphan transient - allocate its own entry so accidental use doesn't
      // blow up, but it's not part of aliasing.
      resources_[r].texture = pool.Acquire(to_props(resources_[r]));
      acquired_transients_.push_back(resources_[r].texture);
    }
  }

  // Greedy first-fit: process transients in ascending first_use. Reuse a
  // previously-allocated slot whose occupant is already dead and whose
  // descriptor matches.
  std::sort(lifetimes.begin(), lifetimes.end(),
            [](const Lifetime& a, const Lifetime& b) {
              return a.first_use < b.first_use;
            });

  struct Slot {
    AttachmentTextureProps props;
    std::shared_ptr<AttachmentTexture> texture;
    uint32_t last_use;
  };
  std::vector<Slot> slots;

  for (const Lifetime& lt : lifetimes) {
    AttachmentTextureProps props = to_props(resources_[lt.resource_index]);
    Slot* reuse = nullptr;
    for (auto& slot : slots) {
      if (slot.last_use < lt.first_use && slot.props == props) {
        reuse = &slot;
        break;
      }
    }
    if (reuse) {
      resources_[lt.resource_index].texture = reuse->texture;
      reuse->last_use = lt.last_use;
    } else {
      auto tex = pool.Acquire(props);
      slots.push_back({props, tex, lt.last_use});
      resources_[lt.resource_index].texture = tex;
      acquired_transients_.push_back(tex);
    }
  }
}

void RenderGraph::DestroyTransientResources() {
  PROFILE_ZONE_SCOPED();
  auto& pool = renderer_.GetTransientResourcePool();
  for (auto& tex : acquired_transients_) {
    pool.Release(tex);
  }
  acquired_transients_.clear();
  for (auto& resource : resources_) {
    if (resource.is_transient) {
      resource.texture = nullptr;
    }
  }
}

void RenderGraph::Execute(VkCommandBuffer cmd) {
  PROFILE_ZONE_SCOPED();
  if (!compiled_) {
    LOG_ERROR("RenderGraph: Execute called before Compile!");
    return;
  }

#ifdef WIESEL_GPU_PROFILING
  // Ensure query pool is large enough for all passes (2 timestamps per pass)
  uint32_t enabled_count = 0;
  for (uint32_t idx : sorted_order_) {
    if (passes_[idx].enabled_) {
      enabled_count++;
    }
  }
  uint32_t required_queries = enabled_count * 2;

  if (!query_pool_created_ || required_queries > query_count_) {
    if (query_pool_created_) {
      DestroyQueryPool();
    }
    query_count_ = std::max(required_queries, 64u);
    CreateQueryPool();
  }

  // Read from the oldest pool (kMaxFramesInFlight frames ago, guaranteed done).
  std::vector<float> gpu_timings_cache;
  uint32_t read_frame = (timing_frame_ + 1) % kTimingFrames;
  auto& read_names = query_pass_names_[read_frame];
  if (query_pool_created_ && !read_names.empty()) {
    uint32_t num_queries = static_cast<uint32_t>(read_names.size()) * 2;
    std::vector<uint64_t> timestamps(num_queries);
    VkResult result = vkGetQueryPoolResults(
        renderer_.GetLogicalDevice(), query_pools_[read_frame], 0, num_queries,
        num_queries * sizeof(uint64_t), timestamps.data(), sizeof(uint64_t),
        VK_QUERY_RESULT_64_BIT);

    if (result == VK_SUCCESS) {
      gpu_timings_cache.resize(read_names.size());
      for (size_t i = 0; i < read_names.size(); i++) {
        uint64_t begin_ts = timestamps[i * 2];
        uint64_t end_ts = timestamps[i * 2 + 1];
        gpu_timings_cache[i] =
            static_cast<float>(end_ts - begin_ts) * timestamp_period_ / 1e6f;
      }
    }
  }

  // Reset the current frame's query pool and pass names
  vkCmdResetQueryPool(cmd, query_pools_[timing_frame_], 0, query_count_);
  query_pass_names_[timing_frame_].clear();
  uint32_t query_idx = 0;
#endif

  pass_timings_.clear();

  // Note: we do NOT reset resource layouts between frames.
  // Layouts persist across frames so that barrier insertion uses the
  // actual Vulkan image layout from the previous frame's end state.

  for (uint32_t idx : sorted_order_) {
    auto& pass = passes_[idx];
    if (!pass.enabled_) {
      continue;
    }

    PROFILE_ZONE_SCOPED_N("RenderPass");
    ZoneText(pass.name_.c_str(), pass.name_.size());

    {
      PROFILE_ZONE_SCOPED_N("Barriers");
      ZoneText(pass.name_.c_str(), pass.name_.size());
      InsertBarriers(cmd, pass);
    }

#ifdef WIESEL_GPU_PROFILING
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        query_pools_[timing_frame_], query_idx * 2);
#endif

    auto cpu_start = std::chrono::high_resolution_clock::now();

    const bool manage_rendering = pass.auto_begin_rendering_;

    if (manage_rendering) {
      PROFILE_ZONE_SCOPED_N("BeginRendering");
      ZoneText(pass.name_.c_str(), pass.name_.size());
      BeginDynamicRendering(cmd, pass);
      if (pass.viewport_size_.x > 0 && pass.viewport_size_.y > 0) {
        renderer_.SetViewport(pass.viewport_size_);
      }
    }

    if (pass.execute_fn_) {
      PROFILE_ZONE_SCOPED_N("Execute");
      ZoneText(pass.name_.c_str(), pass.name_.size());
      pass.execute_fn_(cmd);
    }

    if (manage_rendering) {
      PROFILE_ZONE_SCOPED_N("EndRendering");
      ZoneText(pass.name_.c_str(), pass.name_.size());
      vkCmdEndRendering(cmd);
    }

    auto cpu_end = std::chrono::high_resolution_clock::now();
    float cpu_ms =
        std::chrono::duration<float, std::milli>(cpu_end - cpu_start).count();

    PassTimingResult timing;
    timing.name = pass.name_;
    timing.cpu_time_ms = cpu_ms;

#ifdef WIESEL_GPU_PROFILING
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                        query_pools_[timing_frame_], query_idx * 2 + 1);
    // Apply previous frame's GPU timing if available at same index
    if (query_idx < gpu_timings_cache.size()) {
      timing.gpu_time_ms = gpu_timings_cache[query_idx];
    }
    query_pass_names_[timing_frame_].push_back(pass.name_);
    query_idx++;
#endif

    pass_timings_.push_back(std::move(timing));

    // Update resource layouts for outputs
    UpdateOutputLayouts(pass);
  }

#ifdef WIESEL_GPU_PROFILING
  timing_frame_ = (timing_frame_ + 1) % kTimingFrames;
#endif
}

void RenderGraph::TransitionResource(VkCommandBuffer cmd,
                                     RGResourceData& resource,
                                     VkImageLayout required) {
  if (resource.current_layout == required) {
    return;
  }
  if (!resource.texture) {
    return;
  }

  // Determine layer count from the texture or the desc
  uint32_t layer_count = 1;
  if (resource.is_transient && resource.desc.layer_count > 1) {
    layer_count = resource.desc.layer_count;
  } else if (!resource.is_transient &&
             !resource.texture->image_views_.empty()) {
    layer_count = resource.texture->image_views_[0]->layer_count_;
  }

  renderer_.TransitionImageLayout(
      cmd, resource.texture->images_[0], resource.texture->format_,
      resource.current_layout, required, 1, 0, layer_count);

  resource.current_layout = required;
  resource.texture->current_layout_ = required;
}

void RenderGraph::InsertBarriers(VkCommandBuffer cmd,
                                 const RenderGraphPass& pass) {
  // Transition inputs to their required read layouts
  for (const auto& input : pass.inputs_) {
    auto& resource = resources_[input.resource.index];
    TransitionResource(cmd, resource, RGAccessToLayout(input.access));
  }

  // Transition color/storage outputs declared on the pass. Dynamic rendering
  // requires attachments be in the right layout at vkCmdBeginRendering time.
  for (const auto& output : pass.outputs_) {
    if (output.access == RGAccess::ColorAttachmentWrite) {
      auto& resource = resources_[output.resource.index];
      TransitionResource(cmd, resource,
                         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    } else if (output.access == RGAccess::DepthStencilWrite) {
      auto& resource = resources_[output.resource.index];
      TransitionResource(cmd, resource,
                         VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    } else if (output.access == RGAccess::StorageImageWrite) {
      auto& resource = resources_[output.resource.index];
      TransitionResource(cmd, resource, VK_IMAGE_LAYOUT_GENERAL);
    }
  }

}

void RenderGraph::BeginDynamicRendering(VkCommandBuffer cmd,
                                        const RenderGraphPass& pass) {
  std::vector<VkRenderingAttachmentInfo> color_infos;
  VkRenderingAttachmentInfo depth_info{};
  bool has_depth = false;
  VkRenderingAttachmentInfo stencil_info{};
  bool has_stencil = false;

  uint32_t extent_w = static_cast<uint32_t>(pass.viewport_size_.x);
  uint32_t extent_h = static_cast<uint32_t>(pass.viewport_size_.y);

  for (const auto& out : pass.outputs_) {
    if (out.skip_dependency) {
      // Resolve companions are recorded as skip_dependency outputs; the color
      // they pair with handles them directly. Avoid double-emitting.
      continue;
    }
    auto texture = resources_[out.resource.index].texture;
    if (!texture || texture->image_views_.empty()) {
      continue;
    }
    if (extent_w == 0 || extent_h == 0) {
      extent_w = texture->width_;
      extent_h = texture->height_;
    }
    if (out.access == RGAccess::ColorAttachmentWrite) {
      VkRenderingAttachmentInfo info{};
      info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
      info.imageView = texture->image_views_[0]->handle_;
      info.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
      info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      info.clearValue.color = {{pass.clear_color_.red, pass.clear_color_.green,
                                pass.clear_color_.blue,
                                pass.clear_color_.alpha}};
      if (out.resolve.IsValid()) {
        auto resolve_tex = resources_[out.resolve.index].texture;
        if (resolve_tex && !resolve_tex->image_views_.empty()) {
          info.resolveImageView = resolve_tex->image_views_[0]->handle_;
          info.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
          info.resolveMode =
              renderer_.IsIntegerColorFormat(texture->format_)
                  ? VK_RESOLVE_MODE_SAMPLE_ZERO_BIT
                  : VK_RESOLVE_MODE_AVERAGE_BIT;
        }
      }
      color_infos.push_back(info);
    } else if (out.access == RGAccess::DepthStencilWrite && !has_depth) {
      depth_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
      depth_info.imageView = texture->image_views_[0]->handle_;
      depth_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      depth_info.loadOp = out.load ? VK_ATTACHMENT_LOAD_OP_LOAD
                                   : VK_ATTACHMENT_LOAD_OP_CLEAR;
      depth_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      depth_info.clearValue.depthStencil = {1.0f, 0};
      has_depth = true;
      if (renderer_.HasStencilComponent(texture->format_)) {
        stencil_info = depth_info;
        stencil_info.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        stencil_info.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        has_stencil = true;
      }
    }
  }

  VkRenderingInfo ri{};
  ri.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  ri.renderArea.offset = {0, 0};
  ri.renderArea.extent.width = extent_w;
  ri.renderArea.extent.height = extent_h;
  ri.layerCount = 1;
  ri.colorAttachmentCount = static_cast<uint32_t>(color_infos.size());
  ri.pColorAttachments = color_infos.data();
  ri.pDepthAttachment = has_depth ? &depth_info : nullptr;
  ri.pStencilAttachment = has_stencil ? &stencil_info : nullptr;

  vkCmdBeginRendering(cmd, &ri);
}

void RenderGraph::UpdateOutputLayouts(const RenderGraphPass& pass) {
  for (const auto& output : pass.outputs_) {
    auto layout = RGAccessToLayout(output.access);
    resources_[output.resource.index].current_layout = layout;
    if (resources_[output.resource.index].texture) {
      resources_[output.resource.index].texture->current_layout_ = layout;
    }
  }
}

// Management

void RenderGraph::MarkDirty() {
  dirty_ = true;
}

std::shared_ptr<AttachmentTexture> RenderGraph::GetTexture(
    RGResource handle) const {
  if (!handle.IsValid() || handle.index >= resources_.size()) {
    return nullptr;
  }
  return resources_[handle.index].texture;
}

RenderGraphPass& RenderGraph::GetPass(uint32_t index) {
  return passes_[index];
}

const RenderGraphPass& RenderGraph::GetPass(uint32_t index) const {
  return passes_[index];
}

#ifdef WIESEL_GPU_PROFILING
void RenderGraph::CreateQueryPool() {
  VkQueryPoolCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
  ci.queryType = VK_QUERY_TYPE_TIMESTAMP;
  ci.queryCount = query_count_;

  VkDevice device = renderer_.GetLogicalDevice();
  for (uint32_t i = 0; i < kTimingFrames; i++) {
    vkCreateQueryPool(device, &ci, nullptr, &query_pools_[i]);
  }
  query_pool_created_ = true;
}

void RenderGraph::DestroyQueryPool() {
  if (!query_pool_created_) {
    return;
  }
  VkDevice device = renderer_.GetLogicalDevice();
  for (uint32_t i = 0; i < kTimingFrames; i++) {
    if (query_pools_[i]) {
      VkQueryPool pool = query_pools_[i];
      query_pools_[i] = VK_NULL_HANDLE;
      renderer_.GetDeletionQueue().Push(
          [device, pool]() { vkDestroyQueryPool(device, pool, nullptr); });
    }
  }
  query_pool_created_ = false;
}
#endif

}  // namespace wiesel
