
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
  class DescriptorData {
  public:
    explicit DescriptorData(uint32_t max);
    ~DescriptorData();

    uint32_t m_MaxDescriptorCount;
    VkDescriptorPool m_DescriptorPool;
    std::vector<VkDescriptorSet> m_DescriptorSets;
  };
}