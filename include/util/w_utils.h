//   Copyright 2023 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#define PI 3.14

#include "w_pch.h"
#include "w_attributes.h"
#include <glm/gtx/hash.hpp>

namespace Wiesel {
	std::string GetNameFromVulkanResult(VkResult errorCode);

	struct QueueFamilyIndices {
		std::optional<uint32_t> graphicsFamily;
		std::optional<uint32_t> presentFamily;

		bool IsComplete() {

			return graphicsFamily.has_value() && presentFamily.has_value();
		}
	};

	struct SwapChainSupportDetails {
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};


	using Index = uint32_t;

	struct Vertex {
		glm::vec3 Pos;
		glm::vec3 Color;
		glm::vec2 TexCoord;
		bool HasTexture;

		static VkVertexInputBindingDescription GetBindingDescription() {
			VkVertexInputBindingDescription bindingDescription{};
			bindingDescription.binding = 0;
			bindingDescription.stride = sizeof(Vertex);
			bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			return bindingDescription;
		}

		static std::array<VkVertexInputAttributeDescription, 4> GetAttributeDescriptions() {
			std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions{};

			attributeDescriptions[0].binding = 0;
			attributeDescriptions[0].location = 0;
			attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[0].offset = offsetof(Vertex, Pos);

			attributeDescriptions[1].binding = 0;
			attributeDescriptions[1].location = 1;
			attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[1].offset = offsetof(Vertex, Color);

			attributeDescriptions[2].binding = 0;
			attributeDescriptions[2].location = 2;
			attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
			attributeDescriptions[2].offset = offsetof(Vertex, TexCoord);

			attributeDescriptions[3].binding = 0;
			attributeDescriptions[3].location = 3;
			attributeDescriptions[3].format = VK_FORMAT_R8_UINT;
			attributeDescriptions[3].offset = offsetof(Vertex, HasTexture);

			return attributeDescriptions;
		}

		bool operator==(const Vertex& other) const {
			return Pos == other.Pos && Color == other.Color && TexCoord == other.TexCoord;
		}
	};

	struct vertex_hash {
		std::size_t operator () (const Wiesel::Vertex& vertex) const {
			auto posHash = std::hash<glm::vec3>{}(vertex.Pos);
			auto colorHash = std::hash<glm::vec3>{}(vertex.Color);
			auto texHash = std::hash<glm::vec2>{}(vertex.TexCoord);
			return ((posHash ^ (colorHash << 1)) >> 1) ^ (texHash << 1);
		}
	};

	struct UniformBufferObject {
		glm::mat4 Model;
		glm::mat4 View;
		glm::mat4 Proj;
	};

	template<typename T>
	using Scope = std::unique_ptr<T>;
	template<typename T, typename ... Args>
	constexpr Scope<T> CreateScope(Args&& ... args)
	{
		return std::make_unique<T>(std::forward<Args>(args)...);
	}

	template<typename T>
	using Reference = std::shared_ptr<T>;
	template<typename T, typename ... Args>
	constexpr Reference<T> CreateReference(Args&& ... args)
	{
		return std::make_shared<T>(std::forward<Args>(args)...);
	}

	template<typename A, typename B>
	using Pair = std::pair<A, B>;


	class Time {
	public:
		static float_t GetTime();
	};

	std::vector<char> ReadFile(const std::string& filename);
}

#define WIESEL_BIND_EVENT_FUNCTION(fn) [this](auto&&... args) -> decltype(auto) { return this->fn(std::forward<decltype(args)>(args)...); }
#define WIESEL_BIND_GLOBAL_EVENT_FUNCTION(fn) [](auto&&... args) -> decltype(auto) { return fn(std::forward<decltype(args)>(args)...); }

#define WIESEL_CHECK_VKRESULT(f)																		\
{																										\
	VkResult res = (f);																					\
	if (res != VK_SUCCESS)																				\
	{																									\
		std::cout << "Fatal : VkResult is \"" << Wiesel::GetNameFromVulkanResult(res) << "\" in " << __FILE__ << " at line " << __LINE__ << "\n"; \
		assert(res == VK_SUCCESS);																		\
	}																									\
}

#define BIT(x) (1 << x)
