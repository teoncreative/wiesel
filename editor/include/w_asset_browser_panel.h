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

#include <filesystem>
#include <functional>
#include <string>
#include "asset/w_asset_handle.h"
#include "w_vfs_browser.h"

namespace wiesel::editor {

// Callbacks from the asset browser panel back to the editor. Asset-creation
// entries live in AssetFactoryRegistry instead - they're registered by the
// editor at startup and the panel just iterates the registry for its
// "Create" submenu.
struct AssetBrowserCallbacks {
  std::function<void()> on_scan_assets;
  std::function<void()> on_update_title;
  std::function<void(const std::string& vfs_path)> on_open_scene;
  std::function<void(const std::string& vfs_path)> on_open_prefab;
  std::function<void(const std::filesystem::path&)> on_open_code_editor;
  std::function<void(AssetHandle)> on_open_anim_controller;
  std::function<void(AssetHandle)> on_open_anim_clip;
  std::function<void(AssetHandle)> on_select_asset;
  // Triggered by the panel's top-bar "Folder" button. Editor opens the
  // shared name prompt and creates the directory under the panel's current
  // VFS dir on confirm.
  std::function<void()> on_request_folder;
};

class AssetBrowserPanel {
 public:
  void SetCallbacks(AssetBrowserCallbacks callbacks);

  // Render the full asset browser panel. Pass the open flag for the window.
  void Render(bool& open);

  VfsBrowser& browser() { return browser_; }

  const VfsBrowser& browser() const { return browser_; }

  // Only the app's assets can be modified
  bool IsReadOnly() const {
    return browser_.root() != "app://" || browser_.IsAtTopLevel();
  }

  // Current scene path tracking (for rename/move awareness)
  std::string current_scene_path;

  // Navigate the browser to the folder containing `vfs_path` and select
  // the file. Useful for the command palette's "jump to asset" action.
  void RevealAsset(const std::string& vfs_path);

  // Drop any project-specific local state (selection, rename, search, cached
  // directory listing) and reset the browser to the app:// root.
  void Reset();

 private:
  VfsBrowser browser_;
  AssetBrowserCallbacks callbacks_;

  // Local state
  std::string selected_file_;
  std::string renaming_file_;
  char rename_buf_[256] = {};
  char search_[128] = {};
};

}  // namespace wiesel::editor
