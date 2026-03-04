#pragma once

#include "w_pch.hpp"

namespace Wiesel {

#define WIESEL_MAX_BONES 128
#define WIESEL_MAX_BONE_INFLUENCE 4

// ---- Skeleton ----

struct BoneInfo {
  std::string name;
  int32_t parent_index = -1;  // -1 = root bone
  glm::mat4 inverse_bind_matrix = glm::mat4(1.0f);
};

struct Skeleton {
  std::vector<BoneInfo> bones;
  std::unordered_map<std::string, int32_t> bone_name_to_index;

  int32_t FindBone(const std::string& name) const {
    auto it = bone_name_to_index.find(name);
    return it != bone_name_to_index.end() ? it->second : -1;
  }
};

// ---- Node Hierarchy ----

struct NodeInfo {
  std::string name;
  int32_t parent_index = -1;
  glm::mat4 local_transform = glm::mat4(1.0f);
  std::vector<int32_t> mesh_indices;
  std::vector<int32_t> children;
  int32_t bone_index = -1;  // index into Skeleton::bones, or -1
};

struct NodeHierarchy {
  std::vector<NodeInfo> nodes;
  int32_t root_index = 0;
  std::unordered_map<std::string, int32_t> node_name_to_index;

  int32_t FindNode(const std::string& name) const {
    auto it = node_name_to_index.find(name);
    return it != node_name_to_index.end() ? it->second : -1;
  }
};

// ---- Animation Clips ----

template <typename T>
struct AnimationKey {
  float time;
  T value;
};

struct AnimationChannel {
  std::string node_name;
  std::vector<AnimationKey<glm::vec3>> position_keys;
  std::vector<AnimationKey<glm::quat>> rotation_keys;
  std::vector<AnimationKey<glm::vec3>> scale_keys;
};

struct AnimationClip {
  std::string name;
  float duration = 0.0f;
  float ticks_per_second = 25.0f;
  std::vector<AnimationChannel> channels;
};

// ---- GPU Data ----

struct alignas(16) BoneMatricesUniformData {
  alignas(16) glm::mat4 bone_matrices[WIESEL_MAX_BONES];
};

}  // namespace Wiesel
