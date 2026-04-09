
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

#include "w_pch.h"

struct VkDescriptorSet_T;
typedef VkDescriptorSet_T* VkDescriptorSet;

namespace Wiesel {

class Texture;

std::string PrefixLabel(const char* label);

// Render a small texture thumbnail with hover preview tooltip.
void RenderTexturePreview(const char* label, Texture* tex);

}  // namespace Wiesel

namespace ImGui {
bool ClosableTreeNode(const char* label, bool* visible);
}