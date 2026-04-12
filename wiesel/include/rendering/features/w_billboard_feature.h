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

namespace Wiesel {

struct BillboardPushConstant {
  glm::mat4 mvp;
  glm::vec4 color;
};

struct BillboardVertex {
  glm::vec3 position;
  glm::vec2 uv;
};

class BillboardFeature : public RenderFeature {
 public:
  explicit BillboardFeature(std::shared_ptr<Renderer> renderer);

  const std::string& GetName() const override { return name_; }

  void SetupResources(RenderContext& ctx) override;
  void AddPasses(RenderGraph& graph, RenderResourceRegistry& registry,
                 RenderContext& ctx) override;
  bool IsEnabled(const RenderContext& ctx) const override;

 private:
  void LoadIconTexture(const std::string& name, const std::string& vfs_path);

  static inline std::string name_ = "Billboard";
  std::shared_ptr<Renderer> renderer_;

  // Billboard draw pass
  std::shared_ptr<RenderPass> render_pass_;
  std::shared_ptr<Pipeline> pipeline_;
  std::shared_ptr<BillboardPushConstant> push_constant_;

  // Compositing onto PipelineOutput
  std::shared_ptr<RenderPass> comp_render_pass_;
  std::shared_ptr<Pipeline> comp_pipeline_;
  std::shared_ptr<Pipeline> comp_blend_pipeline_;

  // Quad geometry
  std::shared_ptr<MemoryBuffer> quad_vb_;
  std::shared_ptr<IndexBuffer> quad_ib_;

  // Icon textures and descriptors
  std::shared_ptr<DescriptorSetLayout> icon_desc_layout_;
  std::unordered_map<std::string, std::shared_ptr<Texture>> icon_textures_;
  std::unordered_map<std::string, std::shared_ptr<DescriptorSet>>
      icon_descriptors_;
};

}  // namespace Wiesel