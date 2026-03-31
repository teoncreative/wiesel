
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

#include <assimp/IOStream.hpp>
#include <assimp/IOSystem.hpp>
#include <chrono>
#include <fstream>
#include <future>
#include <thread>
#include "asset/w_asset_loader.h"
#include "asset/w_asset_manager.h"
#include "asset/w_asset_properties.h"
#include "asset/w_asset_property_registry.h"
#include "asset/w_asset_serializer.h"
#include "game/w_game_info.h"
#include "input/w_input.h"
#include "rendering/w_primitives.h"
#include "rendering/w_sprite.h"
#include "rendering/w_sprite_asset.h"
#include "rendering/w_sprite_loader.h"
#include "scene/w_component_serializer.h"
#include "scene/w_entity.h"
#include "scene/w_lights.h"
#include "scene/w_scene_manager.h"
#include "script/w_scriptmanager.h"
#include "ui/w_font.h"
#include "util/w_dialogs.h"
#include "util/w_platform.h"
#include "util/w_thread_pool.h"
#ifdef WIESEL_BACKEND_SDL3
#include "window/w_sdlwindow.h"
#else
#include "window/w_glfwwindow.h"
#endif
#include <cxxopts.hpp>

namespace Wiesel {

// Assimp IOStream backed by a VfsFile
class VfsAssimpIOStream : public Assimp::IOStream {
  friend class VfsAssimpIOSystem;

 public:
  explicit VfsAssimpIOStream(VfsFile file) : file_(std::move(file)) {}

  ~VfsAssimpIOStream() override = default;

  size_t Read(void* pvBuffer, size_t pSize, size_t pCount) override {
    size_t bytes_requested = pSize * pCount;
    size_t bytes_read = file_.Read(pvBuffer, bytes_requested);
    return bytes_read / pSize;
  }

  size_t Write(const void* /*pvBuffer*/, size_t /*pSize*/,
               size_t /*pCount*/) override {
    return 0;  // Read-only
  }

  aiReturn Seek(size_t pOffset, aiOrigin pOrigin) override {
    switch (pOrigin) {
      case aiOrigin_SET:
        file_.Seek(pOffset);
        break;
      case aiOrigin_CUR:
        file_.SeekRelative(static_cast<int64_t>(pOffset));
        break;
      case aiOrigin_END:
        file_.Seek(file_.Size() - pOffset);
        break;
      default:
        return aiReturn_FAILURE;
    }
    return aiReturn_SUCCESS;
  }

  size_t Tell() const override { return file_.Tell(); }

  size_t FileSize() const override { return file_.Size(); }

  void Flush() override {}

 private:
  VfsFile file_;
};

// Assimp IOSystem backed by VFS
class VfsAssimpIOSystem : public Assimp::IOSystem {
 public:
  VfsAssimpIOSystem(std::shared_ptr<VirtualFileSystem> vfs,
                    const std::string& base_dir)
      : vfs_(std::move(vfs)), base_dir_(base_dir) {}

  ~VfsAssimpIOSystem() override = default;

  bool Exists(const char* pFile) const override {
    return vfs_->FileExists(ResolvePath(pFile));
  }

  char getOsSeparator() const override { return '/'; }

  Assimp::IOStream* Open(const char* pFile, const char* /*pMode*/) override {
    std::string resolved = ResolvePath(pFile);
    if (!vfs_->FileExists(resolved)) {
      return nullptr;
    }
    VfsFile file = vfs_->Open(resolved);
    return new VfsAssimpIOStream(std::move(file));
  }

  void Close(Assimp::IOStream* pFile) override { delete pFile; }

 private:
  std::string ResolvePath(const char* pFile) const {
    std::string path(pFile);
    // If already an absolute VFS path, use as-is
    if (!path.empty() && path[0] == '/') {
      return path;
    }
    // Relative path: resolve against the model's base directory
    return base_dir_ + "/" + path;
  }

  std::shared_ptr<VirtualFileSystem> vfs_;
  std::string base_dir_;
};

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
DeveloperConsole Engine::console_;
Application* Engine::application_;
std::shared_ptr<AssetManager> Engine::asset_manager_;
std::shared_ptr<ScriptManager> Engine::script_manager_;
std::shared_ptr<NativeBehaviorRegistry> Engine::behavior_registry_;
std::shared_ptr<ThreadPool> Engine::thread_pool_;
std::shared_ptr<AudioManager> Engine::audio_manager_;
std::shared_ptr<SceneManager> Engine::scene_manager_;
std::shared_ptr<GameInfo> Engine::game_info_;
AssetHandle Engine::primitive_cube_;
AssetHandle Engine::primitive_sphere_;
AssetHandle Engine::primitive_plane_;
AssetHandle Engine::primitive_cylinder_;
AssetHandle Engine::primitive_capsule_;
#ifdef WIESEL_DISCORD_RPC
std::shared_ptr<DiscordRPC> Engine::discord_rpc_;
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
  asset_manager_ = std::make_shared<AssetManager>();

  // Register asset loaders
  asset_manager_->RegisterLoader(
      AssetType::Model,
      std::make_shared<FunctionAssetLoader>(
          [](AssetHandle handle) { return LoadModel(handle); },
          [](AssetHandle handle) { asset_manager_->Unload(handle); }));

