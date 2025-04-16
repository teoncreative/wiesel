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

DescriptorLayout::DescriptorLayout() {
  m_Allocated = false;
}

DescriptorLayout::~DescriptorLayout() {
  Engine::GetRenderer()->DestroyDescriptorLayout(*this);
}

void DescriptorLayout::AddBinding(VkDescriptorType type, VkShaderStageFlags flags) {
  m_Bindings.push_back({
      .Index = static_cast<uint32_t>(m_Bindings.size()),
      .Type = type,
      .Flags = flags,
  });
}

void DescriptorLayout::Bake() {
  if (m_Allocated) {
    return; // todo error or destroy
  }
  std::vector<VkDescriptorSetLayoutBinding> bindings{};
  bindings.reserve(m_Bindings.size());

  for (const auto& item : m_Bindings) {
    VkDescriptorSetLayoutBinding binding{
        .binding = item.Index,
        .descriptorType = item.Type,
        .descriptorCount = 1,
        .stageFlags = item.Flags,
        .pImmutableSamplers = nullptr};
    bindings.push_back(binding);
  }

  VkDescriptorSetLayoutCreateInfo layoutInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = static_cast<uint32_t>(bindings.size()),
      .pBindings = bindings.data()};

  WIESEL_CHECK_VKRESULT(vkCreateDescriptorSetLayout(
      Engine::GetRenderer()->GetLogicalDevice(), &layoutInfo, nullptr, &m_Layout));
  m_Allocated = true;
}

}  // namespace Wiesel
