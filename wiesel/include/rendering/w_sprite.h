//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

//
// Created by Metehan Gezer on 23/04/2025.
//

#ifndef WIESEL_SPRITE_H
#define WIESEL_SPRITE_H

#include "asset/w_asset_handle.h"
#include "core/w_reflect.h"
#include "rendering/w_buffer.h"
#include "rendering/w_descriptor.h"
#include "scene/w_components.h"
#include "util/w_utils.h"
#include "w_pch.h"
#include "w_sampler.h"
#include "w_texture.h"

namespace wiesel {

// GPU resources for a single .wsprite, shared across all entities using it.
// Per-entity UBO and descriptor are on SpriteRendererComponent instead.
struct SpriteGpuData {
  std::shared_ptr<ImageView> view;
  std::shared_ptr<Sampler> sampler;
  std::shared_ptr<MemoryBuffer> vertex_buffer;  // 6 UV verts
  glm::vec2 pixel_size = {0, 0};  // width, height for aspect ratio
};

// Displays a single .wsprite asset.
WCLASS()

class SpriteRendererComponent {
 public:
  friend class Renderer;

  WPROPERTY(Serializable, Animatable)
  AssetHandle sprite_handle_;  // -> .wsprite
  WPROPERTY(Serializable)
  glm::vec2 pivot_ = {0.5f, 0.5f};
  WPROPERTY(Serializable, Animatable)
  glm::vec4 tint_ = {1, 1, 1, 1};
  WPROPERTY(Serializable)
  bool flip_x_ = false;
  WPROPERTY(Serializable)
  bool flip_y_ = false;
  WPROPERTY(Serializable)
  uint8_t sort_layer_ = 0;

  // Per-instance GPU resources (lazily allocated by renderer)
  std::shared_ptr<UniformBuffer> ubo_;
  std::shared_ptr<DescriptorSet> descriptor_;
  AssetHandle bound_sprite_;  // tracks which sprite the descriptor is built for
  std::shared_ptr<SpriteGpuData> gpu_data_;  // cached from asset manager
};


}  // namespace wiesel

#endif  //WIESEL_SPRITE_H
