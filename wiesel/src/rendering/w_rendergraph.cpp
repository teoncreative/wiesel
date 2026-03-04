
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/w_rendergraph.hpp"
#include "rendering/w_renderer.hpp"
#include "rendering/w_renderpass.hpp"
#include "rendering/w_framebuffer.hpp"

#include <algorithm>
#include <queue>

namespace Wiesel {

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
    case RGAccess::Present:
      return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    default:
      return VK_IMAGE_LAYOUT_UNDEFINED;
  }
}

RenderGraph::RenderGraph(Renderer& renderer) : renderer_(renderer) {}

RenderGraph::~RenderGraph() {
  DestroyTransientResources();
}

RGResource RenderGraph::ImportTexture(const std::string& name,
                                      Ref<AttachmentTexture> texture,
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

uint32_t RenderGraph::AddPass(const std::string& name,
                              Ref<RenderPass> render_pass,
                              RGExecuteFn execute) {
  uint32_t index = static_cast<uint32_t>(passes_.size());

  RenderGraphPass pass;
  pass.name_ = name;
  pass.index_ = index;
  pass.render_pass_ = render_pass;
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

void RenderGraph::PassReadsExternalTexture(uint32_t pass, RGResource resource) {
  passes_[pass].inputs_.push_back({resource, RGAccess::ShaderRead, true});
  compiled_ = false;
}

void RenderGraph::PassWritesColor(uint32_t pass, RGResource resource) {
  passes_[pass].outputs_.push_back({resource, RGAccess::ColorAttachmentWrite});
  compiled_ = false;
}

void RenderGraph::PassWritesDepth(uint32_t pass, RGResource resource) {
  passes_[pass].outputs_.push_back({resource, RGAccess::DepthStencilWrite});
  compiled_ = false;
}

void RenderGraph::PassPresents(uint32_t pass, RGResource resource) {
  passes_[pass].outputs_.push_back({resource, RGAccess::Present});
  compiled_ = false;
}

void RenderGraph::SetPassFramebuffer(uint32_t pass, Ref<Framebuffer> fb) {
  passes_[pass].framebuffer_ = fb;
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

void RenderGraph::SetPassManagesRenderPass(uint32_t pass, bool manages) {
  passes_[pass].manages_render_pass_ = manages;
}

void RenderGraph::Compile() {
  PROFILE_ZONE_SCOPED_N("RenderGraph::Compile");
  CreateTransientResources();
  TopologicalSort();
  compiled_ = true;
  dirty_ = false;
}

void RenderGraph::TopologicalSort() {
  uint32_t pass_count = static_cast<uint32_t>(passes_.size());

  // Build adjacency list: for each resource, track which pass writes it
  // and which passes read it. Writers must execute before readers.
  std::unordered_map<uint32_t, uint32_t> resource_writer;  // resource index -> pass index
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
      if (input.skip_dependency) continue;  // External/cross-frame reads
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
    LOG_ERROR("RenderGraph: Cycle detected in pass dependencies! "
              "Sorted {} of {} passes.", sorted_order_.size(), pass_count);
  }
}

void RenderGraph::CreateTransientResources() {
  for (auto& resource : resources_) {
    if (!resource.is_transient) continue;
    if (resource.texture) continue;  // Already created

    AttachmentTextureProps props;
    props.width = resource.desc.width;
    props.height = resource.desc.height;
    props.type = resource.desc.type;
    props.image_count = 1;
    props.image_format = resource.desc.format;
    props.sampling_mode = resource.desc.samples;
    props.sampled = resource.desc.sampled;
    props.layer_count = resource.desc.layer_count;

    resource.texture = renderer_.CreateAttachmentTexture(props);
  }
}

void RenderGraph::DestroyTransientResources() {
  for (auto& resource : resources_) {
    if (resource.is_transient) {
      resource.texture = nullptr;
    }
  }
}

void RenderGraph::Execute(VkCommandBuffer cmd) {
  PROFILE_ZONE_SCOPED_N("RenderGraph::Execute");
  if (!compiled_) {
    LOG_ERROR("RenderGraph: Execute called before Compile!");
    return;
  }

  // Note: we do NOT reset resource layouts between frames.
  // Layouts persist across frames so that barrier insertion uses the
  // actual Vulkan image layout from the previous frame's end state.

  for (uint32_t idx : sorted_order_) {
    auto& pass = passes_[idx];
    if (!pass.enabled_) continue;

    PROFILE_ZONE_SCOPED_N("RenderPass");
    ZoneText(pass.name_.c_str(), pass.name_.size());

    // Insert barriers for inputs (transition to required layouts)
    InsertBarriers(cmd, pass);

    // Begin render pass
    if (pass.manages_render_pass_ && pass.render_pass_ && pass.framebuffer_) {
      pass.render_pass_->Begin(pass.framebuffer_, pass.clear_color_);
      if (pass.viewport_size_.x > 0 && pass.viewport_size_.y > 0) {
        renderer_.SetViewport(pass.viewport_size_);
      }
    }

    // Execute the pass
    if (pass.execute_fn_) {
      pass.execute_fn_(cmd);
    }

    // End render pass
    if (pass.manages_render_pass_ && pass.render_pass_ && pass.framebuffer_) {
      pass.render_pass_->End();
    }

    // Update resource layouts for outputs
    UpdateOutputLayouts(pass);
  }
}

void RenderGraph::TransitionResource(VkCommandBuffer cmd, RGResourceData& resource, VkImageLayout required) {
  if (resource.current_layout == required) return;
  if (!resource.texture) return;

  // Determine layer count from the texture or the desc
  uint32_t layer_count = 1;
  if (resource.is_transient && resource.desc.layer_count > 1) {
    layer_count = resource.desc.layer_count;
  } else if (!resource.is_transient && !resource.texture->image_views_.empty()) {
    layer_count = resource.texture->image_views_[0]->layer_count_;
  }

  renderer_.TransitionImageLayout(
      resource.texture->images_[0],
      resource.texture->format_,
      resource.current_layout,
      required,
      1,
      cmd,
      0,
      layer_count);

  resource.current_layout = required;
  resource.texture->current_layout_ = required;
}

void RenderGraph::InsertBarriers(VkCommandBuffer cmd, const RenderGraphPass& pass) {
  // Transition inputs to their required read layouts
  for (const auto& input : pass.inputs_) {
    auto& resource = resources_[input.resource.index];
    TransitionResource(cmd, resource, RGAccessToLayout(input.access));
  }

  // Transition color attachment outputs to COLOR_ATTACHMENT_OPTIMAL.
  // The render pass Bake() sets initialLayout = COLOR_ATTACHMENT_OPTIMAL for
  // Color/Offscreen attachments, so the image must be in that layout before
  // the render pass begins. Depth and Resolve use initialLayout = UNDEFINED.
  for (const auto& output : pass.outputs_) {
    if (output.access == RGAccess::ColorAttachmentWrite) {
      auto& resource = resources_[output.resource.index];
      TransitionResource(cmd, resource, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    }
  }
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

Ref<AttachmentTexture> RenderGraph::GetTexture(RGResource handle) const {
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

}  // namespace Wiesel
