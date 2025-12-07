
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/w_texture.hpp"

#include "w_engine.hpp"

namespace Wiesel {

Texture::Texture(TextureType texture_type, const std::string& path)
    : type_(texture_type), path_(path) {
  width_ = 0;
  height_ = 0;
  size_ = 0;
  is_allocated_ = false;
  mip_levels_ = 1;
}

Texture::~Texture() {
  Engine::GetRenderer()->DestroyTexture(*this);
}

AttachmentTexture::~AttachmentTexture() {
  Engine::GetRenderer()->DestroyAttachmentTexture(*this);
}

ImageView::~ImageView() {
  vkDestroyImageView(Engine::GetRenderer()->GetLogicalDevice(), handle_, nullptr);
}

}  // namespace Wiesel