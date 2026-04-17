
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "rendering/w_renderer.h"
#include "util/w_vfs.h"
#include "w_application.h"

namespace wiesel {

static constexpr const char* kEngineVersion = "2026.4.1";

class Scene;
class AssetManager;
class ScriptManager;
class SceneManager;
class UIManager;
class CursorManager;
class NativeBehaviorRegistry;
class AudioManager;
class NetworkManager;
class NetworkSceneManager;
class ReplicationManager;
class ThreadPool;
class UserConfig;
class InputManager;
struct GameInfo;
#ifdef WIESEL_DISCORD_RPC
class DiscordRPC;
#endif

struct EngineProperties {
  bool editor_enabled = false;
  bool enable_stdio = false;
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

  WIESEL_GETTER_FN static InputManager& input() { return *input_manager_; }

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

  WIESEL_GETTER_FN static NetworkManager& network() {
    return *network_manager_;
  }

  WIESEL_GETTER_FN static NetworkSceneManager& network_scene_manager() {
    return *network_scene_manager_;
  }

  WIESEL_GETTER_FN static ReplicationManager& replication_manager() {
    return *replication_manager_;
  }

  WIESEL_GETTER_FN static SceneManager& scene_manager() {
    return *scene_manager_;
  }

  WIESEL_GETTER_FN static UIManager& ui_manager() { return *ui_manager_; }

  WIESEL_GETTER_FN static CursorManager& cursor_manager() {
    return *cursor_manager_;
  }

  WIESEL_GETTER_FN static std::shared_ptr<GameInfo> game_info() {
    return game_info_;
  }

  static void SetGameInfo(std::shared_ptr<GameInfo> info);

  // Returns nullptr if no game is loaded yet.
  WIESEL_GETTER_FN static UserConfig* game_config() {
    return game_config_.get();
  }

  static void InitGameConfig(const std::string& game_name);

#ifdef WIESEL_DISCORD_RPC
  WIESEL_GETTER_FN static DiscordRPC& discord_rpc() { return *discord_rpc_; }
#endif
  WIESEL_GETTER_FN static const EngineProperties& properties() {
    return properties_;
  }

  static void BroadcastEvent(Event& event);

 private:

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
  static std::unique_ptr<InputManager> input_manager_;
  static std::unique_ptr<AssetManager> asset_manager_;

  static std::unique_ptr<ScriptManager> script_manager_;
  static std::unique_ptr<NativeBehaviorRegistry> behavior_registry_;
  static std::unique_ptr<ThreadPool> thread_pool_;
  static std::unique_ptr<AudioManager> audio_manager_;
  static std::unique_ptr<NetworkManager> network_manager_;
  static std::unique_ptr<NetworkSceneManager> network_scene_manager_;
  static std::unique_ptr<ReplicationManager> replication_manager_;
  static std::unique_ptr<SceneManager> scene_manager_;
  static std::unique_ptr<UIManager> ui_manager_;
  static std::unique_ptr<CursorManager> cursor_manager_;
  static std::shared_ptr<GameInfo> game_info_;
  static std::unique_ptr<UserConfig> game_config_;
  static Application* application_;
#ifdef WIESEL_DISCORD_RPC
  static std::unique_ptr<DiscordRPC> discord_rpc_;
#endif
};

Application* CreateApp();
}  // namespace wiesel