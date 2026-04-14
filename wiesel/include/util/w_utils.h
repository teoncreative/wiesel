
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#define PI 3.14159265358979f
#define BIT(x) (1 << x)

#include <glm/gtx/hash.hpp>

#include "w_pch.h"

namespace wiesel {

#define WIESEL_SHADOW_CASCADE_COUNT 4
#define WIESEL_SSAO_KERNEL_SIZE 64
#define WIESEL_SSAO_RADIUS 0.5
#define WIESEL_SSAO_NOISE_DIM 16
#define WIESEL_SSAO_BIAS 0.025
#define WIESEL_SHADOWMAP_DIM 4096

std::string GetNameFromVulkanResult(VkResult error_code);

struct QueueFamilyIndices {
  std::optional<uint32_t> graphics_family;
  std::optional<uint32_t> present_family;

  bool IsComplete() {
    return graphics_family.has_value() && present_family.has_value();
  }
};

struct SwapChainSupportDetails {
  VkSurfaceCapabilitiesKHR capabilities;
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> present_modes;
};

enum BakeResult { SUCCESS };

using Index = uint32_t;

enum Vertex3DFlag {
  VertexFlagHasTexture = BIT(0),
  VertexFlagHasNormalMap = BIT(1),
  VertexFlagHasSpecularMap = BIT(2),
  VertexFlagHasHeightMap = BIT(3),
  VertexFlagHasAlbedoMap = BIT(4),
  VertexFlagHasRoughnessMap = BIT(5),
  VertexFlagHasMetallicMap = BIT(6),
  VertexFlagHasOpacityMap = BIT(7),
};

struct Vertex3D {
  glm::vec3 ppos;
  glm::vec3 color;
  glm::vec2 uv;
  glm::vec3 normal;
  glm::vec3 tangent;
  glm::vec3 bi_tangent;
  uint32_t flags;
  glm::ivec4 bone_indices = {0, 0, 0, 0};
  glm::vec4 bone_weights = {0.0f, 0.0f, 0.0f, 0.0f};

  static VkVertexInputBindingDescription GetBindingDescription() {
    VkVertexInputBindingDescription binding_description{};
    binding_description.binding = 0;
    binding_description.stride = sizeof(Vertex3D);
    binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return binding_description;
  }

  static std::vector<VkVertexInputAttributeDescription>
  GetAttributeDescriptions() {
    std::vector<VkVertexInputAttributeDescription> attribute_descriptions{};

    attribute_descriptions.push_back(
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, (uint32_t)offsetof(Vertex3D, ppos)});
    attribute_descriptions.push_back({1, 0, VK_FORMAT_R32G32B32_SFLOAT,
                                      (uint32_t)offsetof(Vertex3D, color)});
    attribute_descriptions.push_back(
        {2, 0, VK_FORMAT_R32G32_SFLOAT, (uint32_t)offsetof(Vertex3D, uv)});
    attribute_descriptions.push_back({3, 0, VK_FORMAT_R32G32B32_SFLOAT,
                                      (uint32_t)offsetof(Vertex3D, normal)});
    attribute_descriptions.push_back({4, 0, VK_FORMAT_R32G32B32_SFLOAT,
                                      (uint32_t)offsetof(Vertex3D, tangent)});
    attribute_descriptions.push_back(
        {5, 0, VK_FORMAT_R32G32B32_SFLOAT,
         (uint32_t)offsetof(Vertex3D, bi_tangent)});
    attribute_descriptions.push_back(
        {6, 0, VK_FORMAT_R32_UINT, (uint32_t)offsetof(Vertex3D, flags)});
    attribute_descriptions.push_back(
        {7, 0, VK_FORMAT_R32G32B32A32_SINT,
         (uint32_t)offsetof(Vertex3D, bone_indices)});
    attribute_descriptions.push_back(
        {8, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
         (uint32_t)offsetof(Vertex3D, bone_weights)});

    return attribute_descriptions;
  }

  bool operator==(const Vertex3D& other) const {
    return ppos == other.ppos && color == other.color && uv == other.uv;
  }
};

struct Vertex2DNoColor {
  glm::vec2 pos;
  glm::vec2 uv;

  static VkVertexInputBindingDescription GetBindingDescription() {
    VkVertexInputBindingDescription binding_description{};
    binding_description.binding = 0;
    binding_description.stride = sizeof(Vertex2DNoColor);
    binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return binding_description;
  }

