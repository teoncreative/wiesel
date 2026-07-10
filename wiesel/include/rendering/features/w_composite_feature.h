
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

namespace wiesel {

class AttachmentTexture;
class DescriptorSet;

class CompositeFeature : public RenderFeature {
 public:
  explicit CompositeFeature(std::shared_ptr<Renderer> renderer);

  const std::string& GetName() const override { return name_; }

  void SetupResources(RenderContext& ctx) override;
  void AddPasses(RenderGraph& graph, RenderResourceRegistry& registry,
                 RenderContext& ctx) override;

 private:
  static inline std::string name_ = "Composite";
  std::shared_ptr<Renderer> renderer_;
  std::shared_ptr<Pipeline> pipeline_;

  std::shared_ptr<DescriptorSet> output_desc_;
  AttachmentTexture* resolve_key_ = nullptr;
};

}  // namespace wiesel
