//
// Created by Claude on 05.03.2026.
//

#pragma once

#include "w_pch.hpp"

namespace Wiesel {

struct RenderOptionsSerialized {
  bool ssao_enabled = true;
  bool bloom_enabled = false;
  float bloom_threshold = 0.7f;
  float bloom_intensity = 0.6f;
  bool motion_blur_enabled = false;
  float motion_blur_strength = 1.0f;
  int motion_blur_samples = 8;
  bool shadows_enabled = true;
  bool vsync = false;
  int aa_mode = 0;  // 0=None, 1=FXAA, 2=TAA
};

struct ProjectSettings {
  std::string name = "Untitled Project";
  int version = 1;
  std::string start_scene;   // scene used when running the game
  std::string last_scene;    // last scene the editor had open
  std::vector<std::string> scenes;
  RenderOptionsSerialized render_options;
};

class Project {
 public:
  Project() = default;

  static bool Create(const std::filesystem::path& directory, const std::string& name);
  static std::unique_ptr<Project> Load(const std::filesystem::path& project_file);
  bool Save() const;

  const std::filesystem::path& GetProjectFile() const { return project_file_; }
  std::filesystem::path GetProjectDirectory() const { return project_file_.parent_path(); }
  std::filesystem::path GetAssetsDirectory() const { return GetProjectDirectory() / "assets"; }
  std::filesystem::path GetScenesDirectory() const { return GetAssetsDirectory() / "scenes"; }

  ProjectSettings& GetSettings() { return settings_; }
  const ProjectSettings& GetSettings() const { return settings_; }

  void AddScene(const std::string& relative_path);
  void RemoveScene(const std::string& relative_path);

  static Project* GetActive() { return active_; }
  static void SetActive(Project* project) { active_ = project; }

 private:
  std::filesystem::path project_file_;
  ProjectSettings settings_;

  static Project* active_;
};

}  // namespace Wiesel
