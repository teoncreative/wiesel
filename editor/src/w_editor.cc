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

#include "w_editor.h"

// clang-format off
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>
// clang-format on

#include "util/w_tracy.h"

#include "asset/w_asset_registry.h"
#include "events/w_keyevents.h"
#include "imgui_internal.h"
#include "input/w_input.h"
#include "rendering/w_renderer.h"
#include "scene/w_components.h"
#include "scene/w_prefab.h"
#include "scene/w_scene_manager.h"
#include "scene/w_scene_serializer.h"
#include "script/w_scriptmanager.h"
#include "util/imgui/imgui_theme.h"
#include "w_editor_components.h"
#include <urkern/platform.h>
#include "w_editor_icons.h"
#include "w_engine.h"
#include "w_thumbnail_cache.h"

#include <RmlUi/Core.h>
#include <RmlUi/Core/Factory.h>
#include "ui/w_ui_document.h"

#ifdef WIESEL_DISCORD_RPC
#include "util/w_discord_rpc.h"
#endif

namespace wiesel::editor {

Scene* scene() {
  return Engine::scene_manager().GetActiveScene();
}

// --- RecentProjects ---

std::vector<std::string> RecentProjects::cached_;
bool RecentProjects::cache_valid_ = false;

std::filesystem::path RecentProjects::GetConfigPath() {
  namespace fs = std::filesystem;
#ifdef _WIN32
  const char* appdata = std::getenv("APPDATA");
  if (appdata) {
    return fs::path(appdata) / "Wiesel" / "recent_projects.json";
  }
  return fs::path(".wiesel") / "recent_projects.json";
#else
  const char* home = std::getenv("HOME");
  if (home) {
    return fs::path(home) / ".wiesel" / "recent_projects.json";
  }
  return fs::path(".wiesel") / "recent_projects.json";
#endif
}

const std::vector<std::string>& RecentProjects::Load() {
  if (cache_valid_) {
    return cached_;
  }

  cached_.clear();
  cache_valid_ = true;
  auto path = GetConfigPath();
  if (!std::filesystem::exists(path)) {
    return cached_;
  }
  std::ifstream file(path);
  if (!file.is_open()) {
    return cached_;
  }
  try {
    nlohmann::json j;
    file >> j;
    if (j.is_array()) {
      for (const auto& item : j) {
        if (item.is_string()) {
          cached_.push_back(item.get<std::string>());
        }
      }
    }
  } catch (const std::exception& e) {
    LOG_ERROR("Failed to load recent projects: {}", e.what());
  }
  return cached_;
}

void RecentProjects::Save(const std::vector<std::string>& paths) {
  auto config_path = GetConfigPath();
  std::filesystem::create_directories(config_path.parent_path());
  std::ofstream file(config_path);
  if (!file.is_open()) {
    return;
  }
  nlohmann::json j = paths;
  file << j.dump(2);
  cache_valid_ = false;
}

void RecentProjects::Add(const std::string& path) {
  std::vector<std::string> recent = Load();  // copy from cache
  // Remove if already present
  std::erase(recent, path);
  // Add to front
  recent.insert(recent.begin(), path);
  // Trim to max
  if (recent.size() > kMaxRecent) {
    recent.resize(kMaxRecent);
  }
  Save(recent);
}

struct ResolutionPreset {
  const char* label;
  glm::vec2 size;  // {0,0} = Free Aspect
};

static const ResolutionPreset kResolutionPresets[] = {
    {"2560x1440", {2560, 1440}}, {"1920x1080", {1920, 1080}},
    {"1600x900", {1600, 900}},   {"1280x720", {1280, 720}},
    {"854x480", {854, 480}},     {"Free Aspect", {0, 0}},
};

// Editor layout constants
static constexpr float kLeftPanelRatio = 0.20f;
static constexpr float kAssetBrowserRatio = 0.25f;
static constexpr float kRightPanelRatio = 0.20f;

static ThumbnailCache thumbnail_cache_instance_;

static void CleanupThumbnailCache() {
  thumbnail_cache_instance_.Clear();
}

EditorLayer::EditorLayer(Application& app) : Layer("Demo Overlay"), app_(app) {}

EditorLayer::~EditorLayer() = default;

void EditorLayer::OnAttach() {
  LOG_DEBUG("OnAttach");
  ThumbnailCache::Set(&thumbnail_cache_instance_);

  // Load editor preferences and apply saved theme
  editor_config_ = std::make_unique<UserConfig>(
      urkern::GetUserDataDirectory("WieselEditor"), "editor_config.json");
  editor_config_->Load();
  auto saved_theme = static_cast<ImGui::Moonlight::Theme>(
      editor_config_->Get<int>("editor.theme", 0));
  ImGui::Moonlight::ApplyTheme(saved_theme);

  // Set editor window icon from engine logo
  {
    auto logo = Engine::vfs()->Open("engine://textures/logo.png");
    if (logo) {
      int w = 0;
      int h = 0;
      int channels = 0;
      stbi_uc* pixels =
          stbi_load_from_memory(logo.Data(), static_cast<int>(logo.Size()), &w,
                                &h, &channels, STBI_rgb_alpha);
      if (pixels) {
        Engine::window()->SetIcon(pixels, w, h);
        stbi_image_free(pixels);
      }
    }
  }

  // Load monospace font for code editor
  {
    auto font_file = Engine::vfs()->Open("engine://fonts/RobotoMono.ttf");
    if (font_file) {
      ImGuiIO& io = ImGui::GetIO();
      void* font_data = IM_ALLOC(font_file.Size());
      memcpy(font_data, font_file.Data(), font_file.Size());
      code_editor_font_ = io.Fonts->AddFontFromMemoryTTF(
          font_data, static_cast<int>(font_file.Size()),
          ImGui::Moonlight::kDefaultMonospacedFontSize);
    }
  }

  // Register editor icon font (must be after all other fonts are added)
  InitEditorIcons();

  // Editor idle throttling
  app_.SetIdleMaxFPS(15.0f);
  app_.SetIdleTimeout(120.0f);
  // Cap FPS until a project is loaded
  app_.SetMaxFPS(60.0f);

#ifdef WIESEL_DISCORD_RPC
  Engine::discord_rpc().Initialize("1483104533247688866");
  Engine::discord_rpc().SetPresence("Wiesel Editor", "Idle", "wiesel_logo",
                                    "Wiesel Engine");
#endif

  // Initialize editor free camera
  editor_camera_transform_.SetPosition(glm::vec3(0.0f, 5.0f, -10.0f));
  editor_camera_transform_.SetScale(glm::vec3(1.0f));
  editor_yaw_ = 0.0f;
  editor_pitch_ = -15.0f;
  editor_camera_transform_.SetRotation(
      glm::vec3(editor_pitch_, editor_yaw_, 0.0f));

  editor_camera_.viewport_size = {1920, 1080};
  editor_camera_.field_of_view = 60.0f;
  editor_camera_.near_plane = 0.1f;
  editor_camera_.far_plane = 2000.0f;
  Engine::renderer()->SetupCameraComponent(editor_camera_);
  Engine::scene_manager().CreateScene();
  scene()->SetRenderResolution(
      kResolutionPresets[resolution_preset_index_].size);

  // Auto-load project if --project was passed on command line
  {
    const auto& project_path = Engine::properties().project_path;
    if (!project_path.empty()) {
      namespace fs = std::filesystem;
      fs::path pp(project_path);
      // If it's a directory, look for a .wiesel file inside
      if (fs::is_directory(pp)) {
        for (const auto& entry : fs::directory_iterator(pp)) {
          if (entry.path().extension() == ".wiesel") {
            pp = entry.path();
            break;
          }
        }
      }
      if (fs::exists(pp) && pp.extension() == ".wiesel") {
        LoadProjectFromPath(pp);
      }
    }
  }

  // Wire up asset browser callbacks
  AssetBrowserCallbacks ab_cb;
  ab_cb.on_scan_assets = [this]() {
    ScanProjectAssets();
  };
  ab_cb.on_update_title = [this]() {
    UpdateWindowTitle();
  };
  ab_cb.on_new_scene = [this]() {
    NewScene();
  };
  ab_cb.on_open_scene = [this](const std::string& vfs) {
    deferred_action_ = DeferredAction::OpenScene;
    deferred_path_ = vfs;
  };
  ab_cb.on_open_prefab = [this](const std::string& vfs) {
    deferred_action_ = DeferredAction::OpenPrefab;
    deferred_path_ = vfs;
  };
  ab_cb.on_open_code_editor = [this](const std::filesystem::path& p) {
    OpenCodeEditor(p);
  };
  ab_cb.on_select_asset = [this](AssetHandle h) {
    inspector_asset_handle_ = h;
    inspector_asset_read_only_ = asset_browser_panel_.IsReadOnly();
    inspector_mode_ = InspectorMode::Asset;
    selected_entity_ = EntityRef{};
  };
  ab_cb.on_slice_texture = [this](AssetHandle h) {
    slice_texture_handle_ = h;
    show_slice_sprites_ = true;
  };
  ab_cb.on_show_create_skybox = [this]() {
    show_create_skybox_ = true;
  };
  ab_cb.on_show_create_cursorset = [this]() {
    show_create_cursorset_ = true;
  };
  ab_cb.on_show_create_sprite = [this]() {
    show_create_sprite_ = true;
  };
  ab_cb.on_open_anim_controller = [this](AssetHandle handle) {
    auto data = Engine::asset_manager().Get<AnimControllerAssetData>(handle);
    if (!data) {
      Engine::asset_manager().LoadSync(handle);
      data = Engine::asset_manager().Get<AnimControllerAssetData>(handle);
    }
    if (data) {
      anim_controller_editor_.Open(handle, data);
    }
  };
  ab_cb.on_show_create_meshcollider = [this]() {
    show_create_meshcollider_ = true;
  };
  ab_cb.on_create_anim_controller = [this]() {
    show_create_animcontroller_ = true;
  };
  asset_browser_panel_.SetCallbacks(std::move(ab_cb));

  command_palette_.SetMonoFont(code_editor_font_);
  RegisterPaletteCommands();
}

void EditorLayer::OnDetach() {
  LOG_DEBUG("OnDetach");
  StopLsp();
  CleanupThumbnailCache();
  editor_camera_.resource_pool.Clear();
  editor_camera_.render_pipeline = nullptr;
}

void EditorLayer::ProcessDeferredActions() {
  if (deferred_action_ == DeferredAction::None) {
    return;
  }

  DeferredAction action = deferred_action_;
  std::string path = std::move(deferred_path_);
  deferred_action_ = DeferredAction::None;
  deferred_path_.clear();

  switch (action) {
    case DeferredAction::OpenScene:
      OpenScene(path);
      break;
    case DeferredAction::OpenPrefab:
      OpenPrefabForEditing(path);
      break;
    case DeferredAction::ClosePrefab:
      ClosePrefabEditor();
      break;
    case DeferredAction::OpenProject:
      LoadProjectFromPath(path);
      break;
    case DeferredAction::CloseProject:
      // Save before closing
      if (active_project_) {
        SaveScene();
        SaveProject();
      }
      ClearScene();
      active_project_.reset();
      current_scene_path_.clear();
      scene_dirty_ = false;
      UpdateWindowTitle();
      break;
    case DeferredAction::StopPlaying:
      editor_state_ = EditorState::Edit;
      Engine::window()->SetCursorMode(CursorModeNormal);
      Engine::window()->SetCursorCaptureSource(CursorCaptureSource::None);
      RestoreSnapshot();
      ImGui::SetWindowFocus(ICON_LC_EYE " Scene");
      break;
    default:
      break;
  }
}

void EditorLayer::OnUpdate(float_t delta_time) {
  PROFILE_ZONE_SCOPED_N("Editor::OnUpdate");

  // Process deferred commands (undo/redo/execute) between frames
  command_stack_.Flush();

  // Update window title when dirty state changes
  if (scene_dirty_ != prev_scene_dirty_) {
    prev_scene_dirty_ = scene_dirty_;
    UpdateWindowTitle();
  }

  // Process deferred scene switches (safe point between frames)
  ProcessDeferredActions();

#ifdef WIESEL_DISCORD_RPC
  Engine::discord_rpc().RunCallbacks();
#endif

  // Only let scripts read input when the Game panel is focused during play
  Engine::input().SetEnabled(editor_state_ == EditorState::Playing &&
                             game_panel_focused_);

  // Process pending scene loads (both edit and play mode)
  if (Engine::scene_manager().BeginFrame()) {
    selected_entity_ = EntityRef{};
    piloting_camera_ = entt::null;
  }

  if (editor_state_ == EditorState::Playing) {
    Engine::scene_manager().OnUpdate(delta_time);
  } else if (editor_state_ == EditorState::Paused) {
    Engine::scene_manager().OnUpdate(0.0f);
  } else {
    Engine::scene_manager().OnUpdateEditor(delta_time);

    // Auto-save in edit mode
    if (scene_dirty_ && !current_scene_path_.empty()) {
      auto_save_timer_ += delta_time;
      if (auto_save_timer_ >= kAutoSaveInterval) {
        AutoSave();
      }
    }
  }

  // Script hot reload: efsw detects changes in background, we reload when focused
  if (script_watcher_.Poll()) {
    script_reload_pending_ = true;
  }
  if (script_reload_pending_ && window_focused_ &&
      editor_state_ != EditorState::Playing) {
    script_reload_pending_ = false;
    DCON_LOG_INFO("Script changes detected, reloading...");
    Engine::script_manager().ReloadAsync();
  }

  // Asset directory change: refresh browser cache
  if (asset_dir_watcher_.Poll()) {
    asset_browser_panel_.browser().Invalidate();
  }

  // UI file hot reload: .rml/.rcss changes
  if (ui_file_watcher_.Poll()) {
    bool has_ui = false;
    for (auto& loaded_scene : Engine::scene_manager().GetLoadedScenes()) {
      auto& reg = loaded_scene->GetRegistry();
      for (auto entity :
           loaded_scene->GetAllEntitiesWith<UIDocumentComponent>()) {
        if (reg.any_of<UIDocumentRuntime>(entity)) {
          reg.remove<UIDocumentRuntime>(entity);
          has_ui = true;
        }
      }
    }
    if (has_ui) {
      Rml::Factory::ClearStyleSheetCache();
      DCON_LOG_INFO("UI files changed, reloading documents");
    }
  }
}

bool EditorLayer::OnMouseMoved(MouseMovedEvent& event) {
  if (event.GetCursorMode() == CursorModeRelative) {
    if (editor_camera_mode_ == EditorCameraMode::Mode2D) {
      // 2D mode: right-click drag = pan
      float pan_speed = editor_2d_zoom_ * 0.003f;
      editor_camera_transform_.Move(event.GetDeltaX() * pan_speed, 0, 0);
      editor_camera_transform_.Move(0, event.GetDeltaX() * pan_speed, 0);
      editor_camera_transform_.MarkChanged();
    } else {
      // Free mode: right-click drag = look
      editor_yaw_ += event.GetDeltaX() * mouse_sensitivity_;
      editor_pitch_ += event.GetDeltaY() * mouse_sensitivity_;
      editor_pitch_ = glm::clamp(editor_pitch_, -89.0f, 89.0f);
      editor_camera_transform_.SetRotation(editor_pitch_, editor_yaw_, 0.0f);
    }
  }
  return false;
}

bool EditorLayer::OnWindowFocusGained(WindowFocusGainedEvent& event) {
  window_focused_ = true;
  return false;
}

bool EditorLayer::OnWindowFocusLost(WindowFocusLostEvent& event) {
  window_focused_ = false;
  return false;
}

bool EditorLayer::OnAssetUnloaded(AssetUnloadedEvent& event) {
  thumbnail_cache_instance_.Remove(event.GetHandle());
  return false;
}

void EditorLayer::OnEvent(Event& event) {
  EventDispatcher dispatcher(event);
  dispatcher.Dispatch<MouseMovedEvent>(WIESEL_BIND_FN(OnMouseMoved));
  dispatcher.Dispatch<WindowFocusGainedEvent>(
      WIESEL_BIND_FN(OnWindowFocusGained));
  dispatcher.Dispatch<WindowFocusLostEvent>(WIESEL_BIND_FN(OnWindowFocusLost));
  dispatcher.Dispatch<AssetUnloadedEvent>(WIESEL_BIND_FN(OnAssetUnloaded));

  // Ctrl+Z / Ctrl+Y for undo/redo (edit mode only, not when code editor focused)
  if (editor_state_ == EditorState::Edit && !code_editor_focused_ &&
      event.GetEventType() == KeyPressedEvent::GetStaticType()) {
    auto& key_event = static_cast<KeyPressedEvent&>(event);
    if (key_event.GetKeyCode() == KeyZ && Engine::window()->IsCtrlDown() &&
        !key_event.IsRepeat()) {
      if (Engine::window()->IsShiftDown()) {
        PerformRedo();
      } else {
        PerformUndo();
      }
      event.handled_ = true;
      return;
    }
    if (key_event.GetKeyCode() == KeyY && Engine::window()->IsCtrlDown() &&
        !key_event.IsRepeat()) {
      PerformRedo();
      event.handled_ = true;
      return;
    }
  }

  if (editor_state_ == EditorState::Playing) {
    bool is_input = event.IsInCategory(kEventCategoryInput);
    if (is_input && !game_panel_focused_) {
      return;
    }
    // Don't forward mouse moves to the game when cursor isn't captured
    if (event.GetEventType() == MouseMovedEvent::GetStaticType()) {
      auto& mouse_event = static_cast<MouseMovedEvent&>(event);
      if (mouse_event.GetCursorMode() != CursorModeRelative) {
        return;
      }
    }
    for (auto& loaded_scene : Engine::scene_manager().GetLoadedScenes()) {
      loaded_scene->OnEvent(event);
    }
  } else {
    auto type = event.GetEventType();
    if (type == WindowResizedEvent::GetStaticType() ||
        type == PipelineRecreatedEvent::GetStaticType() ||
        type == ScriptsReloadedEvent::GetStaticType()) {
      for (auto& loaded_scene : Engine::scene_manager().GetLoadedScenes()) {
        loaded_scene->OnEvent(event);
      }
    }
  }
}

void EditorLayer::OnBeginPresent() {
  PROFILE_ZONE_SCOPED_N("Editor::OnBeginPresent");

  // No project loaded: welcome UI owns the whole viewport.
  if (!active_project_) {
    ImGui::DockSpaceOverViewport();
    RenderStartupDialog();
    return;
  }

  RenderMainMenuBar();
  RenderInfoBar();

  // Global shortcuts route through the palette registry. Suppressed while
  // the Game panel is focused (Play-mode input forwarding), while a text
  // field has focus, or while the palette is already open.
  const bool text_input_active =
      code_editor_focused_ || ImGui::GetIO().WantTextInput;
  if (!game_panel_focused_ && !text_input_active &&
      !command_palette_.is_open()) {
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiMod_Shift |
                                 ImGuiKey_K)) {
      command_palette_.Open();
    } else {
      command_palette_.DispatchShortcuts();
    }
  }
  command_palette_.Render();

  ImGuiID dockspace_id = ImGui::DockSpaceOverViewport();
  InitializeDockspaceLayout(dockspace_id);

  RenderProjectSettingsPopup();
  RenderCodeEditor();
  anim_controller_editor_.Render();
  RenderLspDebugPanel();
  RenderFontDebugPanel();
  RenderEditorSettingsPanel();
  RenderCreateSkyboxPopup();
  RenderCreateSpritePopup();
  RenderSliceSpritesPopup();
  RenderCreateAnimControllerPopup();
  RenderCreateCursorSetPopup();
  RenderCreateMeshColliderPopup();
  file_picker_.Render();
  notifications_.RenderToasts();
  notifications_.RenderHistoryPanel();

  RenderSceneHierarchyPanel();
  RenderInspectorPanel();
  RenderAssetBrowserPanel();
  RenderDeveloperConsolePanel();
  RenderRenderStatsPanel();
  RenderNetworkPanel();
  RenderUndoHistoryPanel();
  RenderSceneViewportPanel();
  RenderGameViewportPanel();

  // Status toast overlay (bottom-center, fades out)
  if (status_toast_timer_ > 0.0f) {
    status_toast_timer_ -= app_.GetDeltaTime();
    float alpha = std::min(status_toast_timer_ / 0.3f, 1.0f);
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoMove;
    ImVec2 viewport_size = ImGui::GetMainViewport()->Size;
    ImGui::SetNextWindowPos(
        ImVec2(viewport_size.x * 0.5f, viewport_size.y - 40.0f),
        ImGuiCond_Always, ImVec2(0.5f, 1.0f));
    ImGui::SetNextWindowBgAlpha(0.6f * alpha);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 6));
    if (ImGui::Begin("##StatusToast", nullptr, flags)) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, alpha));
      ImGui::TextUnformatted(status_toast_text_.c_str());
      ImGui::PopStyleColor();
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
  }
}

