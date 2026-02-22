
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

struct ShadowPipelinePushConstant;

class ShadowFeature : public RenderFeature {
 public:
  explicit ShadowFeature(Ref<Renderer> renderer);

  const std::string& GetName() const override { return name_; }
  void SetupResources(RenderContext& ctx) override;
  void AddPasses(RenderGraph& graph, RenderResourceRegistry& registry,
                 RenderContext& ctx) override;

  Ref<RenderPass> GetRenderPass() const { return render_pass_; }
  Ref<Pipeline> GetPipeline() const { return pipeline_; }

 private:
  static inline std::string name_ = "Shadow";
  Ref<Renderer> renderer_;
  Ref<RenderPass> render_pass_;
  Ref<Pipeline> pipeline_;
  Ref<ShadowPipelinePushConstant> push_constant_;
};

}  // namespace Wiesel
