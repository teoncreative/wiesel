
//
//   Copyright 2025 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "asset/w_asset_handle.hpp"
#include "scene/w_components.hpp"
#include "scene/w_entity.hpp"
#include "w_pch.hpp"

namespace Wiesel {

// Shared drag-drop handler for asset fields (accepts AssetHandle + BrowserFile payloads)
AssetHandle AcceptAssetDragDrop(AssetType required_type);

void InitializeEditorComponents();

void RenderExistingComponents(Entity entity);
void RenderModals(Entity entity);
void RenderAddPopup(Entity entity);
}  // namespace Wiesel
