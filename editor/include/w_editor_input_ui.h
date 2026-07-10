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

#include <string>
#include "game/w_game_info.h"

namespace wiesel::editor {

// Renders the full Input section of Project Settings: mouse sensitivity,
// gamepad indicator, and the sidebar/body contexts editor (actions + axes
// with binding chips, searchable pickers, and press-to-capture bindings).
//
// Transient UI state (rename mode, binding capture popups, add-context
// mode) is kept in file-local statics inside the implementation - callers
// only hand over the settings to mutate and a persisted `selected_context`
// string.
//
// Returns true if `input` was mutated this frame. Caller should then call
// Engine::input().LoadFromSettings(input) and mark the project dirty.
bool RenderInputSettings(InputSettings& input, std::string& selected_context);

}  // namespace wiesel::editor
