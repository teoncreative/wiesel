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

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/RenderInterface.h>

#include "rendering/w_buffer.h"
#include "rendering/w_descriptor.h"
#include "rendering/w_pipeline.h"
#include "rendering/w_texture.h"
#include "w_pch.h"

namespace wiesel {

class Renderer;

// RmlUi vertex format for Vulkan rendering
struct RmlVertex {
  glm::vec2 position;
  uint32_t color;
  glm::vec2 tex_coord;

  static VkVertexInputBindingDescription GetBindingDescription() {
    return {0, sizeof(RmlVertex), VK_VERTEX_INPUT_RATE_VERTEX};
  }

  static std::vector<VkVertexInputAttributeDescription>
  GetAttributeDescriptions() {
    return {
        {0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(RmlVertex, position)},
        {1, 0, VK_FORMAT_R8G8B8A8_UNORM, offsetof(RmlVertex, color)},
        {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(RmlVertex, tex_coord)},
    };
  }
};

// Compiled geometry stored by the render backend
struct RmlCompiledGeometry {
  std::shared_ptr<MemoryBuffer> vertex_buffer;
  std::shared_ptr<IndexBuffer> index_buffer;
  uint32_t index_count = 0;
};

// Vulkan render backend for RmlUi.
// Uses the engine's Renderer for buffer/texture/pipeline management.
class RmlRenderInterface : public Rml::RenderInterface {
 public:
  explicit RmlRenderInterface(std::shared_ptr<Renderer> renderer);
  ~RmlRenderInterface() override;

  // Initialize pipelines and descriptor layouts. Pipelines are baked against
  // the RmlUi offscreen attachment formats (single color + depth-stencil for
  // clip masks).
  void Init(VkFormat color_format, VkFormat depth_stencil_format);

  // Rml::RenderInterface implementation
  Rml::CompiledGeometryHandle CompileGeometry(
      Rml::Span<const Rml::Vertex> vertices,
      Rml::Span<const int> indices) override;
  void RenderGeometry(Rml::CompiledGeometryHandle geometry,
                      Rml::Vector2f translation,
                      Rml::TextureHandle texture) override;
  void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;

  Rml::TextureHandle LoadTexture(Rml::Vector2i& texture_dimensions,
                                 const Rml::String& source) override;
  Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source,
                                     Rml::Vector2i source_dimensions) override;
  void ReleaseTexture(Rml::TextureHandle texture) override;

  void EnableScissorRegion(bool enable) override;
  void SetScissorRegion(Rml::Rectanglei region) override;

  void EnableClipMask(bool enable) override;
  void RenderToClipMask(Rml::ClipMaskOperation operation,
                        Rml::CompiledGeometryHandle geometry,
                        Rml::Vector2f translation) override;

  void SetTransform(const Rml::Matrix4f* transform) override;

  // Render a Rml::Context into a framebuffer at the given size.
  void RenderToTexture(VkCommandBuffer cmd, Rml::Context* context,
                       glm::vec2 size);

  VkFormat GetColorFormat() const { return color_format_; }
  VkFormat GetDepthStencilFormat() const { return depth_stencil_format_; }

 private:
  void RenderGeometryWithPipeline(Pipeline* pipeline,
                                  Rml::CompiledGeometryHandle geometry,
                                  Rml::Vector2f translation,
                                  Rml::TextureHandle texture);
  void BuildProjection();

  std::shared_ptr<Renderer> renderer_;
  VkFormat color_format_ = VK_FORMAT_UNDEFINED;
  VkFormat depth_stencil_format_ = VK_FORMAT_UNDEFINED;
  std::shared_ptr<DescriptorSetLayout> descriptor_layout_;

  // Pipelines
  std::shared_ptr<Pipeline> pipeline_;               // Normal, no stencil
  std::shared_ptr<Pipeline> pipeline_stencil_test_;  // Stencil EQUAL test
  std::shared_ptr<Pipeline> pipeline_stencil_set_;   // Stencil REPLACE write
  std::shared_ptr<Pipeline> pipeline_stencil_incr_;  // Stencil INCR write

  // Per-texture descriptor cache
  std::unordered_map<Rml::TextureHandle, std::shared_ptr<DescriptorSet>>
      texture_descriptors_;

  // Blank texture for untextured geometry
  std::shared_ptr<Texture> blank_texture_;
  std::shared_ptr<DescriptorSet> blank_descriptor_;

  // Active state during rendering
  VkCommandBuffer active_cmd_ = VK_NULL_HANDLE;
  glm::vec2 viewport_size_{1920, 1080};
  bool scissor_enabled_ = false;
  Rml::Matrix4f transform_rml_ = Rml::Matrix4f::Identity();

  // Clip mask state
  bool clip_mask_enabled_ = false;
  int stencil_ref_ = 0;

  // Full-screen quad for stencil clearing (mid-pass)
  Rml::CompiledGeometryHandle fullscreen_quad_ = 0;

  // Stored textures (prevent deallocation)
  std::unordered_map<Rml::TextureHandle, std::shared_ptr<Texture>>
      loaded_textures_;
  Rml::TextureHandle next_texture_id_ = 1;

 public:
  struct PushConstantData {
    glm::mat4 transform;
    glm::vec2 translate;
  };

 private:
  std::shared_ptr<PushConstantData> push_constant_data_;
  Rml::Matrix4f projection_;

  std::shared_ptr<DescriptorSet> GetOrCreateDescriptor(
      Rml::TextureHandle texture);
};

}  // namespace wiesel
