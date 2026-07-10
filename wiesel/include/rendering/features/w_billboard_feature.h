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

struct BillboardPushConstant {
  glm::mat4 mvp;
  glm::vec4 color;
  uint32_t entity_id;
  uint32_t padding[3];
};

struct BillboardTextPushConstant {
  glm::mat4 mvp;
  glm::vec4 color;
  glm::vec4 uv_rect;
  uint32_t entity_id;
  uint32_t padding[3];
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
  std::shared_ptr<DescriptorSet> GetOrCreateTextureDescriptor(
      std::shared_ptr<Texture> texture);
  std::shared_ptr<DescriptorSet> GetOrCreateTextAtlasDescriptor(
      std::shared_ptr<ImageView> atlas_view);

  static inline std::string name_ = "Billboard";
  std::shared_ptr<Renderer> renderer_;

  // Billboard draw pass
  std::shared_ptr<Pipeline> pipeline_;             // depth test (occluded hidden)
  std::shared_ptr<Pipeline> pipeline_no_depth_;    // always on top
  std::shared_ptr<Pipeline> pipeline_occluded_;    // depth inverse (faded pass)
  std::shared_ptr<BillboardPushConstant> push_constant_;

  // Text rendering pipelines (one per occlusion mode)
  std::shared_ptr<Pipeline> text_pipeline_;
  std::shared_ptr<Pipeline> text_pipeline_no_depth_;
  std::shared_ptr<Pipeline> text_pipeline_occluded_;
  std::shared_ptr<BillboardTextPushConstant> text_push_constant_;
  std::shared_ptr<DescriptorSetLayout> text_desc_layout_;
  std::unordered_map<void*, std::shared_ptr<DescriptorSet>>
      text_atlas_descriptors_;

  // Compositing onto PipelineOutput
  std::shared_ptr<Pipeline> comp_pipeline_;
  std::shared_ptr<Pipeline> comp_blend_pipeline_;

  // Quad geometry
  std::shared_ptr<MemoryBuffer> quad_vb_;
  std::shared_ptr<IndexBuffer> quad_ib_;

  // Icon textures and descriptors (editor-only gizmos)
  std::shared_ptr<DescriptorSetLayout> icon_desc_layout_;
  std::unordered_map<std::string, std::shared_ptr<Texture>> icon_textures_;
  std::unordered_map<std::string, std::shared_ptr<DescriptorSet>>
      icon_descriptors_;

  // Descriptor cache for user textures (BillboardRendererComponent)
  std::unordered_map<Texture*, std::shared_ptr<DescriptorSet>>
      texture_descriptors_;

  // Transient-pool-backed bindings, rebuilt per-frame when the pool-assigned
  // textures change.
  std::shared_ptr<DescriptorSet> draw_output_desc_;
  AttachmentTexture* draw_color_resolve_key_ = nullptr;
  AttachmentTexture* draw_entity_id_resolve_key_ = nullptr;

  std::shared_ptr<DescriptorSet> comp_input_desc_;
  std::shared_ptr<DescriptorSet> comp_output_desc_;
  AttachmentTexture* comp_output_key_ = nullptr;
  AttachmentTexture* comp_input_key_ = nullptr;
};

}  // namespace wiesel