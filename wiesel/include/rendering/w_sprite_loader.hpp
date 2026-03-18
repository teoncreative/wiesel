
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

#include "asset/w_asset_handle.hpp"
#include "rendering/w_sprite.hpp"

namespace Wiesel {

// Loads a .wspritesheet asset and builds a SpriteAsset from it.
std::shared_ptr<SpriteAsset> LoadSpriteSheet(const AssetHandle& handle);

// Loads a .wspriteanim asset and configures a SpriteComponent.
// Sets up the sprite asset (from the referenced sheet), clips, and controller.
bool LoadSpriteAnim(const AssetHandle& handle, SpriteComponent& out);

}  // namespace Wiesel