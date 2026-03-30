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
// Created by Metehan Gezer on 18/04/2025.
//

#ifndef WIESEL_PARENT_W_EDITOR_H
#define WIESEL_PARENT_W_EDITOR_H

#include "TextEditor.h"
#include "behavior/w_behavior.h"
#include "events/w_appevents.h"
#include "events/w_mouseevents.h"
#include "rendering/w_camera.h"
#include "scene/w_scene.h"
#include "w_application.h"
#include "w_asset_browser_panel.h"
#include "w_lsp_autocomplete.h"
#include "w_lsp_client.h"
#include "w_project.h"
#include "w_vfs_browser.h"

namespace Wiesel::Editor {

class RecentProjects {
 public:
  static constexpr int kMaxRecent = 10;

  static std::vector<std::string> Load();
  static void Save(const std::vector<std::string>& paths);
  static void Add(const std::string& path);

 private:
  static std::filesystem::path GetConfigPath();
};

enum class EditorState { Edit, Playing };

class EditorLayer : public Layer {
 public:
  explicit EditorLayer(Application& app);
  ~EditorLayer() override;

  void OnAttach() override;
  void OnDetach() override;
  void OnUpdate(float_t delta_time) override;
  void OnEvent(Event& event) override;
  bool OnMouseMoved(MouseMovedEvent& event);
  bool OnWindowFocusGained(WindowFocusGainedEvent& event);
  bool OnWindowFocusLost(WindowFocusLostEvent& event);
  bool OnAssetUnloaded(AssetUnloadedEvent& event);

  void RenderEntity(Entity& entity, entt::entity entity_id, int depth,
                    bool& ignore_menu);
  void UpdateHierarchyOrder();

  void OnBeginPresent() override;
  void OnPostPresent() override;
  void OnPrePresent() override;

 private:
  void TakeSnapshot();
  void RestoreSnapshot();

  // Toolbar / Menu
  void RenderMainMenuBar();
  void RenderProjectSettingsPopup();
  void RenderCreateSkyboxPopup();
  void RenderCreateSpritePopup();
  void RenderSliceSpritesPopup();
  void RenderCreateSpriteAnimPopup();
  void RenderCreateSpriteControllerPopup();
  void RenderEntityInspector(entt::entity handle);
  void NewProject();
  void OpenProject();
  void SaveProject();
  void NewScene();
  void SaveScene();
  void SaveSceneAs();
  void ClearScene();
  void OpenScene(const std::string& vfs_path);
  void LoadProjectFromPath(const std::filesystem::path& path);
  void RenderStartupDialog();
  void UpdateWindowTitle();
  void AutoSave();
  void ScanProjectAssets();
  void ExportGame();
  void ExportGame(const std::filesystem::path& path);

  // OnBeginPresent sub-panels
  void InitializeDockspaceLayout(ImGuiID dockspace_id);
  void RenderSceneHierarchyPanel();
  void RenderEntityInspectorPanel();
  void RenderAssetBrowserPanel();
  void RenderDeveloperConsolePanel();
  void RenderRenderStatsPanel();
  void RenderSceneViewportPanel();
  void RenderGameViewportPanel();
  bool DrawPlayStopButtons();

  Application& app_;

  // Project
  std::string current_scene_path_;
  bool scene_dirty_ = false;
  bool prev_scene_dirty_ = false;
  float auto_save_timer_ = 0.0f;
  static constexpr float kAutoSaveInterval = 30.0f;  // seconds

  // Play/Stop state
  EditorState editor_state_ = EditorState::Edit;

  // Editor camera
  enum class EditorCameraMode {
    Free,
    Mode2D,
  };

  CameraComponent editor_camera_;
  TransformComponent editor_camera_transform_;
  EditorCameraMode editor_camera_mode_ = EditorCameraMode::Free;
  float editor_yaw_ = 0.0f;
  float editor_pitch_ = 0.0f;
  float camera_speed_ = 10.0f;
  float mouse_sensitivity_ = 160.0f;
  float editor_2d_zoom_ = 5.0f;  // ortho size in 2D mode
  bool cursor_captured_ = false;
  entt::entity piloting_camera_ =
      entt::null;  // entity whose camera we're piloting
  glm::vec2 pending_pick_ndc_ = {-1,
                                 -1};  // NDC coords for fallback sprite picking
  bool game_panel_focused_ = false;
  bool scene_panel_visible_ = true;
  bool game_panel_visible_ = true;
  int resolution_preset_index_ = 0;  // index into kResolutionPresets
  bool show_about_popup_ = false;
  bool show_project_settings_ = false;
  int project_settings_category_ = 0;
  std::string selected_input_context_;
  int selected_input_item_ =
      -1;  // index into actions or axes of selected context
  bool show_grid_ = true;
  bool show_create_skybox_ = false;
  bool show_create_spriteanim_ = false;
  bool show_create_sprite_ = false;
  bool show_create_spritecontroller_ = false;
  bool show_slice_sprites_ = false;
  AssetHandle slice_texture_handle_;  // texture being sliced
  AssetBrowserPanel asset_browser_panel_;
  VfsFilePicker file_picker_;

  // Asset properties panel
  bool show_asset_properties_ = false;
  AssetHandle properties_asset_handle_;
  void RenderAssetPropertiesPanel();

  // Prefab editing
  bool editing_prefab_ = false;
  std::string editing_prefab_path_;
  std::string prefab_return_scene_path_;
  void OpenPrefabForEditing(const std::string& vfs_path);
  void SavePrefab();
  void ClosePrefabEditor();

  // Deferred scene actions (executed between frames, not during rendering)
  enum class DeferredAction {
    None,
    OpenScene,
    OpenPrefab,
    ClosePrefab,
    OpenProject,
    StopPlaying
  };
  DeferredAction deferred_action_ = DeferredAction::None;
  std::string deferred_path_;
  void ProcessDeferredActions();

  // Code editor
  bool code_editor_open_ = false;
  std::filesystem::path code_editor_path_;
  TextEditor text_editor_;
  bool code_editor_unsaved_ = false;
  ImFont* code_editor_font_ = nullptr;
  bool code_editor_focused_ = false;
  std::string code_editor_uri_;
  LspClient lsp_client_;
  std::unique_ptr<LspAutocompleteProvider> lsp_autocomplete_;
  bool lsp_initialized_ = false;
  bool semantic_tokens_received_ = false;
  void OpenCodeEditor(const std::filesystem::path& path);
  void RenderCodeEditor();
  void RenderLspDebugPanel();
  void SaveCodeEditorFile();
  void StartLsp();
  void StopLsp();

  // Scene snapshot for Play/Stop restore (full scene JSON)
  std::string play_mode_snapshot_;
};
}  // namespace Wiesel::Editor

#endif  //WIESEL_PARENT_W_EDITOR_H