void EditorLayer::InitializeDockspaceLayout(ImGuiID dockspace_id) {
  if (!layout_initialized_) {
    layout_initialized_ = true;

    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

    ImGuiID dock_bottom, dock_top;
    ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Down, kAssetBrowserRatio,
                                &dock_bottom, &dock_top);

    ImGuiID dock_left, dock_center_right;
    ImGui::DockBuilderSplitNode(dock_top, ImGuiDir_Left, kLeftPanelRatio,
                                &dock_left, &dock_center_right);

    ImGuiID dock_right, dock_center;
    ImGui::DockBuilderSplitNode(dock_center_right, ImGuiDir_Right,
                                kRightPanelRatio, &dock_right, &dock_center);

    ImGui::DockBuilderDockWindow(ICON_LC_LAYERS " Scene Hierarchy",
                                 dock_left);
    ImGui::DockBuilderDockWindow(ICON_LC_CAMERA " Game", dock_center);
    ImGui::DockBuilderDockWindow(ICON_LC_EYE " Scene", dock_center);
    ImGuiID scene_window_id = ImHashStr(ICON_LC_EYE " Scene");
    ImGui::DockBuilderGetNode(dock_center)->SelectedTabId = scene_window_id;
    ImGui::DockBuilderDockWindow(ICON_LC_SQUARE_MOUSE_POINTER " Inspector", dock_right);
    ImGui::DockBuilderDockWindow(ICON_LC_FOLDER_OPEN " Asset Browser",
                                 dock_bottom);
    ImGui::DockBuilderDockWindow(ICON_LC_TERMINAL " Developer Console",
                                 dock_bottom);
    ImGui::DockBuilderDockWindow(ICON_LC_GAUGE " Render Stats",
                                 dock_bottom);

    ImGui::DockBuilderFinish(dockspace_id);
  }
}

