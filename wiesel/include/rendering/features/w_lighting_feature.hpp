
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

class LightingFeature : public RenderFeature {
 public:
  explicit LightingFeature(Ref<Renderer> renderer);

  const std::string& GetName() const override { return name_; }
  void SetupResources(RenderContext& ctx) override;
  void AddPasses(RenderGraph& graph, RenderResourceRegistry& registry,
                 RenderContext& ctx) override;

  Ref<RenderPass> GetRenderPass() const { return render_pass_; }
  Ref<Pipeline> GetLightingPipeline() const { return lighting_pipeline_; }
  Ref<Pipeline> GetSkyboxPipeline() const { return skybox_pipeline_; }

 private:
  static inline std::string name_ = "Lighting";
  Ref<Renderer> renderer_;
  Ref<RenderPass> render_pass_;
  Ref<Pipeline> lighting_pipeline_;
  Ref<Pipeline> rt_lighting_pipeline_;
  Ref<DescriptorSetLayout> rt_shadow_desc_layout_;
  Ref<Pipeline> skybox_pipeline_;
};

}  // namespace Wiesel
