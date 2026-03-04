
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

namespace Wiesel {

struct CanvasScreenPushConstant {
  glm::vec2 screen_size;
};

struct alignas(16) CanvasElementUniformData {
  alignas(8) glm::vec2 position;
  alignas(8) glm::vec2 size;
  alignas(16) glm::vec4 color;
  alignas(16) glm::vec4 uv_rect;
};

class CanvasFeature : public RenderFeature {
 public:
  explicit CanvasFeature(Ref<Renderer> renderer);

  const std::string& GetName() const override { return name_; }
  void SetupResources(RenderContext& ctx) override;
  void AddPasses(RenderGraph& graph, RenderResourceRegistry& registry,
                 RenderContext& ctx) override;

  Ref<DescriptorSetLayout> GetElementLayout() const {
    return canvas_element_layout_;
  }
  Ref<DescriptorSetLayout> GetTexturedLayout() const {
    return canvas_textured_layout_;
  }

 private:
  static inline std::string name_ = "Canvas";
  Ref<Renderer> renderer_;
  Ref<RenderPass> render_pass_;
  Ref<Pipeline> rect_pipeline_;
  Ref<Pipeline> image_pipeline_;
  Ref<Pipeline> text_pipeline_;
  Ref<DescriptorSetLayout> canvas_element_layout_;
  Ref<DescriptorSetLayout> canvas_textured_layout_;
  Ref<CanvasScreenPushConstant> screen_size_push_;

  // Second pass: composite canvas onto final PipelineOutput
  Ref<RenderPass> comp_render_pass_;
  Ref<Pipeline> comp_pipeline_;
};

}  // namespace Wiesel
