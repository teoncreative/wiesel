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
#include "game/w_game_loader.h"
#include "w_project.h"

#include <nlohmann/json.hpp>

namespace wiesel {

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

  // Convenience: mount, scan, compile scripts, apply options.
  // Pass load_start_scene=true to also open the start scene.
  static bool LoadAll(Project& project, bool load_start_scene = true);
};

}  // namespace wiesel
