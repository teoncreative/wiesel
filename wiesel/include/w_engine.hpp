
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
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
class NativeBehaviorRegistry;
#ifdef WIESEL_DISCORD_RPC
class DiscordRPC;
#endif

struct EngineProperties {
    bool editor_enabled = false;
    std::filesystem::path engine_assets_path;
    std::filesystem::path editor_assets_path;
    std::filesystem::path app_assets_path;
    std::filesystem::path project_path;
    std::filesystem::path user_data_path;
    bool dev_mode = false;  // Affects hot reloading, logging, etc.

    static EngineProperties Parse(int argc, char** argv);
};

class Engine {
 public:
  static void InitEngine(const EngineProperties& props);
  static void InitWindow(const WindowProperties&& props);
  static void InitRenderer(const RendererProperties&& props);

  static void CleanupAssets();
  static void CleanupRenderer();
  static void CleanupWindow();
  static void CleanupEngine();

  WIESEL_GETTER_FN static Ref<Renderer> renderer() { return renderer_; }
  WIESEL_GETTER_FN static Ref<AppWindow> window() { return window_; }
  WIESEL_GETTER_FN static Ref<VirtualFileSystem> vfs() { return vfs_; }
  WIESEL_GETTER_FN static DeveloperConsole& console() { return console_; }
  WIESEL_GETTER_FN static AssetManager& asset_manager() { return *asset_manager_; }
  WIESEL_GETTER_FN static ScriptManager& script_manager() { return *script_manager_; }
  WIESEL_GETTER_FN static NativeBehaviorRegistry& behavior_registry() { return *behavior_registry_; }
#ifdef WIESEL_DISCORD_RPC
  WIESEL_GETTER_FN static DiscordRPC& discord_rpc() { return *discord_rpc_; }
#endif
  WIESEL_GETTER_FN static const EngineProperties& properties() {
    return properties_;
  }

  static aiScene* LoadAssimpModel(const std::string& path,
                                  bool convert_to_left_handed = true);

  static void LoadModelAsync(AssetHandle handle);


 private:
  static glm::mat4 ConvertMatrix(const aiMatrix4x4& from);
  static bool LoadTexture(Model& model, std::shared_ptr<Mesh> mesh, aiMaterial* mat,
                         aiTextureType type, const aiScene& scene);
  static void PreDecodeTextures(Model& model, const aiScene& scene);
  static std::shared_ptr<Texture> CreateTextureFromEmbedded(aiTexture* aiTex, TextureType type);
  static unsigned char* ConvertBGRAtoRGBA(void* bgra_data, int width, int height);
  static std::shared_ptr<Mesh> ProcessMesh(Model& model, aiMesh* aiMesh,
                                     const aiScene& aiScene);
  static void ProcessNode(Model& model, aiNode* node, const aiScene& scene,
                         std::vector<std::shared_ptr<Mesh>>& meshes,
                         int32_t parent_node_index);
  static void ExtractAnimations(Model& model, const aiScene& scene);

  static void InitializeVfs();

 private:
  static EngineProperties properties_;
  static std::shared_ptr<Renderer> renderer_;
  static std::shared_ptr<AppWindow> window_;
  static std::shared_ptr<VirtualFileSystem> vfs_;
  static DeveloperConsole console_;
  static std::shared_ptr<AssetManager> asset_manager_;
  static std::shared_ptr<ScriptManager> script_manager_;
  static std::shared_ptr<NativeBehaviorRegistry> behavior_registry_;
#ifdef WIESEL_DISCORD_RPC
  static std::shared_ptr<DiscordRPC> discord_rpc_;
#endif
};

Application* CreateApp();
}  // namespace Wiesel