  asset_manager_->RegisterLoader(
      AssetType::Texture,
      std::make_shared<FunctionAssetLoader>(
          [](AssetHandle handle) { return LoadTextureAsset(handle); },
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
              gpu->vertex_buffer = Engine::renderer()->CreateVertexBuffer(uvs);

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

  InitializeVfs();
  InitializeComponentSerializers();
  InitializeAssetSerializers();
  InputManager::Init();
  InitializeAssetProperties();
  behavior_registry_ = std::make_shared<NativeBehaviorRegistry>();

  // Thread pool for async asset loading: min(cpu_cores, 8)
  uint32_t pool_size = std::min(std::thread::hardware_concurrency(), 8u);
  if (pool_size == 0) {
    pool_size = 4;
  }
  thread_pool_ = std::make_shared<ThreadPool>(pool_size);
  LOG_INFO("Asset thread pool: {} workers", pool_size);

  scene_manager_ = std::make_shared<SceneManager>();
  audio_manager_ = std::make_shared<AudioManager>();
  audio_manager_->Init();
  script_manager_ = std::make_shared<ScriptManager>();
  script_manager_->Init({.EnableDebugger = true});
#ifdef WIESEL_DISCORD_RPC
  discord_rpc_ = std::make_shared<DiscordRPC>();
#endif
}

void Engine::InitWindow(const WindowProperties&& props) {
#ifdef WIESEL_BACKEND_SDL3
  window_ = std::make_shared<SdlAppWindow>(std::move(props));
#else
  window_ = std::make_shared<GlfwAppWindow>(std::move(props));
#endif
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
}

void Engine::CleanupApplication() {
  LOG_INFO("Cleaning up application...");
  delete application_;
  application_ = nullptr;
}

void Engine::SetGameInfo(std::shared_ptr<GameInfo> info) {
  game_info_ = std::move(info);
}

aiScene* Engine::LoadAssimpModel(const std::string& path,
                                 bool convert_to_left_handed) {
  LOG_INFO("Loading model: {}", path);

  // Derive base directory for resolving relative references (e.g. .bin, textures)
  std::string base_dir = path;
  size_t last_slash = base_dir.rfind('/');
  if (last_slash != std::string::npos) {
    base_dir = base_dir.substr(0, last_slash);
  }

  Assimp::Importer importer;
  importer.SetIOHandler(new VfsAssimpIOSystem(vfs_, base_dir));
  importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE,
                              aiPrimitiveType_LINE | aiPrimitiveType_POINT);
  uint32_t flags = aiProcess_Triangulate | aiProcess_CalcTangentSpace |
                   aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices |
                   aiProcess_LimitBoneWeights |
                   aiProcessPreset_TargetRealtime_Fast;
  if (convert_to_left_handed) {
    flags |= aiProcess_ConvertToLeftHanded;
  }
  importer.ReadFile(path.c_str(), flags);
  aiScene* scene = importer.GetOrphanedScene();

  if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) ||
      !scene->mRootNode) {
    LOG_ERROR("Failed to load model {}: {}", path, importer.GetErrorString());
    return nullptr;
  }

  return scene;
}

bool Engine::LoadTextureAsset(AssetHandle handle) {
  const AssetMetadata* meta = asset_manager_->GetMetadata(handle);
  if (!meta) {
    return false;
  }

  VfsFile file = vfs_->Open(meta->virtual_source_path);
  if (!file) {
    LOG_ERROR("LoadTexture: file not found: {}", meta->virtual_source_path);
    return false;
  }

  meta->load_progress.store(0.1f);

  int w, h, channels;
  stbi_uc* pixels =
      stbi_load_from_memory(file.Data(), static_cast<int>(file.Size()), &w, &h,
                            &channels, STBI_rgb_alpha);
  if (!pixels) {
    LOG_ERROR("LoadTexture: decode failed: {}", meta->virtual_source_path);
    return false;
  }

  meta->load_progress.store(0.5f);

  // Read asset properties (filtering, mipmaps, etc.)
  const auto* tex_props = meta->GetProperties<TextureAssetProperties>();
  TextureAssetProperties defaults;
  const auto& ap = tex_props ? *tex_props : defaults;

  VkCommandPool pool = renderer_->CreateTransientCommandPool();
  Renderer::SetThreadCommandPool(pool);
  renderer_->BeginBatchUpload();

  bool use_srgb = ap.asset_type == TextureAssetType::Default;

  TextureProps props;
  props.width = w;
  props.height = h;
  props.image_format =
      use_srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
  props.generate_mipmaps = ap.generate_mipmaps;

  VkFilter vk_filter = (ap.filter_mode == TextureFilterMode::Nearest)
                           ? VK_FILTER_NEAREST
                           : VK_FILTER_LINEAR;
  VkSamplerAddressMode vk_wrap;
  switch (ap.wrap_mode) {
    case TextureWrapMode::Clamp:
      vk_wrap = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
      break;
    case TextureWrapMode::Mirror:
      vk_wrap = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
      break;
    default:
      vk_wrap = VK_SAMPLER_ADDRESS_MODE_REPEAT;
      break;
  }
  SamplerProps sampler;
  sampler.mag_filter = vk_filter;
  sampler.min_filter = vk_filter;
  sampler.address_mode = vk_wrap;

  auto texture = renderer_->CreateTexture(pixels, 4, props, sampler);
  stbi_image_free(pixels);

  renderer_->EndBatchUpload();
  Renderer::SetThreadCommandPool(VK_NULL_HANDLE);
  vkDestroyCommandPool(renderer_->GetLogicalDevice(), pool, nullptr);

  if (!texture) {
    return false;
  }

  meta->load_progress.store(0.95f);
  asset_manager_->Store<Texture>(handle, texture);
  return true;
}

