
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
  explicit LightingFeature(std::shared_ptr<Renderer> renderer);

  const std::string& GetName() const override { return name_; }
  void SetupResources(RenderContext& ctx) override;
  void AddPasses(RenderGraph& graph, RenderResourceRegistry& registry,
                 RenderContext& ctx) override;

  std::shared_ptr<RenderPass> GetRenderPass() const { return render_pass_; }
  std::shared_ptr<Pipeline> GetLightingPipeline() const { return lighting_pipeline_; }
  std::shared_ptr<Pipeline> GetSkyboxPipeline() const { return skybox_pipeline_; }

 private:
  static inline std::string name_ = "Lighting";
  std::shared_ptr<Renderer> renderer_;
  std::shared_ptr<RenderPass> render_pass_;
  std::shared_ptr<Pipeline> lighting_pipeline_;
  std::shared_ptr<Pipeline> rt_lighting_pipeline_;
  std::shared_ptr<DescriptorSetLayout> rt_shadow_desc_layout_;
  std::shared_ptr<Pipeline> skybox_pipeline_;
};

}  // namespace Wiesel
