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

// Shared modal popup chrome used by editor settings/wizard popups. Mirrors
// the command palette + name prompt style: centered, dim background, no
// native title bar, scaled icon + title header with a close button on the
// right, and a body region indented by WindowPadding.

namespace wiesel::editor::ui::popup {

// Flag set shared by the modal popups below: no title bar, no resize/move,
// no docking, no scrollbar/scroll, no saved settings, no nav. Callers that
// need AlwaysAutoResize or other extras should OR them in on top.
extern const ImGuiWindowFlags kModalFlags;

// Opens the modal popup window and renders the title row + close button.
// Caller renders the body content normally between Begin/End. Esc and
// outside-click both cancel; the close button cancels too. On cancel the
// `*p_open` passed in is set to false.
//
// Returns true if the popup is currently visible. Must pair with End() only
// when Begin() returned true (same contract as ImGui::Begin).
//
//   `id`    ImGui popup id string.
//   `icon`  optional ICON_LC_* glyph prepended to the title.
//   `title` title text.
//   `p_open`(non-null) flipped to false on cancel.
//   `size`  window size; (0,0) auto-sizes.
bool Begin(const char* id, const char* icon, const char* title, bool* p_open,
           const ImVec2& size = ImVec2(0.0f, 0.0f));
void End();

// Opens a centered, headerless modal popup - for wizard-style popups
// (command palette, name prompt) where the first row is a custom input,
// not a standard title + close header.
//
// Same chrome as Begin() (viewport-centered, dim bg, Esc and outside-click
// dismiss, WindowPadding=0 so callers own their padding) but without the
// title row. Pair with EndCentered() when the return is true.
//
//   `id`          ImGui popup id string.
//   `size`        window size; pass (0,0) to auto-size.
//   `auto_resize` when true, OR ImGuiWindowFlags_AlwaysAutoResize into the
//                 flag set - matches the name-prompt pattern where the
//                 popup fits its content's height.
//   `p_open`      (non-null) flipped to false on Esc / outside-click.
bool BeginCentered(const char* id, const ImVec2& size, bool auto_resize,
                   bool* p_open);
void EndCentered();

// Square button with a centered ICON_LC_X glyph, sized to the current frame
// height. Used by popup chrome but exposed for reuse on any "close this
// thing" button. Returns true when clicked this frame.
bool CloseIconButton(const char* id);

}  // namespace wiesel::editor::ui::popup