void EditorLayer::OnPostPresent() {
  // Execute pending entity pick readback (GPU is idle after EndPresent fence)
  Renderer* renderer = Engine::renderer().get();
  entt::entity picked;
  uint8_t scene_index;
  if (renderer->ExecuteEntityPick(picked, scene_index)) {
    EntityRef ref;
    if (picked != entt::null) {
      auto& loaded = Engine::scene_manager().GetLoadedScenes();
      if (scene_index < loaded.size() && loaded[scene_index]->IsValid(picked)) {
        ref = {picked, loaded[scene_index]->GetHandle()};
      }
    } else if (pending_pick_ndc_.x >= -1.0f) {
      ref = FindSpriteAtNDC(pending_pick_ndc_);
    }

    if (ref) {
      selected_entity_ = ref;
      scroll_to_selected_ = true;
    } else {
      selected_entity_ = EntityRef{};
    }
    pending_pick_ndc_ = {-2, -2};  // reset
  }

  Engine::scene_manager().EndFrame();
}

void EditorLayer::PerformUndo() {
  if (command_stack_.CanUndo()) {
    IEditorCommand* cmd = command_stack_.GetCurrent();
    std::string desc = cmd ? cmd->GetDescription() : "action";
    command_stack_.Undo();
    ShowStatusToast("Undo: " + desc);
    scene_dirty_ = true;
  }
}

