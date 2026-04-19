
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
#include "scene/w_components.h"
#include "scene/w_entity.h"
#include "w_pch.h"

namespace wiesel::editor {
class CommandStack;
}

namespace wiesel {

// Shared drag-drop handler for asset fields (accepts AssetHandle + BrowserFile payloads)
AssetHandle AcceptAssetDragDrop(AssetType required_type);

void InitializeEditorComponents();
void InitializeScriptFieldRenderers();

// Set the active command stack for undo/redo tracking in inspector widgets.
// Must be called before RenderExistingComponents each frame.
void SetInspectorCommandStack(editor::CommandStack* stack);

void RenderExistingComponents(Entity entity);
void RenderModals(Entity entity);
void RenderAddPopup(Entity entity);

// Returns the icon (UTF-8 glyph) of the highest-priority component on the
// entity, or an empty string if no component on the entity has an icon.
// Priority ties are broken by registration order (later wins).
std::string_view GetEntityIcon(Entity entity);
}  // namespace wiesel
