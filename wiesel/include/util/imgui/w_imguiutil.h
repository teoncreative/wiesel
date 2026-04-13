
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

#include "imgui.h"
#include "w_pch.h"

namespace Wiesel {

class Texture;

std::string PrefixLabel(const char* label);

// Render a small texture thumbnail with hover preview tooltip.
void RenderTexturePreview(const char* label, Texture* tex);

}  // namespace Wiesel

namespace ImGui {
bool ClosableTreeNode(const char* label, bool* visible);

// TreeNodeEx with extra vertical padding for more comfortable row height.
bool PaddedTreeNodeEx(const char* label, ImGuiTreeNodeFlags flags,
                      float padding_y = 3.0f, float rounding = 4.0f);
}  // namespace ImGui