void EditorLayer::PerformRedo() {
  if (command_stack_.CanRedo()) {
    command_stack_.Redo();
    IEditorCommand* cmd = command_stack_.GetCurrent();
    std::string desc = cmd ? cmd->GetDescription() : "action";
    ShowStatusToast("Redo: " + desc);
    scene_dirty_ = true;
  }
}

void EditorLayer::ShowStatusToast(const std::string& text) {
  status_toast_text_ = text;
  status_toast_timer_ = kStatusToastDuration;
}

void EditorLayer::TakeSnapshot() {
  play_mode_snapshot_ = scene_serializer::SerializeToString(*scene());
}

void EditorLayer::RestoreSnapshot() {
  selected_entity_ = EntityRef{};

  // Unload all additive scenes that were loaded during play mode
  Engine::scene_manager().UnloadAllAdditiveScenes();
  Engine::scene_manager().ClearPending();

  ClearScene();

  Scene* scene = editor::scene();
  scene_serializer::DeserializeFromString(*scene, play_mode_snapshot_);
  play_mode_snapshot_.clear();

  // Setup camera components that were deserialized
  for (auto entity : scene->GetAllEntitiesWith<CameraComponent>()) {
    auto& cam = scene->GetComponent<CameraComponent>(entity);
    Engine::renderer()->SetupCameraComponent(cam);
  }

  scene->ResetPhysicsWorld();
  scene->ResetScriptStates();
  scene->ResetFirstUpdate();
}