  static std::vector<VkVertexInputAttributeDescription>
  GetAttributeDescriptions() {
    std::vector<VkVertexInputAttributeDescription> attribute_descriptions{};

    attribute_descriptions.push_back(
        {0, 0, VK_FORMAT_R32G32_SFLOAT,
         (uint32_t)offsetof(Vertex2DNoColor, pos)});
    attribute_descriptions.push_back({1, 0, VK_FORMAT_R32G32_SFLOAT,
                                      (uint32_t)offsetof(Vertex2DNoColor, uv)});

    return attribute_descriptions;
  }

  bool operator==(const Vertex2DNoColor& other) const {
    return pos == other.pos && uv == other.uv;
  }
};

struct VertexSprite {
  glm::vec2 uv;

  static std::vector<VkVertexInputBindingDescription> GetBindingDescriptions() {
    std::vector<VkVertexInputBindingDescription> binding_descriptions{};

    binding_descriptions.push_back(
        {0, sizeof(VertexSprite), VK_VERTEX_INPUT_RATE_VERTEX});

    return binding_descriptions;
  }

  static std::vector<VkVertexInputAttributeDescription>
  GetAttributeDescriptions() {
    std::vector<VkVertexInputAttributeDescription> attribute_descriptions{};

    attribute_descriptions.push_back(
        {0, 0, VK_FORMAT_R32G32_SFLOAT, (uint32_t)offsetof(VertexSprite, uv)});

    return attribute_descriptions;
  }

  bool operator==(const VertexSprite& other) const { return uv == other.uv; }
};

// GLM aligned gentypes must be enabled for UBO structs to match std140 layout.
// Without it, glm::vec3 is 12 bytes instead of 16, breaking all UBO offsets.
static_assert(sizeof(glm::vec3) == 16,
    "glm::vec3 must be 16 bytes for std140 UBO layout. "
    "Define GLM_FORCE_INTRINSICS and GLM_FORCE_DEFAULT_ALIGNED_GENTYPES.");
static_assert(sizeof(glm::mat3) == 48,
    "glm::mat3 must be 48 bytes for std140 UBO layout.");

struct alignas(16) MatricesUniformData {
  alignas(16) glm::mat4 model_matrix;
  alignas(16) glm::mat3 normal_matrix;
  // Packed: (scene_index << 24) | (entity_handle + 1). 0 = no entity.
  uint32_t entity_id = 0;
  float _pad1[3]{};  // pad to next vec4 boundary (offset 128)
  alignas(16) glm::vec4 color_tint{1.0f, 1.0f, 1.0f, 1.0f};
  alignas(16) glm::vec4 material_params{
      1.0f, 1.0f, 1.0f,
      0.0f};  // x=roughness, y=metallic, z=specular (0-1 multipliers)
};

struct alignas(16) SpriteUniformData {
  alignas(16) glm::mat4 model_matrix;
  alignas(16) glm::vec4 tint;
  alignas(4) int flip_x;
  alignas(4) int flip_y;
};

struct alignas(16) CameraUniformData {
  alignas(16) glm::mat4 view_matrix;
  alignas(16) glm::mat4 projection;
  alignas(16) glm::mat4 inv_projection;
  alignas(16) glm::vec3 position;
  float near_plane;
  float far_plane;
  float _pad1[2];
  glm::vec4 cascade_splits;
  uint32_t enable_ssao;
  uint32_t debug_cascades;
  float _pad2[2];
  alignas(16) glm::mat4 prev_view_projection;
  alignas(16) glm::vec2 taa_jitter_offset;
  float _pad3[2];
  alignas(16) glm::vec4 ambient;  // xyz = color, w = intensity
};

struct alignas(16) ShadowMapMatricesUniformData {
  alignas(16) glm::mat4 view_projection_matrix[WIESEL_SHADOW_CASCADE_COUNT];
  alignas(16) int32_t enable_shadows;
};

struct alignas(16) SSAOKernelUniformData {
  alignas(16) glm::vec4 samples[WIESEL_SSAO_KERNEL_SIZE];
};

struct SSAOSpecializationData {
  uint32_t kernel_size = WIESEL_SSAO_KERNEL_SIZE;
  float radius = WIESEL_SSAO_RADIUS;

