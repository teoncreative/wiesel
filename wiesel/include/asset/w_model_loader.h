
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

#include <memory>

#include "asset/w_asset_handle.h"

struct aiScene;

namespace Wiesel {

// Load a model asset (Assimp import, mesh GPU upload, child texture loading).
bool LoadModelAsset(AssetHandle handle);

// Load a texture asset (standalone, external, or embedded via fragment URI).
bool LoadTextureAsset(AssetHandle handle);

// Import an Assimp scene from a VFS path.
std::unique_ptr<aiScene> LoadAssimpScene(const std::string& path,
                                         bool convert_to_left_handed = true);

}  // namespace Wiesel
