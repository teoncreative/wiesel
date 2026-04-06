
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

#include <entt/entt.hpp>
#include "rendering/w_render_feature.h"

namespace Wiesel {

struct CanvasScreenPushConstant {
  glm::vec2 screen_size;
};

struct CanvasWorldPushConstant {
  glm::mat4 model_matrix;
  glm::vec2 canvas_size;
  glm::vec2 world_size;
};

struct alignas(16) CanvasElementUniformData {
  alignas(8) glm::vec2 position;
  alignas(8) glm::vec2 size;
  alignas(16) glm::vec4 color;
  alignas(16) glm::vec4 uv_rect;
  uint32_t entity_id;
  float premultiplied;  // 1.0 = already premultiplied alpha, 0.0 = standard
  float _pad[2];
};

// Per-canvas offscreen resources for ScreenSpaceCamera canvases
// (and overlay canvases when rendered in the editor scene view).
struct PerCanvasResources {
  std::shared_ptr<AttachmentTexture> texture;
  std::shared_ptr<AttachmentTexture> entity_id_texture;
  std::shared_ptr<Framebuffer> framebuffer;
  std::shared_ptr<DescriptorSet>
      output_descriptor;  // for sampling in 3D quad pass
  uint32_t width = 0;
  uint32_t height = 0;
};

class CanvasFeature : public RenderFeature {
 public:
  explicit CanvasFeature(std::shared_ptr<Renderer> renderer);

  const std::string& GetName() const override { return name_; }

  void SetupResources(RenderContext& ctx) override;
  void AddPasses(RenderGraph& graph, RenderResourceRegistry& registry,
                 RenderContext& ctx) override;

  std::shared_ptr<DescriptorSetLayout> GetElementLayout() const {
    return canvas_element_layout_;
  }

  std::shared_ptr<DescriptorSetLayout> GetTexturedLayout() const {
    return canvas_textured_layout_;
  }

 private:
  static inline std::string name_ = "Canvas";
  std::shared_ptr<Renderer> renderer_;
  std::shared_ptr<RenderPass> render_pass_;
  std::shared_ptr<Pipeline> rect_pipeline_;
  std::shared_ptr<Pipeline> image_pipeline_;
  std::shared_ptr<Pipeline> text_pipeline_;
  std::shared_ptr<DescriptorSetLayout> canvas_element_layout_;
  std::shared_ptr<DescriptorSetLayout> canvas_textured_layout_;
  std::shared_ptr<CanvasScreenPushConstant> screen_size_push_;

  // World-space canvas pipelines (use canvas_world.vert + same frag shaders)
  std::shared_ptr<RenderPass> world_render_pass_;
  std::shared_ptr<Pipeline> world_rect_pipeline_;
  std::shared_ptr<Pipeline> world_image_pipeline_;
  std::shared_ptr<Pipeline> world_text_pipeline_;
  std::shared_ptr<CanvasWorldPushConstant> world_push_;

  // Second pass: composite canvas onto final PipelineOutput
  std::shared_ptr<RenderPass> comp_render_pass_;
  std::shared_ptr<Pipeline> comp_pipeline_;

  // RmlUi offscreen render pass (framebuffers created per-document)
  std::shared_ptr<RenderPass> rmlui_render_pass_;

  // Per-canvas offscreen resources for ScreenSpaceCamera canvases
  // (and overlay canvases when rendering in editor scene view).
  // Keyed by the canvas entity.
  std::unordered_map<entt::entity, PerCanvasResources> per_canvas_resources_;
};

}  // namespace Wiesel
