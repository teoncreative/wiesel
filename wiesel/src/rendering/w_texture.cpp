
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

Texture::Texture(TextureType textureType, const std::string& path)
    : m_Type(textureType), m_Path(path) {
  m_Width = 0;
  m_Height = 0;
  m_Size = 0;
  m_IsAllocated = false;
  m_MipLevels = 1;
}

Texture::~Texture() {
  Engine::GetRenderer()->DestroyTexture(*this);
}

ColorImage::~ColorImage() {
  Engine::GetRenderer()->DestroyColorImage(*this);
}

DepthStencil::~DepthStencil() {
  Engine::GetRenderer()->DestroyDepthStencil(*this);
}
}  // namespace Wiesel