bool Engine::LoadModel(AssetHandle handle) {
  const AssetMetadata* meta = asset_manager_->GetMetadata(handle);
  if (!meta) {
    LOG_ERROR("LoadModel: invalid handle");
    return false;
  }

  std::string path = meta->virtual_source_path;
  std::string textures_dir = path;
  size_t last_slash = textures_dir.rfind('/');
  if (last_slash != std::string::npos) {
    textures_dir = textures_dir.substr(0, last_slash);
  }

  // This function does the actual model loading work.
  // Safe to call from any thread — uses a per-thread command pool for GPU uploads.
  {
    using Clock = std::chrono::high_resolution_clock;
    auto t0 = Clock::now();

    aiScene* assimp_scene = LoadAssimpModel(path);
    meta->load_progress.store(0.2f);

    auto t1 = Clock::now();
    LOG_INFO("Model import (Assimp): {:.1f}s",
             std::chrono::duration<double>(t1 - t0).count());

    if (!assimp_scene) {
      Engine::asset_manager().SetLoadState(handle, AssetLoadState::Loading,
                                           AssetLoadState::Failed);
      return false;
    }

    // All GPU work happens here on the background thread using a dedicated
    // command pool.  Queue submissions are mutex-protected in EndSingleTimeCommands.
    VkCommandPool upload_pool = renderer_->CreateTransientCommandPool();
    Renderer::SetThreadCommandPool(upload_pool);

    std::shared_ptr<Model> model = std::make_shared<Model>();
    model->model_path = path;
    model->textures_path = textures_dir;
    model->meshes.clear();
    model->textures.clear();
    model->node_hierarchy.nodes.clear();
    model->node_hierarchy.node_name_to_index.clear();

    // Pre-decode all textures in parallel (CPU-bound stbi_load)
    auto t_predecode = Clock::now();
    PreDecodeTextures(*model, *assimp_scene);
    auto t_predecode_end = Clock::now();
    LOG_INFO(
        "Parallel texture decode: {:.1f}s ({} textures)",
        std::chrono::duration<double>(t_predecode_end - t_predecode).count(),
        model->decoded_texture_cache.size());
    meta->load_progress.store(0.5f);

    // Batch all GPU uploads (textures + vertex/index buffers) into a single
    // command buffer submission to avoid per-resource GPU sync overhead.
    renderer_->BeginBatchUpload();

    auto t2 = Clock::now();
    ProcessNode(*model, assimp_scene->mRootNode, *assimp_scene, model->meshes,
                -1);
    auto t3 = Clock::now();
    LOG_INFO("ProcessNode (meshes + textures): {:.1f}s",
             std::chrono::duration<double>(t3 - t2).count());
    meta->load_progress.store(0.8f);

    // Pre-compute per-mesh world transforms and bounding volumes
    model->ComputeMeshNodeTransforms();
    model->ComputeBounds();

    // Check if any mesh has transparency
    for (const auto& m : model->meshes) {
      if (m->has_transparency) {
        model->has_transparent_meshes = true;
        break;
      }
    }

    // Free pre-decoded pixel data now that GPU textures are created
    model->decoded_texture_cache.clear();

    ExtractAnimations(*model, *assimp_scene);
    uint64_t vertices = 0;
    for (const std::shared_ptr<Mesh>& item : model->meshes) {
      item->Allocate();
      vertices += item->vertices.size();
    }
    auto t4 = Clock::now();
    LOG_INFO("Mesh allocation: {:.1f}s",
             std::chrono::duration<double>(t4 - t3).count());

    renderer_->EndBatchUpload();

    auto t5 = Clock::now();
    LOG_INFO("Batch GPU upload: {:.1f}s",
             std::chrono::duration<double>(t5 - t4).count());
    meta->load_progress.store(0.95f);

    Renderer::SetThreadCommandPool(VK_NULL_HANDLE);
    vkDestroyCommandPool(renderer_->GetLogicalDevice(), upload_pool, nullptr);
    delete assimp_scene;

    LOG_INFO("Loaded {} meshes!", model->meshes.size());
    LOG_INFO("Loaded {} textures!", model->textures.size());
    LOG_INFO("Loaded {} vertices!", vertices);
    if (model->has_skeleton) {
      LOG_INFO("Loaded {} bones!", model->skeleton.bones.size());
    }
    if (model->has_animations) {
      LOG_INFO("Loaded {} animation clips!", model->animation_clips.size());
      for (const auto& clip : model->animation_clips) {
        LOG_INFO("  Clip '{}': {:.1f} ticks, {:.0f} tps, {} channels",
                 clip.name, clip.duration, clip.ticks_per_second,
                 clip.channels.size());
      }
    }

    // Register on main thread (no GPU work, just data structure updates)
    application_->SubmitToMainThread([model, handle]() {
      for (auto& [texPath, tex] : model->textures) {
        asset_manager_->RegisterAndStore<Texture>(texPath, AssetType::Texture,
                                                  texPath, tex);
      }
      // Register materials extracted from meshes as assets
      // Derive the model's directory for saving .wmat files
      std::string model_dir;
      std::string model_stem;
      {
        size_t sl = model->model_path.rfind('/');
        model_dir =
            (sl != std::string::npos) ? model->model_path.substr(0, sl) : "";
        std::string filename = (sl != std::string::npos)
                                   ? model->model_path.substr(sl + 1)
                                   : model->model_path;
        size_t dot = filename.rfind('.');
        model_stem =
            (dot != std::string::npos) ? filename.substr(0, dot) : filename;
      }

      for (size_t i = 0; i < model->meshes.size(); i++) {
        auto& mesh = model->meshes[i];
        if (mesh->mat) {
          std::string mat_name = mesh->mat->name.empty()
                                     ? "Material_" + std::to_string(i)
                                     : mesh->mat->name;

          // VFS path for the .wmat file alongside the model
          std::string wmat_vfs_path =
              model_dir + "/" + model_stem + "_" + mat_name + ".wmat";

          // Check if already registered (re-import case)
          AssetHandle existing =
              asset_manager_->FindBySourcePath(wmat_vfs_path);
          if (existing.IsValid()) {
            mesh->material_handle = existing;
            mesh->mat->asset_handle = existing;
            asset_manager_->Store<Material>(existing, mesh->mat);
            asset_manager_->SetLoadState(existing, AssetLoadState::Unloaded,
                                         AssetLoadState::Loaded);
          } else {
            AssetHandle mat_handle = asset_manager_->RegisterAndStore<Material>(
                mat_name, AssetType::Material, wmat_vfs_path, mesh->mat);
            mesh->material_handle = mat_handle;
            mesh->mat->asset_handle = mat_handle;
          }

          // Save .wmat file to physical path if available
          auto wmat_physical = vfs_->GetPhysicalPath(wmat_vfs_path);
          if (wmat_physical.has_value()) {
            namespace fs = std::filesystem;
            fs::path wmat_path = *wmat_physical;
            if (!fs::exists(wmat_path)) {
              fs::create_directories(wmat_path.parent_path());
              nlohmann::json j = mesh->mat->Serialize();
              std::ofstream ofs(wmat_path);
              if (ofs.is_open()) {
                ofs << j.dump(2);
                LOG_INFO("Saved material: {}", wmat_path.string());
              }
            }
          }
        }
      }
      asset_manager_->SetLoadState(handle, AssetLoadState::Loading,
                                   AssetLoadState::Loaded);
      asset_manager_->Store(handle, model);
    });
  }
  return true;
}