void EditorLayer::SyncEditorSelectedComponent() {
  // Remove from all entities first
  for (auto& loaded_scene : Engine::scene_manager().GetLoadedScenes()) {
    auto view = loaded_scene->GetAllEntitiesWith<EditorSelectedComponent>();
    for (entt::entity entity : view) {
      loaded_scene->RemoveComponent<EditorSelectedComponent>(entity);
    }
  }
  // Add to selected entity
  if (selected_entity_) {
    if (Entity entity = selected_entity_.Resolve()) {
      entity.RemoveComponent<EditorSelectedComponent>();
      entity.AddComponent<EditorSelectedComponent>();
    }
  }
}

void EditorLayer::OnPrePresent() {
  PROFILE_ZONE_SCOPED_N("Editor::OnPrePresent");

  Renderer* renderer = Engine::renderer().get();
  VkCommandBuffer cmd = renderer->GetCommandBuffer().handle_;

  // Sync EditorSelectedComponent marker with current selection
  SyncEditorSelectedComponent();

  if (editor_state_ == EditorState::Playing) {
    // PLAY MODE: Only render viewports that are actually visible.
    // This avoids double rendering when Scene and Game are tabbed.

    if (scene_panel_visible_) {
      auto editor_output =
          editor_camera_.resource_pool.GetTexture("PipelineOutput");
      if (editor_output) {
        renderer->TransitionImageLayout(
            cmd, editor_output->images_[0], editor_output->format_,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1, 0, 1);
      }

      PROFILE_PLOT("Scene Width",
                   static_cast<double>(editor_camera_.viewport_size.x));
      PROFILE_PLOT("Scene Height",
                   static_cast<double>(editor_camera_.viewport_size.y));
      Engine::scene_manager().RenderEditorView(
          editor_camera_, editor_camera_transform_, show_grid_);
      PROFILE_FRAME_MARK_NAMED("Scene");

      // Transition editor PipelineOutput to SHADER_READ for ImGui sampling
      editor_output = editor_camera_.resource_pool.GetTexture("PipelineOutput");
      if (editor_output) {
        renderer->TransitionImageLayout(
            cmd, editor_output->images_[0], editor_output->format_,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 0, 1);
      }
    }

    if (game_panel_visible_) {
      PROFILE_FRAME_MARK_NAMED("Game");
      Engine::scene_manager().RenderGameView();
    }
  } else {
    // EDIT MODE: only render viewports that are visible.
    if (scene_panel_visible_) {
      PROFILE_FRAME_MARK_NAMED("Scene");
      Engine::scene_manager().RenderEditorView(
          editor_camera_, editor_camera_transform_, show_grid_);
    }

    if (game_panel_visible_) {
      PROFILE_FRAME_MARK_NAMED("Game");
      Engine::scene_manager().RenderGameView();
    }
  }
}

