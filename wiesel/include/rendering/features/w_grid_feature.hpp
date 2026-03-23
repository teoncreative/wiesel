
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

struct GridUniformData {
  alignas(16) glm::mat4 inv_view_projection;
  alignas(16) glm::mat4 view_projection;
  alignas(16) glm::vec4 camera_pos;  // w unused
  float grid_scale = 1.0f;
  float fade_distance = 100.0f;
};

class GridFeature : public RenderFeature {
 public:
  explicit GridFeature(std::shared_ptr<Renderer> renderer);

  const std::string& GetName() const override { return name_; }

  void SetupResources(RenderContext& ctx) override;
  void AddPasses(RenderGraph& graph, RenderResourceRegistry& registry,
                 RenderContext& ctx) override;
  bool IsEnabled(const RenderContext& ctx) const override;

 private:
  static inline std::string name_ = "Grid";
  std::shared_ptr<Renderer> renderer_;
  std::shared_ptr<RenderPass> render_pass_;
  std::shared_ptr<Pipeline> pipeline_;
};

}  // namespace Wiesel