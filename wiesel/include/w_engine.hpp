
//
//   Copyright 2025 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "rendering/w_renderer.hpp"
#include "util/w_command.hpp"
#include "util/w_vfs.hpp"
#include "w_application.hpp"

namespace Wiesel {

class Scene;
class AssetManager;
class ScriptManager;
class SceneManager;
class NativeBehaviorRegistry;
class AudioManager;
class ThreadPool;
struct GameInfo;
#ifdef WIESEL_DISCORD_RPC
class DiscordRPC;
#endif

struct EngineProperties {
  bool editor_enabled = false;
  std::filesystem::path engine_assets_path;
  std::filesystem::path editor_assets_path;
  std::filesystem::path app_assets_path;
  std::filesystem::path project_path;    // editor-only: .wiesel file
  std::filesystem::path game_info_path;  // runtime: gameinfo.wgame file
  std::filesystem::path user_data_path;

  static EngineProperties Parse(int argc, char** argv);
};

class Engine {
 public:
  static void InitEngine(const EngineProperties& props);
  static void InitWindow(const WindowProperties&& props);
  static void InitRenderer(const RendererProperties&& props);
  static void InitApplication();

  static void CleanupAssets();
  static void CleanupRenderer();
  static void CleanupWindow();
  static void CleanupEngine();
  static void CleanupApplication();

  WIESEL_GETTER_FN static std::shared_ptr<Renderer> renderer() {
    return renderer_;
  }

  WIESEL_GETTER_FN static std::shared_ptr<AppWindow> window() {
    return window_;
  }

  WIESEL_GETTER_FN static std::shared_ptr<VirtualFileSystem> vfs() {
    return vfs_;
  }

  WIESEL_GETTER_FN static Application& app() { return *application_; }

  WIESEL_GETTER_FN static DeveloperConsole& console() { return console_; }

  WIESEL_GETTER_FN static AssetManager& asset_manager() {
    return *asset_manager_;
  }

  WIESEL_GETTER_FN static ScriptManager& script_manager() {
    return *script_manager_;
  }

  WIESEL_GETTER_FN static NativeBehaviorRegistry& behavior_registry() {
    return *behavior_registry_;
  }

  WIESEL_GETTER_FN static ThreadPool& thread_pool() { return *thread_pool_; }

  WIESEL_GETTER_FN static AudioManager& audio() { return *audio_manager_; }

  WIESEL_GETTER_FN static SceneManager& scene_manager() {
    return *scene_manager_;
  }

  WIESEL_GETTER_FN static std::shared_ptr<GameInfo> game_info() {
    return game_info_;
  }

  static void SetGameInfo(std::shared_ptr<GameInfo> info);
#ifdef WIESEL_DISCORD_RPC
  WIESEL_GETTER_FN static DiscordRPC& discord_rpc() { return *discord_rpc_; }
#endif
  WIESEL_GETTER_FN static const EngineProperties& properties() {
    return properties_;
  }

  static aiScene* LoadAssimpModel(const std::string& path,
                                  bool convert_to_left_handed = true);

  static bool LoadTextureAsset(AssetHandle handle);
  static bool LoadModel(AssetHandle handle);
  static void LoadModelAsync(AssetHandle handle);

  static void BroadcastEvent(Event& event);

 private:
  static glm::mat4 ConvertMatrix(const aiMatrix4x4& from);
  static bool LoadTexture(Model& model, std::shared_ptr<Mesh> mesh,
                          aiMaterial* mat, aiTextureType type,
                          const aiScene& scene);
  static std::shared_ptr<Texture> LoadEmbeddedTexture(Model& model,
                                                      int tex_index,
                                                      TextureType tex_type,
                                                      const aiScene& scene);
  static std::shared_ptr<Texture> LoadExternalTexture(
      Model& model, const std::string& texture_path, TextureType tex_type);
  static void PreDecodeTextures(Model& model, const aiScene& scene);
  static std::shared_ptr<Texture> CreateTextureFromEmbedded(aiTexture* aiTex,
                                                            TextureType type);
  static unsigned char* ConvertBGRAtoRGBA(void* bgra_data, int width,
                                          int height);
  static std::shared_ptr<Mesh> ProcessMesh(Model& model, aiMesh* aiMesh,
                                           const aiScene& aiScene);
  static void ProcessNode(Model& model, aiNode* node, const aiScene& scene,
                          std::vector<std::shared_ptr<Mesh>>& meshes,
                          int32_t parent_node_index);
  static void ExtractAnimations(Model& model, const aiScene& scene);

  static void InitializeVfs();
  static void RegisterPrimitives();

  // Built-in primitive model handles
  static AssetHandle primitive_cube_;
  static AssetHandle primitive_sphere_;
  static AssetHandle primitive_plane_;
  static AssetHandle primitive_cylinder_;
  static AssetHandle primitive_capsule_;

 public:
  static AssetHandle GetPrimitive(const std::string& name);

 private:
  static EngineProperties properties_;
  static std::shared_ptr<Renderer> renderer_;
  static std::shared_ptr<AppWindow> window_;
  static std::shared_ptr<VirtualFileSystem> vfs_;
  static DeveloperConsole console_;
  static std::shared_ptr<AssetManager> asset_manager_;
  static std::shared_ptr<ScriptManager> script_manager_;
  static std::shared_ptr<NativeBehaviorRegistry> behavior_registry_;
  static std::shared_ptr<ThreadPool> thread_pool_;
  static std::shared_ptr<AudioManager> audio_manager_;
  static std::shared_ptr<SceneManager> scene_manager_;
  static std::shared_ptr<GameInfo> game_info_;
  static Application* application_;
#ifdef WIESEL_DISCORD_RPC
  static std::shared_ptr<DiscordRPC> discord_rpc_;
#endif
};

Application* CreateApp();
}  // namespace Wiesel