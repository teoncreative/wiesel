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

#include "w_project.h"

#include <nlohmann/json.hpp>

#include "util/w_gamepadcodes.h"
#include "util/w_keycodes.h"
#include "util/w_logger.h"
#include "w_engine.h"

namespace wiesel {

// --- Project ---

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

  Project proj;
  proj.project_file_ = directory / (name + ".wiesel");
  proj.settings_.name = name;
  proj.game_info_.name = name;

  // Default "keyboard" context
  InputContext keyboard;
  keyboard.name = "keyboard";
  keyboard.actions = {
      {"Up", {KeyArrowUp, KeyW}, {}},
      {"Down", {KeyArrowDown, KeyS}, {}},
      {"Left", {KeyArrowLeft, KeyA}, {}},
      {"Right", {KeyArrowRight, KeyD}, {}},
      {"Jump", {KeySpace}, {}},
      {"Enter", {KeyEnter}, {}},
      {"Shift", {KeyLeftShift, KeyRightShift}, {}},
      {"Control", {KeyLeftControl, KeyRightControl}, {}},
      {"Tab", {KeyTab}, {}},
  };
  keyboard.axes = {
      {"Horizontal", {KeyArrowRight, KeyD}, {KeyArrowLeft, KeyA}},
      {"Vertical", {KeyArrowUp, KeyW}, {KeyArrowDown, KeyS}},
  };
  proj.game_info_.input.contexts["keyboard"] = std::move(keyboard);

  // Default "gamepad" context
  InputContext gamepad;
  gamepad.name = "gamepad";
  gamepad.actions = {
      {"Jump", {}, {GamepadButtonA}},
      {"Enter", {}, {GamepadButtonStart}},
  };
  gamepad.axes = {
      {"Horizontal", {}, {}, GamepadAxisLeftX, false, 0.15f},
      {"Vertical", {}, {}, GamepadAxisLeftY, true, 0.15f},
  };
  proj.game_info_.input.contexts["gamepad"] = std::move(gamepad);

  return proj.Save();
}

