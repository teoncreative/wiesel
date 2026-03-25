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

#include "asset/w_asset_handle.h"

namespace Wiesel {

// Loads a .wspriteanim asset (list of .wsprite handles + durations + loop).
bool LoadSpriteAnimAsset(const AssetHandle& handle);

// Loads a .wspritecontroller asset (state machine definition).
bool LoadSpriteControllerAsset(const AssetHandle& handle);

}  // namespace Wiesel
