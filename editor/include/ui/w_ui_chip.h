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

#include <imgui.h>

// Pill-shaped "chip" widgets used by input bindings, tag editors, etc.
// A Chip shows a label plus a right-anchored X for removal; AddChipButton
// is a ghost-styled pill the caller uses to open a picker popup.

namespace wiesel::editor::ui::chip {

// Filled + muted pill with a label and a right-side X for removal.
// Clicking the X returns true (remove-requested). The chip's body is
// not interactive beyond the X - callers wanting click-to-rebind can
// overlay their own InvisibleButton before calling Chip() if needed.
//
//   `label` displayed text (e.g. "Space", "A", "LCtrl").
// Returns true on the frame the X is clicked.
bool Chip(const char* label);

// Ghost-styled pill used as the "+ Add" button in a row of chips. Matches
// the Chip() pill shape but paints a dashed outline + muted text to read
// as a ghost/placeholder. Returns true when clicked this frame - callers
// typically call ImGui::OpenPopup() in response.
//
//   `label` displayed text (e.g. "+ Add Key", "+ Add Button").
bool AddChipButton(const char* label);

}  // namespace wiesel::editor::ui::chip
