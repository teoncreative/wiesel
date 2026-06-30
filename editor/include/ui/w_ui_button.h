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

// Button-style widgets: toolbar groups and buttons used across the editor
// viewport toolbar, play controls, and anywhere else that wants the framed
// toolbar look.

namespace wiesel::editor::ui::button {

// Begin a framed group of buttons sharing a single input-style background
// and border. Auto-sizes to its contents. Pair each BeginToolbarGroup with
// EndToolbarGroup.
void BeginToolbarGroup(const char* id);
void EndToolbarGroup();

// Square icon button tailored for toolbar groups: transparent background,
// muted label text by default; lifts to ImGuiCol_ButtonHovered + white text
// on hover, and uses ButtonActive bg when `active` is true or the user is
// holding. `label` may be an icon glyph followed by `##<id>` for uniqueness.
bool ToolbarButton(const char* label, bool active = false);

}  // namespace wiesel::editor::ui::button
