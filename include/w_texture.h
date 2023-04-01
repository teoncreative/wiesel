
//   Copyright 2023 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "w_pch.h"

namespace Wiesel {
	enum TextureType {
		TextureTypeTexture,
		TextureTypeDepthStencil
	};

	class Texture {
	public:
		Texture(TextureType textureType, const std::string& path);
		~Texture();

		TextureType m_TextureType;
		VkImage m_Image;
		VkDeviceMemory m_DeviceMemory;
		VkImageView m_ImageView;
		VkSampler m_Sampler;

		int32_t m_Width;
		int32_t m_Height;
		int32_t m_Channels;
		VkDeviceSize m_Size;

		bool m_Allocated;
		std::string m_Path;
	};
}