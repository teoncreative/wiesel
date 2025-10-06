
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

#include "rendering/w_renderpass.hpp"
#include "util/w_utils.hpp"
#include "w_descriptorlayout.hpp"
#include "w_pch.hpp"
#include "w_shader.hpp"

namespace Wiesel {
enum CullMode { CullModeNone, CullModeBack, CullModeFront, CullModeBoth };

struct PipelineProperties {
  VkSampleCountFlagBits msaa_samples;
  CullMode cull_mode;
  bool enable_wireframe;
  bool enable_alpha_blending;
  bool enable_depth_test = true;
  bool enable_depth_write = true;
};

struct PushConstant {
  VkShaderStageFlags flags;
  uint32_t size;
  uint32_t offset;
  Ref<void> ref;
};

struct Pipeline {
  explicit Pipeline(PipelineProperties properties);
  ~Pipeline();

  void SetRenderPass(Ref<RenderPass> pass);
  void AddInputLayout(Ref<DescriptorSetLayout> layout);
  void AddDynamicState(VkDynamicState state);
  void AddShader(Ref<Shader> shader);
  template<typename T>
  void AddShader(Ref<Shader> shader, T* data, std::vector<VkSpecializationMapEntry> mapEntries) {
    shaders_.push_back({
        .shader = shader,
        .specialization = {
            .data = data,
            .data_size = sizeof(*data),
            .map_entries = mapEntries
        }
    });
  }

  void SetVertexData(VkVertexInputBindingDescription input_binding_description, std::vector<VkVertexInputAttributeDescription> attribute_descriptions);
  void SetVertexData(std::vector<VkVertexInputBindingDescription> input_binding_descriptions, std::vector<VkVertexInputAttributeDescription> attribute_descriptions);

  template<typename T>
  void AddPushConstant(Ref<T> ref, VkShaderStageFlags flags) {
    push_constants_.push_back(PushConstant{
        .flags = flags,
        .size = sizeof(T),
        .offset = static_cast<uint32_t>(push_constants_.size()),
        .ref = std::static_pointer_cast<void>(ref)
    });
  }

  void Bake();

  void Bind(PipelineBindPoint bind_point);
  struct SpecializationData {
    std::vector<VkSpecializationMapEntry> map_entries;
    size_t data_size;
    void* data;
  };
  struct ShaderInfo {
    Ref<Shader> shader;
    SpecializationData specialization;
  };
  PipelineProperties properties_;
  std::vector<ShaderInfo> shaders_;
  std::vector<VkDynamicState> dynamic_states_;
  Ref<RenderPass> m_RenderPass;
  std::vector<Ref<DescriptorSetLayout>> descriptor_layouts_;
  VkPipelineLayout layout_{};
  VkPipeline pipeline_{};
  bool has_vertex_binding_ = false;
  std::vector<VkVertexInputBindingDescription> vertex_input_binding_descriptions_;
  std::vector<VkVertexInputAttributeDescription> vertex_attribute_descriptions_;
  std::vector<PushConstant> push_constants_;
  bool is_allocated_ = false;
};

}  // namespace Wiesel