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

#include "asset/w_asset_handle.hpp"
#include "game/w_game_loader.hpp"
#include "w_project.hpp"

#include <nlohmann/json.hpp>

namespace Wiesel {

// Editor-only project loader. Handles asset importing (read/write),
// .meta file creation, and project-level loading.
class ProjectLoader {
 public:
  // Mount a project's VFS and set it as active.
  static bool MountProject(Project& project);

  // Scan assets with full import capability (generates handles, writes .meta).
  static void ScanAssets(Project& project);

  // Apply settings from the project's GameInfo.
  static void ApplyRenderOptions(Project& project);
  static void ApplyInputSettings(Project& project);
  static bool LoadStartScene(Project& project);

  // Convenience: do everything (mount, scan, scripts, options, start scene).
  static bool LoadAll(Project& project);

  // Import a single asset file via VFS. Handles JSON-embedded handles for
  // JSON asset types and .meta sidecar files for binary assets.
  // Can generate new handles and write them back (editor-only capability).
  static AssetHandle ImportAsset(const std::string& name, AssetType type,
                                 const std::string& vfs_path);

  static void WriteMetaFile(const std::filesystem::path& meta_path,
                            const AssetHandle& handle,
                            AssetType type = AssetType::None,
                            const void* properties = nullptr);
};

}  // namespace Wiesel
