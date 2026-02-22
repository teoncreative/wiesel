
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

class TAAFeature : public RenderFeature {
 public:
  explicit TAAFeature(Ref<Renderer> renderer);

  const std::string& GetName() const override { return name_; }
  void SetupResources(RenderContext& ctx) override;
  void AddPasses(RenderGraph& graph, RenderResourceRegistry& registry,
                 RenderContext& ctx) override;
  bool IsEnabled(const RenderContext& ctx) const override;

 private:
  static inline std::string name_ = "TAA";
  Ref<Renderer> renderer_;
  Ref<RenderPass> render_pass_;
  Ref<Pipeline> taa_pipeline_;
  Ref<Pipeline> copy_pipeline_;
};

}  // namespace Wiesel