void EditorLayer::RegisterPaletteCommands() {
  // File
  command_palette_.Register({
      .id = "file.save_scene",
      .label = "Save Scene",
      .category = "File",
      .icon = ICON_LC_SAVE,
      .shortcut_text = "Ctrl+S",
      .shortcut = ImGuiMod_Ctrl | ImGuiKey_S,
      .action = [this] {
        if (!current_scene_path_.empty()) {
          SaveScene();
          SaveProject();
        } else {
          SaveSceneAs();
        }
      },
      .enabled = [this] { return editor_state_ == EditorState::Edit; },
  });
  command_palette_.Register({
      .id = "file.save_scene_as",
      .label = "Save Scene As...",
      .category = "File",
      .icon = ICON_LC_SAVE,
      .action = [this] { SaveSceneAs(); },
  });
  command_palette_.Register({
      .id = "file.new_scene",
      .label = "New Scene",
      .category = "File",
      .icon = ICON_LC_FILE_PLUS,
      .action = [this] { NewScene(); },
  });

  // Edit
  command_palette_.Register({
      .id = "edit.undo",
      .label = "Undo",
      .category = "Edit",
      .icon = ICON_LC_UNDO_2,
      .shortcut_text = "Ctrl+Z",
      .shortcut = ImGuiMod_Ctrl | ImGuiKey_Z,
      .action = [this] { PerformUndo(); },
  });
  command_palette_.Register({
      .id = "edit.redo",
      .label = "Redo",
      .category = "Edit",
      .icon = ICON_LC_REDO_2,
      .shortcut_text = "Ctrl+Y",
      .shortcut = ImGuiMod_Ctrl | ImGuiKey_Y,
      .action = [this] { PerformRedo(); },
  });

  // Entity — require a selected entity to enable.
  auto has_selection = [this] { return static_cast<bool>(selected_entity_); };
  command_palette_.Register({
      .id = "entity.copy",
      .label = "Copy Entity",
      .category = "Entity",
      .icon = ICON_LC_COPY,
      .shortcut_text = "Ctrl+C",
      .shortcut = ImGuiMod_Ctrl | ImGuiKey_C,
      .action =
          [this] {
            if (!selected_entity_) {
              return;
            }
            Entity entity = selected_entity_.Resolve();
            nlohmann::json j = Prefab::SerializeEntityTree(entity);
            entity_clipboard_ = j.dump();
          },
      .enabled = has_selection,
  });
  command_palette_.Register({
      .id = "entity.paste",
      .label = "Paste Entity",
      .category = "Entity",
      .icon = ICON_LC_CLIPBOARD_PASTE,
      .shortcut_text = "Ctrl+V",
      .shortcut = ImGuiMod_Ctrl | ImGuiKey_V,
      .action =
          [this] {
            if (entity_clipboard_.empty()) {
              return;
            }
            try {
              nlohmann::json j = nlohmann::json::parse(entity_clipboard_);
              Scene* paste_scene = editor::scene();
              Entity new_entity =
                  Prefab::DeserializeEntityTree(*paste_scene, j);
              if (new_entity) {
                command_stack_.Execute(
                    std::make_unique<EntityCreateCommand>(new_entity.ToRef()));
                selected_entity_ = new_entity.ToRef();
                scroll_to_selected_ = true;
                scene_dirty_ = true;
              }
            } catch (const std::exception& e) {
              LOG_ERROR("Failed to paste entity: {}", e.what());
            }
          },
      .enabled = [this] { return !entity_clipboard_.empty(); },
  });
  command_palette_.Register({
      .id = "entity.duplicate",
      .label = "Duplicate Entity",
      .category = "Entity",
      .icon = ICON_LC_COPY,
      .shortcut_text = "Ctrl+D",
      .shortcut = ImGuiMod_Ctrl | ImGuiKey_D,
      .action =
          [this] {
            if (!selected_entity_) {
              return;
            }
            Entity entity = selected_entity_.Resolve();
            Scene* dup_scene = entity.GetScene();
            nlohmann::json j = Prefab::SerializeEntityTree(entity);
            Entity new_entity =
                Prefab::DeserializeEntityTree(*dup_scene, j);
            if (!new_entity) {
              return;
            }
            Entity parent = entity.GetParent();
            if (parent) {
              dup_scene->LinkEntities(parent, new_entity);
            }
            command_stack_.Execute(
                std::make_unique<EntityCreateCommand>(new_entity.ToRef()));
            selected_entity_ = new_entity.ToRef();
            scroll_to_selected_ = true;
            scene_dirty_ = true;
          },
      .enabled = has_selection,
  });
  command_palette_.Register({
      .id = "entity.delete",
      .label = "Delete Entity",
      .category = "Entity",
      .icon = ICON_LC_TRASH_2,
      .shortcut_text = "Delete",
      .shortcut = ImGuiKey_Delete,
      .action =
          [this] {
            if (!selected_entity_) {
              return;
            }
            command_stack_.Execute(std::make_unique<EntityDeleteCommand>(
                selected_entity_.Resolve()));
            selected_entity_ = kInvalidEntityRef;
            scene_dirty_ = true;
          },
      .enabled = has_selection,
  });

  // View
  command_palette_.Register({
      .id = "view.command_palette",
      .label = "Show Command Palette",
      .category = "View",
      .icon = ICON_LC_TERMINAL,
      .shortcut_text = "Ctrl+Shift+K",
      // No shortcut here — the global binding is handled separately so the
      // palette can open itself without dispatching a closing action.
      .action = [this] { command_palette_.Open(); },
  });

  // Entity search: enumerate every loaded scene's hierarchy and yield
  // matches as transient Commands. Action selects the entity like a
  // viewport click does.
  command_palette_.AddResultProvider(
      [this](const std::string& filter_lower,
             std::vector<Command>& out) {
        auto lower = [](std::string s) {
          for (auto& c : s) {
            c = static_cast<char>(
                std::tolower(static_cast<unsigned char>(c)));
          }
          return s;
        };
        for (const auto& loaded : Engine::scene_manager().GetLoadedScenes()) {
          Scene* s = loaded.get();
          for (auto eid : s->GetAllEntitiesWith<TagComponent>()) {
            const auto& tag = s->GetComponent<TagComponent>(eid);
            if (lower(tag.name).find(filter_lower) == std::string::npos) {
              continue;
            }
            Entity entity{eid, s};
            EntityRef ref = entity.ToRef();
            out.push_back(Command{
                .id = "entity:" + std::to_string(static_cast<uint32_t>(eid)),
                .label = tag.name,
                .category = "Entity",
                .icon = std::string(GetEntityIcon(entity)),
                .trailing_label = "Entity",
                .action =
                    [this, ref] {
                      selected_entity_ = ref;
                      scroll_to_selected_ = true;
                    },
            });
          }
        }
      });

  // Asset search: iterate registered assets; action reveals the file in
  // the asset browser panel.
  command_palette_.AddResultProvider(
      [this](const std::string& filter_lower,
             std::vector<Command>& out) {
        auto lower = [](std::string s) {
          for (auto& c : s) {
            c = static_cast<char>(
                std::tolower(static_cast<unsigned char>(c)));
          }
          return s;
        };
        for (AssetHandle h : Engine::asset_manager().GetAll()) {
          const AssetMetadata* meta =
              Engine::asset_manager().GetMetadata(h);
          if (!meta) {
            continue;
          }
          if (lower(meta->name).find(filter_lower) == std::string::npos) {
            continue;
          }
          std::string path = meta->virtual_source_path;
          // Use the filename (with extension) as the label so the user
          // sees the extension clearly.
          std::string filename =
              std::filesystem::path(path).filename().string();
          if (filename.empty()) {
            filename = meta->name;
          }
          out.push_back(Command{
              .id = "asset:" + path,
              .label = filename,
              .category = "Asset",
              .icon = ICON_LC_FILE,
              .trailing_label = AssetTypeToString(meta->type),
              .action =
                  [this, path] {
                    asset_browser_panel_.RevealAsset(path);
                  },
          });
        }
      });
}

}  // namespace wiesel::editor
