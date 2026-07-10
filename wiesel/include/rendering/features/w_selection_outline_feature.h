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

#include "rendering/w_descriptor.h"
#include "rendering/w_descriptorlayout.h"
#include "rendering/w_render_feature.h"

namespace wiesel {

struct SelectionMaskPushConstant {
  glm::mat4 mvp;
  uint32_t use_skinning;
  float padding[3];
};

struct SelectionBlurPushConstant {
  float radius;
};

struct SelectionOutlineCompositePushConstant {
  glm::vec4 outline_color;
};

class AttachmentTexture;

class SelectionOutlineFeature : public RenderFeature {
 public:
  explicit SelectionOutlineFeature(std::shared_ptr<Renderer> renderer);

  const std::string& GetName() const override { return name_; }

  void SetupResources(RenderContext& ctx) override;
  void AddPasses(RenderGraph& graph, RenderResourceRegistry& registry,
                 RenderContext& ctx) override;
  bool IsEnabled(const RenderContext& ctx) const override;

 private:
  static inline std::string name_ = "SelectionOutline";
  std::shared_ptr<Renderer> renderer_;

  // Mask pass
  std::shared_ptr<Pipeline> mask_pipeline_;
  std::shared_ptr<SelectionMaskPushConstant> mask_push_constant_;

  // Blur passes (separable H + V)
  std::shared_ptr<Pipeline> blur_h_pipeline_;
  std::shared_ptr<Pipeline> blur_v_pipeline_;
  std::shared_ptr<SelectionBlurPushConstant> blur_push_constant_;

  // Composite pass
  std::shared_ptr<Pipeline> comp_pipeline_;
  std::shared_ptr<DescriptorSetLayout> comp_desc_layout_;
  std::shared_ptr<SelectionOutlineCompositePushConstant> comp_push_constant_;

  // Single-sampler blur stage bindings.
  struct BlurBindings {
    std::shared_ptr<DescriptorSet> input_desc;
    AttachmentTexture* output_key = nullptr;
    AttachmentTexture* input_key = nullptr;
  };
  BlurBindings blur_h_bindings_;
  BlurBindings blur_v_bindings_;

  // Composite reads (scene, blur_v, mask), writes final output.
  std::shared_ptr<DescriptorSet> comp_input_desc_;
  std::shared_ptr<DescriptorSet> output_desc_;
  AttachmentTexture* comp_output_key_ = nullptr;
  AttachmentTexture* comp_scene_key_ = nullptr;
  AttachmentTexture* comp_blur_key_ = nullptr;
  AttachmentTexture* comp_mask_key_ = nullptr;
};

}  // namespace wiesel
