
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

#include "rendering/w_texture.hpp"
#include "w_pch.hpp"

namespace Wiesel {

// Generates a small RGBA texture with text rendered onto a colored background.
// Uses FreeType to rasterize the text. Result is cached by key.
// Returns a shared_ptr to the texture (cached, do not destroy).
std::shared_ptr<Texture> GetOrCreateLabelTexture(const std::string& key,
                                                 const std::string& text,
                                                 const glm::vec4& bg_color,
                                                 const glm::vec4& text_color,
                                                 int font_size = 24);

}  // namespace Wiesel