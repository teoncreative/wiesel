//
// Created by Claude on 05.03.2026.
//

#include "project/w_project.hpp"

#include <nlohmann/json.hpp>

#include "util/w_logger.hpp"

namespace Wiesel {

Project* Project::active_ = nullptr;

bool Project::Create(const std::filesystem::path& directory,
                     const std::string& name) {
  namespace fs = std::filesystem;

  std::error_code ec;
  fs::create_directories(directory, ec);
  if (ec) {
    LOG_ERROR("Failed to create project directory: {}", ec.message());
    return false;
  }

  fs::create_directories(directory / "assets", ec);
  fs::create_directories(directory / "assets" / "models", ec);
  fs::create_directories(directory / "assets" / "textures", ec);
  fs::create_directories(directory / "assets" / "scenes", ec);

  ProjectSettings settings;
  settings.name = name;
  settings.start_scene = "scenes/main.wscene";
  settings.last_scene = "scenes/main.wscene";
  settings.scenes.push_back("scenes/main.wscene");

  nlohmann::json j;
  j["name"] = settings.name;
  j["version"] = settings.version;
  j["start_scene"] = settings.start_scene;
  j["last_scene"] = settings.last_scene;
  j["scenes"] = settings.scenes;

  std::filesystem::path project_file = directory / (name + ".wiesel");
  std::ofstream file(project_file);
  if (!file.is_open()) {
    LOG_ERROR("Failed to create project file: {}", project_file.string());
    return false;
  }
  file << j.dump(2);
  return true;
}

std::unique_ptr<Project> Project::Load(
    const std::filesystem::path& project_file) {
  std::ifstream file(project_file);
  if (!file.is_open()) {
    LOG_ERROR("Failed to open project file: {}", project_file.string());
    return nullptr;
  }

  nlohmann::json j;
  try {
    file >> j;
  } catch (const nlohmann::json::parse_error& e) {
    LOG_ERROR("Failed to parse project file: {}", e.what());
    return nullptr;
  }

  auto project = std::make_unique<Project>();
  project->project_file_ = std::filesystem::absolute(project_file);
  project->settings_.name = j.value("name", "Untitled Project");
  project->settings_.version = j.value("version", 1);
  project->settings_.start_scene = j.value("start_scene", j.value("default_scene", ""));
  project->settings_.last_scene = j.value("last_scene", "");

  if (j.contains("scenes") && j["scenes"].is_array()) {
    for (const auto& scene : j["scenes"]) {
      project->settings_.scenes.push_back(scene.get<std::string>());
    }
  }

  return project;
}

bool Project::Save() const {
  nlohmann::json j;
  j["name"] = settings_.name;
  j["version"] = settings_.version;
  j["start_scene"] = settings_.start_scene;
  j["last_scene"] = settings_.last_scene;
  j["scenes"] = settings_.scenes;

  std::ofstream file(project_file_);
  if (!file.is_open()) {
    LOG_ERROR("Failed to save project file: {}", project_file_.string());
    return false;
  }
  file << j.dump(2);
  return true;
}

void Project::AddScene(const std::string& relative_path) {
  auto it = std::find(settings_.scenes.begin(), settings_.scenes.end(),
                      relative_path);
  if (it == settings_.scenes.end()) {
    settings_.scenes.push_back(relative_path);
  }
}

void Project::RemoveScene(const std::string& relative_path) {
  std::erase(settings_.scenes, relative_path);
}

}  // namespace Wiesel