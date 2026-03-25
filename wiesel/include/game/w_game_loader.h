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
#include "game/w_game_info.h"

#include <nlohmann/json.hpp>

namespace Wiesel {

// Runtime game loader - read-only asset scanning and configuration.
// Used by standalone games. Never generates handles or writes files.
class GameLoader {
 public:
  // Mount an assets directory to VFS /app.
  static bool MountAssets(const std::filesystem::path& assets_dir);

  // Scan the mounted /app directory for assets (read-only).
  // Reads handles from .meta files and JSON asset files.
  // Skips assets without pre-existing handles.
  static void ScanAssets();

  // Apply render options to the renderer.
  static void ApplyRenderOptions(const GameInfo& info);

  // Capture current renderer settings into serialized render options.
  static void CaptureRenderOptions(RenderOptionsSerialized& out_opts);

  // Apply input settings to the input manager.
  static void ApplyInputSettings(const GameInfo& info);

  // Load the start scene into the active scene.
  static bool LoadStartScene(const GameInfo& info);

  // Convenience: mount, scan, apply settings, load start scene.
  static bool LoadAll(const GameInfo& info,
                      const std::filesystem::path& assets_dir);

  struct MetaFileData {
    AssetHandle handle;
    nlohmann::json properties;  // empty if none
  };

  static MetaFileData ReadMetaFile(const std::filesystem::path& meta_path);
};

}  // namespace Wiesel
