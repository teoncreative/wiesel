
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
#include "rendering/w_rt_pipeline.hpp"
#include "rendering/w_acceleration_structure.hpp"

namespace Wiesel {

static constexpr int kMaxRTShadowLights = 32;

struct alignas(16) RTShadowLight {
  glm::vec4 pos_or_dir;  // xyz = direction/position, w = 0 for dir, 1 for point
  glm::vec4 params;      // x = range (point lights), yzw = unused
};

struct RTShadowLightUBO {
  RTShadowLight lights[kMaxRTShadowLights];
  alignas(16) int count;
};

class RTShadowFeature : public RenderFeature {
 public:
  explicit RTShadowFeature(Ref<Renderer> renderer);

  const std::string& GetName() const override { return name_; }
  void SetupResources(RenderContext& ctx) override;
  void AddPasses(RenderGraph& graph, RenderResourceRegistry& registry,
                 RenderContext& ctx) override;
  bool IsEnabled(const RenderContext& ctx) const override;

 private:
  static inline std::string name_ = "RTShadow";
  Ref<Renderer> renderer_;
  Ref<RTPipeline> rt_pipeline_;
  Ref<DescriptorSetLayout> rt_descriptor_layout_;
  Ref<UniformBuffer> shadow_lights_ubo_;
  uint32_t mask_width_ = 0;
  uint32_t mask_height_ = 0;
};

}  // namespace Wiesel