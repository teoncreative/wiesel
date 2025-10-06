//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/w_descriptorlayout.hpp"

#include "w_engine.hpp"

namespace Wiesel {

DescriptorSetLayout::DescriptorSetLayout() {
  allocated_ = false;
}

DescriptorSetLayout::~DescriptorSetLayout() {
  Engine::GetRenderer()->DestroyDescriptorLayout(*this);
}

void DescriptorSetLayout::AddBinding(VkDescriptorType type, VkShaderStageFlags flags) {
  bindings_.push_back({
      .index = static_cast<uint32_t>(bindings_.size()),
      .type = type,
      .flags = flags,
  });
}

void DescriptorSetLayout::Bake() {
  if (allocated_) {
    return; // todo error or destroy
  }
  std::vector<VkDescriptorSetLayoutBinding> bindings{};
  bindings.reserve(bindings_.size());

  for (const auto& item : bindings_) {
    VkDescriptorSetLayoutBinding binding{
        .binding = item.index,
        .descriptorType = item.type,
        .descriptorCount = 1,
        .stageFlags = item.flags,
        .pImmutableSamplers = nullptr};
    bindings.push_back(binding);
  }

  VkDescriptorSetLayoutCreateInfo layoutInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = static_cast<uint32_t>(bindings.size()),
      .pBindings = bindings.data()};

  WIESEL_CHECK_VKRESULT(vkCreateDescriptorSetLayout(
      Engine::GetRenderer()->GetLogicalDevice(), &layoutInfo, nullptr, &layout_));
  allocated_ = true;
}

}  // namespace Wiesel
