//
// Created by Metehan Gezer on 24/04/2025.
//

#include "rendering/w_sprite.hpp"
#include "w_engine.hpp"

namespace Wiesel {

SpriteTexture::~SpriteTexture() {
}

SpriteAsset::~SpriteAsset() {

}

Ref<SpriteTexture> LoadSpriteTexture(const std::vector<std::string>& paths) {
  Ref<SpriteTexture> texture = CreateReference<SpriteTexture>();

  struct ImageEntry {
    int w, h, channels;
    stbi_uc* pixels;
  };
  int wMax = 0;
  int hMax = 0;
  int channels = 0;
  std::vector<ImageEntry> list;
  list.reserve(paths.size());
  for (size_t i = 0; i < paths.size(); ++i) {
    ImageEntry entry;
    entry.pixels =
        stbi_load(paths[i].c_str(), &entry.w, &entry.h, &entry.channels, STBI_rgb_alpha);
    if (!entry.pixels) {
      throw std::runtime_error("failed to load texture image: " + paths[i]);
    }
    if (i != 0 && entry.channels != channels) {
      throw std::runtime_error("failed to load texture image: " + paths[i]);
    }
    list.push_back(entry);
    wMax = std::max(entry.w, wMax);
    hMax = std::max(entry.h, hMax);
    channels = entry.channels;
  }

  VkDeviceSize totalSize;
  stbi_uc* allPixels;

  texture->Size.x = wMax;
  texture->Size.y = hMax;
  texture->DataLength = texture->Size.x * texture->Size.y * STBI_rgb_alpha;
  totalSize = texture->DataLength * paths.size();
  allPixels = new stbi_uc[totalSize];

  for (int i = 0; i < list.size(); ++i) {
    const ImageEntry& data = list[i];
    memcpy(allPixels + i * texture->DataLength, data.pixels, texture->DataLength);
    stbi_image_free(data.pixels);
  }
  list.clear();

  Ref<Renderer> renderer = Engine::GetRenderer();
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  renderer->CreateBuffer(totalSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               stagingBuffer, stagingBufferMemory);

  void* data;
  vkMapMemory(renderer->GetLogicalDevice(), stagingBufferMemory, 0, totalSize, 0, &data);
  memcpy(data, allPixels, static_cast<size_t>(totalSize));
  vkUnmapMemory(renderer->GetLogicalDevice(), stagingBufferMemory);

  renderer->CreateImage(texture->Size.x, texture->Size.y, 1,
              VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8G8B8A8_UNORM,
              VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                  VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture->Image,
              texture->DeviceMemory, paths.size() == 1 ? 0 : VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT, paths.size());

    renderer->TransitionImageLayout(
        texture->Image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, 0, paths.size());
  for (uint32_t layer = 0; layer < paths.size(); layer++) {
    renderer->CopyBufferToImage(stagingBuffer, texture->Image,
                      static_cast<uint32_t>(texture->Size.x),
                      static_cast<uint32_t>(texture->Size.y),
                      texture->DataLength * layer, layer);
  }

  vkDestroyBuffer(renderer->GetLogicalDevice(), stagingBuffer, nullptr);
  vkFreeMemory(renderer->GetLogicalDevice(), stagingBufferMemory, nullptr);
  delete[] allPixels;

  renderer->TransitionImageLayout(texture->Image, VK_FORMAT_R8G8B8A8_UNORM,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                  1, 0, paths.size());
  return texture;
}

void SpriteAsset::UpdateTransform(glm::mat4 transform_matrix) {
  if (!is_allocated_) { [[unlikely]]
    return;
  }

  for (const auto& item : frames_) {
    SpriteUniformData matrices{};
    matrices.ModelMatrix = transform_matrix;
    memcpy(item.uniform_buffer->data_, &matrices, sizeof(SpriteUniformData));
  }
}

AddFrameResult SpriteBuilder::AddFrame(float_t durationSeconds, glm::vec2 uvPos, glm::vec2 uvSize) {
  if (fixed_size_) {
    uvSize = fixed_uv_size_;
  }
  if (uvSize.x <= 0 || uvSize.y <= 0) {
    return AddFrameResult::UVSizeShouldBeLargerThanZero;
  }
  frames_.emplace_back(
      glm::vec4{
          uvPos,
          uvSize
      },
      durationSeconds
  );

  return AddFrameResult::Success;
}

Ref<SpriteAsset> SpriteBuilder::Build() {
  Ref<SpriteAsset> asset = CreateReference<SpriteAsset>();
  asset->type_ = SpriteTypeAtlas;
  asset->frames_ = frames_;
  asset->atlas_size_ = atlas_size_;
  asset->texture_ = LoadSpriteTexture({atlas_path_});
  asset->sampler_ = sampler_ ? sampler_ : Engine::GetRenderer()->GetDefaultLinearSampler();
  Ref<ImageView> view = Engine::GetRenderer()->CreateImageView(
      asset->texture_->Image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT,
      1, VK_IMAGE_VIEW_TYPE_2D, 0, 1);
  for (SpriteAsset::Frame& item : asset->frames_) {
    item.view = view;
    /*item.VertexBuffer = Engine::GetRenderer()->CreateVertexBuffer(std::vector<VertexSprite>{
        {{item.UVRect.x, item.UVRect.y}},
        {{item.UVRect.x + item.UVRect.z, item.UVRect.y}},
        {{item.UVRect.x, item.UVRect.y + item.UVRect.w}},
        {{item.UVRect.x + item.UVRect.z, item.UVRect.y + item.UVRect.w}},
    }); */
    float u0 = item.uv_rect.x           / atlas_size_.x; // left
    float v0 = item.uv_rect.y           / atlas_size_.y; // bottom
    float u1 = (item.uv_rect.x + item.uv_rect.z) / atlas_size_.x; // right
    float v1 = (item.uv_rect.y + item.uv_rect.w) / atlas_size_.y; // top

    std::vector<VertexSprite> uvs = {
        {{u0, v0}},   // UV for vertex 0 (bottom-left)
        {{u1, v0}},   //    "     1 (bottom-right)
        {{u1, v1}},   //    "     2 (top-right)

        {{u0, v0}},   //    "     3 (bottom-left again)
        {{u1, v1}},   //    "     4 (top-right)
        {{u0, v1}},   //    "     5 (top-left)
    };

    item.vertex_buffer = Engine::GetRenderer()->CreateVertexBuffer(uvs);
    item.uniform_buffer = Engine::GetRenderer()->CreateUniformBuffer(
        sizeof(SpriteUniformData));
    item.descriptor = CreateReference<DescriptorSet>();
    item.descriptor->SetLayout(Engine::GetRenderer()->GetSpriteDrawDescriptorLayout());
    item.descriptor->AddCombinedImageSampler(0, view, asset->sampler_);
    item.descriptor->AddUniformBuffer(1, item.uniform_buffer);
    item.descriptor->Bake();
  }
  asset->is_allocated_ = true;
  return asset;
}
}