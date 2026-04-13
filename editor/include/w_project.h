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
// Created by Metehan Gezer on 05.03.2026.
//

#pragma once

#include "game/w_game_info.h"
#include "w_pch.h"

namespace Wiesel {

struct ProjectSettings {
  std::string name = "Untitled Project";
  int version = 1;
  AssetHandle last_scene;

  // Editor state (persisted but only used by editor)
  struct EditorCameraState {
    glm::vec3 position = {0, 5, -10};
    float yaw = 0.0f;
    float pitch = -15.0f;
    float speed = 10.0f;
    float sensitivity = 160.0f;
    int mode = 0;  // 0 = Free, 1 = 2D
    float zoom_2d = 5.0f;
    float fov = 60.0f;
  } editor_camera;
};

enum class ProjectLoadResult {
  Success,
  FileNotFound,
  ParseError,
  IncompatibleVersion,
};

class Project {
 public:
  Project() = default;

  static bool Create(const std::filesystem::path& directory,
                     const std::string& name);
  static std::pair<ProjectLoadResult, std::unique_ptr<Project>> Load(
      const std::filesystem::path& project_file);
  bool Save() const;

  const std::filesystem::path& GetProjectFile() const { return project_file_; }

  std::filesystem::path GetProjectDirectory() const {
    return project_file_.parent_path();
  }

  std::filesystem::path GetAssetsDirectory() const {
    return GetProjectDirectory() / "assets";
  }

  std::filesystem::path GetScenesDirectory() const {
    return GetAssetsDirectory() / "scenes";
  }

  std::filesystem::path GetGameInfoPath() const {
    return GetProjectDirectory() / "gameinfo.wgame";
  }

  ProjectSettings& GetSettings() { return settings_; }

  const ProjectSettings& GetSettings() const { return settings_; }

  GameInfo& GetGameInfo() { return game_info_; }

  const GameInfo& GetGameInfo() const { return game_info_; }

 private:
  std::filesystem::path project_file_;
  ProjectSettings settings_;
  GameInfo game_info_;
};

}  // namespace Wiesel
