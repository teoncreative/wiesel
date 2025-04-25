//
// Created by Metehan Gezer on 24/04/2025.
//

#include "rendering/w_sprite.hpp"
#include "w_engine.hpp"

namespace Wiesel {

SpriteAsset::~SpriteAsset() {

}

SpriteTexture::~SpriteTexture() {

}

Ref<SpriteTexture> LoadSpriteTexture(const std::vector<std::string>& paths, Ref<Sampler> sampler) {
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
  for (size_t i = 0; i < 6; ++i) {
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
  totalSize = texture->DataLength * 6;
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
              texture->DeviceMemory, VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT, paths.size());

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

  texture->Sampler = sampler;
  texture->View = renderer->CreateImageView(
      texture->Image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT,
      1, VK_IMAGE_VIEW_TYPE_2D_ARRAY, 0, paths.size());
  renderer->TransitionImageLayout(texture->Image, VK_FORMAT_R8G8B8A8_UNORM,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                  1, 0, paths.size());
  return texture;
}
}