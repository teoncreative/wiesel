//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "w_editor.h"

#include <wpak/wpak.h>
#include "asset/w_asset_manager.h"
#include "game/w_game_loader.h"
#include "mono_compiler.h"
#include "scene/w_prefab.h"
#include "scene/w_scene_manager.h"
#include "scene/w_scene_serializer.h"
#include "script/w_scriptmanager.h"
#include "util/w_dialogs.h"
#include "util/w_discord_rpc.h"
#include "util/w_platform.h"
#include "w_engine.h"
#include "w_project_loader.h"
#include "w_thumbnail_cache.h"
#include "w_undo.h"

namespace Wiesel::Editor {

// Defined in w_editor.cc
std::shared_ptr<Scene> scene();

// Save a scene to disk, preserving the asset_handle field.
static bool SaveSceneToFile(const std::shared_ptr<Scene>& s,
                            const std::filesystem::path& path) {
  SceneSerializer serializer(s);
  nlohmann::json root = nlohmann::json::parse(serializer.SerializeToString());

  // Preserve existing asset_handle from the file
  {
    std::ifstream existing(path);
    if (existing.is_open()) {
      try {
        nlohmann::json old_json;
        existing >> old_json;
        std::string handle_str = old_json.value("asset_handle", "");
        if (!handle_str.empty()) {
          root["asset_handle"] = handle_str;
        }
      } catch (const std::exception& e) {
        DCON_LOG_ERROR("Failed to read scene: {}", e.what());
      }
    }
  }

  std::ofstream file(path);
  if (!file.is_open()) {
    return false;
  }
  file << root.dump(2);
  return true;
}

void EditorLayer::NewProject() {
  Dialogs::SelectFolderDialog([this](const std::string& folder) {
    if (folder.empty()) {
      return;
    }

    // Ask for project name via a simple approach: use folder name
    std::filesystem::path dir(folder);
    std::string name = dir.filename().string();

    if (Project::Create(dir, name)) {
      auto [result, proj] = Project::Load(dir / (name + ".wiesel"));
      if (result == ProjectLoadResult::Success && proj) {
        active_project_ = std::move(proj);
        Engine::SetGameInfo(
            std::make_shared<GameInfo>(active_project_->GetGameInfo()));
        auto project = active_project_;

        // Mount project assets
        auto* vfs = Engine::vfs().get();
        vfs->Unmount("app://");
        vfs->Mount("app://", project->GetAssetsDirectory().string());

        // Create the default scene file
        ClearScene();
        current_scene_path_ = "app://scenes/main.wscene";
        SaveScene();

        ScanProjectAssets();
        Engine::script_manager().ReloadAsync();
        RecentProjects::Add(
            std::filesystem::absolute(dir / (name + ".wiesel")).string());
        UpdateWindowTitle();
        DCON_LOG_INFO("Created project: {} at {}", name, folder);
      }
    }
  });
}

void EditorLayer::OpenProject() {
  Dialogs::OpenFileDialog({{"Wiesel Project", "wiesel"}},
                          [this](const std::string& file) {
                            if (file.empty()) {
                              return;
                            }
                            LoadProjectFromPath(file);
                          });
}

void EditorLayer::SaveProject() {
  auto project = active_project_;
  if (project) {
    // Capture current render options into project settings
    GameLoader::CaptureRenderOptions(project->GetGameInfo().render_options);

    // Save editor camera state
    auto& ec = project->GetSettings().editor_camera;
    ec.position = editor_camera_transform_.GetPosition();
    ec.yaw = editor_yaw_;
    ec.pitch = editor_pitch_;
    ec.speed = camera_speed_;
    ec.sensitivity = mouse_sensitivity_;
    ec.mode = (editor_camera_mode_ == EditorCameraMode::Mode2D) ? 1 : 0;
    ec.zoom_2d = editor_2d_zoom_;
    ec.fov = editor_camera_.field_of_view;

    project->Save();
    DCON_LOG_INFO("Project saved");
  }
}

void EditorLayer::NewScene() {
  if (!active_project_) {
    return;
  }

  AutoSave();

  Engine::vfs()->CreateDirectory("app://scenes");

  std::string base_name = "new_scene";
  std::string vfs_path = "app://scenes/" + base_name + ".wscene";
  int counter = 1;
  while (Engine::vfs()->FileExists(vfs_path)) {
    vfs_path = "app://scenes/" + base_name + "_" + std::to_string(counter++) +
               ".wscene";
  }

  ClearScene();
  current_scene_path_ = vfs_path;
  SaveScene();
  ScanProjectAssets();
  UpdateWindowTitle();
}

void EditorLayer::OpenScene(const std::string& vfs_path) {
  AutoSave();
  command_stack_.Clear();

  Engine::scene_manager().LoadSceneFromPath(vfs_path);

  current_scene_path_ = vfs_path;
  scene_dirty_ = false;

  // Track last opened scene in project
  if (std::shared_ptr<Project> p = active_project_) {
    AssetHandle scene_handle =
        Engine::asset_manager().FindBySourcePath(vfs_path);
    if (scene_handle.IsValid()) {
      p->GetSettings().last_scene = scene_handle;
    }
    p->Save();
  }

  UpdateWindowTitle();
}

void EditorLayer::SaveScene() {
  if (current_scene_path_.empty()) {
    SaveSceneAs();
    return;
  }

  // Ensure parent directory exists
  Engine::vfs()->CreateDirectory(
      VirtualFileSystem::Parent(current_scene_path_));

  // Resolve VFS to physical for SaveSceneToFile (needs std::ofstream)
  auto physical = Engine::vfs()->ResolvePhysicalPath(current_scene_path_);
  if (!physical) {
    DCON_LOG_ERROR("Cannot resolve scene path: {}", current_scene_path_);
    return;
  }

  if (SaveSceneToFile(scene(), *physical)) {
    scene_dirty_ = false;
    UpdateWindowTitle();
    auto_save_timer_ = 0.0f;

    if (active_project_) {
      active_project_->Save();

      std::string scene_name = VirtualFileSystem::Stem(current_scene_path_);
      Engine::scene_manager().RegisterScene(scene_name, current_scene_path_);

      AssetManager& mgr = Engine::asset_manager();
      if (!mgr.FindBySourcePath(current_scene_path_).IsValid()) {
        mgr.Register(scene_name, AssetType::Scene, current_scene_path_);
      }
    }
  }
}

void EditorLayer::SaveSceneAs() {
  file_picker_.OpenSave("Save Scene As", ".wscene",
                        [this](const std::string& vfs_path) {
                          current_scene_path_ = vfs_path;
                          SaveScene();
                          ScanProjectAssets();
                        });
}

void EditorLayer::ClearScene() {
  command_stack_.Clear();
  // Stop playing if active
  if (editor_state_ == EditorState::Playing) {
    editor_state_ = EditorState::Edit;
  }

  has_selected_entity_ = false;
  ThumbnailCache::Get()->Clear();

  // Remove all entities
  auto& hierarchy = scene()->GetSceneHierarchy();
  std::vector<entt::entity> to_remove(hierarchy.begin(), hierarchy.end());
  for (auto entity_id : to_remove) {
    Entity entity{entity_id, scene().get()};
    scene()->RemoveEntity(entity);
  }
  scene()->ProcessDestroyQueue();

  scene()->ResetPhysicsWorld();
  scene_dirty_ = false;
}

void EditorLayer::UpdateWindowTitle() {
  std::string title = "Wiesel Editor";
  auto project = active_project_;
  if (project) {
    title += " - " + project->GetSettings().name;
  }
  if (editing_prefab_) {
    title += " - [Prefab] " + editing_prefab_path_;
  } else if (!current_scene_path_.empty()) {
    title += " - " + VirtualFileSystem::Stem(current_scene_path_);
  }
  if (scene_dirty_) {
    title += " *";
  }
  Engine::window()->SetTitle(title);
}

void EditorLayer::AutoSave() {
  if (!scene_dirty_ || current_scene_path_.empty()) {
    return;
  }

  auto physical = Engine::vfs()->ResolvePhysicalPath(current_scene_path_);
  if (!physical) {
    return;
  }
  if (SaveSceneToFile(scene(), *physical)) {
    scene_dirty_ = false;
    auto_save_timer_ = 0.0f;
    UpdateWindowTitle();
    LOG_DEBUG("Auto-saved scene: {}", current_scene_path_);
  }
}

void EditorLayer::LoadProjectFromPath(const std::filesystem::path& path) {
  namespace fs = std::filesystem;

  auto [load_result, proj] = Project::Load(path);
  if (load_result != ProjectLoadResult::Success || !proj) {
    return;
  }

  active_project_ = std::move(proj);
  Engine::SetGameInfo(
      std::make_shared<GameInfo>(active_project_->GetGameInfo()));
  std::shared_ptr<Project> project = active_project_;

  // Remove startup FPS cap now that a project is loaded
  app_.SetMaxFPS(0.0f);

  ProjectLoader::LoadAll(*project, false);

  // Start watching app directory for script hot reload
  std::optional<std::filesystem::path> app_dir =
      Engine::vfs()->GetPhysicalPath("app://");
  if (app_dir.has_value()) {
    script_watcher_.SetExtensionFilter(".cs");
    script_watcher_.Watch(*app_dir, true);

    // Watch for UI file hot reload (.rml/.rcss)
    ui_file_watcher_.SetPatternFilter("\\.(rml|rcss)$");
    ui_file_watcher_.Watch(*app_dir, true);

    // Watch for any filesystem changes to refresh the asset browser
    asset_dir_watcher_.Watch(*app_dir, true);
  }

  // Open last scene or start scene (prefer last_scene, fall back to start)
  auto resolve_scene_vfs = [](const AssetHandle& handle) -> std::string {
    if (!handle.IsValid()) {
      return "";
    }
    const AssetMetadata* meta = Engine::asset_manager().GetMetadata(handle);
    if (!meta || meta->virtual_source_path.empty()) {
      return "";
    }
    return meta->virtual_source_path;
  };

  std::string scene_to_open =
      resolve_scene_vfs(project->GetSettings().last_scene);
  if (scene_to_open.empty()) {
    scene_to_open = resolve_scene_vfs(project->GetGameInfo().start_scene);
  }

  if (!scene_to_open.empty()) {
    OpenScene(scene_to_open);
  }

  // Restore editor camera state
  {
    ProjectSettings::EditorCameraState& ec =
        project->GetSettings().editor_camera;
    editor_camera_transform_.SetPosition(ec.position);
    editor_camera_transform_.SetRotation(ec.pitch, ec.yaw, 0.0f);
    editor_yaw_ = ec.yaw;
    editor_pitch_ = ec.pitch;
    camera_speed_ = ec.speed;
    mouse_sensitivity_ = ec.sensitivity;
    editor_camera_.field_of_view = ec.fov;
    editor_2d_zoom_ = ec.zoom_2d;

    if (ec.mode == 1) {
      editor_camera_mode_ = EditorCameraMode::Mode2D;
      editor_camera_.projection_mode = ProjectionMode::Orthographic;
      editor_camera_.ortho_size = ec.zoom_2d;
    } else {
      editor_camera_mode_ = EditorCameraMode::Free;
      editor_camera_.projection_mode = ProjectionMode::Perspective;
    }
    editor_camera_.view_changed = true;
    editor_camera_.resource_pipeline_version = 0;
  }

  RecentProjects::Add(fs::absolute(path).string());
  UpdateWindowTitle();
#ifdef WIESEL_DISCORD_RPC
  Engine::discord_rpc().SetPresence("Working on " + project->GetSettings().name,
                                    "Editing", "wiesel_logo", "Wiesel Engine");
#endif
  DCON_LOG_INFO("Opened project: {}", project->GetSettings().name);
}

void EditorLayer::ScanProjectAssets() {
  std::shared_ptr<Wiesel::Project> project = active_project_;
  if (project) {
    ProjectLoader::ScanAssets(*project);
    project->Save();

    ThumbnailCache::Get()->RemoveStale();

    // Clear stale skybox reference if its asset was deleted
    AssetHandle sky = scene()->GetSkyboxAsset();
    if (sky.IsValid() && !Engine::asset_manager().HasAsset(sky)) {
      scene()->SetSkyboxAsset({});
    }

    // Recompile scripts if any .cs files changed
    Engine::script_manager().ReloadAsync();
  }
}

void EditorLayer::OpenPrefabForEditing(const std::string& vfs_path) {
  namespace fs = std::filesystem;

  // Save current scene state
  AutoSave();
  prefab_return_scene_path_ = current_scene_path_;

  // Clear scene and load prefab as a temporary scene
  ClearScene();
  AssetHandle prefab_handle =
      Engine::asset_manager().FindBySourcePath(vfs_path);
  Entity root = Prefab::Instantiate(scene(), prefab_handle);
  if (root.handle() == entt::null) {
    DCON_LOG_ERROR("Failed to open prefab for editing: {}", vfs_path);
    // Restore previous scene
    if (!prefab_return_scene_path_.empty()) {
      OpenScene(prefab_return_scene_path_);
    }
    return;
  }

  editing_prefab_ = true;
  editing_prefab_path_ = vfs_path;
  current_scene_path_.clear();
  scene_dirty_ = false;
  UpdateWindowTitle();

  // Setup camera components
  for (entt::entity entity : scene()->GetAllEntitiesWith<CameraComponent>()) {
    auto& cam = scene()->GetComponent<CameraComponent>(entity);
    Engine::renderer()->SetupCameraComponent(cam);
  }

  DCON_LOG_INFO("Editing prefab: {}", vfs_path);
}

void EditorLayer::SavePrefab() {
  if (!editing_prefab_ || editing_prefab_path_.empty()) {
    return;
  }

  // Find the root entity (first in hierarchy - the prefab root)
  std::vector<entt::entity>& hierarchy = scene()->GetSceneHierarchy();
  if (hierarchy.empty()) {
    DCON_LOG_ERROR("Cannot save prefab: scene is empty");
    return;
  }

  Entity root = {hierarchy[0], scene().get()};
  if (Prefab::SaveToFile(root, editing_prefab_path_)) {
    scene_dirty_ = false;
    DCON_LOG_INFO("Prefab saved: {}", editing_prefab_path_);
  }
}

void EditorLayer::ClosePrefabEditor() {
  if (!editing_prefab_) {
    return;
  }

  editing_prefab_ = false;
  editing_prefab_path_.clear();

  // Return to previous scene
  if (!prefab_return_scene_path_.empty()) {
    OpenScene(prefab_return_scene_path_);
    prefab_return_scene_path_.clear();
  } else {
    ClearScene();
  }
}

void EditorLayer::ExportGame() {
  if (!active_project_) {
    return;
  }

  // Save current scene first
  if (!current_scene_path_.empty()) {
    SaveScene();
  }
  SaveProject();

  Dialogs::SelectFolderDialog([this](const std::string& selected_dir) {
    namespace fs = std::filesystem;
    if (selected_dir.empty()) {
      return;
    }

    fs::path export_dir =
        fs::path(selected_dir) / active_project_->GetSettings().name;
    ExportGame(selected_dir);
  });
}

void EditorLayer::ExportGame(const std::filesystem::path& export_dir) {
  namespace fs = std::filesystem;
  fs::create_directories(export_dir);

  DCON_LOG_INFO("Exporting game to: {}", export_dir.string());

  int next_pak_index_ = 1;

  // Write gameinfo.wgame with search_paths for the export
  {
    GameInfo export_info = active_project_->GetGameInfo();
    export_info.search_paths = {"."};
    export_info.Save(export_dir / "gameinfo.wgame");
  }

  // Pack app assets (excluding build artifacts and source files)
  fs::path src_assets = active_project_->GetAssetsDirectory();
  if (fs::exists(src_assets)) {
    Wpak::Result<std::vector<Wpak::PackEntry>> app_files =
        Wpak::CollectFiles(src_assets);
    if (app_files.success) {
      // Filter out files that shouldn't be in the export
      std::vector<Wpak::PackEntry> filtered;
      for (const Wpak::PackEntry& entry : app_files.value) {
        fs::path rel(entry.relative_path);
        std::string ext = rel.extension().string();
        std::string first_dir =
            rel.begin() != rel.end() ? rel.begin()->string() : "";

        // Skip build artifacts and source files
        if (first_dir == "obj" || first_dir == "bin" || first_dir == "out") {
          continue;
        }
        if (ext == ".cs" || ext == ".csproj" || ext == ".sln" ||
            ext == ".pdb" || ext == ".mdb") {
          continue;
        }

        filtered.push_back(entry);
      }

      auto pack_result =
          Wpak::WriteArchive(export_dir, filtered, "app://", next_pak_index_);
      if (!pack_result.success) {
        DCON_LOG_ERROR("Export: Failed to pack app assets: {}",
                       pack_result.error.message);
        return;
      }
      next_pak_index_ += static_cast<int>(pack_result.value.size());
      DCON_LOG_INFO("Export: Packed {} assets ({} excluded)", filtered.size(),
                    app_files.value.size() - filtered.size());
    }
  }

  // Compile scripts (build in project's out/ dir, copy DLLs to export)
  {
    fs::path project_dir = active_project_->GetProjectDirectory();
    fs::path build_out = project_dir / "out";
#ifdef NDEBUG
    bool debug_build = false;
#else
    bool debug_build = true;
#endif

    // Core.dll
    std::vector<std::string> core_sources;
    std::optional<fs::path> core_physical =
        Engine::vfs()->GetPhysicalPath("engine://scripts");
    if (core_physical.has_value() && fs::exists(*core_physical)) {
      for (const fs::directory_entry& entry :
           fs::recursive_directory_iterator(*core_physical)) {
        if (entry.is_regular_file() && entry.path().extension() == ".cs") {
          core_sources.push_back(entry.path().string());
        }
      }
    }
    if (!core_sources.empty()) {
      DotNetProject core("Core");
      core.SetOutputPath((build_out / "Core.dll").string());
      core.SetSources(core_sources);
      CompileResult result = core.Build(debug_build);
      if (!result.success) {
        DCON_LOG_ERROR("Export: Core compilation failed:\n{}", result.output);
        return;
      }
      fs::copy_file(build_out / "Core.dll", export_dir / "Core.dll",
                    fs::copy_options::overwrite_existing);
      if (debug_build && fs::exists(build_out / "Core.pdb")) {
        fs::copy_file(build_out / "Core.pdb", export_dir / "Core.pdb",
                      fs::copy_options::overwrite_existing);
      }
    }

    // App.dll
    std::vector<std::string> app_sources;
    for (const fs::directory_entry& entry :
         fs::recursive_directory_iterator(src_assets)) {
      if (entry.is_regular_file() && entry.path().extension() == ".cs") {
        app_sources.push_back(entry.path().string());
      }
    }
    if (!app_sources.empty()) {
      std::vector<std::string> link_libs;
      fs::path core_dll = build_out / "Core.dll";
      if (fs::exists(core_dll)) {
        link_libs.push_back(core_dll.string());
      }
      DotNetProject app("App");
      app.SetOutputPath((build_out / "App.dll").string());
      app.SetSources(app_sources);
      app.SetReferences(link_libs);
      CompileResult result = app.Build(debug_build);
      if (!result.success) {
        DCON_LOG_ERROR("Export: App compilation failed:\n{}", result.output);
        return;
      }
      fs::copy_file(build_out / "App.dll", export_dir / "App.dll",
                    fs::copy_options::overwrite_existing);
      if (debug_build && fs::exists(build_out / "App.pdb")) {
        fs::copy_file(build_out / "App.pdb", export_dir / "App.pdb",
                      fs::copy_options::overwrite_existing);
      }
    }
  }

  // Pack engine assets
  {
    std::optional<fs::path> engine_assets =
        Engine::vfs()->GetPhysicalPath("engine://");
    if (engine_assets.has_value() && fs::exists(*engine_assets)) {
      Wpak::Result<std::vector<Wpak::PackEntry>> files =
          Wpak::CollectFiles(*engine_assets);
      if (files.success) {
        auto pack_result = Wpak::WriteArchive(export_dir, files.value,
                                              "engine://", next_pak_index_);
        if (!pack_result.success) {
          DCON_LOG_ERROR("Export: Failed to pack engine assets: {}",
                         pack_result.error.message);
        } else {
          next_pak_index_ += static_cast<int>(pack_result.value.size());
        }
      }
    }
  }

  // Copy runtime executable, renamed to game name
  {
#ifdef _WIN32
    std::string runtime_name = "wiesel-runtime.exe";
    std::string game_ext = ".exe";
#else
    std::string runtime_name = "wiesel-runtime";
    std::string game_ext = "";
#endif
    // Search next to the editor executable, then CWD
    fs::path exe_dir = GetExecutableDirectory();
    fs::path runtime_src;
    for (const fs::path& search_dir :
         {exe_dir, fs::current_path(), exe_dir / ".." / "runtime"}) {
      fs::path candidate = search_dir / runtime_name;
      if (fs::exists(candidate)) {
        runtime_src = candidate;
        break;
      }
    }

    if (!runtime_src.empty()) {
      fs::path dst_name =
          export_dir / (active_project_->GetSettings().name + game_ext);
      fs::copy_file(runtime_src, dst_name,
                    fs::copy_options::overwrite_existing);
      DCON_LOG_INFO("Export: Copied runtime as {}",
                    dst_name.filename().string());
    } else {
      DCON_LOG_WARN(
          "Export: wiesel-runtime not found. Build the 'wiesel-runtime' "
          "target first.");
    }
  }

  DCON_LOG_INFO("Export complete: {}", export_dir.string());

  // Open the export directory
  OpenFileInDefaultEditor(export_dir);
}

void EditorLayer::InstantiateModelAsset(AssetHandle handle) {
  const auto* meta = Engine::asset_manager().GetMetadata(handle);
  if (!meta || meta->type != AssetType::Model) {
    return;
  }
  auto entity = scene()->InstantiateModel(handle, meta->name);
  if (entity.handle() != entt::null) {
    auto shared_scene = Engine::scene_manager().FindSceneByPtr(scene().get());
    if (shared_scene) {
      command_stack_.Execute(
          std::make_unique<EntityCreateCommand>(shared_scene, entity.handle()));
    }
    selected_entity_ = entity.handle();
    selected_entity_scene_ = shared_scene;
    has_selected_entity_ = true;
    scene_dirty_ = true;
  }
}

}  // namespace Wiesel::Editor
