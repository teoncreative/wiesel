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

namespace wiesel {
class Texture;
}

// Form-field style helpers: prefix labels, asset-drop targets, texture
// thumbnails. These are the inputs callers reach for inside component
// inspectors and asset inspectors.

namespace wiesel::editor::ui::field {

// Render a right-aligned left-side label for the next input widget, splitting
// the available width so labels and controls stay aligned across a form.
// Returns the `##<label>` id string to pass as the next ImGui control's
// label, e.g.:
//   ImGui::DragFloat(ui::field::PrefixLabel("Position").c_str(), &pos);
std::string PrefixLabel(const char* label);

// Renders a small texture thumbnail next to `label`; on hover shows a larger
// preview (up to 256px) in a tooltip. `tex` may be null/unloaded - the
// helper prints a disabled placeholder instead.
void RenderTexturePreview(const char* label, Texture* tex);

// Renders a non-interactive square badge at the current cursor position,
// sized to GetFrameHeight() (so it aligns vertically with an adjacent
// InputText/Combo row) and styled like an input field - ImGuiCol_FrameBg
// with a ImGuiCol_Border outline. `icon` is an ICON_LC_* glyph painted
// centered inside. Used in inspector headers as a type indicator next to
// a name input.
void TypeIconBadge(const char* icon);

}  // namespace wiesel::editor::ui::field
