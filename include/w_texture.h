
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
		TextureTypeDepthStencil,
		TextureTypeColorImage
	};

	class Texture {
	public:
		Texture(TextureType textureType, const std::string& path);
		~Texture();

		TextureType m_Type;
		VkImage m_Image;
		VkDeviceMemory m_DeviceMemory;
		VkImageView m_ImageView;
		VkSampler m_Sampler;
		uint32_t m_MipLevels;

		int32_t m_Width;
		int32_t m_Height;
		int32_t m_Channels;
		VkDeviceSize m_Size;

		bool m_IsAllocated;
		std::string m_Path;
	};

	struct TextureProps {
		TextureProps() : GenerateMipmaps(true), ImageFormat(VK_FORMAT_R8G8B8A8_SRGB), MagFilter(VK_FILTER_LINEAR), MinFilter(VK_FILTER_LINEAR), MaxAnistropy(-1.0f) {}
		TextureProps(bool generateMipmaps, VkFormat imageFormat, VkFilter magFilter, VkFilter minFilter) : GenerateMipmaps(generateMipmaps), ImageFormat(imageFormat), MagFilter(magFilter), MinFilter(minFilter) {}

		bool GenerateMipmaps;
		VkFormat ImageFormat;
		VkFilter MagFilter;
		VkFilter MinFilter;
		float MaxAnistropy;
	};
}