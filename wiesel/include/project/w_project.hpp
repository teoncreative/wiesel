//
// Created by Metehan Gezer on 05.03.2026.
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
  int msaa_mode = 0;  // SamplingMode enum value
};

// Input action: a named action mapped to keyboard keys and/or gamepad buttons
struct InputAction {
  std::string name;                      // e.g. "Jump", "Fire"
  std::vector<int32_t> keys;             // KeyCode values
  std::vector<int32_t> buttons;          // GamepadButton values
};

// Input axis: mapped to keys (digital) and/or a gamepad stick/trigger (analog)
struct InputAxisMapping {
  std::string name;                      // e.g. "Horizontal", "Vertical"
  std::vector<int32_t> positive_keys;    // Keys that push toward +1
  std::vector<int32_t> negative_keys;    // Keys that push toward -1
  int32_t gamepad_axis = -1;             // GamepadAxis (-1 = none)
  bool invert_axis = false;              // Flip gamepad axis direction
  float dead_zone = 0.15f;
  float gravity = 3.0f;                  // How fast digital axis returns to 0
  float sensitivity = 3.0f;              // How fast digital axis reaches 1/-1
};

// A named set of input bindings (e.g. "keyboard", "gamepad", "keyboard_p2")
struct InputContext {
  std::string name;
  std::vector<InputAction> actions;
  std::vector<InputAxisMapping> axes;
};

struct InputSettings {
  std::map<std::string, InputContext> contexts;  // name -> context
  float mouse_sensitivity_x = 80.0f;
  float mouse_sensitivity_y = 80.0f;
  float mouse_axis_limit_y = 75.0f;
};

struct ProjectSettings {
  std::string name = "Untitled Project";
  int version = 1;
  std::string start_scene;   // scene used when running the game
  std::string last_scene;    // last scene the editor had open
  std::vector<std::string> scenes;
  RenderOptionsSerialized render_options;
  InputSettings input;

  // Editor state (persisted but only used by editor)
  struct EditorCameraState {
    glm::vec3 position = {0, 5, -10};
    float yaw = 180.0f;
    float pitch = -15.0f;
    float speed = 10.0f;
    float sensitivity = 160.0f;
    int mode = 0;  // 0 = Free, 1 = 2D
    float zoom_2d = 5.0f;
    float fov = 60.0f;
  } editor_camera;
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

 private:
  std::filesystem::path project_file_;
  ProjectSettings settings_;
};

}  // namespace Wiesel