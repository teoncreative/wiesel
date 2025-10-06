
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

#include "util/w_utils.hpp"
#include "w_pch.hpp"

namespace Wiesel {

class DescriptorSetLayout {
 public:
  explicit DescriptorSetLayout();
  ~DescriptorSetLayout();

  void AddBinding(VkDescriptorType type, VkShaderStageFlags flags);
  void Bake();

  bool allocated_;
  VkDescriptorSetLayout layout_;
  struct Binding {
    uint32_t index;
    VkDescriptorType type;
    VkShaderStageFlags flags;
  };
  std::vector<Binding> bindings_;
};
}  // namespace Wiesel