
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

class TAAFeature : public RenderFeature {
 public:
  explicit TAAFeature(std::shared_ptr<Renderer> renderer);

  const std::string& GetName() const override { return name_; }

  void SetupResources(RenderContext& ctx) override;
  void AddPasses(RenderGraph& graph, RenderResourceRegistry& registry,
                 RenderContext& ctx) override;
  bool IsEnabled(const RenderContext& ctx) const override;

 private:
  static inline std::string name_ = "TAA";
  std::shared_ptr<Renderer> renderer_;
  std::shared_ptr<RenderPass> render_pass_;
  std::shared_ptr<Pipeline> taa_pipeline_;
  std::shared_ptr<Pipeline> copy_pipeline_;

  // TAA history must survive across frames. Feature-owned; the graph
  // imports it each frame.
  std::shared_ptr<AttachmentTexture> history_texture_;
  uint32_t history_width_ = 0;
  uint32_t history_height_ = 0;

  // TAA pass bindings (reads input + history + depth, writes output).
  std::shared_ptr<Framebuffer> taa_framebuffer_;
  std::shared_ptr<DescriptorSet> taa_input_desc_;
  std::shared_ptr<DescriptorSet> output_desc_;
  AttachmentTexture* taa_output_key_ = nullptr;
  AttachmentTexture* taa_input_key_ = nullptr;
  AttachmentTexture* taa_history_key_ = nullptr;
  AttachmentTexture* taa_depth_key_ = nullptr;

  // History copy pass bindings (reads taa output, writes history).
  std::shared_ptr<Framebuffer> copy_framebuffer_;
  std::shared_ptr<DescriptorSet> copy_input_desc_;
  AttachmentTexture* copy_history_key_ = nullptr;
  AttachmentTexture* copy_input_key_ = nullptr;
};

}  // namespace wiesel
