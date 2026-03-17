
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "asset/w_asset_handle.hpp"
#include "project/w_project.hpp"
#include "scene/w_scene.hpp"

namespace Wiesel {

class ProjectLoader {
 public:
  // Mount a project's VFS and set it as active.
  static bool MountProject(Project& project);

  // Scan .meta files, register assets, auto-import new files.
  static void ScanAssets(Project& project);

  // Apply saved render options from the project.
  static void ApplyRenderOptions(Project& project);

  // Apply input mappings from the project.
  static void ApplyInputSettings(Project& project);

  // Load the project's start scene into the given scene object.
  static bool LoadStartScene(Project& project, Ref<Scene> scene);

  // Convenience: do everything (mount, scan, scripts, options, start scene).
  static bool LoadAll(Project& project, Ref<Scene> scene);

  // Utilities (used by editor for import/browser)
  static AssetType ExtToAssetType(const std::string& ext);
  static std::string ReadMetaFile(const std::filesystem::path& meta_path);
  static void WriteMetaFile(const std::filesystem::path& meta_path,
                            const AssetHandle& handle);
};

}  // namespace Wiesel