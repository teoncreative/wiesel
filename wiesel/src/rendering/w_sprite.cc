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
// Created by Metehan Gezer on 24/04/2025.
//

#include "rendering/w_sprite.h"
#include "w_engine.h"

namespace Wiesel {

SpriteTexture::~SpriteTexture() {}

SpriteAsset::~SpriteAsset() {}

std::shared_ptr<SpriteTexture> LoadSpriteTexture(
    const std::vector<std::string>& paths) {
  std::shared_ptr<SpriteTexture> texture = std::make_shared<SpriteTexture>();

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
    VfsFile vfs_sprite = Engine::vfs()->Open(paths[i]);
    entry.pixels = stbi_load_from_memory(
        vfs_sprite.Data(), static_cast<int>(vfs_sprite.Size()), &entry.w,
        &entry.h, &entry.channels, STBI_rgb_alpha);
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
    memcpy(allPixels + i * texture->DataLength, data.pixels,
           texture->DataLength);
    stbi_image_free(data.pixels);
  }
  list.clear();

  std::shared_ptr<Renderer> renderer = Engine::renderer();
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  renderer->CreateBuffer(totalSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         stagingBuffer, stagingBufferMemory);

  void* data;
  vkMapMemory(renderer->GetLogicalDevice(), stagingBufferMemory, 0, totalSize,
              0, &data);
  memcpy(data, allPixels, static_cast<size_t>(totalSize));
  vkUnmapMemory(renderer->GetLogicalDevice(), stagingBufferMemory);

  renderer->CreateImage(
      texture->Size.x, texture->Size.y, 1, SamplingMode::DISABLED,
      VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
          VK_IMAGE_USAGE_SAMPLED_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture->Image,
      texture->DeviceMemory,
      paths.size() == 1 ? 0 : VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT,
      paths.size());

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
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1,
                                  0, paths.size());
  return texture;
}

void SpriteAsset::UpdateTransform(glm::mat4 transform_matrix) {
  UpdateTransform(transform_matrix, {1, 1, 1, 1}, false, false, -1);
}

void SpriteAsset::UpdateTransform(glm::mat4 transform_matrix,
                                  const glm::vec4& tint, bool flip_x,
                                  bool flip_y, int frame_index) {
  if (!is_allocated_) {
    return;
  }

  if (frame_index >= 0 && frame_index < static_cast<int>(frames_.size())) {
    // Update only the specified frame
    auto& item = frames_[frame_index];
    SpriteUniformData data{};
    data.model_matrix = transform_matrix;
    data.tint = tint;
    data.flip_x = flip_x ? 1 : 0;
    data.flip_y = flip_y ? 1 : 0;
    memcpy(item.uniform_buffer->data_, &data, sizeof(SpriteUniformData));
  } else {
    // Update all frames (legacy path)
    for (auto& item : frames_) {
      SpriteUniformData data{};
      data.model_matrix = transform_matrix;
      data.tint = tint;
      data.flip_x = flip_x ? 1 : 0;
      data.flip_y = flip_y ? 1 : 0;
      memcpy(item.uniform_buffer->data_, &data, sizeof(SpriteUniformData));
    }
  }
}

void SpriteComponent::Play(const std::string& clip_name, bool restart) {
  if (!restart && direct_clip_ == clip_name && playing_) {
    return;
  }
  direct_clip_ = clip_name;
  frame_timer_ = 0.0f;
  playing_ = true;
  const SpriteClip* clip = FindClip(clip_name);
  if (clip) {
    current_frame_ = clip->start_frame;
  }
}

void SpriteComponent::Stop() {
  playing_ = false;
}

const SpriteClip* SpriteComponent::FindClip(const std::string& name) const {
  for (auto& c : clips) {
    if (c.name == name) {
      return &c;
    }
  }
  return nullptr;
}

const SpriteClip* SpriteComponent::GetActiveClip() const {
  // State machine takes priority
  if (!state_machine.controller.IsEmpty()) {
    const auto* state = state_machine.GetCurrentState();
    if (state) {
      return FindClip(state->clip_name);
    }
  }
  // Direct play
  if (!direct_clip_.empty()) {
    return FindClip(direct_clip_);
  }
  return nullptr;
}

AddFrameResult SpriteBuilder::AddFrame(float_t duration_seconds,
                                       glm::vec2 uv_pos, glm::vec2 uv_size) {
  if (fixed_size_) {
    uv_size = fixed_uv_size_;
  }
  if (uv_size.x <= 0 || uv_size.y <= 0) {
    return AddFrameResult::UVSizeShouldBeLargerThanZero;
  }
  frames_.emplace_back(glm::vec4{uv_pos, uv_size}, duration_seconds);

  return AddFrameResult::Success;
}

void SpriteBuilder::AddGridFrames(glm::ivec2 cell_size, int start_col,
                                  int start_row, int count,
                                  float frame_duration) {
  int cols_per_row = static_cast<int>(atlas_size_.x) / cell_size.x;
  int col = start_col;
  int row = start_row;

  for (int i = 0; i < count; i++) {
    glm::vec2 pos(col * cell_size.x, row * cell_size.y);
    glm::vec2 size(cell_size.x, cell_size.y);
    AddFrame(frame_duration, pos, size);

    col++;
    if (col >= cols_per_row) {
      col = 0;
      row++;
    }
  }
}

