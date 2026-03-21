#pragma once

// Moonlight ImGui theme by deathsu/Madam-Herta
// https://github.com/Madam-Herta/Moonlight/

namespace ImGui {
namespace Moonlight {

// Load SourceSansProRegular and sets it as a default font.
// You may want to call ImGui::GetIO().Fonts->Clear() before this
void LoadFont(float size = 19.0f);

// Sets the ImGui style to Moonlight
void StyleColorsMoonlight();

}  // namespace Moonlight
}  // namespace ImGui