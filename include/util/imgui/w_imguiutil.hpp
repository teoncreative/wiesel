
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

#include "w_pch.hpp"
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <ImGuizmo.h>

namespace Wiesel {
  std::string PrefixLabel(const char* label);
}

namespace ImGui {
  bool ClosableTreeNode(const char* label, bool* visible);
}