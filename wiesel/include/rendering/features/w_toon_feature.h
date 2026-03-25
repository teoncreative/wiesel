
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

struct ToonPushConstants {
  int bands = 4;
  float edge_threshold = 0.1f;
  float edge_strength = 0.8f;
};

class ToonFeature : public RenderFeature {
 public:
  explicit ToonFeature(std::shared_ptr<Renderer> renderer);

  const std::string& GetName() const override { return name_; }

  void SetupResources(RenderContext& ctx) override;
  void AddPasses(RenderGraph& graph, RenderResourceRegistry& registry,
                 RenderContext& ctx) override;
  bool IsEnabled(const RenderContext& ctx) const override;

  ToonPushConstants& GetParams() { return *push_constants_; }

 private:
  static inline std::string name_ = "Toon";
  std::shared_ptr<Renderer> renderer_;
  std::shared_ptr<RenderPass> render_pass_;
  std::shared_ptr<DescriptorSetLayout> toon_input_layout_;
  std::shared_ptr<Pipeline> pipeline_;
  std::shared_ptr<ToonPushConstants> push_constants_;
};

}  // namespace Wiesel
