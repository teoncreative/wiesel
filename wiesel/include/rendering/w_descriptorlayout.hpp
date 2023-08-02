
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
class DescriptorLayout {
 public:
  explicit DescriptorLayout();
  ~DescriptorLayout();

  bool m_Allocated;
  VkDescriptorSetLayout m_Layout;
};
}  // namespace Wiesel