std::pair<ProjectLoadResult, std::unique_ptr<Project>> Project::Load(
    const std::filesystem::path& project_file) {
  namespace fs = std::filesystem;

  std::ifstream file(project_file);
  if (!file.is_open()) {
    LOG_ERROR("Failed to open project file: {}", project_file.string());
    return {ProjectLoadResult::FileNotFound, nullptr};
  }

  nlohmann::json j;
  try {
    file >> j;
  } catch (const nlohmann::json::parse_error& e) {
    LOG_ERROR("Failed to parse project file: {}", e.what());
    return {ProjectLoadResult::ParseError, nullptr};
  }

  // Check engine version compatibility
  static const std::vector<std::string> kCompatibleVersions = {
      kEngineVersion,
  };
  std::string saved_engine_version = j.value("engine_version", "");
  if (!saved_engine_version.empty()) {
    bool compatible = false;
    for (const auto& v : kCompatibleVersions) {
      if (saved_engine_version == v) {
        compatible = true;
        break;
      }
    }
    if (!compatible) {
      LOG_ERROR(
          "Project was saved with engine version {} which is not compatible "
          "with current engine version {}",
          saved_engine_version, kEngineVersion);
      return {ProjectLoadResult::IncompatibleVersion, nullptr};
    }
  }

  auto project = std::make_unique<Project>();
  project->project_file_ = fs::absolute(project_file);

  // Editor-only fields
  project->settings_.name = j.value("name", "Untitled Project");
  project->settings_.version = j.value("version", 1);
  project->settings_.last_scene =
      AssetHandle::FromString(j.value("last_scene", ""));

  // Editor camera state
  if (j.contains("editor_camera")) {
    auto& ecj = j["editor_camera"];
    auto& ec = project->settings_.editor_camera;
    if (ecj.contains("position") && ecj["position"].is_array() &&
        ecj["position"].size() >= 3) {
      ec.position = {ecj["position"][0], ecj["position"][1],
                     ecj["position"][2]};
    }
    ec.yaw = ecj.value("yaw", 0.0f);
    ec.pitch = ecj.value("pitch", -15.0f);
    ec.speed = ecj.value("speed", 10.0f);
    ec.sensitivity = ecj.value("sensitivity", 160.0f);
    ec.mode = ecj.value("mode", 0);
    ec.zoom_2d = ecj.value("zoom_2d", 5.0f);
    ec.fov = ecj.value("fov", 60.0f);
  }

  // Load GameInfo from gameinfo.wgame
  fs::path game_info_path = project->GetGameInfoPath();
  if (fs::exists(game_info_path)) {
    auto info = GameInfo::Load(game_info_path);
    if (info) {
      project->game_info_ = std::move(*info);
    }
  } else {
    // Migration: extract runtime fields from old .wiesel format
    project->game_info_.name = project->settings_.name;
    project->game_info_.start_scene =
        AssetHandle::FromString(j.value("start_scene", ""));

    // Render options
    if (j.contains("render_options")) {
      auto& ro = j["render_options"];
      auto& opts = project->game_info_.render_options;
      if (ro.contains("ambient_color") && ro["ambient_color"].is_array()) {
        opts.ambient_color = {ro["ambient_color"][0], ro["ambient_color"][1],
                              ro["ambient_color"][2]};
      }
      opts.ambient_intensity = ro.value("ambient_intensity", 0.03f);
      opts.ssao_enabled = ro.value("ssao_enabled", true);
      opts.bloom_enabled = ro.value("bloom_enabled", false);
      opts.bloom_threshold = ro.value("bloom_threshold", 0.7f);
      opts.bloom_intensity = ro.value("bloom_intensity", 0.6f);
      opts.motion_blur_enabled = ro.value("motion_blur_enabled", false);
      opts.motion_blur_strength = ro.value("motion_blur_strength", 1.0f);
      opts.motion_blur_samples = ro.value("motion_blur_samples", 8);
      opts.shadows_enabled = ro.value("shadows_enabled", true);
      opts.vsync = ro.value("vsync", false);
      opts.aa_mode = ro.value("aa_mode", 0);
      opts.msaa_mode = ro.value("msaa_mode", 0);
    }

    // Input settings - use GameInfo::Load's logic by saving and reloading
    // Actually, just parse inline since we already have the JSON
    if (j.contains("input")) {
      // Create a temporary JSON with just the input section and use GameInfo
      nlohmann::json temp;
      temp["name"] = project->game_info_.name;
      temp["start_scene"] = "";
      temp["input"] = j["input"];
      // Write temp gameinfo to migrate
      std::ofstream out(game_info_path);
      if (out.is_open()) {
        // Also include render options
        if (j.contains("render_options")) {
          temp["render_options"] = j["render_options"];
        }
        out << temp.dump(2);
        out.close();
        // Re-load properly
        auto info = GameInfo::Load(game_info_path);
        if (info) {
          project->game_info_ = std::move(*info);
        }
      }
    }

    LOG_INFO("Migrated project settings to gameinfo.wgame");
  }

  return {ProjectLoadResult::Success, std::move(project)};
}

bool Project::Save() const {
  // Save editor-only state to .wiesel
  {
    nlohmann::json j;
    j["name"] = settings_.name;
    j["version"] = settings_.version;
    j["engine_version"] = kEngineVersion;
    j["last_scene"] =
        settings_.last_scene.IsValid() ? settings_.last_scene.ToString() : "";

    // Editor camera state
    {
      auto& ec = settings_.editor_camera;
      nlohmann::json ecj;
      ecj["position"] = {ec.position.x, ec.position.y, ec.position.z};
      ecj["yaw"] = ec.yaw;
      ecj["pitch"] = ec.pitch;
      ecj["speed"] = ec.speed;
      ecj["sensitivity"] = ec.sensitivity;
      ecj["mode"] = ec.mode;
      ecj["zoom_2d"] = ec.zoom_2d;
      ecj["fov"] = ec.fov;
      j["editor_camera"] = ecj;
    }

    std::ofstream file(project_file_);
    if (!file.is_open()) {
      LOG_ERROR("Failed to save project file: {}", project_file_.string());
      return false;
    }
    file << j.dump(2);
  }

  // Save runtime config to gameinfo.wgame
  game_info_.Save(GetGameInfoPath());

  return true;
}

}  // namespace wiesel
