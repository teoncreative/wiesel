
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "w_game_application.hpp"
#include "project/w_project_loader.hpp"
#include "scene/w_scene_manager.hpp"

namespace Wiesel {

bool GameApplication::LoadProjectAndScene(
    const std::filesystem::path& project_path) {
  auto proj = Project::Load(project_path);
  if (!proj) {
    LOG_ERROR("Failed to load project: {}", project_path.string());
    return false;
  }

  Engine::SetProject(std::move(proj));

  SceneManager& sm = Engine::scene_manager();
  if (!sm.GetActiveScene()) {
    sm.CreateScene();
  }

  return ProjectLoader::LoadAll(*Engine::project());
}

}  // namespace Wiesel