void Engine::LoadModelAsync(AssetHandle handle) {
  if (!asset_manager_->SetLoadState(handle, AssetLoadState::Unloaded,
                                    AssetLoadState::Loading)) {
    return;
  }
  thread_pool_->Submit([handle]() {
    if (!LoadModel(handle)) {
      asset_manager_->SetLoadState(handle, AssetLoadState::Loading,
                                   AssetLoadState::Failed);
    }
  });
}

void Engine::PreDecodeTextures(Model& model, const aiScene& scene) {
  // Only pre-decode embedded textures. External textures are loaded as
  // standalone assets via LoadTextureAsset or on-demand in LoadTexture.
  const aiTextureType texture_types[] = {
      aiTextureType_DIFFUSE,           aiTextureType_NORMALS,
      aiTextureType_SPECULAR,          aiTextureType_BASE_COLOR,
      aiTextureType_DIFFUSE_ROUGHNESS, aiTextureType_METALNESS};

  struct EmbeddedTask {
    int index;
    std::string key;
    aiTexture* tex;
  };

  std::vector<EmbeddedTask> embedded_tasks;
  for (unsigned int m = 0; m < scene.mNumMaterials; m++) {
    aiMaterial* mat = scene.mMaterials[m];
    for (auto tex_type : texture_types) {
      for (unsigned int i = 0; i < mat->GetTextureCount(tex_type); i++) {
        aiString str;
        mat->GetTexture(tex_type, i, &str);
        std::string s = str.C_Str();
        if (s.empty() || s[0] != '*') {
          continue;
        }
        int idx = std::atoi(s.c_str() + 1);
        if (idx < 0 || static_cast<unsigned>(idx) >= scene.mNumTextures) {
          continue;
        }
        std::string key =
            model.textures_path + "/embedded_" + std::to_string(idx);
        if (model.decoded_texture_cache.contains(key)) {
          continue;
        }
        embedded_tasks.push_back({idx, key, scene.mTextures[idx]});
        model.decoded_texture_cache[key] = nullptr;
      }
    }
  }

  if (embedded_tasks.empty()) {
    return;
  }

  std::vector<
      std::future<std::pair<std::string, std::shared_ptr<DecodedTextureData>>>>
      futures;
  for (auto& task : embedded_tasks) {
    futures.push_back(std::async(
        std::launch::async,
        [task]()
            -> std::pair<std::string, std::shared_ptr<DecodedTextureData>> {
          auto decoded = std::make_shared<DecodedTextureData>();
          if (task.tex->mHeight == 0) {
            decoded->pixels = stbi_load_from_memory(
                reinterpret_cast<unsigned char*>(task.tex->pcData),
                task.tex->mWidth, &decoded->width, &decoded->height,
                &decoded->channels, STBI_rgb_alpha);
          } else {
            decoded->width = task.tex->mWidth;
            decoded->height = task.tex->mHeight;
            decoded->channels = 4;
            int size = decoded->width * decoded->height;
            decoded->pixels = static_cast<stbi_uc*>(malloc(size * 4));
            if (!decoded->pixels) {
              return {task.key, nullptr};
            }
            aiTexel* texels = task.tex->pcData;
            for (int i = 0; i < size; i++) {
              decoded->pixels[i * 4 + 0] = texels[i].r;
              decoded->pixels[i * 4 + 1] = texels[i].g;
              decoded->pixels[i * 4 + 2] = texels[i].b;
              decoded->pixels[i * 4 + 3] = texels[i].a;
            }
          }
          if (!decoded->pixels) {
            return {task.key, nullptr};
          }
          int pixel_count = decoded->width * decoded->height;
          for (int p = 0; p < pixel_count; p++) {
            uint8_t a = decoded->pixels[p * 4 + 3];
            if (a > 0 && a < 255) {
              decoded->has_semi_transparency = true;
              break;
            }
          }
          return {task.key, decoded};
        }));
  }

  for (auto& f : futures) {
    auto [key, data] = f.get();
    if (data) {
      model.decoded_texture_cache[key] = std::move(data);
    }
  }
}

