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
#include "rendering/w_skybox.h"

namespace wiesel {

class IBLFeature : public RenderFeature {
 public:
  explicit IBLFeature(std::shared_ptr<Renderer> renderer);
  ~IBLFeature();

  const std::string& GetName() const override { return name_; }

  void SetupResources(RenderContext& ctx) override;
  void AddPasses(RenderGraph& graph, RenderResourceRegistry& registry,
                 RenderContext& ctx) override;
  bool IsEnabled(const RenderContext& ctx) const override;

  bool HasIBLMaps() const { return maps_generated_; }

 private:
  void GenerateIBLMaps(std::shared_ptr<Texture> env_cubemap);
  void GenerateBRDFLUT();
  void Cleanup();

  static inline std::string name_ = "IBL";
  std::shared_ptr<Renderer> renderer_;

  // BRDF LUT (generated once)
  std::shared_ptr<AttachmentTexture> brdf_lut_;
  bool brdf_generated_ = false;

  // Irradiance cubemap
  std::shared_ptr<AttachmentTexture> irradiance_map_;

  // Prefiltered environment cubemap
  std::shared_ptr<AttachmentTexture> prefilter_map_;

  // IBL descriptor
  std::shared_ptr<DescriptorSet> ibl_descriptor_;

  // Track which skybox we generated for
  Texture* last_env_texture_ = nullptr;
  bool maps_generated_ = false;

  static constexpr uint32_t kBRDFLUTSize = 512;
  static constexpr uint32_t kIrradianceSize = 32;
  static constexpr uint32_t kPrefilterSize = 128;
  static constexpr uint32_t kPrefilterMipLevels = 5;
};

}  // namespace wiesel