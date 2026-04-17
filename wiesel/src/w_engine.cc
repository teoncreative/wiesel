
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "w_engine.h"
#include "audio/w_audio.h"
#include "behavior/w_native_behavior.h"
#include "networking/w_network.h"
#include "networking/w_network_component_serializer.h"
#include "script/w_script_field_registry.h"
#include "networking/w_network_scene_manager.h"
#include "networking/w_replication_manager.h"
#include "networking/w_replication_packets.h"
#include "util/w_discord_rpc.h"

#include <cxxopts.hpp>
#include <thread>
#include "asset/w_asset_manager.h"
#include "asset/w_asset_registry.h"
#include "asset/w_model_loader.h"
#include "core/w_reflect_init.h"
#include "cursor/w_cursor.h"
#include "game/w_game_info.h"
#include "input/w_input.h"
#include "physics/w_mesh_collider_asset.h"
#include "rendering/w_primitives.h"
#include "rendering/w_sprite.h"
#include "rendering/w_sprite_asset.h"
#include "scene/w_component_serializer.h"
#include "scene/w_entity.h"
#include "scene/w_lights.h"
#include "scene/w_scene_manager.h"
#include "script/w_scriptmanager.h"
#include "ui/w_font.h"
#include "ui/w_ui_document.h"
#include "ui/w_ui_manager.h"
#include "util/w_dialogs.h"
#include <urkern/platform.h>
#include <urkern/thread_pool.h>
#include "util/w_user_config.h"
#include "window/w_sdlwindow.h"

