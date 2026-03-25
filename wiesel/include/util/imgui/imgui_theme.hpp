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