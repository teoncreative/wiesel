//
// Created by Metehan Gezer on 23/04/2025.
//

#ifndef WIESEL_SPRITE_H
#define WIESEL_SPRITE_H

#include <w_engine.hpp>
#include "rendering/w_descriptor.hpp"
#include "util/w_utils.hpp"
#include "w_pch.hpp"
#include "w_sampler.hpp"
#include "w_texture.hpp"

namespace Wiesel {

struct SpriteTexture {
  VkImage Image;
  Ref<ImageView> View;
  VkDeviceMemory DeviceMemory;
  VkFormat Format;
  Ref<Sampler> Sampler;
  glm::ivec2 Size;
  uint32_t DataLength;
  VkImageAspectFlags AspectFlags;

  ~SpriteTexture();
};

Ref<SpriteTexture> LoadSpriteTexture(const std::vector<std::string>& paths, Ref<Sampler> sampler);

enum SpriteType {
  SpriteTypeAtlas,
  SpriteTypeArray,
};

class SpriteAsset {
 public:
  SpriteAsset() = default;
  ~SpriteAsset();

 private:
  friend class Renderer;
  friend class SpriteBuilderAtlas;
  friend class SpriteBuilderArray;

  struct Frame {
    union {
      uint32_t LayerIndex;    // for SpriteTypeArray
      glm::vec4 UVRect;       // for SpriteTypeAtlas
    };
    float_t Duration;

    Frame(uint32_t idx, float_t d = 0.0f) : LayerIndex(idx), Duration(d) {}
    Frame(const glm::vec4 &uv, float_t d = 0.0f) : UVRect(uv), Duration(d) {}
  };

  SpriteType m_Type;
  glm::vec2 m_AtlasSize;
  Ref<SpriteTexture> m_Texture;
  Ref<DescriptorSet> m_Descriptors;
  std::vector<Frame> m_Frames;
};

enum class AddFrameResult {
  Success,
  UVSizeShouldBeLargerThanZero
};

class SpriteBuilderAtlas {
 public:
  SpriteBuilderAtlas(const std::string& atlasPath, glm::vec2 atlasSize) : m_AtlasPath(atlasPath), m_AtlasSize(atlasSize) {

  }

  void SetFixedSize(glm::vec2 size) {
    m_FixedUVSize = size;
    m_FixedSize = true;
  }

  AddFrameResult AddFrame(float_t durationSeconds, glm::vec2 uvPos, glm::vec2 uvSize = {0, 0}) {
    if (m_FixedSize) {
      uvSize = m_FixedUVSize;
    }
    if (uvSize.x <= 0 || uvSize.y <= 0) {
      return AddFrameResult::UVSizeShouldBeLargerThanZero;
    }
    m_Frames.emplace_back(
        glm::vec4{
            uvPos,
            uvSize
        },
        durationSeconds
    );
    return AddFrameResult::Success;
  }

  void SetSampler(Ref<Sampler> sampler) {
      m_Sampler = sampler;
  }

  Ref<SpriteAsset> Build() {
    Ref<SpriteAsset> asset = CreateReference<SpriteAsset>();
    asset->m_Type = SpriteTypeAtlas;
    asset->m_Frames = m_Frames;
    asset->m_AtlasSize = m_AtlasSize;
    asset->m_Texture = LoadSpriteTexture({m_AtlasPath},
                                         m_Sampler ? m_Sampler : Engine::GetRenderer()->GetDefaultLinearSampler());
    // Todo descriptors
    return asset;
  }
 private:
  bool m_FixedSize;
  std::string m_AtlasPath;
  glm::vec2 m_AtlasSize;
  glm::vec2 m_FixedUVSize;
  Ref<Sampler> m_Sampler;
  std::vector<SpriteAsset::Frame> m_Frames;
};

class SpriteBuilderArray {
 public:


 private:
};

class SpriteComponent {
 public:

 private:
  Ref<SpriteAsset> m_AssetHandle;
  uint32_t m_CurrentFrame;
  float_t m_FrameTimer;
  glm::vec2 m_Pivot;
  glm::vec4 m_Tint;
  bool m_FlipX, m_FlipY;
  uint8_t m_SortLayer;

};


}

#endif  //WIESEL_SPRITE_H
