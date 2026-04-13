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

namespace Wiesel {

struct BloomExtractPushConstants {
  float threshold;
  float clamp_value;
};

struct BloomBlurPushConstants {
  float radius;
};

struct BloomCompositePushConstants {
  glm::vec4 tint_intensity;  // rgb=tint, a=intensity
};

class BloomFeature : public RenderFeature {
 public:
  explicit BloomFeature(std::shared_ptr<Renderer> renderer);

  const std::string& GetName() const override { return name_; }

  void SetupResources(RenderContext& ctx) override;
  void AddPasses(RenderGraph& graph, RenderResourceRegistry& registry,
                 RenderContext& ctx) override;
  bool IsEnabled(const RenderContext& ctx) const override;

 private:
  static inline std::string name_ = "Bloom";
  std::shared_ptr<Renderer> renderer_;
  std::shared_ptr<RenderPass> render_pass_;

  std::shared_ptr<Pipeline> extract_pipeline_;
  std::shared_ptr<Pipeline> blur_h_pipeline_;
  std::shared_ptr<Pipeline> blur_v_pipeline_;
  std::shared_ptr<Pipeline> composite_pipeline_;

  // HQ uses extra blur iterations
  std::shared_ptr<Pipeline> blur_h2_pipeline_;
  std::shared_ptr<Pipeline> blur_v2_pipeline_;

  std::shared_ptr<BloomExtractPushConstants> extract_push_;
  std::shared_ptr<BloomBlurPushConstants> blur_push_;
  std::shared_ptr<BloomCompositePushConstants> composite_push_;
};

}  // namespace Wiesel