// Resolve an embedded texture from the pre-decoded cache or by direct decode.
std::shared_ptr<Texture> Engine::LoadEmbeddedTexture(Model& model,
                                                     int tex_index,
                                                     TextureType tex_type,
                                                     const aiScene& scene) {
  std::string key =
      model.textures_path + "/embedded_" + std::to_string(tex_index);

  // Reuse if already loaded for another mesh in this model
  auto existing = model.textures.find(key);
  if (existing != model.textures.end()) {
    return existing->second;
  }

  // Try pre-decoded cache from PreDecodeTextures
  std::shared_ptr<Texture> texture;
  auto cache_it = model.decoded_texture_cache.find(key);
  if (cache_it != model.decoded_texture_cache.end() && cache_it->second) {
    auto& decoded = cache_it->second;
    bool is_color =
        tex_type == TextureTypeDiffuse || tex_type == TextureTypeBaseColor;
    TextureProps props;
    props.type = tex_type;
    props.width = decoded->width;
    props.height = decoded->height;
    props.image_format =
        is_color ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
    props.generate_mipmaps = true;
    texture = renderer_->CreateTexture(decoded->pixels, 4, props, {});
  } else {
    texture = CreateTextureFromEmbedded(scene.mTextures[tex_index], tex_type);
  }

  if (texture) {
    model.textures.insert({key, texture});
  }
  return texture;
}

// Resolve an external texture from AssetManager or by loading from VFS.
std::shared_ptr<Texture> Engine::LoadExternalTexture(
    Model& model, const std::string& texture_path, TextureType tex_type) {
  // Reuse if already loaded for another mesh in this model
  auto existing = model.textures.find(texture_path);
  if (existing != model.textures.end()) {
    return existing->second;
  }

  std::shared_ptr<Texture> texture;

  // Check if already loaded as a standalone asset
  AssetHandle tex_handle = asset_manager_->FindBySourcePath(texture_path);
  if (tex_handle.IsValid() &&
      asset_manager_->GetLoadState(tex_handle) == AssetLoadState::Loaded) {
    texture = asset_manager_->Get<Texture>(tex_handle);
  }

  // Fallback: load directly from VFS
  if (!texture) {
    texture = renderer_->CreateTexture(texture_path, {tex_type}, {});
  }

  if (texture) {
    model.textures.insert({texture_path, texture});
  }
  return texture;
}

// Normalize an Assimp texture path to a VFS-relative filename.
static std::string NormalizeTexturePath(const std::string& raw,
                                        const std::string& textures_dir) {
  std::string s = raw;
  size_t last_sep = s.find_last_of("/\\");
  if (last_sep != std::string::npos &&
      (s.find(':') != std::string::npos || s[0] == '/')) {
    s = s.substr(last_sep + 1);
  }
  return textures_dir + "/" + s;
}

bool Engine::LoadTexture(Model& model, std::shared_ptr<Mesh> mesh,
                         aiMaterial* mat, aiTextureType type,
                         const aiScene& scene) {
  size_t count = mat->GetTextureCount(type);
  if (count > 1) {
    LOG_WARN("Mesh has more than one texture for type {}",
             std::to_string(type));
  }

  TextureType tex_type = static_cast<TextureType>(type);

  for (unsigned int i = 0; i < count; i++) {
    aiString str;
    mat->GetTexture(type, i, &str);
    std::string s = std::string(str.C_Str());
    if (s.empty()) {
      continue;
    }

    std::shared_ptr<Texture> texture;

    if (s[0] == '*') {
      // Embedded texture
      int tex_index = std::atoi(s.c_str() + 1);
      if (tex_index < 0 || tex_index >= static_cast<int>(scene.mNumTextures)) {
        continue;
      }
      texture = LoadEmbeddedTexture(model, tex_index, tex_type, scene);
    } else {
      // External texture file
      std::string path = NormalizeTexturePath(s, model.textures_path);
      texture = LoadExternalTexture(model, path, tex_type);
    }

    if (!texture) {
      continue;
    }
    Material::Set(mesh->mat, texture, tex_type);
    return true;
  }
  return false;
}

