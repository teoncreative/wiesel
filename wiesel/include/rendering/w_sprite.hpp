//
// Created by Metehan Gezer on 23/04/2025.
//

#ifndef WIESEL_SPRITE_H
#define WIESEL_SPRITE_H

#include "w_pch.hpp"
#include "rendering/w_descriptor.hpp"
#include "rendering/w_buffer.hpp"
#include "util/w_utils.hpp"
#include "w_sampler.hpp"
#include "w_texture.hpp"
#include "scene/w_components.hpp"

namespace Wiesel {

struct SpriteTexture {
  VkImage Image;
  VkDeviceMemory DeviceMemory;
  VkFormat Format;
  glm::ivec2 Size;
  uint32_t DataLength;
  VkImageAspectFlags AspectFlags;

  ~SpriteTexture();
};

Ref<SpriteTexture> LoadSpriteTexture(const std::vector<std::string>& paths);

enum SpriteType {
  SpriteTypeAtlas,
  SpriteTypeArray,
};

class SpriteAsset {
 public:
  SpriteAsset() = default;
  ~SpriteAsset();

  void UpdateTransform(glm::mat4 transform_matrix);

 private:
  friend class Renderer;
  friend class SpriteBuilder;

  struct Frame {
    glm::vec4 uv_rect;
    uint32_t instance_id;
    float_t duration;
    Ref<ImageView> view;
    Ref<DescriptorSet> descriptor;
    Ref<MemoryBuffer> vertex_buffer;
    Ref<UniformBuffer> uniform_buffer;

    Frame(const glm::vec4 &uv, float_t d = 0.0f) : uv_rect(uv), duration(d) {}
  };

  SpriteType type_;
  glm::vec2 atlas_size_;
  Ref<SpriteTexture> texture_;
  Ref<Sampler> sampler_;
  std::vector<Frame> frames_;
  bool is_allocated_ = false;
};

enum class AddFrameResult {
  Success,
  UVSizeShouldBeLargerThanZero
};

class SpriteBuilder {
 public:
  SpriteBuilder(const std::string& atlas_path, glm::vec2 atlas_size) : atlas_path_(atlas_path), atlas_size_(atlas_size) {

  }

  void SetFixedSize(glm::vec2 size) {
    fixed_uv_size_ = size;
    fixed_size_ = true;
  }

  AddFrameResult AddFrame(float_t durationSeconds, glm::vec2 uvPos, glm::vec2 uvSize = {0, 0});

  void SetSampler(Ref<Sampler> sampler) {
      sampler_ = sampler;
  }

  Ref<SpriteAsset> Build();
 private:
  bool fixed_size_ = false;
  std::string atlas_path_;
  glm::vec2 atlas_size_;
  glm::vec2 fixed_uv_size_;
  Ref<Sampler> sampler_;
  std::vector<SpriteAsset::Frame> frames_;
};

class SpriteComponent {
 public:
  friend class Renderer;
  Ref<SpriteAsset> asset_handle_;
  // TODO
  glm::vec2 pivot_;
  glm::vec4 tint_;
  uint32_t current_frame_ = 0;
  float_t frame_timer_ = 0.0f;
  bool flip_x_ = false, flip_y_ = false;
  uint8_t sort_layer_ = 0;

};


}

#endif  //WIESEL_SPRITE_H
