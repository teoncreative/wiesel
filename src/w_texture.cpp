//   Copyright 2023 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0

#include "w_texture.h"
#include "w_renderer.h"

namespace Wiesel {

	Texture::Texture(TextureType textureType, const std::string& path) : m_TextureType(textureType), m_Path(path)  {
		m_Width = 0;
		m_Height = 0;
		m_Size = 0;
		m_Allocated = false;
		m_MipLevels = 1;
	}

	Texture::~Texture() {
        switch (m_TextureType) {
            case TextureTypeDepthStencil: {
                Renderer::GetRenderer()->DestroyDepthStencil(*this);
                break;
            }
            case TextureTypeTexture: {
                Renderer::GetRenderer()->DestroyTexture(*this);
                break;
            }
			case TextureTypeColorImage: {
				Renderer::GetRenderer()->DestroyColorImage(*this);
				break;
			}
			default: {
				throw std::runtime_error("texture type is not implemented!");
			}
        }
	}
}