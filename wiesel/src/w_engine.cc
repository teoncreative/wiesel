
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
#include "util/w_discord_rpc.h"

#include <cxxopts.hpp>
#include <thread>
#include "asset/w_asset_loader.h"
#include "asset/w_asset_manager.h"
#include "asset/w_asset_properties.h"
#include "asset/w_asset_property_registry.h"
#include "asset/w_asset_serializer.h"
#include "asset/w_model_loader.h"
#include "asset/w_sprite_loader.h"
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
#include "util/w_platform.h"
#include "util/w_thread_pool.h"
#include "util/w_user_config.h"
#include "window/w_sdlwindow.h"

namespace Wiesel {

EngineProperties EngineProperties::Parse(int argc, char** argv) {
  EngineProperties config;
  std::filesystem::path exe_dir = GetExecutableDirectory();

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
      } else {
        // Release: Look for pak or embedded
        config.engine_assets_path = exe_dir / "engine.pak";
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
      } else {
        config.editor_assets_path = exe_dir / "editor.pak";
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
    config.user_data_path = GetUserDataDirectory();
  }

  return config;
}

EngineProperties Engine::properties_;
std::shared_ptr<Renderer> Engine::renderer_;
std::shared_ptr<AppWindow> Engine::window_;
std::shared_ptr<VirtualFileSystem> Engine::vfs_;
Application* Engine::application_;
std::unique_ptr<DeveloperConsole> Engine::console_;
std::unique_ptr<InputManager> Engine::input_manager_;
std::unique_ptr<AssetManager> Engine::asset_manager_;
std::unique_ptr<ScriptManager> Engine::script_manager_;
std::unique_ptr<NativeBehaviorRegistry> Engine::behavior_registry_;
std::unique_ptr<ThreadPool> Engine::thread_pool_;
std::unique_ptr<AudioManager> Engine::audio_manager_;
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
  console_ = std::make_unique<DeveloperConsole>();
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

  // Register asset loaders
  asset_manager_->RegisterLoader(
      AssetType::Model,
      std::make_shared<FunctionAssetLoader>(
          [](AssetHandle handle) { return LoadModelAsset(handle); },
          [](AssetHandle handle) { asset_manager_->Unload(handle); }));

  asset_manager_->RegisterLoader(
      AssetType::Texture,
      std::make_shared<FunctionAssetLoader>(
          [](AssetHandle handle) { return LoadTextureAsset(handle); },
          [](AssetHandle handle) { asset_manager_->Unload(handle); }));

  // Material (.wmat) loader: reads JSON, stores Material.
  asset_manager_->RegisterLoader(
      AssetType::Material,
      std::make_shared<FunctionAssetLoader>(
          [](AssetHandle handle) {
            const auto* meta = asset_manager_->GetMetadata(handle);
            if (!meta || meta->virtual_source_path.empty()) {
              return false;
            }
            VfsFile file = Engine::vfs()->Open(meta->virtual_source_path);
            if (!file) {
              return false;
            }
            auto chars = file.AsChars();
            std::string content(chars.begin(), chars.end());
            auto j = nlohmann::json::parse(content, nullptr, false);
            if (j.is_discarded()) {
              LOG_ERROR("Invalid .wmat file: {}", meta->virtual_source_path);
              return false;
            }
            auto mat = Material::Deserialize(j);
            mat->name = meta->name;
            mat->asset_handle = handle;
            asset_manager_->Store<Material>(handle, mat);
            return true;
          },
          [](AssetHandle handle) { asset_manager_->Unload(handle); }));

  // Font loader: reads font file, creates FT_Face, stores FontAsset.
  // Size-specific rasterization happens lazily in FontCache::Get.
  asset_manager_->RegisterLoader(
      AssetType::Font,
      std::make_shared<FunctionAssetLoader>(
          [](AssetHandle handle) {
            const auto* meta = asset_manager_->GetMetadata(handle);
            if (!meta) {
              return false;
            }
            auto asset = std::make_shared<FontAsset>(meta->virtual_source_path);
            if (!asset->IsLoaded()) {
              return false;
            }
            const auto* props = meta->GetProperties<FontAssetProperties>();
            if (props) {
              asset->SetAAMode(props->aa_mode);
            }
            asset_manager_->Store<FontAsset>(handle, asset);
            return true;
          },
          [](AssetHandle handle) {
            FontCache::Invalidate(handle);
            asset_manager_->Unload(handle);
          }));

  // Sprite (.wsprite) loader: reads JSON, stores SpriteAssetData.
  asset_manager_->RegisterLoader(
      AssetType::Sprite,
      std::make_shared<FunctionAssetLoader>(
          [](AssetHandle handle) {
            const auto* meta = asset_manager_->GetMetadata(handle);
            if (!meta) {
              return false;
            }
            auto file = Engine::vfs()->Open(meta->virtual_source_path);
            if (!file) {
              return false;
            }
            auto chars = file.AsChars();
            std::string json_str(chars.begin(), chars.end());
            nlohmann::json j = nlohmann::json::parse(json_str, nullptr, false);
            if (j.is_discarded() || !j.contains("texture") ||
                !j.contains("rect")) {
              LOG_ERROR("Invalid .wsprite file: {}", meta->virtual_source_path);
              return false;
            }

            auto data = std::make_shared<SpriteAssetData>();
            data->texture_handle =
                AssetHandle::FromString(j["texture"].get<std::string>());
            data->rect = {j["rect"][0].get<float>(), j["rect"][1].get<float>(),
                          j["rect"][2].get<float>(), j["rect"][3].get<float>()};
            if (j.contains("pivot") && j["pivot"].is_array()) {
              data->pivot = {j["pivot"][0].get<float>(),
                             j["pivot"][1].get<float>()};
            }

            // Ensure the backing texture is loaded
            if (data->texture_handle.IsValid()) {
              auto tex = asset_manager_->Get<Texture>(data->texture_handle);
              if (!tex) {
                asset_manager_->LoadSync(data->texture_handle);
              }
            }

            asset_manager_->Store(handle, data);
            asset_manager_->AddDependency(handle, data->texture_handle);

            // Build GPU resources for rendering
            auto tex = asset_manager_->Get<Texture>(data->texture_handle);
            if (tex && tex->is_allocated_ && tex->image_view_) {
              auto gpu = std::make_shared<SpriteGpuData>();
              gpu->view = tex->image_view_;
              gpu->sampler =
                  tex->sampler_ ? tex->sampler_
                                : Engine::renderer()->GetDefaultLinearSampler();
              gpu->pixel_size = {data->rect.z, data->rect.w};

              float tw = static_cast<float>(tex->width_);
              float th = static_cast<float>(tex->height_);
              glm::vec4 uv = data->GetUVRect(tw, th);

              float u0 = uv.x;
              float v0 = uv.y;
              float u1 = uv.x + uv.z;
              float v1 = uv.y + uv.w;

              std::vector<VertexSprite> uvs = {
                  {{u0, v0}}, {{u1, v0}}, {{u1, v1}},
                  {{u0, v0}}, {{u1, v1}}, {{u0, v1}},
              };
              gpu->vertex_buffer = Engine::renderer()->CreateVertexBuffer(
                  "SpriteGpuData::vertex_buffer", uvs);

              asset_manager_->Store(handle, gpu);
            }

            return true;
          },
          [](AssetHandle handle) { asset_manager_->Unload(handle); }));

  // SpriteAnim (.wspriteanim) loader
  asset_manager_->RegisterLoader(
      AssetType::SpriteAnim,
      std::make_shared<FunctionAssetLoader>(
          [](AssetHandle handle) { return LoadSpriteAnimAsset(handle); },
          [](AssetHandle handle) { asset_manager_->Unload(handle); }));

  // SpriteController (.wspritecontroller) loader
  asset_manager_->RegisterLoader(
      AssetType::SpriteController,
      std::make_shared<FunctionAssetLoader>(
          [](AssetHandle handle) { return LoadSpriteControllerAsset(handle); },
          [](AssetHandle handle) { asset_manager_->Unload(handle); }));

  // UIDocument (.rml) loader
  asset_manager_->RegisterLoader(
      AssetType::UIDocument,
      std::make_shared<FunctionAssetLoader>(
          [](AssetHandle handle) {
            const auto* meta = asset_manager_->GetMetadata(handle);
            if (!meta || meta->virtual_source_path.empty()) {
              return false;
            }
            auto asset = std::make_shared<UIDocumentAsset>();
            asset->vfs_path = meta->virtual_source_path;
            asset_manager_->Store<UIDocumentAsset>(handle, asset);
            return true;
          },
          [](AssetHandle handle) { asset_manager_->Unload(handle); }));

  // UIStylesheet (.rcss) loader
  asset_manager_->RegisterLoader(
      AssetType::UIStylesheet,
      std::make_shared<FunctionAssetLoader>(
          [](AssetHandle handle) {
            const auto* meta = asset_manager_->GetMetadata(handle);
            if (!meta || meta->virtual_source_path.empty()) {
              return false;
            }
            auto asset = std::make_shared<UIStylesheetAsset>();
            asset->vfs_path = meta->virtual_source_path;
            asset_manager_->Store<UIStylesheetAsset>(handle, asset);
            return true;
          },
          [](AssetHandle handle) { asset_manager_->Unload(handle); }));

  // MeshCollider (.wmeshcol) loader: reads JSON, builds Jolt shape.
  asset_manager_->RegisterLoader(
      AssetType::MeshCollider,
      std::make_shared<FunctionAssetLoader>(
          [](AssetHandle handle) {
            if (!AssetSerializerRegistry::Load(handle)) {
              return false;
            }
            auto data = asset_manager_->Get<MeshColliderAssetData>(handle);
            if (data) {
              BuildCollisionShape(*data);
            }
            return data != nullptr;
          },
          [](AssetHandle handle) { asset_manager_->Unload(handle); }));

  InitializeVfs();
  InitializeComponentSerializers();
  InitializeAssetSerializers();
  input_manager_ = std::make_unique<InputManager>();
  InitializeAssetProperties();
  behavior_registry_ = std::make_unique<NativeBehaviorRegistry>();

  // Thread pool for async asset loading: min(cpu_cores, 8)
  uint32_t pool_size = std::min(std::thread::hardware_concurrency(), 8u);
  if (pool_size == 0) {
    pool_size = 4;
  }
  thread_pool_ = std::make_unique<ThreadPool>(pool_size);
  LOG_INFO("Asset thread pool: {} workers", pool_size);

  scene_manager_ = std::make_unique<SceneManager>();
  audio_manager_ = std::make_unique<AudioManager>();
  audio_manager_->Init();
  script_manager_ = std::make_unique<ScriptManager>();
  script_manager_->Init({.EnableDebugger = true});
