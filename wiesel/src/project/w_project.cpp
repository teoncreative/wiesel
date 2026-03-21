//
// Created by Metehan Gezer on 05.03.2026.
//

#include "project/w_project.hpp"

#include <nlohmann/json.hpp>

#include "util/w_gamepadcodes.hpp"
#include "util/w_keycodes.hpp"
#include "util/w_logger.hpp"

namespace Wiesel {

Project* Project::active_ = nullptr;

// --- Serialization helpers ---

static nlohmann::json SerializeAction(const InputAction& action) {
  nlohmann::json aj;
  aj["name"] = action.name;

  nlohmann::json keys = nlohmann::json::array();
  for (auto k : action.keys) keys.push_back(KeyCodeToString(k));
  aj["keys"] = keys;

  if (!action.buttons.empty()) {
    nlohmann::json btns = nlohmann::json::array();
    for (auto b : action.buttons) btns.push_back(GamepadButtonToString(b));
    aj["buttons"] = btns;
  }
  return aj;
}

static nlohmann::json SerializeAxis(const InputAxisMapping& axis) {
  nlohmann::json axj;
  axj["name"] = axis.name;

  nlohmann::json pos = nlohmann::json::array();
  for (auto k : axis.positive_keys) pos.push_back(KeyCodeToString(k));
  axj["positive_keys"] = pos;

  nlohmann::json neg = nlohmann::json::array();
  for (auto k : axis.negative_keys) neg.push_back(KeyCodeToString(k));
  axj["negative_keys"] = neg;

  if (axis.gamepad_axis >= 0) {
    axj["stick"] = GamepadAxisToString(axis.gamepad_axis);
    if (axis.invert_axis) axj["invert"] = true;
    if (axis.dead_zone != 0.15f) axj["dead_zone"] = axis.dead_zone;
  }

  axj["gravity"] = axis.gravity;
  axj["sensitivity"] = axis.sensitivity;
  return axj;
}

static InputAction DeserializeAction(const nlohmann::json& aj) {
  InputAction action;
  action.name = aj.value("name", "");

  // Keys: string or legacy integer
  if (aj.contains("keys") && aj["keys"].is_array()) {
    for (auto& k : aj["keys"]) {
      int32_t code = k.is_string() ? StringToKeyCode(k.get<std::string>())
                                   : k.get<int32_t>();
      if (code != KeyUnknown) action.keys.push_back(code);
    }
  }

  // Gamepad buttons
  if (aj.contains("buttons") && aj["buttons"].is_array()) {
    for (auto& b : aj["buttons"]) {
      int32_t btn = b.is_string() ? StringToGamepadButton(b.get<std::string>())
                                  : b.get<int32_t>();
      if (btn >= 0) action.buttons.push_back(btn);
    }
  }
  return action;
}

static InputAxisMapping DeserializeAxis(const nlohmann::json& axj) {
  InputAxisMapping axis;
  axis.name = axj.value("name", "");
  axis.gravity = axj.value("gravity", 3.0f);
  axis.sensitivity = axj.value("sensitivity", 3.0f);

  auto parse_key = [](const nlohmann::json& k) -> int32_t {
    return k.is_string() ? StringToKeyCode(k.get<std::string>()) : k.get<int32_t>();
  };

  if (axj.contains("positive_keys") && axj["positive_keys"].is_array()) {
    for (auto& k : axj["positive_keys"]) {
      int32_t code = parse_key(k);
      if (code != KeyUnknown) axis.positive_keys.push_back(code);
    }
  }
  if (axj.contains("negative_keys") && axj["negative_keys"].is_array()) {
    for (auto& k : axj["negative_keys"]) {
      int32_t code = parse_key(k);
      if (code != KeyUnknown) axis.negative_keys.push_back(code);
    }
  }

  // Gamepad axis
  if (axj.contains("stick") && axj["stick"].is_string()) {
    axis.gamepad_axis = StringToGamepadAxis(axj["stick"].get<std::string>());
    axis.invert_axis = axj.value("invert", false);
    axis.dead_zone = axj.value("dead_zone", 0.15f);
  }

  return axis;
}

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

  ProjectSettings settings;
  settings.name = name;
  settings.start_scene = "scenes/main.wscene";
  settings.last_scene = "scenes/main.wscene";
  settings.scenes.push_back("scenes/main.wscene");

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
  settings.input.contexts["keyboard"] = std::move(keyboard);

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
  settings.input.contexts["gamepad"] = std::move(gamepad);

  Project proj;
  proj.settings_ = settings;
  proj.project_file_ = directory / (name + ".wiesel");
  return proj.Save();
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