std::shared_ptr<SpriteAsset> SpriteBuilder::Build() {
  std::shared_ptr<SpriteAsset> asset = std::make_shared<SpriteAsset>();
  asset->type_ = SpriteTypeAtlas;

  // Multi-image mode: stitch individual frames into a horizontal strip
  if (!multi_frame_paths_.empty()) {
    // Load all images to get dimensions
    struct FrameImage {
      stbi_uc* pixels;
      int w, h;
    };

    std::vector<FrameImage> images;
    int max_h = 0;
    int total_w = 0;

    for (auto& path : multi_frame_paths_) {
      VfsFile file = Engine::vfs()->Open(path);
      if (!file) {
        LOG_ERROR("SpriteBuilder: failed to open frame image: {}", path);
        for (auto& img : images) {
          stbi_image_free(img.pixels);
        }
        return nullptr;
      }
      int w, h, ch;
      stbi_uc* px =
          stbi_load_from_memory(file.Data(), static_cast<int>(file.Size()), &w,
                                &h, &ch, STBI_rgb_alpha);
      if (!px) {
        LOG_ERROR("SpriteBuilder: failed to load frame image: {}", path);
        for (auto& img : images) {
          stbi_image_free(img.pixels);
        }
        return nullptr;
      }
      images.push_back({px, w, h});
      total_w += w;
      if (h > max_h) {
        max_h = h;
      }
    }

    // Create stitched atlas (horizontal strip)
    atlas_size_ = {static_cast<float>(total_w), static_cast<float>(max_h)};
    std::vector<uint8_t> atlas_data(total_w * max_h * 4, 0);

    int x_offset = 0;
    for (size_t i = 0; i < images.size(); i++) {
      auto& img = images[i];
      // Copy rows into atlas
      for (int row = 0; row < img.h; row++) {
        memcpy(atlas_data.data() + (row * total_w + x_offset) * 4,
               img.pixels + row * img.w * 4, img.w * 4);
      }

      // Auto-add frame for this image
      frames_.emplace_back(
          glm::vec4{static_cast<float>(x_offset), 0.0f,
                    static_cast<float>(img.w), static_cast<float>(img.h)},
          0.1f);

      x_offset += img.w;
      stbi_image_free(img.pixels);
    }

    // Upload stitched atlas to GPU
    TextureProps props;
    props.type = TextureTypeDiffuse;
    props.generate_mipmaps = false;
    props.image_format = VK_FORMAT_R8G8B8A8_UNORM;
    props.width = total_w;
    props.height = max_h;

    auto tex =
        Engine::renderer()->CreateTexture(atlas_data.data(), 4, props, {});
    if (!tex) {
      LOG_ERROR("SpriteBuilder: failed to create stitched atlas texture");
      return nullptr;
    }

    // Wrap in a SpriteTexture-like structure
    // We'll use the texture directly and create the image view from it
    asset->texture_ = std::make_shared<SpriteTexture>();
    asset->texture_->Image = tex->image_;
    asset->texture_->DeviceMemory = tex->device_memory_;
    asset->texture_->Size = {total_w, max_h};
    asset->texture_->Format = VK_FORMAT_R8G8B8A8_UNORM;

    // Prevent the Texture destructor from freeing these (SpriteTexture owns them now)
    tex->image_ = VK_NULL_HANDLE;
    tex->device_memory_ = VK_NULL_HANDLE;
    tex->is_allocated_ = false;

    asset->sampler_ =
        sampler_ ? sampler_ : Engine::renderer()->GetDefaultLinearSampler();
  } else {
    // Single atlas mode (original path)
    asset->texture_ = LoadSpriteTexture({virtual_atlas_path_});
  }

  asset->frames_ = frames_;
  asset->atlas_size_ = atlas_size_;
  asset->sampler_ =
      sampler_ ? sampler_ : Engine::renderer()->GetDefaultLinearSampler();
  std::shared_ptr<ImageView> view = Engine::renderer()->CreateImageView(
      asset->texture_->Image, VK_FORMAT_R8G8B8A8_UNORM,
      VK_IMAGE_ASPECT_COLOR_BIT, 1, VK_IMAGE_VIEW_TYPE_2D, 0, 1);
  for (SpriteAsset::Frame& item : asset->frames_) {
    item.view = view;
    /*item.VertexBuffer = Engine::renderer()->CreateVertexBuffer(std::vector<VertexSprite>{
        {{item.UVRect.x, item.UVRect.y}},
        {{item.UVRect.x + item.UVRect.z, item.UVRect.y}},
        {{item.UVRect.x, item.UVRect.y + item.UVRect.w}},
        {{item.UVRect.x + item.UVRect.z, item.UVRect.y + item.UVRect.w}},
    }); */
    float u0 = item.uv_rect.x / atlas_size_.x;                     // left
    float v0 = item.uv_rect.y / atlas_size_.y;                     // bottom
    float u1 = (item.uv_rect.x + item.uv_rect.z) / atlas_size_.x;  // right
    float v1 = (item.uv_rect.y + item.uv_rect.w) / atlas_size_.y;  // top

    std::vector<VertexSprite> uvs = {
        {{u0, v0}},  // UV for vertex 0 (bottom-left)
        {{u1, v0}},  //    "     1 (bottom-right)
        {{u1, v1}},  //    "     2 (top-right)

        {{u0, v0}},  //    "     3 (bottom-left again)
        {{u1, v1}},  //    "     4 (top-right)
        {{u0, v1}},  //    "     5 (top-left)
    };

    item.vertex_buffer = Engine::renderer()->CreateVertexBuffer(uvs);
    item.uniform_buffer =
        Engine::renderer()->CreateUniformBuffer(sizeof(SpriteUniformData));
    item.descriptor = std::make_shared<DescriptorSet>();
    item.descriptor->SetLayout(
        Engine::renderer()->GetDescriptorLayout("SpriteDraw"));
    item.descriptor->AddCombinedImageSampler(0, view, asset->sampler_);
    item.descriptor->AddUniformBuffer(1, item.uniform_buffer);
    item.descriptor->Bake();
  }
  asset->is_allocated_ = true;
  return asset;
}
}  // namespace Wiesel