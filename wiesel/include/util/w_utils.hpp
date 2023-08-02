
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#define PI 3.14
#define BIT(x) (1 << x)

#include <glm/gtx/hash.hpp>

#include "w_pch.hpp"

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

enum VertexFlag {
  VertexFlagHasTexture = BIT(0),
  VertexFlagHasNormalMap = BIT(1),
  VertexFlagHasSpecularMap = BIT(2),
  VertexFlagHasHeightMap = BIT(3),
  VertexFlagHasAlbedoMap = BIT(4),
  VertexFlagHasRoughnessMap = BIT(5),
  VertexFlagHasMetallicMap = BIT(6),
};

struct Vertex {
  glm::vec3 Pos;
  glm::vec3 Color;
  glm::vec2 UV;
  uint32_t Flags;
  glm::vec3 Normal;
  glm::vec3 Tangent;
  glm::vec3 BiTangent;

  static VkVertexInputBindingDescription GetBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDescription;
  }

  static std::vector<VkVertexInputAttributeDescription>
  GetAttributeDescriptions() {
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};

    attributeDescriptions.push_back(
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Pos)});
    attributeDescriptions.push_back(
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Color)});
    attributeDescriptions.push_back(
        {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, UV)});
    attributeDescriptions.push_back(
        {3, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Normal)});
    attributeDescriptions.push_back(
        {4, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, Tangent)});
    attributeDescriptions.push_back(
        {5, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, BiTangent)});
    attributeDescriptions.push_back(
        {6, 0, VK_FORMAT_R32_UINT, offsetof(Vertex, Flags)});

    return attributeDescriptions;
  }

  bool operator==(const Vertex& other) const {
    return Pos == other.Pos && Color == other.Color && UV == other.UV;
  }
};

struct vertex_hash {
  std::size_t operator()(const Wiesel::Vertex& vertex) const {
    auto posHash = std::hash<glm::vec3>{}(vertex.Pos);
    auto colorHash = std::hash<glm::vec3>{}(vertex.Color);
    auto texHash = std::hash<glm::vec2>{}(vertex.UV);
    return ((posHash ^ (colorHash << 1)) >> 1) ^ (texHash << 1);
  }
};

struct UniformBufferObject {
  alignas(16) glm::mat4 ModelMatrix;
  alignas(16) glm::vec3 Scale;
  alignas(16) glm::mat3 NormalMatrix;
  alignas(16) glm::mat4 RotationMatrix;
  alignas(16) glm::mat4 CameraViewMatrix;
  alignas(16) glm::mat4 CameraProjection;
  alignas(16) glm::vec3 CameraPosition;
};

template <typename T>
using Weak = std::weak_ptr<T>;

template <typename T>
using Scope = std::unique_ptr<T>;

template <typename T, typename... Args>
constexpr Scope<T> CreateScope(Args&&... args) {
  return std::make_unique<T>(std::forward<Args>(args)...);
}

template <typename T>
using Ref = std::shared_ptr<T>;

template <typename T, typename... Args>
constexpr Ref<T> CreateReference(Args&&... args) {
  return std::make_shared<T>(std::forward<Args>(args)...);
}

template <typename A, typename B>
using Pair = std::pair<A, B>;

class Time {
 public:
  static float_t GetTime();
};

std::vector<char> ReadFile(const std::string& filename);
std::vector<uint32_t> ReadFileUint32(const std::string& filename);

inline void TrimLeft(std::string& s) {
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
            return !std::isspace(ch);
          }));
}
}  // namespace Wiesel

#define WIESEL_CHECK_VKRESULT(f)                                    \
  {                                                                 \
    VkResult res = (f);                                             \
    if (res != VK_SUCCESS) {                                        \
      std::cout << "Fatal : VkResult is \""                         \
                << Wiesel::GetNameFromVulkanResult(res) << "\" in " \
                << __FILE__ << " at line " << __LINE__ << "\n";     \
      assert(res == VK_SUCCESS);                                    \
    }                                                               \
  }

// https://github.com/TheCherno/Hazel
#define WIESEL_BIND_FN(fn)                      \
  [this](auto&&... args) -> decltype(auto) {                \
    return this->fn(std::forward<decltype(args)>(args)...); \
  }

#define WIESEL_BIND_GLOBAL_FN(fn)         \
  [](auto&&... args) -> decltype(auto) {              \
    return fn(std::forward<decltype(args)>(args)...); \
  }
