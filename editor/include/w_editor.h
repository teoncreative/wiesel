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
#include "w_anim_controller_editor.h"

// clang-format off
// Import order important
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>
#include <ImGuizmo.h>
// clang-format on

#include "behavior/w_behavior.h"
#include "events/w_appevents.h"
#include "events/w_mouseevents.h"
#include "rendering/w_camera.h"
#include "scene/w_entity.h"
#include "scene/w_scene.h"
#include "util/w_filewatcher.h"
#include "util/w_user_config.h"
#include "w_application.h"
#include "w_asset_browser_panel.h"
#include "w_lsp_autocomplete.h"
#include "w_lsp_client.h"
#include "w_notifications.h"
#include "w_project.h"
#include "w_undo.h"
#include "w_vfs_browser.h"

namespace wiesel::editor {

class RecentProjects {
 public:
  static constexpr int kMaxRecent = 10;

  static const std::vector<std::string>& Load();
  static void Save(const std::vector<std::string>& paths);
  static void Add(const std::string& path);

 private:
  static std::filesystem::path GetConfigPath();
  static std::vector<std::string> cached_;
  static bool cache_valid_;
};

enum class EditorState { Edit, Playing, Paused };

struct HierarchyDragPayload {
  EntityRef entity_ref;
};

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

  void RenderEntity(Entity& entity, int depth,
                    bool& ignore_menu);
  void UpdateHierarchyOrder();

  void OnBeginPresent() override;
  void OnPostPresent() override;
  void OnPrePresent() override;

 private:
  void TakeSnapshot();
  void RestoreSnapshot();

  // Sync EditorSelectedComponent marker with current selection
  void SyncEditorSelectedComponent();

  // Entity picking helpers
  EntityRef FindSpriteAtNDC(glm::vec2 ndc);
  bool FindSpritesInScene(Scene* scene,
                          const glm::mat4& vp, glm::vec2 pick_ndc,
                          entt::entity& best, float& best_depth);

  // Toolbar / Menu
  void RenderMainMenuBar();
  void RenderProjectSettingsPopup();
  void RenderCreateSkyboxPopup();
  void RenderCreateSpritePopup();
  void RenderSliceSpritesPopup();
  void RenderCreateAnimControllerPopup();
  void RenderCreateCursorSetPopup();
  void RenderCreateMeshColliderPopup();
  void RenderEntityInspector(Entity selected_entity);
  void RenderAssetPropertiesPanel();
  void NewProject();
  void OpenProject();
  void SaveProject();
  void NewScene();
  void SaveScene();
  void SaveSceneAs();

  // Instantiate a model asset into the current scene with undo support.
  void InstantiateModelAsset(AssetHandle handle);
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
  void RenderInspectorPanel();
  void RenderAssetBrowserPanel();
  void RenderDeveloperConsolePanel();
  void RenderRenderStatsPanel();
  void RenderUndoHistoryPanel();
  void RenderSceneViewportPanel();
  void RenderResolutionDropdown();
  void RenderGameViewportPanel();
  bool DrawPlayStopButtons();

  Application& app_;
  std::shared_ptr<Project> active_project_;

  // Undo/redo
  CommandStack command_stack_;
  std::string status_toast_text_;
  float status_toast_timer_ = 0.0f;
  static constexpr float kStatusToastDuration = 1.5f;
  void ShowStatusToast(const std::string& text);
  void PerformUndo();
  void PerformRedo();

  // Selection state
  EntityRef selected_entity_{};
  bool scroll_to_selected_ = false;

  // Hierarchy state
  char hierarchy_search_[256] = {};
  EntityRef renaming_entity_{};
  char rename_entity_buf_[256] = {};
  std::unordered_set<EntityRef> open_ancestors_;
  std::string entity_clipboard_;

  struct SceneHierarchyData {
    EntityRef move_from{};
    EntityRef move_to{};
    bool bottom_part = false;
  };

  SceneHierarchyData hierarchy_data_;

  // Gizmo state
  ImGuizmo::OPERATION current_op_ = ImGuizmo::TRANSLATE;
  ImGuizmo::MODE current_mode_ = ImGuizmo::LOCAL;

  // Panel visibility
  bool panel_scene_hierarchy_ = true;
  bool panel_components_ = true;
  bool panel_asset_browser_ = true;
  bool panel_console_ = true;
  bool panel_stats_ = true;
  bool panel_scene_view_ = true;
  bool panel_game_view_ = true;
  bool panel_lsp_debug_ = false;
  bool panel_editor_settings_ = false;
  bool panel_undo_history_ = false;
  bool panel_font_debug_ = false;
  bool panel_network_ = false;
  bool layout_initialized_ = false;

  // File watchers
  FileWatcher script_watcher_;
  FileWatcher ui_file_watcher_;
  FileWatcher asset_dir_watcher_;
  bool script_reload_pending_ = false;
  bool window_focused_ = true;

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
  bool show_create_sprite_ = false;
  bool show_create_animcontroller_ = false;
  bool show_create_cursorset_ = false;
  bool show_create_meshcollider_ = false;
  bool show_slice_sprites_ = false;
  AssetHandle slice_texture_handle_;  // texture being sliced
  AssetBrowserPanel asset_browser_panel_;
  VfsFilePicker file_picker_;
  NotificationManager notifications_;

  // Inspector mode: entity or asset
  enum class InspectorMode { Entity, Asset };
  InspectorMode inspector_mode_ = InspectorMode::Entity;
  AssetHandle inspector_asset_handle_;
  bool inspector_asset_read_only_ = false;

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
    CloseProject,
    StopPlaying
  };
  DeferredAction deferred_action_ = DeferredAction::None;
  std::string deferred_path_;
  void ProcessDeferredActions();

  // Animation controller editor
  AnimControllerEditor anim_controller_editor_;

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

  // Hover state
  int hover_line_ = -1;
  int hover_col_ = -1;
  float hover_timer_ = 0.0f;
  bool hover_requested_ = false;
  std::string hover_text_;

  // Signature help state
  bool signature_active_ = false;
  bool signature_pending_ = false;
  char signature_trigger_ = 0;
  int signature_line_ = -1;
  int signature_col_ = -1;
  LspSignatureHelp signature_help_;
  void OpenCodeEditor(const std::filesystem::path& path);
  void RenderCodeEditor();
  void RenderLspDebugPanel();
  void RenderFontDebugPanel();
  void RenderEditorSettingsPanel();
  void RenderNetworkPanel();
  void SaveCodeEditorFile();
  void StartLsp();
  void StopLsp();

  // Scene snapshot for Play/Stop restore (full scene JSON)
  std::string play_mode_snapshot_;

  // Editor preferences (theme, layout, etc.)
  std::unique_ptr<UserConfig> editor_config_;
};
}  // namespace wiesel::editor

#endif  //WIESEL_PARENT_W_EDITOR_H