  std::vector<VkSpecializationMapEntry> GetSpecializationMapEntries() {
    std::vector<VkSpecializationMapEntry> entries;
    entries.push_back(VkSpecializationMapEntry{
        .constantID = 0,
        .offset = (uint32_t)offsetof(SSAOSpecializationData, kernel_size),
        .size = sizeof(kernel_size)});
    entries.push_back(VkSpecializationMapEntry{
        .constantID = 1,
        .offset = (uint32_t)offsetof(SSAOSpecializationData, radius),
        .size = sizeof(radius)});
    return entries;
  }
};

/*template <typename T>
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
constexpr std::shared_ptr<T> CreateReference(Args&&... args) {
  return std::make_shared<T>(std::forward<Args>(args)...);
}*/

template <typename A, typename B>
using Pair = std::pair<A, B>;

class Time {
 public:
  static float_t GetTime();
};

enum class AntiAliasingMode { None = 0, FXAA = 1, TAA = 2 };

enum class SamplingMode {
  DISABLED = 0,
  X2 = 1,
  X4 = 2,
  X8 = 3,
  X16 = 4,
  X32 = 5,
  X64 = 6
};

inline const char* ToString(SamplingMode sampling_mode) {
  switch (sampling_mode) {
    case SamplingMode::DISABLED:
      return "Disabled";
    case SamplingMode::X2:
      return "2x";
    case SamplingMode::X4:
      return "4x";
    case SamplingMode::X8:
      return "8x";
    case SamplingMode::X16:
      return "16x";
    case SamplingMode::X32:
      return "32x";
    case SamplingMode::X64:
      return "64x";
    default:
      return "Unknown";
  }
}

inline VkSampleCountFlagBits ToVkSampleCountFlagBits(SamplingMode mode) {
  switch (mode) {
    case SamplingMode::X2:
      return VK_SAMPLE_COUNT_2_BIT;
    case SamplingMode::X4:
      return VK_SAMPLE_COUNT_4_BIT;
    case SamplingMode::X8:
      return VK_SAMPLE_COUNT_8_BIT;
    case SamplingMode::X16:
      return VK_SAMPLE_COUNT_16_BIT;
    case SamplingMode::X32:
      return VK_SAMPLE_COUNT_32_BIT;
    case SamplingMode::X64:
      return VK_SAMPLE_COUNT_64_BIT;
    default:
      return VK_SAMPLE_COUNT_1_BIT;
  }
}

std::vector<char> ReadVirtualFile(const std::string& virtual_path);
std::vector<uint32_t> ReadVirtualFileUint32(const std::string& virtual_path);

std::string FormatVariableName(const std::string& name);

inline void TrimLeft(std::string& s) {
  s.erase(s.begin(), std::ranges::find_if(s, [](unsigned char ch) {
            return !std::isspace(ch);
          }));
}
}  // namespace wiesel

#define WIESEL_CONCAT_IMPL(x, y) x##y
#define WIESEL_CONCAT(x, y) WIESEL_CONCAT_IMPL(x, y)

#define WIESEL_UNIQUE_NAME(base) WIESEL_CONCAT(base, __LINE__)

#define WIESEL_CHECK_VKRESULT_NAMED(f, name)                         \
  do {                                                               \
    VkResult name = (f);                                             \
    if (name != VK_SUCCESS) {                                        \
      std::cout << "Fatal : VkResult is \""                          \
                << wiesel::GetNameFromVulkanResult(name) << "\" in " \
                << __FILE__ << " at line " << __LINE__ << "\n";      \
      assert(name == VK_SUCCESS);                                    \
    }                                                                \
  } while (0)

#define WIESEL_CHECK_VKRESULT(f) \
  WIESEL_CHECK_VKRESULT_NAMED(f, WIESEL_UNIQUE_NAME(res))

// https://github.com/TheCherno/Hazel
#define WIESEL_BIND_FN(fn)                                  \
  [this](auto&&... args) -> decltype(auto) {                \
    return this->fn(std::forward<decltype(args)>(args)...); \
  }

#define WIESEL_BIND_GLOBAL_FN(fn)                     \
  [](auto&&... args) -> decltype(auto) {              \
    return fn(std::forward<decltype(args)>(args)...); \
  }
