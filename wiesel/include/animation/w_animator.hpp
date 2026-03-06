#pragma once

#include "animation/w_animation.hpp"
#include "w_pch.hpp"

namespace Wiesel {

struct Model;

class Animator {
 public:
  // Evaluate an animation clip at the given time.
  // Writes bone_matrices (for skinning) and node_transforms (global transform per node).
  static void Evaluate(const Model& model, const AnimationClip& clip,
                       float time, std::vector<glm::mat4>& bone_matrices,
                       std::vector<glm::mat4>& node_transforms);

  // Blend two sets of node transforms using TRS decompose-lerp-recompose,
  // then recompute bone skinning matrices from the blended node transforms.
  // t=0 gives fully 'a', t=1 gives fully 'b'.
  static void BlendAndSkin(const Model& model,
                           const std::vector<glm::mat4>& node_a,
                           const std::vector<glm::mat4>& node_b, float t,
                           std::vector<glm::mat4>& out_bone_matrices,
                           std::vector<glm::mat4>& out_node_transforms);

 private:
  static glm::vec3 InterpolatePosition(const AnimationChannel& channel,
                                       float time);
  static glm::quat InterpolateRotation(const AnimationChannel& channel,
                                       float time);
  static glm::vec3 InterpolateScale(const AnimationChannel& channel,
                                    float time);
  static glm::mat4 MakeTransform(const glm::vec3& pos, const glm::quat& rot,
                                 const glm::vec3& scale);
};

}  // namespace Wiesel
