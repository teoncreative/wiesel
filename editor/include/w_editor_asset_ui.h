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

#include <functional>
#include <string>
#include <vector>
#include "asset/w_asset_handle.h"

namespace wiesel::editor {

// Editor-side registry of ImGui renderers for asset inspector UI. The engine's
// AssetRegistry knows how to load / serialize assets; everything to do with
// "how a particular asset type is edited in the inspector" lives here.
//
// RenderProperties -> edits the generic asset properties object (void*),
// i.e. the contents of the .meta file (import settings).
// RenderAsset      -> edits the asset payload itself (material, cursor set,
// etc.).
// Custom action shown on an asset's right-click context menu. Each action
// is scoped to a single AssetType. Multiple actions per type are allowed
// and rendered in registration order.
struct AssetContextAction {
  std::string label;
  std::string icon;  // optional ICON_LC_* glyph
  std::function<void(AssetHandle)> action;
};

class AssetUiRegistry {
 public:
  using RenderAssetFn = std::function<bool(AssetHandle)>;
  using RenderPropertiesFn = std::function<bool(void*)>;

  static void SetRenderAsset(AssetType type, RenderAssetFn fn);
  static void SetRenderProperties(AssetType type, RenderPropertiesFn fn);

  // Append a context-menu action for the given asset type. Asset browser
  // iterates these for the selected file's type.
  static void AddContextAction(AssetType type, AssetContextAction action);

  // Returns nullptr if no renderer is registered for the given type.
  static const RenderAssetFn* GetRenderAsset(AssetType type);
  static const RenderPropertiesFn* GetRenderProperties(AssetType type);

  // Returns an empty vector if no actions are registered for the given type.
  static const std::vector<AssetContextAction>& GetContextActions(
      AssetType type);
};

// Register all built-in asset inspector renderers. Call once after
// Engine::InitEngine() so the asset types exist.
void InstallEditorAssetUI();

// Inspector renderers that mutate an asset's GPU resources can't tear them
// down mid-frame, because they are now in the draw list.
// This function queues them to be unloaded between frames once ImGui
// has presented. Idempotent for repeated pushes per handle.
void QueueAssetReload(AssetHandle handle);

// Process the deferred reload queue. Call once per frame after ImGui has
// finished submitting (e.g. from EditorLayer::OnPostPresent).
void DrainPendingAssetReloads();

// Shared asset picker widget used by asset inspector popups. Defined here so
// it can be used across editor-side UI code.
bool AssetCombo(const char* label, AssetType type, AssetHandle& selected,
                bool allow_none = true, const char* none_label = "(None)");
bool AssetCombo(const char* label, std::initializer_list<AssetType> types,
                AssetHandle& selected, bool allow_none = true,
                const char* none_label = "(None)");

}  // namespace wiesel::editor