std::shared_ptr<Texture> Engine::CreateTextureFromEmbedded(aiTexture* aiTex,
                                                           TextureType type) {
  int width, height;
  unsigned char* pixelData = nullptr;

  if (aiTex->mHeight == 0) {
    // Compressed format - decode using stb_image
    int channels;
    pixelData = stbi_load_from_memory(
        reinterpret_cast<unsigned char*>(aiTex->pcData), aiTex->mWidth, &width,
        &height, &channels, STBI_rgb_alpha);

    if (!pixelData) {
      LOG_ERROR("Failed to decode embedded texture: {}", stbi_failure_reason());
      return nullptr;
    }
  } else {
    // Uncompressed format - convert BGRA to RGBA
    width = aiTex->mWidth;
    height = aiTex->mHeight;
    pixelData = ConvertBGRAtoRGBA(aiTex->pcData, width, height);

    if (!pixelData) {
      LOG_ERROR("Failed to convert embedded texture format");
      return nullptr;
    }
  }

  TextureProps props;
  props.width = width;
  props.height = height;
  props.type = type;
  bool is_color = type == TextureTypeDiffuse || type == TextureTypeBaseColor;
  props.image_format =
      is_color ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
  props.generate_mipmaps = true;

  auto texture = renderer_->CreateTexture(pixelData, 4, props, {});

  stbi_image_free(pixelData);  // Works for both cases

  return texture;
}

unsigned char* Engine::ConvertBGRAtoRGBA(void* bgra_data, int width,
                                         int height) {
  aiTexel* texels = reinterpret_cast<aiTexel*>(bgra_data);
  int size = width * height;

  unsigned char* rgba_data =
      static_cast<unsigned char*>(malloc(size * 4 * sizeof(unsigned char)));

  if (!rgba_data) {
    return nullptr;
  }

  for (int i = 0; i < size; i++) {
    rgba_data[i * 4 + 0] = texels[i].r;
    rgba_data[i * 4 + 1] = texels[i].g;
    rgba_data[i * 4 + 2] = texels[i].b;
    rgba_data[i * 4 + 3] = texels[i].a;
  }

  return rgba_data;
}

