
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

struct FxaaPushConstants {
  glm::vec2 inverse_screen_size;
};

class FXAAFeature : public RenderFeature {
 public:
  explicit FXAAFeature(Ref<Renderer> renderer);

  const std::string& GetName() const override { return name_; }
  void SetupResources(RenderContext& ctx) override;
  void AddPasses(RenderGraph& graph, RenderResourceRegistry& registry,
                 RenderContext& ctx) override;
  bool IsEnabled(const RenderContext& ctx) const override;

 private:
  static inline std::string name_ = "FXAA";
  Ref<Renderer> renderer_;
  Ref<RenderPass> render_pass_;
  Ref<Pipeline> pipeline_;
  Ref<FxaaPushConstants> push_constants_;
};

}  // namespace Wiesel
