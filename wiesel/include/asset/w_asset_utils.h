//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

//
// Created by Metehan Gezer on 24.03.2026.
//

#pragma once

#include "asset/w_asset_handle.h"

namespace Wiesel {

// Map file extension to asset type.
AssetType ExtToAssetType(const std::string& ext);

// Returns true if this asset type stores its handle inside its JSON file
// (scenes, prefabs, materials, skyboxes, spritesheets, sprite anims).
// Returns false for binary assets that use .meta sidecar files.
bool IsJsonAssetType(AssetType type);

}  // namespace Wiesel
