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

namespace Wiesel::Editor {

// Callbacks from the asset browser panel back to the editor.
struct AssetBrowserCallbacks {
  std::function<void()> on_scan_assets;
  std::function<void()> on_update_title;
  std::function<void()> on_new_scene;
  std::function<void(const std::string& vfs_path)> on_open_scene;
  std::function<void(const std::string& vfs_path)> on_open_prefab;
  std::function<void(const std::filesystem::path&)> on_open_code_editor;
  std::function<void(AssetHandle)> on_open_anim_controller;
  std::function<void(AssetHandle)> on_select_asset;
  std::function<void(AssetHandle)> on_slice_texture;
  std::function<void()> on_show_create_skybox;
  std::function<void()> on_show_create_sprite;
  std::function<void()> on_show_create_cursorset;
  std::function<void()> on_show_create_meshcollider;
  std::function<void()> on_create_anim_controller;
};

class AssetBrowserPanel {
 public:
  void SetCallbacks(AssetBrowserCallbacks callbacks);

  // Render the full asset browser panel. Pass the open flag for the window.
  void Render(bool& open);

  VfsBrowser& browser() { return browser_; }

  const VfsBrowser& browser() const { return browser_; }

  // Current scene path tracking (for rename/move awareness)
  std::string current_scene_path;

 private:
  VfsBrowser browser_;
  AssetBrowserCallbacks callbacks_;

  // Local state
  std::string selected_file_;
  std::string renaming_file_;
  char rename_buf_[256] = {};
  char new_folder_name_[128] = {};
  char new_script_name_[128] = {};
  bool open_script_popup_ = false;
  bool open_folder_popup_ = false;
};

}  // namespace Wiesel::Editor