namespace wiesel {

EngineProperties EngineProperties::Parse(int argc, char** argv) {
  EngineProperties config;
  std::filesystem::path exe_dir = urkern::GetExecutableDirectory();

  // Setup cxxopts
  cxxopts::Options options(argv[0], "Wiesel Game Engine");

  options.add_options()("e,editor", "Enable editor layer",
                        cxxopts::value<bool>()->default_value("false"))(
      "engine-assets", "Path to engine assets directory or pak file",
      cxxopts::value<std::string>())(
      "editor-assets", "Path to editor assets directory or pak file",
      cxxopts::value<std::string>())("app-assets",
                                     "Path to application assets directory",
                                     cxxopts::value<std::string>())(
      "project", "Path to project file (editor)",
      cxxopts::value<std::string>())("game-info", "Path to gameinfo.wgame file",
                                     cxxopts::value<std::string>())(
      "enable-stdio", "Keep console window visible (runtime)",
      cxxopts::value<bool>()->default_value("false"))(
      "h,help", "Print usage information");

  try {
    auto result = options.parse(argc, argv);

    // Handle help
    if (result.count("help")) {
      std::cout << options.help() << std::endl;
      std::exit(0);
    }

    // Parse flags
    config.editor_enabled = result["editor"].as<bool>();
    config.enable_stdio = result["enable-stdio"].as<bool>();

    // Parse optional paths
    if (result.count("engine-assets")) {
      config.engine_assets_path = result["engine-assets"].as<std::string>();
    }

    if (result.count("editor-assets")) {
      config.editor_assets_path = result["editor-assets"].as<std::string>();
    }

    if (result.count("app-assets")) {
      config.app_assets_path = result["app-assets"].as<std::string>();
    }

    if (result.count("project")) {
      config.project_path = result["project"].as<std::string>();
    }

    if (result.count("game-info")) {
      config.game_info_path = result["game-info"].as<std::string>();
    }

  } catch (const cxxopts::exceptions::exception& e) {
    std::cerr << "Error parsing arguments: " << e.what() << std::endl;
    std::cerr << options.help() << std::endl;
    std::exit(1);
  }

  // Fallback to environment variables
  if (config.engine_assets_path.empty()) {
    if (const char* env = std::getenv("WIESEL_ENGINE_ASSETS")) {
      config.engine_assets_path = env;
    }
  }

  if (config.editor_assets_path.empty() && config.editor_enabled) {
    if (const char* env = std::getenv("WIESEL_EDITOR_ASSETS")) {
      config.editor_assets_path = env;
    }
  }

  if (config.app_assets_path.empty()) {
    if (const char* env = std::getenv("WIESEL_APP_ASSETS")) {
      config.app_assets_path = env;
    }
  }

  if (config.project_path.empty()) {
    if (const char* env = std::getenv("WIESEL_PROJECT_PATH")) {
      config.project_path = env;
    }
  }

  if (config.game_info_path.empty()) {
    if (const char* env = std::getenv("WIESEL_GAME_INFO")) {
      config.game_info_path = env;
    }
  }

  // Auto-discover gameinfo.wgame next to exe or in CWD
  if (config.game_info_path.empty()) {
    std::filesystem::path cwd_info = "gameinfo.wgame";
    std::filesystem::path exe_info = exe_dir / "gameinfo.wgame";
    if (std::filesystem::exists(cwd_info)) {
      config.game_info_path = cwd_info;
    } else if (std::filesystem::exists(exe_info)) {
      config.game_info_path = exe_info;
    }
  }

  // Final fallback to default locations
  if (config.engine_assets_path.empty()) {
    // Development: Look for source tree
    std::filesystem::path dev_path = exe_dir / "../../../engine/assets";
    if (std::filesystem::exists(dev_path)) {
      config.engine_assets_path = dev_path;
    } else {
      // Bundle: Look for engine/ directory next to executable
      std::filesystem::path bundle_path = exe_dir / "engine";
      if (std::filesystem::exists(bundle_path)) {
        config.engine_assets_path = bundle_path;
      }
    }
  }

  if (config.editor_assets_path.empty() && config.editor_enabled) {
    std::filesystem::path dev_path = exe_dir / "../../../editor/assets";
    if (std::filesystem::exists(dev_path)) {
      config.editor_assets_path = dev_path;
    } else {
      // Bundle: Look for editor/ directory next to executable
      std::filesystem::path bundle_path = exe_dir / "editor";
      if (std::filesystem::exists(bundle_path)) {
        config.editor_assets_path = bundle_path;
      }
    }
  }

  if (config.app_assets_path.empty()) {
    // Default to assets/ relative to current working directory
    std::filesystem::path default_app =
        std::filesystem::current_path() / "assets";
    if (std::filesystem::exists(default_app)) {
      config.app_assets_path = default_app;
    }
  }

  if (config.user_data_path.empty()) {
    config.user_data_path = urkern::GetUserDataDirectory("Wiesel");
  }

  return config;
}

EngineProperties Engine::properties_;
std::shared_ptr<Renderer> Engine::renderer_;
std::shared_ptr<AppWindow> Engine::window_;
std::shared_ptr<VirtualFileSystem> Engine::vfs_;
Application* Engine::application_;
std::unique_ptr<InputManager> Engine::input_manager_;
std::unique_ptr<AssetManager> Engine::asset_manager_;
std::unique_ptr<ScriptManager> Engine::script_manager_;
std::unique_ptr<NativeBehaviorRegistry> Engine::behavior_registry_;
std::unique_ptr<urkern::ThreadPool> Engine::thread_pool_;
std::unique_ptr<AudioManager> Engine::audio_manager_;
std::unique_ptr<NetworkManager> Engine::network_manager_;
std::unique_ptr<NetworkSceneManager> Engine::network_scene_manager_;
std::unique_ptr<ReplicationManager> Engine::replication_manager_;
std::unique_ptr<SceneManager> Engine::scene_manager_;
std::unique_ptr<UIManager> Engine::ui_manager_;
std::unique_ptr<CursorManager> Engine::cursor_manager_;
std::shared_ptr<GameInfo> Engine::game_info_;
std::unique_ptr<UserConfig> Engine::game_config_;
AssetHandle Engine::primitive_cube_;
AssetHandle Engine::primitive_sphere_;
AssetHandle Engine::primitive_plane_;
AssetHandle Engine::primitive_cylinder_;
AssetHandle Engine::primitive_capsule_;
#ifdef WIESEL_DISCORD_RPC
std::unique_ptr<DiscordRPC> Engine::discord_rpc_;
#endif

void Engine::InitEngine(const EngineProperties& props) {
  properties_ = props;
  LOG_INFO("Current work directory: {}",
           std::filesystem::current_path().string());
  LOG_INFO(" - engine_assets_path: {}", props.engine_assets_path.string());
  LOG_INFO(" - editor_enabled: {}", props.editor_enabled);
  LOG_INFO(" - editor_assets_path: {}", props.editor_assets_path.string());
  LOG_INFO(" - app_assets_path: {}", props.app_assets_path.string());
  LOG_INFO(" - project_path: {}", props.project_path.string());
  LOG_INFO(" - user_data_path: {}", props.user_data_path.string());
  asset_manager_ = std::make_unique<AssetManager>();

  InitializeVfs();
  InitializeReflection();
  InitializeComponentSerializers();
  InitializeAssetRegistry();
  input_manager_ = std::make_unique<InputManager>();
  behavior_registry_ = std::make_unique<NativeBehaviorRegistry>();

  // Thread pool for async asset loading: min(cpu_cores, 8)
  uint32_t pool_size = std::min(std::thread::hardware_concurrency(), 8u);
  if (pool_size == 0) {
    pool_size = 4;
  }
  thread_pool_ = std::make_unique<urkern::ThreadPool>(pool_size);
  LOG_INFO("Asset thread pool: {} workers", pool_size);

  network_manager_ = std::make_unique<NetworkManager>();
  network_manager_->Init();
  network_scene_manager_ = std::make_unique<NetworkSceneManager>();
  replication_manager_ = std::make_unique<ReplicationManager>();
  InitializeScriptFieldTypes();
  InitializeNetworkComponentSerializers();
  RegisterReplicationPackets(*network_manager_);
  scene_manager_ = std::make_unique<SceneManager>();
  audio_manager_ = std::make_unique<AudioManager>();
  audio_manager_->Init();
  script_manager_ = std::make_unique<ScriptManager>();
  script_manager_->Init({.enable_debugger = true});
#ifdef WIESEL_DISCORD_RPC
  discord_rpc_ = std::make_unique<DiscordRPC>();
#endif
}

void Engine::InitWindow(const WindowProperties&& props) {
  window_ = std::make_shared<SdlAppWindow>(std::move(props));
  dialogs::Init();
}

void Engine::InitRenderer(const RendererProperties&& props) {
  if (window_ == nullptr) {
    LOG_ERROR("Window should be initialized before renderer!");
    abort();
  }
  renderer_ = std::make_shared<Renderer>(window_);
  renderer_->Initialize(std::move(props));
  RegisterPrimitives();
  ui_manager_ = std::make_unique<UIManager>();
  ui_manager_->Init();
  cursor_manager_ = std::make_unique<CursorManager>();
}

void Engine::InitApplication() {
  LOG_INFO("Initializing app...");
  application_ = CreateApp();
  application_->Init();
}

void Engine::InitializeVfs() {
  namespace fs = std::filesystem;
  vfs_ = std::make_shared<VirtualFileSystem>();

  // Mount physical directories (dev mode)
  auto mount_dir = [](const std::string& prefix, const fs::path& path,
                      int priority) {
    if (!path.empty() && fs::exists(path) && fs::is_directory(path)) {
      vfs_->Mount(prefix, path, priority);
    }
  };

  mount_dir("engine://", properties_.engine_assets_path, 100);
  if (properties_.editor_enabled) {
    mount_dir("editor://", properties_.editor_assets_path, 90);
  }
  mount_dir("app://", properties_.app_assets_path, 80);
  mount_dir("user://", properties_.user_data_path, 0);

  // Scan executable directory for .wpak files (release mode bootstrap)
  fs::path exe_dir = urkern::GetExecutableDirectory();
  if (fs::exists(exe_dir)) {
    std::vector<fs::path> wpak_files;
    for (const auto& entry : fs::directory_iterator(exe_dir)) {
      if (entry.is_regular_file() && entry.path().extension() == ".wpak" &&
          Wpak::IsWpakFile(entry.path())) {
        wpak_files.push_back(entry.path());
      }
    }
    std::sort(wpak_files.begin(), wpak_files.end());
    for (size_t i = 0; i < wpak_files.size(); i++) {
      int priority = 60 - static_cast<int>(i);
      try {
        vfs_->MountPak(wpak_files[i], priority);
        LOG_INFO("Mounted wpak: {}", wpak_files[i].filename().string());
      } catch (const std::exception& e) {
        LOG_ERROR("Failed to mount wpak {}: {}",
                  wpak_files[i].filename().string(), e.what());
      }
    }
  }
}

void Engine::RegisterPrimitives() {
  asset_manager_->RegisterAndStore<Model>(kPrimitiveCube, "Cube", AssetType::Model, "engine://models/cube.obj",
      Primitives::CreateCube());
  vfs_->RegisterVirtualEntry("engine://models/cube.obj");
  asset_manager_->RegisterAndStore<Model>(
      kPrimitiveSphere, "Sphere", AssetType::Model, "engine://models/sphere.obj", Primitives::CreateSphere());
  vfs_->RegisterVirtualEntry("engine://models/sphere.obj");
  asset_manager_->RegisterAndStore<Model>(
      kPrimitivePlane, "Plane", AssetType::Model, "engine://models/plane.obj",
      Primitives::CreatePlane());
  vfs_->RegisterVirtualEntry("engine://models/plane.obj");
  asset_manager_->RegisterAndStore<Model>(
      kPrimitiveCylinder, "Cylinder", AssetType::Model,
      "engine://models/cylinder.obj", Primitives::CreateCylinder());
  vfs_->RegisterVirtualEntry("engine://models/cylinder.obj");
  asset_manager_->RegisterAndStore<Model>(
      kPrimitiveCapsule, "Capsule", AssetType::Model,
      "engine://models/capsule.obj", Primitives::CreateCapsule());
  vfs_->RegisterVirtualEntry("engine://models/capsule.obj");

  primitive_cube_ = kPrimitiveCube;
  primitive_sphere_ = kPrimitiveSphere;
  primitive_plane_ = kPrimitivePlane;
  primitive_cylinder_ = kPrimitiveCylinder;
  primitive_capsule_ = kPrimitiveCapsule;

  LOG_INFO("Registered {} primitive shapes", 5);
}

AssetHandle Engine::GetPrimitive(const std::string& name) {
  if (name == "Cube") {
    return primitive_cube_;
  }
  if (name == "Sphere") {
    return primitive_sphere_;
  }
  if (name == "Plane") {
    return primitive_plane_;
  }
  if (name == "Cylinder") {
    return primitive_cylinder_;
  }
  if (name == "Capsule") {
    return primitive_capsule_;
  }
  return {};
}

void Engine::BroadcastEvent(Event& event) {
  if (application_) {
    application_->OnEvent(event);
  }
}

void Engine::CleanupAssets() {
  asset_manager_->Clear();
  asset_manager_ = nullptr;
}

void Engine::CleanupRenderer() {
  cursor_manager_ = nullptr;
  ui_manager_ = nullptr;
  renderer_->Cleanup();
  renderer_ = nullptr;
}

void Engine::CleanupWindow() {
  window_ = nullptr;
  dialogs::Destroy();
}

void Engine::CleanupEngine() {
  // Shut down thread pool first (waits for pending tasks)
  thread_pool_ = nullptr;
#ifdef WIESEL_DISCORD_RPC
  discord_rpc_ = nullptr;
#endif
  replication_manager_ = nullptr;
  network_scene_manager_ = nullptr;
  if (network_manager_) {
    network_manager_->Shutdown();
    network_manager_ = nullptr;
  }
  scene_manager_ = nullptr;
  game_info_ = nullptr;
  script_manager_ = nullptr;
  behavior_registry_ = nullptr;
  if (audio_manager_) {
    audio_manager_->Shutdown();
    audio_manager_ = nullptr;
  }
  if (game_config_) {
    game_config_->Save();
    game_config_ = nullptr;
  }
  input_manager_ = nullptr;
}

void Engine::CleanupApplication() {
  LOG_INFO("Cleaning up application...");
  delete application_;
  application_ = nullptr;
}

void Engine::InitGameConfig(const std::string& game_name) {
  if (game_config_) {
    game_config_->Save();
  }
  game_config_ = std::make_unique<UserConfig>(urkern::GetUserDataDirectory(game_name),
                                              "game_config.json");
  game_config_->Load();
}

void Engine::SetGameInfo(std::shared_ptr<GameInfo> info) {
  game_info_ = std::move(info);
}

}  // namespace wiesel
