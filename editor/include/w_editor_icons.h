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

#include "util/imgui/imgui_lucide.h"

namespace wiesel::editor {

// Initialize the editor icon font system. Call once in EditorLayer::OnAttach
// after all other fonts have been added.
void InitEditorIcons();

}  // namespace wiesel::editor
