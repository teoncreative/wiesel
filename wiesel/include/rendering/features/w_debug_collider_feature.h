
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

#include "rendering/w_buffer.h"
#include "rendering/w_descriptor.h"
#include "rendering/w_descriptorlayout.h"
#include "rendering/w_render_feature.h"
#include "rendering/w_texture.h"

namespace wiesel {

struct DebugColliderPushConstant {
  glm::mat4 mvp;
  glm::mat4 model;
  glm::vec4 color;
};

// Filled geometry (triangle lists for translucent overlays, with UVs)
struct OverlayVertex {
  glm::vec3 position;
  glm::vec2 uv;
};

class DebugColliderFeature : public RenderFeature {
 public:
  explicit DebugColliderFeature(std::shared_ptr<Renderer> renderer);

  const std::string& GetName() const override { return name_; }

  void SetupResources(RenderContext& ctx) override;
  void AddPasses(RenderGraph& graph, RenderResourceRegistry& registry,
                 RenderContext& ctx) override;

 private:
  void GenerateBoxGeometry();
  void GenerateSphereGeometry();
  void GenerateFilledBoxGeometry();
  void GenerateFilledSphereGeometry();

  static inline std::string name_ = "DebugColliders";
  std::shared_ptr<Renderer> renderer_;

  // Wireframe rendering
  std::shared_ptr<RenderPass> render_pass_;
  std::shared_ptr<Pipeline> pipeline_;
  std::shared_ptr<Pipeline>
      no_depth_pipeline_;  // camera frustums (always visible)
  std::shared_ptr<Pipeline> filled_pipeline_;  // translucent filled
  std::shared_ptr<DebugColliderPushConstant> push_constant_;

  // Compositing onto PipelineOutput
  std::shared_ptr<RenderPass> comp_render_pass_;
  std::shared_ptr<Pipeline> comp_pipeline_;

  // Box wireframe geometry
  std::shared_ptr<MemoryBuffer> box_vertex_buffer_;
  std::shared_ptr<IndexBuffer> box_index_buffer_;
  uint32_t box_index_count_ = 0;

  // Sphere wireframe geometry
  std::shared_ptr<MemoryBuffer> sphere_vertex_buffer_;
  std::shared_ptr<IndexBuffer> sphere_index_buffer_;
  uint32_t sphere_index_count_ = 0;

  std::shared_ptr<MemoryBuffer> filled_box_vb_;
  std::shared_ptr<IndexBuffer> filled_box_ib_;
  uint32_t filled_box_ic_ = 0;
  std::shared_ptr<MemoryBuffer> filled_sphere_vb_;
  std::shared_ptr<IndexBuffer> filled_sphere_ib_;
  uint32_t filled_sphere_ic_ = 0;

  // Label textures + descriptors (cached by name)
  std::shared_ptr<DescriptorSetLayout> overlay_desc_layout_;
  std::unordered_map<std::string, std::shared_ptr<DescriptorSet>>
      label_descriptors_;
  std::shared_ptr<DescriptorSet> GetOrCreateLabelDescriptor(
      const std::string& label, const glm::vec4& bg_color,
      const glm::vec4& text_color);

  // Cached debug geometry for complex colliders
  struct CachedDebugData {
    std::shared_ptr<MemoryBuffer> vb;
    std::shared_ptr<IndexBuffer> ib;
    uint32_t index_count = 0;
    glm::mat4 model = glm::mat4(1.0f);
  };

  std::vector<CachedDebugData> hf_cache_;
  bool hf_cache_valid_ = false;

  std::vector<CachedDebugData> mesh_collider_cache_;
  bool mesh_collider_cache_valid_ = false;
};

}  // namespace wiesel