#ifdef WIESEL_DISCORD_RPC
  discord_rpc_ = std::make_unique<DiscordRPC>();
#endif
}

void Engine::InitWindow(const WindowProperties&& props) {
  window_ = std::make_shared<SdlAppWindow>(std::move(props));
  Dialogs::Init();
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
  vfs_ = std::make_shared<VirtualFileSystem>();
  vfs_->Mount("engine://", properties_.engine_assets_path, 100);
  if (properties_.editor_enabled && !properties_.editor_assets_path.empty()) {
    vfs_->Mount("editor://", properties_.editor_assets_path, 90);
  }
  if (!properties_.app_assets_path.empty()) {
    vfs_->Mount("app://", properties_.app_assets_path, 80);
  }
  if (!properties_.user_data_path.empty()) {
    vfs_->Mount("user://", properties_.user_data_path, 0);
  }
}

void Engine::RegisterPrimitives() {
  asset_manager_->RegisterAndStore<Model>(kPrimitiveCube, "Cube",
                                          AssetType::Model, "primitive://cube",
                                          Primitives::CreateCube());
  asset_manager_->RegisterAndStore<Model>(
      kPrimitiveSphere, "Sphere", AssetType::Model, "primitive://sphere",
      Primitives::CreateSphere());
  asset_manager_->RegisterAndStore<Model>(kPrimitivePlane, "Plane",
                                          AssetType::Model, "primitive://plane",
                                          Primitives::CreatePlane());
  asset_manager_->RegisterAndStore<Model>(
      kPrimitiveCylinder, "Cylinder", AssetType::Model, "primitive://cylinder",
      Primitives::CreateCylinder());
  asset_manager_->RegisterAndStore<Model>(
      kPrimitiveCapsule, "Capsule", AssetType::Model, "primitive://capsule",
      Primitives::CreateCapsule());

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
  Dialogs::Destroy();
}

void Engine::CleanupEngine() {
  // Shut down thread pool first (waits for pending tasks)
  thread_pool_ = nullptr;
#ifdef WIESEL_DISCORD_RPC
  discord_rpc_ = nullptr;
#endif
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
  console_ = nullptr;
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
  game_config_ = std::make_unique<UserConfig>(GetUserDataDirectory(game_name),
                                              "game_config.json");
  game_config_->Load();
}

void Engine::SetGameInfo(std::shared_ptr<GameInfo> info) {
  game_info_ = std::move(info);
}

}  // namespace Wiesel

