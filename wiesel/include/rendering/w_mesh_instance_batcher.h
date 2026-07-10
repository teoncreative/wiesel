
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

#include "rendering/w_material.h"
#include "rendering/w_mesh.h"
#include "rendering/w_mesh_renderer.h"
#include "rendering/w_pipeline.h"
#include "rendering/w_renderer.h"
#include "scene/w_components.h"
#include "util/w_utils.h"

namespace wiesel {

// Accumulates visible mesh instances grouped by (mesh, material) and, on
// Flush, writes the transforms into the renderer's instance SSBO and issues
// one vkCmdDrawIndexed per group with instanceCount = group size. A single
// instance of this class should cover exactly one pipeline scope inside a
// pass (caller is responsible for binding the pipeline before Flush).
class MeshInstanceBatcher {
 public:
  MeshInstanceBatcher(Renderer* renderer) : renderer_(renderer) {}

  void Add(const std::shared_ptr<Mesh>& mesh,
           const std::shared_ptr<Material>& material,
           const std::shared_ptr<DescriptorSet>& mesh_descriptor,
           const MatricesUniformData& data) {
    PROFILE_ZONE_SCOPED();
    const uintptr_t key =
        reinterpret_cast<uintptr_t>(mesh.get()) ^
        (reinterpret_cast<uintptr_t>(material.get()) << 1);
    auto& batch = batches_[key];
    if (batch.instances.empty()) {
      batch.mesh = mesh;
      batch.descriptor = mesh_descriptor;
    }
    batch.instances.push_back(data);
  }

  bool Empty() const { return batches_.empty(); }

  // Write accumulated per-instance data to the renderer's instance SSBO and
  // issue an instanced draw per group. Clears state after flushing so the
  // batcher can be reused for another pipeline scope in the same pass.
  void Flush(VkCommandBuffer cmd, Pipeline* pipeline,
             const std::shared_ptr<DescriptorSet>& global_desc,
             const std::shared_ptr<DescriptorSet>& bone_desc,
             const std::shared_ptr<DescriptorSet>& ibl_desc = nullptr) {
    PROFILE_ZONE_SCOPED();
    if (batches_.empty()) {
      return;
    }

    uint32_t total = 0;
    for (auto& kv : batches_) {
      total += static_cast<uint32_t>(kv.second.instances.size());
    }
    MatricesUniformData* dst = nullptr;
    uint32_t base_instance = renderer_->ReserveInstanceRange(total, dst);

    uint32_t offset = 0;
    for (auto& kv : batches_) {
      auto& batch = kv.second;
      std::memcpy(dst + offset, batch.instances.data(),
                  batch.instances.size() * sizeof(MatricesUniformData));

      VkBuffer vb[] = {batch.mesh->vertex_buffer->buffer_handle_};
      VkDeviceSize offsets[] = {0};
      vkCmdBindVertexBuffers(cmd, 0, 1, vb, offsets);
      vkCmdBindIndexBuffer(cmd, batch.mesh->index_buffer->buffer_handle_, 0,
                           batch.mesh->index_buffer->index_type_);

      if (ibl_desc) {
        VkDescriptorSet sets[4] = {
            batch.descriptor->descriptor_set_, global_desc->descriptor_set_,
            bone_desc->descriptor_set_, ibl_desc->descriptor_set_};
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline->layout_, 0, 4, sets, 0, nullptr);
      } else {
        VkDescriptorSet sets[3] = {batch.descriptor->descriptor_set_,
                                   global_desc->descriptor_set_,
                                   bone_desc->descriptor_set_};
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline->layout_, 0, 3, sets, 0, nullptr);
      }

      uint32_t instance_count =
          static_cast<uint32_t>(batch.instances.size());
      uint32_t index_count =
          static_cast<uint32_t>(batch.mesh->indices.size());
      vkCmdDrawIndexed(cmd, index_count, instance_count, 0, 0,
                       base_instance + offset);
      renderer_->UpdateDrawStats(batch.mesh, instance_count);

      offset += instance_count;
    }

    batches_.clear();
  }

 private:
  struct Batch {
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<DescriptorSet> descriptor;
    std::vector<MatricesUniformData> instances;
  };
  Renderer* renderer_;
  std::unordered_map<uintptr_t, Batch> batches_;
};

inline MatricesUniformData BuildInstanceData(
    const MeshRendererComponent& mr, const TransformComponent& transform,
    entt::entity entity_handle, uint32_t scene_index) {
  PROFILE_ZONE_SCOPED();
  MatricesUniformData m{};
  m.model_matrix = transform.GetTransformMatrix();
  m.normal_matrix = transform.GetNormalMatrix();
  if (entity_handle != entt::null) {
    m.entity_id = (scene_index << 24) |
                  (static_cast<uint32_t>(entity_handle) + 1);
  }
  if (mr.material_instance) {
    const auto& props = mr.material_instance->GetResolvedProps();
    m.color_tint = props.color_tint;
    m.material_params = glm::vec4(props.roughness, props.metallic,
                                  props.specular, props.alpha_cutoff);
  }
  return m;
}

inline MatricesUniformData BuildInstanceData(
    const SkinnedMeshRendererComponent& mr,
    const TransformComponent& transform, entt::entity entity_handle,
    uint32_t scene_index) {
  PROFILE_ZONE_SCOPED();
  MatricesUniformData m{};
  m.model_matrix = transform.GetTransformMatrix();
  m.normal_matrix = transform.GetNormalMatrix();
  if (entity_handle != entt::null) {
    m.entity_id = (scene_index << 24) |
                  (static_cast<uint32_t>(entity_handle) + 1);
  }
  if (mr.material_instance) {
    const auto& props = mr.material_instance->GetResolvedProps();
    m.color_tint = props.color_tint;
    m.material_params = glm::vec4(props.roughness, props.metallic,
                                  props.specular, props.alpha_cutoff);
  }
  return m;
}

}  // namespace wiesel
