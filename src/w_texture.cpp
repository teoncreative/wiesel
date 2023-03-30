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

	Texture::Texture() {
		m_Width = 0;
		m_Height = 0;
		m_Size = 0;
		m_Allocated = false;
	}

	Texture::~Texture() {
		Renderer::GetRenderer()->DestroyTexture(*this);
	}
}