  if (j.contains("render_options")) {
    auto& ro = j["render_options"];
    auto& opts = project->settings_.render_options;
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

  // Input settings
  if (j.contains("input")) {
    auto& ij = j["input"];
    auto& input = project->settings_.input;
    input.mouse_sensitivity_x = ij.value("mouse_sensitivity_x", 80.0f);
    input.mouse_sensitivity_y = ij.value("mouse_sensitivity_y", 80.0f);
    input.mouse_axis_limit_y = ij.value("mouse_axis_limit_y", 75.0f);

    if (ij.contains("contexts") && ij["contexts"].is_object()) {
      for (auto& [ctx_name, ctx_json] : ij["contexts"].items()) {
        InputContext ctx;
        ctx.name = ctx_name;

        if (ctx_json.contains("actions") && ctx_json["actions"].is_array()) {
          for (auto& aj : ctx_json["actions"]) {
            auto action = DeserializeAction(aj);
            if (!action.name.empty()) ctx.actions.push_back(std::move(action));
          }
        }
        if (ctx_json.contains("axes") && ctx_json["axes"].is_array()) {
          for (auto& axj : ctx_json["axes"]) {
            auto axis = DeserializeAxis(axj);
            if (!axis.name.empty()) ctx.axes.push_back(std::move(axis));
          }
        }
        input.contexts[ctx_name] = std::move(ctx);
      }
    }

    // Legacy: flat actions/axes at input root -> migrate into "keyboard" context
    if (!ij.contains("contexts") && (ij.contains("actions") || ij.contains("axes"))) {
      InputContext legacy;
      legacy.name = "keyboard";
      if (ij.contains("actions") && ij["actions"].is_array()) {
        for (auto& aj : ij["actions"]) {
          auto action = DeserializeAction(aj);
          if (!action.name.empty()) legacy.actions.push_back(std::move(action));
        }
      }
      if (ij.contains("axes") && ij["axes"].is_array()) {
        for (auto& axj : ij["axes"]) {
          auto axis = DeserializeAxis(axj);
          if (!axis.name.empty()) legacy.axes.push_back(std::move(axis));
        }
      }
      input.contexts["keyboard"] = std::move(legacy);
    }
  }

  // Editor camera state
  if (j.contains("editor_camera")) {
    auto& ecj = j["editor_camera"];
    auto& ec = project->settings_.editor_camera;
    if (ecj.contains("position") && ecj["position"].is_array() && ecj["position"].size() >= 3) {
      ec.position = {ecj["position"][0], ecj["position"][1], ecj["position"][2]};
    }
    ec.yaw = ecj.value("yaw", 180.0f);
    ec.pitch = ecj.value("pitch", -15.0f);
    ec.speed = ecj.value("speed", 10.0f);
    ec.sensitivity = ecj.value("sensitivity", 160.0f);
    ec.mode = ecj.value("mode", 0);
    ec.zoom_2d = ecj.value("zoom_2d", 5.0f);
    ec.fov = ecj.value("fov", 60.0f);
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

  {
    auto& opts = settings_.render_options;
    j["render_options"] = {
        {"ssao_enabled", opts.ssao_enabled},
        {"bloom_enabled", opts.bloom_enabled},
        {"bloom_threshold", opts.bloom_threshold},
        {"bloom_intensity", opts.bloom_intensity},
        {"motion_blur_enabled", opts.motion_blur_enabled},
        {"motion_blur_strength", opts.motion_blur_strength},
        {"motion_blur_samples", opts.motion_blur_samples},
        {"shadows_enabled", opts.shadows_enabled},
        {"vsync", opts.vsync},
        {"aa_mode", opts.aa_mode},
        {"msaa_mode", opts.msaa_mode},
    };
  }

  // Input settings
  {
    auto& input = settings_.input;
    nlohmann::json ij;
    ij["mouse_sensitivity_x"] = input.mouse_sensitivity_x;
    ij["mouse_sensitivity_y"] = input.mouse_sensitivity_y;
    ij["mouse_axis_limit_y"] = input.mouse_axis_limit_y;

    nlohmann::json contexts_json = nlohmann::json::object();
    for (auto& [ctx_name, ctx] : input.contexts) {
      nlohmann::json cj;

      nlohmann::json actions_json = nlohmann::json::array();
      for (auto& action : ctx.actions) {
        actions_json.push_back(SerializeAction(action));
      }
      cj["actions"] = actions_json;

      nlohmann::json axes_json = nlohmann::json::array();
      for (auto& axis : ctx.axes) {
        axes_json.push_back(SerializeAxis(axis));
      }
      cj["axes"] = axes_json;

      contexts_json[ctx_name] = cj;
    }
    ij["contexts"] = contexts_json;

    j["input"] = ij;
  }

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