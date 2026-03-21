//
// Created by Metehan Gezer on 23/04/2025.
//

#ifndef WIESEL_SPRITE_H
#define WIESEL_SPRITE_H

#include "animation/w_state_machine.hpp"
#include "asset/w_asset_handle.hpp"
#include "rendering/w_buffer.hpp"
#include "rendering/w_descriptor.hpp"
#include "scene/w_components.hpp"
#include "util/w_utils.hpp"
#include "w_pch.hpp"
#include "w_sampler.hpp"
#include "w_texture.hpp"

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

std::shared_ptr<SpriteTexture> LoadSpriteTexture(const std::vector<std::string>& paths);

enum SpriteType {
  SpriteTypeAtlas,
  SpriteTypeArray,
};

class SpriteAsset {
 public:
  SpriteAsset() = default;
  ~SpriteAsset();

  void UpdateTransform(glm::mat4 transform_matrix);
  void UpdateTransform(glm::mat4 transform_matrix,
                       const glm::vec4& tint,
                       bool flip_x, bool flip_y,
                       int frame_index = -1);

  struct Frame {
    glm::vec4 uv_rect;
    uint32_t instance_id;
    float_t duration;
    std::shared_ptr<ImageView> view;
    std::shared_ptr<DescriptorSet> descriptor;
    std::shared_ptr<MemoryBuffer> vertex_buffer;
    std::shared_ptr<UniformBuffer> uniform_buffer;

    Frame(const glm::vec4 &uv, float_t d = 0.0f) : uv_rect(uv), duration(d) {}
  };

  const std::shared_ptr<SpriteTexture>& GetTexture() const { return texture_; }
  const std::shared_ptr<Sampler>& GetSampler() const { return sampler_; }
  const std::vector<Frame>& GetFrames() const { return frames_; }
  std::vector<Frame>& GetFrames() { return frames_; }
  bool IsAllocated() const { return is_allocated_; }

 private:
  friend class Renderer;
  friend class SpriteBuilder;

  SpriteType type_;
  glm::vec2 atlas_size_;
  std::shared_ptr<SpriteTexture> texture_;
  std::shared_ptr<Sampler> sampler_;
  std::vector<Frame> frames_;
  bool is_allocated_ = false;
};

enum class AddFrameResult {
  Success,
  UVSizeShouldBeLargerThanZero
};

class SpriteBuilder {
 public:
  // Single atlas image with known size
  SpriteBuilder(const std::string& virtual_atlas_path, glm::vec2 atlas_size)
      : virtual_atlas_path_(virtual_atlas_path), atlas_size_(atlas_size) {
  }

  // Multiple individual images — will be stitched into a horizontal strip atlas
  SpriteBuilder(const std::vector<std::string>& frame_paths)
      : multi_frame_paths_(frame_paths) {
  }

  void SetFixedSize(glm::vec2 size) {
    fixed_uv_size_ = size;
    fixed_size_ = true;
  }

  AddFrameResult AddFrame(float_t duration_seconds, glm::vec2 uv_pos, glm::vec2 uv_size = {0, 0});

  // Auto-slice a grid: adds count frames starting from (start_col, start_row),
  // scanning left-to-right, top-to-bottom. cell_size is in pixels.
  void AddGridFrames(glm::ivec2 cell_size, int start_col, int start_row,
                     int count, float frame_duration);

  void SetSampler(std::shared_ptr<Sampler> sampler) {
      sampler_ = sampler;
  }

  std::shared_ptr<SpriteAsset> Build();
 private:
  bool fixed_size_ = false;
  std::string virtual_atlas_path_;
  glm::vec2 atlas_size_;
  glm::vec2 fixed_uv_size_;
  std::shared_ptr<Sampler> sampler_;
  std::vector<SpriteAsset::Frame> frames_;
  std::vector<std::string> multi_frame_paths_;  // for multi-image stitching
};

// A named range of frames within a SpriteAsset.
// Each clip maps to an AnimationState in the state machine via clip_name.
struct SpriteClip {
  std::string name;           // matches AnimationState::clip_name
  uint32_t start_frame = 0;
  uint32_t frame_count = 1;
  float frame_duration = 0.1f;
  bool loop = true;
};

class SpriteComponent {
 public:
  friend class Renderer;

  // The sprite sheet asset
  std::shared_ptr<SpriteAsset> asset_;
  AssetHandle asset_handle_;  // handle to .wspritesheet or .wspriteanim

  // Visual properties
  glm::vec2 pivot_ = {0.5f, 0.5f};
  glm::vec4 tint_ = {1, 1, 1, 1};
  bool flip_x_ = false;
  bool flip_y_ = false;
  uint8_t sort_layer_ = 0;

  // Animation clips (frame ranges)
  std::vector<SpriteClip> clips;

  // State machine (optional - if empty, just plays clips directly)
  StateMachineRuntime state_machine;

  // Playback state
  uint32_t current_frame_ = 0;
  float_t frame_timer_ = 0.0f;
  bool playing_ = true;

  // Play a clip by name directly (bypasses state machine)
  void Play(const std::string& clip_name, bool restart = true);
  void Stop();

  // Get clip by name
  const SpriteClip* FindClip(const std::string& name) const;

  // Get the currently active clip (from state machine or direct play)
  const SpriteClip* GetActiveClip() const;

 private:
  std::string direct_clip_;  // set by Play() when not using state machine
};


}

#endif  //WIESEL_SPRITE_H