std::shared_ptr<Mesh> Engine::ProcessMesh(Model& model, aiMesh* aiMesh,
                                          const aiScene& aiScene) {
  std::vector<Vertex3D> vertices;
  std::vector<Index> indices;

  aiMaterial* material = aiScene.mMaterials[aiMesh->mMaterialIndex];

  std::shared_ptr<Mesh> mesh = std::make_shared<Mesh>();

  // Extract material name from assimp
  aiString ai_mat_name;
  if (material->Get(AI_MATKEY_NAME, ai_mat_name) == AI_SUCCESS) {
    mesh->mat->name = ai_mat_name.C_Str();
  }

  uint32_t flags = 0;
  flags |= VertexFlagHasTexture *
           LoadTexture(model, mesh, material, aiTextureType_DIFFUSE, aiScene);
  flags |= VertexFlagHasNormalMap *
           LoadTexture(model, mesh, material, aiTextureType_NORMALS, aiScene);
  flags |= VertexFlagHasSpecularMap *
           LoadTexture(model, mesh, material, aiTextureType_SPECULAR, aiScene);
  flags |=
      VertexFlagHasAlbedoMap *
      LoadTexture(model, mesh, material, aiTextureType_BASE_COLOR, aiScene);
  flags |= VertexFlagHasRoughnessMap *
           LoadTexture(model, mesh, material, aiTextureType_DIFFUSE_ROUGHNESS,
                       aiScene);
  flags |= VertexFlagHasMetallicMap *
           LoadTexture(model, mesh, material, aiTextureType_METALNESS, aiScene);

  // Check base texture for semi-transparency (diffuse or albedo)
  auto check_transparency = [&](aiTextureType type) {
    if (material->GetTextureCount(type) == 0) {
      return;
    }
    aiString str;
    material->GetTexture(type, 0, &str);
    std::string s = str.C_Str();
    if (s.empty()) {
      return;
    }
    std::string key;
    if (s[0] == '*') {
      int idx = std::atoi(s.c_str() + 1);
      key = model.textures_path + "/embedded_" + std::to_string(idx);
    } else {
      size_t last_sep = s.find_last_of("/\\");
      if (last_sep != std::string::npos &&
          (s.find(':') != std::string::npos || s[0] == '/')) {
        s = s.substr(last_sep + 1);
      }
      key = model.textures_path + "/" + s;
    }
    auto it = model.decoded_texture_cache.find(key);
    if (it != model.decoded_texture_cache.end() && it->second &&
        it->second->has_semi_transparency) {
      mesh->has_transparency = true;
    }
  };
  check_transparency(aiTextureType_DIFFUSE);
  check_transparency(aiTextureType_BASE_COLOR);

  // Read PBR material factors from assimp (glTF roughness/metallic workflow)
  // When a map exists, the factor acts as a multiplier (default 1.0 = full texture).
  // When no map exists, the factor IS the value directly.
  float roughness_factor;
  if (material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness_factor) !=
      AI_SUCCESS) {
    roughness_factor = (flags & VertexFlagHasRoughnessMap) ? 1.0f : 0.5f;
  }
  float metallic_factor;
  if (material->Get(AI_MATKEY_METALLIC_FACTOR, metallic_factor) != AI_SUCCESS) {
    metallic_factor = (flags & VertexFlagHasMetallicMap) ? 1.0f : 0.0f;
  }
  mesh->mat->SetProperty("roughness", roughness_factor);
  mesh->mat->SetProperty("metallic", metallic_factor);

  // Specular is engine-specific (not standard glTF PBR).
  // With map: factor as multiplier (1.0). Without map: small dielectric F0.
  float specular_factor;
  if (material->Get(AI_MATKEY_SPECULAR_FACTOR, specular_factor) != AI_SUCCESS) {
    specular_factor = (flags & VertexFlagHasSpecularMap) ? 1.0f : 0.08f;
  }
  mesh->mat->SetProperty("specular", specular_factor);

  bool has_unsupported_textures = false;
  for (size_t type = aiTextureType_NONE; type < AI_TEXTURE_TYPE_MAX; type++) {
    if (type == aiTextureType_UNKNOWN) {
      continue;
    }
    if (type == aiTextureType_DIFFUSE || type == aiTextureType_NORMALS ||
        type == aiTextureType_SPECULAR || type == aiTextureType_BASE_COLOR ||
        type == aiTextureType_DIFFUSE_ROUGHNESS ||
        type == aiTextureType_METALNESS) {
      continue;
    }
    int count = material->GetTextureCount(static_cast<aiTextureType>(type));
    if (count > 0) {
      has_unsupported_textures = true;
    }
  }
  if (has_unsupported_textures) {
    LOG_WARN("Mesh has unsupported textures!");
  }

  for (unsigned int i = 0; i < aiMesh->mNumVertices; i++) {
    Vertex3D vertex{};
    glm::vec3 vector;
    // positions
    vector.x = aiMesh->mVertices[i].x;
    vector.y = aiMesh->mVertices[i].y;
    vector.z = aiMesh->mVertices[i].z;
    vertex.ppos = vector;

    // normals
    vector.x = aiMesh->mNormals[i].x;
    vector.y = aiMesh->mNormals[i].y;
    vector.z = aiMesh->mNormals[i].z;
    vertex.normal = vector;

    // texture coordinates
    if (aiMesh->mTextureCoords[0]) {
      glm::vec2 vec;
      vec.x = aiMesh->mTextureCoords[0][i].x;
      vec.y = aiMesh->mTextureCoords[0][i].y;
      vertex.uv = vec;
    } else {
      vertex.uv = glm::vec2(0.0f, 0.0f);
    }
    vertex.flags = flags;

    if (aiMesh->mColors[0]) {
      vertex.color = {aiMesh->mColors[0][i].r, aiMesh->mColors[0][i].g,
                      aiMesh->mColors[0][i].b};
    } else {
      // Fall back to material diffuse color when no vertex colors exist
      aiColor4D diffuse_color(1.0f, 1.0f, 1.0f, 1.0f);
      material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse_color);
      vertex.color = {diffuse_color.r, diffuse_color.g, diffuse_color.b};
    }

    // tangent
    if (aiMesh->mTangents) {
      vector.x = aiMesh->mTangents[i].x;
      vector.y = aiMesh->mTangents[i].y;
      vector.z = aiMesh->mTangents[i].z;
      vertex.tangent = vector;
    } else {
      vertex.tangent = {1.0f, 0.0f, 0.0f};
    }

    // bitangent
    if (aiMesh->mBitangents) {
      vector.x = aiMesh->mBitangents[i].x;
      vector.y = aiMesh->mBitangents[i].y;
      vector.z = aiMesh->mBitangents[i].z;
      vertex.bi_tangent = vector;
    } else {
      vertex.bi_tangent = {0.0f, 0.0f, 1.0f};
    }

    float handedness = glm::dot(glm::cross(vertex.normal, vertex.tangent),
                                vertex.bi_tangent) < 0.0f
                           ? -1.0f
                           : 1.0f;
    if (handedness < 0.0f) {
      vertex.tangent *= -1.0f;
    }

    mesh->vertices.push_back(vertex);
  }

  // Extract bone weights
  if (aiMesh->mNumBones > 0) {
    for (uint32_t b = 0; b < aiMesh->mNumBones; b++) {
      aiBone* bone = aiMesh->mBones[b];
      std::string bone_name = bone->mName.C_Str();

      // Register bone in skeleton if not already present
      int32_t bone_index;
      auto it = model.skeleton.bone_name_to_index.find(bone_name);
      if (it != model.skeleton.bone_name_to_index.end()) {
        bone_index = it->second;
      } else {
        bone_index = static_cast<int32_t>(model.skeleton.bones.size());
        BoneInfo bone_info;
        bone_info.name = bone_name;
        bone_info.inverse_bind_matrix = ConvertMatrix(bone->mOffsetMatrix);
        model.skeleton.bones.push_back(bone_info);
        model.skeleton.bone_name_to_index[bone_name] = bone_index;
      }

      // Write bone weights into vertices
      for (uint32_t w = 0; w < bone->mNumWeights; w++) {
        uint32_t vertex_id = bone->mWeights[w].mVertexId;
        float weight = bone->mWeights[w].mWeight;
        auto& v = mesh->vertices[vertex_id];
        for (int s = 0; s < WIESEL_MAX_BONE_INFLUENCE; s++) {
          if (v.bone_weights[s] == 0.0f) {
            v.bone_indices[s] = bone_index;
            v.bone_weights[s] = weight;
            break;
          }
        }
      }
    }
    model.has_skeleton = true;
  }

  // now walk through each of the mesh's faces (a face is a mesh its triangle) and retrieve the corresponding vertex indices.
  for (unsigned int i = 0; i < aiMesh->mNumFaces; i++) {
    aiFace face = aiMesh->mFaces[i];

    // Retrieve all indices of the face and store them in the indices vector
    for (int j = 0; j < face.mNumIndices; j++) {
      mesh->indices.push_back(face.mIndices[j]);
    }
  }

  return mesh;
}

