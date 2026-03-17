
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

#include "rendering/w_render_feature.hpp"
#include "rendering/w_buffer.hpp"

namespace Wiesel {

struct DebugColliderPushConstant {
  glm::mat4 mvp;
  glm::vec4 color;
};

class DebugColliderFeature : public RenderFeature {
 public:
  explicit DebugColliderFeature(std::shared_ptr<Renderer> renderer);

  const std::string& GetName() const override { return name_; }
  void SetupResources(RenderContext& ctx) override;
  void AddPasses(RenderGraph& graph, RenderResourceRegistry& registry,
                 RenderContext& ctx) override;
  // Always enabled so resources are always set up (pipeline output chain).
  // The actual drawing is skipped inside AddPasses when debug_colliders is off.

 private:
  void GenerateBoxGeometry();
  void GenerateSphereGeometry();

  static inline std::string name_ = "DebugColliders";
  std::shared_ptr<Renderer> renderer_;

  // Wireframe rendering
  std::shared_ptr<RenderPass> render_pass_;
  std::shared_ptr<Pipeline> pipeline_;
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

  // Cached heightfield debug geometry
  struct HeightfieldDebugData {
    std::shared_ptr<MemoryBuffer> vb;
    std::shared_ptr<IndexBuffer> ib;
    uint32_t index_count = 0;
    glm::mat4 model = glm::mat4(1.0f);
  };
  std::vector<HeightfieldDebugData> hf_cache_;
  bool hf_cache_valid_ = false;
};

}  // namespace Wiesel
