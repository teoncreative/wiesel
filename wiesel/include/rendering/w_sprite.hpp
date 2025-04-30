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

  void UpdateTransform(glm::mat4 transformMatrix);

 private:
  friend class Renderer;
  friend class SpriteBuilder;

  struct Frame {
    glm::vec4 UVRect;
    uint32_t InstanceId;
    float_t Duration;
    Ref<ImageView> View;
    Ref<DescriptorSet> Descriptor;
    Ref<MemoryBuffer> VertexBuffer;
    Ref<UniformBuffer> UniformBuffer;

    Frame(const glm::vec4 &uv, float_t d = 0.0f) : UVRect(uv), Duration(d) {}
  };

  SpriteType m_Type;
  glm::vec2 m_AtlasSize;
  Ref<SpriteTexture> m_Texture;
  Ref<Sampler> m_Sampler;
  std::vector<Frame> m_Frames;
  bool m_IsAllocated = false;
};

enum class AddFrameResult {
  Success,
  UVSizeShouldBeLargerThanZero
};

class SpriteBuilder {
 public:
  SpriteBuilder(const std::string& atlasPath, glm::vec2 atlasSize) : m_AtlasPath(atlasPath), m_AtlasSize(atlasSize) {

  }

  void SetFixedSize(glm::vec2 size) {
    m_FixedUVSize = size;
    m_FixedSize = true;
  }

  AddFrameResult AddFrame(float_t durationSeconds, glm::vec2 uvPos, glm::vec2 uvSize = {0, 0});

  void SetSampler(Ref<Sampler> sampler) {
      m_Sampler = sampler;
  }

  Ref<SpriteAsset> Build();
 private:
  bool m_FixedSize = false;
  std::string m_AtlasPath;
  glm::vec2 m_AtlasSize;
  glm::vec2 m_FixedUVSize;
  Ref<Sampler> m_Sampler;
  std::vector<SpriteAsset::Frame> m_Frames;
};

class SpriteComponent {
 public:
  friend class Renderer;
  Ref<SpriteAsset> m_AssetHandle;
  // TODO
  glm::vec2 m_Pivot;
  glm::vec4 m_Tint;
  uint32_t m_CurrentFrame = 0;
  float_t m_FrameTimer = 0.0f;
  bool m_FlipX = false, m_FlipY = false;
  uint8_t m_SortLayer = 0;

};


}

#endif  //WIESEL_SPRITE_H