void Engine::ProcessNode(Model& model, aiNode* node, const aiScene& scene,
                         std::vector<std::shared_ptr<Mesh>>& meshes,
                         int32_t parent_node_index) {
  int32_t node_index = static_cast<int32_t>(model.node_hierarchy.nodes.size());

  NodeInfo node_info;
  node_info.name = node->mName.C_Str();
  node_info.parent_index = parent_node_index;
  node_info.local_transform = ConvertMatrix(node->mTransformation);
  node_info.bone_index = -1;

  model.node_hierarchy.node_name_to_index[node_info.name] = node_index;

  for (uint32_t i = 0; i < node->mNumMeshes; i++) {
    int32_t mesh_index = static_cast<int32_t>(meshes.size());
    aiMesh* mesh_ptr = scene.mMeshes[node->mMeshes[i]];
    std::shared_ptr<Mesh> mesh = ProcessMesh(model, mesh_ptr, scene);
    if (mesh == nullptr) {
      continue;
    }
    mesh->node_index = node_index;
    node_info.mesh_indices.push_back(mesh_index);
    meshes.push_back(mesh);
  }

  // Reserve placeholder children indices
  for (uint32_t i = 0; i < node->mNumChildren; i++) {
    node_info.children.push_back(-1);
  }

  model.node_hierarchy.nodes.push_back(std::move(node_info));

  // Recurse children, filling in child indices
  for (uint32_t i = 0; i < node->mNumChildren; i++) {
    int32_t child_index =
        static_cast<int32_t>(model.node_hierarchy.nodes.size());
    model.node_hierarchy.nodes[node_index].children[i] = child_index;
    ProcessNode(model, node->mChildren[i], scene, meshes, node_index);
  }
}

void Engine::ExtractAnimations(Model& model, const aiScene& scene) {
  if (scene.mNumAnimations == 0) {
    return;
  }
  model.has_animations = true;

  for (uint32_t a = 0; a < scene.mNumAnimations; a++) {
    aiAnimation* anim = scene.mAnimations[a];
    AnimationClip clip;
    clip.name = anim->mName.C_Str();
    clip.duration = static_cast<float>(anim->mDuration);
    clip.ticks_per_second = anim->mTicksPerSecond > 0
                                ? static_cast<float>(anim->mTicksPerSecond)
                                : 25.0f;

    for (uint32_t c = 0; c < anim->mNumChannels; c++) {
      aiNodeAnim* chan = anim->mChannels[c];
      AnimationChannel ch;
      ch.node_name = chan->mNodeName.C_Str();

      ch.position_keys.reserve(chan->mNumPositionKeys);
      for (uint32_t k = 0; k < chan->mNumPositionKeys; k++) {
        ch.position_keys.push_back(
            {static_cast<float>(chan->mPositionKeys[k].mTime),
             glm::vec3(chan->mPositionKeys[k].mValue.x,
                       chan->mPositionKeys[k].mValue.y,
                       chan->mPositionKeys[k].mValue.z)});
      }

      ch.rotation_keys.reserve(chan->mNumRotationKeys);
      for (uint32_t k = 0; k < chan->mNumRotationKeys; k++) {
        ch.rotation_keys.push_back(
            {static_cast<float>(chan->mRotationKeys[k].mTime),
             glm::quat(chan->mRotationKeys[k].mValue.w,
                       chan->mRotationKeys[k].mValue.x,
                       chan->mRotationKeys[k].mValue.y,
                       chan->mRotationKeys[k].mValue.z)});
      }

      ch.scale_keys.reserve(chan->mNumScalingKeys);
      for (uint32_t k = 0; k < chan->mNumScalingKeys; k++) {
        ch.scale_keys.push_back(
            {static_cast<float>(chan->mScalingKeys[k].mTime),
             glm::vec3(chan->mScalingKeys[k].mValue.x,
                       chan->mScalingKeys[k].mValue.y,
                       chan->mScalingKeys[k].mValue.z)});
      }

      clip.channels.push_back(std::move(ch));
    }

    model.animation_clips.push_back(std::move(clip));
  }

  // Link bone parent indices via node hierarchy
  for (auto& bone : model.skeleton.bones) {
    auto node_it = model.node_hierarchy.node_name_to_index.find(bone.name);
    if (node_it != model.node_hierarchy.node_name_to_index.end()) {
      int32_t node_idx = node_it->second;
      model.node_hierarchy.nodes[node_idx].bone_index =
          model.skeleton.bone_name_to_index[bone.name];
      // Walk up to find parent bone
      int32_t parent_node = model.node_hierarchy.nodes[node_idx].parent_index;
      while (parent_node >= 0) {
        auto parent_bone_it = model.skeleton.bone_name_to_index.find(
            model.node_hierarchy.nodes[parent_node].name);
        if (parent_bone_it != model.skeleton.bone_name_to_index.end()) {
          bone.parent_index = parent_bone_it->second;
          break;
        }
        parent_node = model.node_hierarchy.nodes[parent_node].parent_index;
      }
    }
  }
}

glm::mat4 Engine::ConvertMatrix(const aiMatrix4x4& from) {
  return glm::transpose(glm::make_mat4(&from.a1));
}

}  // namespace Wiesel