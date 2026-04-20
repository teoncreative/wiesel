
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

#include "rendering/w_render_feature.h"

namespace wiesel {

class AttachmentTexture;
class DescriptorSet;
class Framebuffer;

class SSAOFeature : public RenderFeature {
 public:
  explicit SSAOFeature(std::shared_ptr<Renderer> renderer);

  const std::string& GetName() const override { return name_; }

  void SetupResources(RenderContext& ctx) override;
  void AddPasses(RenderGraph& graph, RenderResourceRegistry& registry,
                 RenderContext& ctx) override;
  bool IsEnabled(const RenderContext& ctx) const override;

 private:
  static inline std::string name_ = "SSAO";
  std::shared_ptr<Renderer> renderer_;
  std::shared_ptr<RenderPass> gen_render_pass_;
  std::shared_ptr<Pipeline> gen_pipeline_;
  std::shared_ptr<RenderPass> blur_horz_render_pass_;
  std::shared_ptr<Pipeline> blur_horz_pipeline_;
  std::shared_ptr<RenderPass> blur_vert_render_pass_;
  std::shared_ptr<Pipeline> blur_vert_pipeline_;

  // Bindings built on top of pool-assigned transient textures. Cached across
  // frames and invalidated when the pool returns a different texture pointer
  // than last time. Rebuilding happens in the pass resolve_fn after Compile.
  struct GenBindings {
    std::shared_ptr<Framebuffer> framebuffer;
    std::shared_ptr<DescriptorSet> gen_desc;
    AttachmentTexture* color_key = nullptr;
    AttachmentTexture* view_pos_key = nullptr;
    AttachmentTexture* normal_key = nullptr;
    AttachmentTexture* depth_key = nullptr;
  } gen_bindings_;

  struct BlurBindings {
    std::shared_ptr<Framebuffer> framebuffer;
    std::shared_ptr<DescriptorSet> input_desc;
    AttachmentTexture* output_key = nullptr;
    AttachmentTexture* input_key = nullptr;
    AttachmentTexture* depth_key = nullptr;
  } blur_h_bindings_, blur_v_bindings_;

  // Descriptor reading the final SSAO output. Published to the camera
  // resource pool so downstream features (Lighting, Composite) keep reading
  // it by name. Rebuilt when the final output texture changes.
  std::shared_ptr<DescriptorSet> final_output_desc_;
  AttachmentTexture* final_output_key_ = nullptr;
  AttachmentTexture* final_output_depth_key_ = nullptr;
};

}  // namespace wiesel
