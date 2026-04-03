
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

class TransparencyFeature : public RenderFeature {
 public:
  explicit TransparencyFeature(std::shared_ptr<Renderer> renderer);

  const std::string& GetName() const override { return name_; }

  void SetupResources(RenderContext& ctx) override;
  void AddPasses(RenderGraph& graph, RenderResourceRegistry& registry,
                 RenderContext& ctx) override;
  bool IsEnabled(const RenderContext& ctx) const override;

 private:
  static inline std::string name_ = "Transparency";
  std::shared_ptr<Renderer> renderer_;
  std::shared_ptr<RenderPass> render_pass_;
  std::shared_ptr<Pipeline> pipeline_;
  std::shared_ptr<Pipeline> ibl_pipeline_;
};

}  // namespace Wiesel
