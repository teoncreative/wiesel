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
#include <wpak/wpak.h>
#include <unordered_set>
#include "asset/w_asset_serializer.h"
#include "mono_compiler.h"
#include "rendering/w_skybox.h"
#include "rendering/w_sprite_asset.h"
#include "util/w_discord_rpc.h"
#include "w_csharp_lang.h"
#include "w_rml_lang.h"

// clang-format off
// Import order important
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>
#include <ImGuizmo.h>
#include <misc/cpp/imgui_stdlib.h>
// clang-format off

#include "util/w_tracy.h"

#include "asset/w_asset_manager.h"
#include "asset/w_asset_property_registry.h"
#include "imgui_internal.h"
#include "input/w_input.h"
#include "layer/w_layerscene.h"
#include "physics/w_collider.h"
#include "physics/w_physics_world.h"
#include "w_project_loader.h"
#include "asset/w_asset_utils.h"
#include "rendering/w_material.h"
#include "rendering/w_sprite.h"
#include "rendering/w_sprite_asset.h"
#include "w_thumbnail_cache.h"
#include "rendering/w_texture.h"
#include "scene/w_component_serializer.h"
#include "scene/w_lights.h"
#include "w_editor_components.h"
#include "scene/w_prefab.h"
#include "scene/w_scene_manager.h"
#include "scene/w_scene_serializer.h"
#include "script/w_scriptmanager.h"
#include "ui/w_font.h"
#include "util/imgui/w_imguiutil.h"
#include "util/w_dialogs.h"
#include "util/w_filewatcher.h"
#include "util/w_gamepadcodes.h"
#include "util/w_natural_sort.h"
#include "util/w_platform.h"
#include <RmlUi/Core.h>
#include <RmlUi/Core/Factory.h>
#include "ui/w_ui_document.h"
#include "w_editor_icons.h"
#include "w_engine.h"

namespace Wiesel::Editor {

std::shared_ptr<Scene> scene() {
  return Engine::scene_manager().GetActiveScene();
}

static std::shared_ptr<Project> active_project_;

std::shared_ptr<Project> project() {
  return active_project_;
}

// --- RecentProjects ---

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

std::vector<std::string> RecentProjects::Load() {
  std::vector<std::string> result;
  auto path = GetConfigPath();
  if (!std::filesystem::exists(path)) {
    return result;
  }
  std::ifstream file(path);
  if (!file.is_open()) {
    return result;
  }
  try {
    nlohmann::json j;
    file >> j;
    if (j.is_array()) {
      for (const auto& item : j) {
        if (item.is_string()) {
          result.push_back(item.get<std::string>());
        }
      }
    }
  } catch (const std::exception& e) {
    LOG_ERROR("Failed to load recent projects: {}", e.what());
  }
  return result;
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
}

void RecentProjects::Add(const std::string& path) {
  auto recent = Load();
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

// Todo move these to the editor overlay instead
static entt::entity selected_entity_;
static bool has_selected_entity_ = false;
static bool scroll_to_selected_ = false;
static char hierarchy_search_[256] = {};
static entt::entity renaming_entity_ = entt::null;
static char rename_entity_buf_[256] = {};
static std::unordered_set<entt::entity> open_ancestors_;
static std::string entity_clipboard_;
static ImGuizmo::OPERATION current_op_ = ImGuizmo::TRANSLATE;
static ImGuizmo::MODE current_mode_ = ImGuizmo::LOCAL;

// Panel visibility (toggled via Window menu)
static bool panel_scene_hierarchy_ = true;
static bool panel_components_ = true;
static bool panel_asset_browser_ = true;
static bool panel_console_ = true;
static bool panel_stats_ = true;
static bool panel_scene_view_ = true;
static bool panel_game_view_ = true;
static bool panel_lsp_debug_ = false;
static bool layout_initialized_ = false;

static struct SceneHierarchyData {
  entt::entity move_from = entt::null;
  entt::entity move_to = entt::null;
  bool bottom_part = false;
} hierarchy_data_;

static FileWatcher script_watcher_;
static FileWatcher ui_file_watcher_;
static bool script_reload_pending_ = false;
static bool window_focused_ = true;

struct ResolutionPreset {
  const char* label;
  glm::vec2 size;  // {0,0} = Free Aspect
};

static const ResolutionPreset kResolutionPresets[] = {
    {"2560x1440", {2560, 1440}}, {"1920x1080", {1920, 1080}},
    {"1600x900", {1600, 900}},   {"1280x720", {1280, 720}},
    {"854x480", {854, 480}},     {"Free Aspect", {0, 0}},
};
static constexpr int kResolutionPresetCount =
    sizeof(kResolutionPresets) / sizeof(kResolutionPresets[0]);

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
        LOG_ERROR("Failed to read scene: {}", e.what());
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


// Helper: renders the contents of an "Add" entity menu.
// If parent != entt::null, the entity is linked as a child.
// Returns true if an entity was created.
static bool RenderAddEntityMenu(Scene* scene, bool& dirty,
                                entt::entity parent = entt::null,
                                const glm::vec3* spawn_pos = nullptr) {
  Entity created{entt::null, nullptr};

  if (ImGui::MenuItem("Empty Entity")) {
    created = scene->CreateEntity();
  }

  if (ImGui::BeginMenu("3D Shape")) {
    const char* shapes[] = {"Cube", "Sphere", "Plane", "Cylinder", "Capsule"};
    for (const char* shape : shapes) {
      if (ImGui::MenuItem(shape)) {
        created = scene->CreateEntity(shape);
        auto& mc = created.AddComponent<ModelComponent>();
        mc.model_handle = Engine::GetPrimitive(shape);
      }
    }
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("Light")) {
    if (ImGui::MenuItem("Directional Light")) {
      created = scene->CreateEntity("Directional Light");
      created.AddComponent<LightDirectComponent>();
    }
    if (ImGui::MenuItem("Point Light")) {
      created = scene->CreateEntity("Point Light");
      created.AddComponent<LightPointComponent>();
    }
    ImGui::EndMenu();
  }

  if (ImGui::MenuItem("Camera")) {
    created = scene->CreateEntity("Camera");
    created.AddComponent<CameraComponent>();
  }

  if (created.handle() != entt::null) {
    if (parent != entt::null) {
      scene->LinkEntities(parent, created);
    }
    if (spawn_pos) {
      auto& tc = created.GetComponent<TransformComponent>();
      tc.SetPosition(*spawn_pos);
    }
    dirty = true;
    return true;
  }

  return false;
}

// Editor layout constants
static constexpr float kLeftPanelRatio = 0.20f;
static constexpr float kHierarchySplitRatio = 0.45f;
static constexpr float kAssetBrowserRatio = 0.25f;
static constexpr float kRightPanelRatio = 0.20f;
static constexpr float kResolutionComboWidth = 130.0f;
static constexpr float kSettingsButtonWidth = 24.0f;

static ThumbnailCache thumbnail_cache_instance_;

static void CleanupThumbnailCache() {
  thumbnail_cache_instance_.Clear();
}

EditorLayer::EditorLayer(Application& app) : Layer("Demo Overlay"), app_(app) {}

EditorLayer::~EditorLayer() = default;

void EditorLayer::OnAttach() {
  LOG_DEBUG("OnAttach");
  ThumbnailCache::Set(&thumbnail_cache_instance_);

  // Set editor window icon from engine logo
  {
    auto logo = Engine::vfs()->Open("engine://textures/logo.png");
    if (logo) {
      int w = 0;
      int h = 0;
      int channels = 0;
      stbi_uc* pixels = stbi_load_from_memory(
          logo.Data(), static_cast<int>(logo.Size()), &w, &h, &channels,
          STBI_rgb_alpha);
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
          font_data, static_cast<int>(font_file.Size()), 36.0f);
      if (code_editor_font_) {
        code_editor_font_->Scale = 0.5f;
      }
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
  editor_camera_.far_plane = 500.0f;
  editor_camera_.field_of_view = 60.0f;
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
  ab_cb.on_scan_assets = [this]() { ScanProjectAssets(); };
  ab_cb.on_update_title = [this]() { UpdateWindowTitle(); };
  ab_cb.on_new_scene = [this]() { NewScene(); };
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
    properties_asset_handle_ = h;
  };
  ab_cb.on_slice_texture = [this](AssetHandle h) {
    slice_texture_handle_ = h;
    show_slice_sprites_ = true;
  };
  ab_cb.on_show_create_skybox = [this]() { show_create_skybox_ = true; };
  ab_cb.on_show_create_sprite = [this]() { show_create_sprite_ = true; };
  ab_cb.on_show_create_spriteanim = [this]() {
    show_create_spriteanim_ = true;
  };
  ab_cb.on_show_create_spritecontroller = [this]() {
    show_create_spritecontroller_ = true;
  };
  asset_browser_panel_.SetCallbacks(std::move(ab_cb));
}

void EditorLayer::OnDetach() {
  LOG_DEBUG("OnDetach");
  StopLsp();
  CleanupThumbnailCache();
  editor_camera_.resource_pool.Clear();
  editor_camera_.render_pipeline = nullptr;
  Engine::scene_manager().Cleanup();
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
    case DeferredAction::StopPlaying:
      editor_state_ = EditorState::Edit;
      Engine::window()->SetCursorMode(CursorModeNormal);
      Engine::window()->SetCursorCaptureSource(CursorCaptureSource::None);
      RestoreSnapshot();
      ImGui::SetWindowFocus(ICON_CAMERA " Scene");
      break;
    default:
      break;
  }
}

void EditorLayer::OnUpdate(float_t delta_time) {
  PROFILE_ZONE_SCOPED_N("Editor::OnUpdate");

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
  InputManager::SetEnabled(editor_state_ == EditorState::Playing &&
                           game_panel_focused_);

  // Process pending scene loads (both edit and play mode)
  if (Engine::scene_manager().BeginFrame()) {
    has_selected_entity_ = false;
    piloting_camera_ = entt::null;
  }

  if (editor_state_ == EditorState::Playing) {
    scene()->OnUpdate(delta_time);
  } else {
    scene()->OnUpdateEditor(delta_time);

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
    LOG_INFO("Script changes detected, reloading...");
    Engine::script_manager().ReloadAsync();
  }

  // UI file hot reload: .rml/.rcss changes
  if (ui_file_watcher_.Poll()) {
    bool has_ui = false;
    if (scene()) {
      auto& registry = scene()->GetRegistry();
      for (auto entity : registry.view<UIDocumentComponent>()) {
        auto& doc = registry.get<UIDocumentComponent>(entity);
        if (doc.rml_context_) {
          doc.data_model.Shutdown();
          Rml::RemoveContext(doc.context_name_);
          doc.rml_context_ = nullptr;
          doc.rml_document_ = nullptr;
          doc.offscreen_texture_ = nullptr;
          doc.offscreen_stencil_ = nullptr;
          doc.offscreen_descriptor_ = nullptr;
          doc.offscreen_framebuffer_ = nullptr;
          doc.offscreen_size_ = {0, 0};
          has_ui = true;
        }
      }
    }
    if (has_ui) {
      Rml::Factory::ClearStyleSheetCache();
      LOG_INFO("UI files changed, reloading documents");
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

  if (editor_state_ == EditorState::Playing) {
    bool is_input = event.IsInCategory(EventCategory::kEventCategoryInput);
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
    scene()->OnEvent(event);
  } else {
    auto type = event.GetEventType();
    if (type == WindowResizeEvent::GetStaticType() ||
        type == PipelineRecreatedEvent::GetStaticType() ||
        type == ScriptsReloadedEvent::GetStaticType()) {
      scene()->OnEvent(event);
    }
  }
}

static ThumbnailEntry GetOrCreateThumbnail(AssetHandle handle,
                                           const AssetMetadata& meta) {
  return thumbnail_cache_instance_.GetOrCreate(handle, meta);
}

// Reusable asset picker combo. Shows all assets of the given type with
// thumbnail previews on hover. Returns true if selection changed.
// `selected` is updated to the chosen handle (or null if "(None)" picked).
// `allow_none` adds a "(None)" option at the top.
static bool AssetCombo(const char* label, AssetType type, AssetHandle& selected,
                       bool allow_none = true,
                       const char* none_label = "(None)") {
  bool changed = false;
  std::string display;

  if (selected.IsValid()) {
    const auto* meta = Engine::asset_manager().GetMetadata(selected);
    display = meta ? meta->name : "(Unknown)";
  } else {
    display = allow_none ? none_label : "(Select...)";
  }

  if (ImGui::BeginCombo(label, display.c_str())) {
    if (allow_none) {
      if (ImGui::Selectable(none_label, !selected.IsValid())) {
        selected = {};
        changed = true;
      }
    }

    auto assets = Engine::asset_manager().GetAllOfType(type);
    // Sort by name naturally
    std::sort(assets.begin(), assets.end(),
              [](const AssetHandle& a, const AssetHandle& b) {
                const auto* ma = Engine::asset_manager().GetMetadata(a);
                const auto* mb = Engine::asset_manager().GetMetadata(b);
                if (!ma || !mb) {
                  return false;
                }
                return NaturalLess(ma->name, mb->name);
              });
    for (const auto& handle : assets) {
      const auto* meta = Engine::asset_manager().GetMetadata(handle);
      if (!meta) {
        continue;
      }

      bool is_selected = (handle == selected);
      if (ImGui::Selectable(meta->name.c_str(), is_selected)) {
        selected = handle;
        changed = true;
      }

      // Thumbnail preview on hover
      if (ImGui::IsItemHovered()) {
        ThumbnailEntry thumb = GetOrCreateThumbnail(handle, *meta);
        if (thumb.texture_id) {
          ImGui::BeginTooltip();
          ImGui::Image(reinterpret_cast<ImTextureID>(thumb.texture_id),
                       thumb.FitSize(128), thumb.uv0, thumb.uv1);
          ImGui::EndTooltip();
        }
      }

      if (is_selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  // Also show thumbnail next to the combo for the current selection
  if (selected.IsValid()) {
    const auto* meta = Engine::asset_manager().GetMetadata(selected);
    if (meta) {
      ThumbnailEntry thumb = GetOrCreateThumbnail(selected, *meta);
      if (thumb.texture_id) {
        ImGui::SameLine();
        ImGui::Image(
            reinterpret_cast<ImTextureID>(thumb.texture_id),
            ImVec2(ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight()),
            thumb.uv0, thumb.uv1);
      }
    }
  }

  return changed;
}

// Check if an entity or any of its descendants match the search filter.
static bool EntityMatchesSearch(Scene* scene, entt::entity entity_id,
                                const std::string& filter) {
  if (filter.empty()) {
    return true;
  }
  auto& tag = scene->GetComponent<TagComponent>(entity_id);
  std::string name_lower = tag.name;
  std::ranges::transform(name_lower, name_lower.begin(), ::tolower);
  if (name_lower.find(filter) != std::string::npos) {
    return true;
  }
  // Check children recursively
  if (scene->HasComponent<TreeComponent>(entity_id)) {
    auto& tree = scene->GetComponent<TreeComponent>(entity_id);
    for (auto child : tree.childs) {
      if (EntityMatchesSearch(scene, child, filter)) {
        return true;
      }
    }
  }
  return false;
}

void EditorLayer::RenderEntity(Entity& entity, entt::entity entity_id,
                               int depth, bool& ignore_menu) {
  auto& tag_component = entity.GetComponent<TagComponent>();

  // Filter by search
  std::string filter(hierarchy_search_);
  std::ranges::transform(filter, filter.begin(), ::tolower);
  if (!filter.empty() &&
      !EntityMatchesSearch(scene().get(), entity_id, filter)) {
    return;
  }

  bool has_children =
      entity.child_handles() && !entity.child_handles()->empty();
  bool is_selected = has_selected_entity_ && selected_entity_ == entity_id;

  bool is_renaming = renaming_entity_ == entity_id;

  // If renaming, show input field instead of tree node
  if (is_renaming) {
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##rename_entity", rename_entity_buf_,
                         sizeof(rename_entity_buf_),
                         ImGuiInputTextFlags_EnterReturnsTrue |
                         ImGuiInputTextFlags_AutoSelectAll)) {
      if (rename_entity_buf_[0] != '\0') {
        tag_component.name = rename_entity_buf_;
        scene_dirty_ = true;
      }
      renaming_entity_ = entt::null;
    }
    // Cancel on escape
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
      renaming_entity_ = entt::null;
    }
    // Lose focus = confirm
    if (!ImGui::IsItemActive() && !ImGui::IsItemDeactivated()) {
      // Not yet focused - set focus
      ImGui::SetKeyboardFocusHere(-1);
    } else if (ImGui::IsItemDeactivated() && renaming_entity_ != entt::null) {
      if (rename_entity_buf_[0] != '\0') {
        tag_component.name = rename_entity_buf_;
        scene_dirty_ = true;
      }
      renaming_entity_ = entt::null;
    }
    return;
  }

  // Build unique label
  std::string label = tag_component.name + "##" +
                      std::to_string(static_cast<uint32_t>(entity_id));

  ImGuiTreeNodeFlags flags =
      ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth |
      ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_Framed;
  if (is_selected) {
    flags |= ImGuiTreeNodeFlags_Selected;
  }
  if (!has_children) {
    flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
  }
  // Auto-open nodes when searching or scrolling to a descendant
  if (!filter.empty() || open_ancestors_.contains(entity_id)) {
    ImGui::SetNextItemOpen(true);
  }

  bool node_open = ImGui::TreeNodeEx(label.c_str(), flags);

  // Scroll to selected entity when requested (e.g., after viewport click)
  if (is_selected && scroll_to_selected_) {
    ImGui::ScrollToItem();
    scroll_to_selected_ = false;
    open_ancestors_.clear();
  }

  // Selection on release (not during drag)
  if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(0) &&
      !ImGui::IsItemToggledOpen() && !ImGui::IsDragDropActive()) {
    selected_entity_ = entity_id;
    has_selected_entity_ = true;
  }

  // Double-click to navigate editor camera to entity
  if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
    if (entity.HasComponent<TransformComponent>()) {
      auto& tc = entity.GetComponent<TransformComponent>();
      glm::vec3 target = tc.GetWorldPosition();
      float distance = 5.0f;
      glm::vec3 cam_forward = editor_camera_transform_.GetForward();
      editor_camera_transform_.SetPosition(target - cam_forward * distance);
      editor_camera_.view_changed = true;
    }
  }

  // F2 to rename selected entity
  if (is_selected && ImGui::IsKeyPressed(ImGuiKey_F2)) {
    renaming_entity_ = entity_id;
    strncpy(rename_entity_buf_, tag_component.name.c_str(),
            sizeof(rename_entity_buf_) - 1);
    rename_entity_buf_[sizeof(rename_entity_buf_) - 1] = '\0';
  }

  // Drag & drop source
  ImGuiDragDropFlags src_flags = ImGuiDragDropFlags_SourceNoDisableHover;
  if (ImGui::BeginDragDropSource(src_flags)) {
    ImGui::Text("%s", tag_component.name.c_str());
    ImGui::SetDragDropPayload("SceneHierarchy Entity", &entity_id,
                              sizeof(entt::entity));
    ImGui::EndDragDropSource();
  }

  // Drag & drop target: drop ON entity to make it a child
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload* payload =
            ImGui::AcceptDragDropPayload("SceneHierarchy Entity")) {
      hierarchy_data_.move_from = *static_cast<entt::entity*>(payload->Data);
      hierarchy_data_.move_to = entity_id;
      hierarchy_data_.bottom_part = false;
    }
    ImGui::EndDragDropTarget();
  }

  // Context menu
  if (ImGui::BeginPopupContextItem()) {
    selected_entity_ = entity_id;
    has_selected_entity_ = true;
    if (ImGui::BeginMenu("Add Child")) {
      RenderAddEntityMenu(scene().get(), scene_dirty_, entity_id);
      ImGui::EndMenu();
    }
    if (ImGui::MenuItem("Save as Prefab...")) {
      file_picker_.OpenSave(
          "Save as Prefab", ".wprefab",
          [this, entity_id](const std::string& vfs_path) {
            auto physical = Engine::vfs()->ResolvePhysicalPath(vfs_path);
            if (!physical) {
              return;
            }
            Entity ent{entity_id, scene().get()};
            Prefab::SaveToFile(ent, *physical);
            ScanProjectAssets();
          });
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Delete")) {
      scene()->RemoveEntity(entity);
      has_selected_entity_ = false;
      scene_dirty_ = true;
    }
    ImGui::EndPopup();
    ignore_menu = true;
  }

  // Recurse into children if the tree node is open
  if (has_children && node_open) {
    for (const auto& child_entity_id : *entity.child_handles()) {
      Entity child = {child_entity_id, scene().get()};
      RenderEntity(child, child_entity_id, depth + 1, ignore_menu);
    }
    ImGui::TreePop();
  }
}

void EditorLayer::OnBeginPresent() {
  PROFILE_ZONE_SCOPED_N("Editor::OnBeginPresent");

  RenderMainMenuBar();

  // Show startup dialog if no project loaded
  if (!project()) {
    ImGui::DockSpaceOverViewport();
    RenderStartupDialog();
    return;
  }

  ImGuiID dockspace_id = ImGui::DockSpaceOverViewport();
  InitializeDockspaceLayout(dockspace_id);

  RenderProjectSettingsPopup();
  RenderAssetPropertiesPanel();
  RenderCodeEditor();
  RenderLspDebugPanel();
  RenderCreateSkyboxPopup();
  RenderCreateSpritePopup();
  RenderSliceSpritesPopup();
  RenderCreateSpriteAnimPopup();
  RenderCreateSpriteControllerPopup();
  file_picker_.Render();

  RenderSceneHierarchyPanel();
  RenderEntityInspectorPanel();
  RenderAssetBrowserPanel();
  RenderDeveloperConsolePanel();
  RenderRenderStatsPanel();
  RenderSceneViewportPanel();
  RenderGameViewportPanel();
}

void EditorLayer::InitializeDockspaceLayout(ImGuiID dockspace_id) {
  if (!layout_initialized_) {
    layout_initialized_ = true;

    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

    ImGuiID dock_bottom, dock_top;
    ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Down,
                                kAssetBrowserRatio, &dock_bottom, &dock_top);

    ImGuiID dock_left, dock_center_right;
    ImGui::DockBuilderSplitNode(dock_top, ImGuiDir_Left, kLeftPanelRatio,
                                &dock_left, &dock_center_right);

    ImGuiID dock_right, dock_center;
    ImGui::DockBuilderSplitNode(dock_center_right, ImGuiDir_Right,
                                kRightPanelRatio, &dock_right, &dock_center);

    ImGui::DockBuilderDockWindow(ICON_HIERARCHY " Scene Hierarchy", dock_left);
    ImGui::DockBuilderDockWindow(ICON_CAMERA " Game", dock_center);
    ImGui::DockBuilderDockWindow(ICON_CAMERA " Scene", dock_center);
    ImGuiID scene_window_id = ImHashStr(ICON_CAMERA " Scene");
    ImGui::DockBuilderGetNode(dock_center)->SelectedTabId = scene_window_id;
    ImGui::DockBuilderDockWindow("Entity Inspector", dock_right);
    ImGui::DockBuilderDockWindow("Asset Properties", dock_right);
    ImGui::DockBuilderDockWindow(ICON_BROWSER " Asset Browser", dock_bottom);
    ImGui::DockBuilderDockWindow(ICON_CONSOLE " Developer Console", dock_bottom);
    ImGui::DockBuilderDockWindow("Render Stats", dock_bottom);

    ImGui::DockBuilderFinish(dockspace_id);
  }
}

void EditorLayer::RenderSceneHierarchyPanel() {
  bool& scene_open = panel_scene_hierarchy_;
  if (scene_open) {
    if (ImGui::Begin(ICON_HIERARCHY " Scene Hierarchy", &scene_open)) {
      bool ignoreMenu = false;

      if (editing_prefab_) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              ImVec4(0.3f, 0.6f, 0.9f, 1.0f));
        float width = ImGui::GetContentRegionAvail().x;
        ImGui::Text("Editing: %s", editing_prefab_path_.c_str());
        if (ImGui::Button("Save Prefab", ImVec2(width * 0.48f, 0))) {
          SavePrefab();
        }
        ImGui::SameLine();
        ImGui::PopStyleColor(2);
        if (ImGui::Button("Close", ImVec2(width * 0.48f, 0))) {
          deferred_action_ = DeferredAction::ClosePrefab;
        }
        ImGui::Separator();
      }

      if (scroll_to_selected_ && has_selected_entity_) {
        open_ancestors_.clear();
        entt::entity walk = selected_entity_;
        while (walk != entt::null) {
          if (scene()->HasComponent<TreeComponent>(walk)) {
            entt::entity parent =
                scene()->GetComponent<TreeComponent>(walk).parent;
            if (parent != entt::null) {
              open_ancestors_.insert(parent);
            }
            walk = parent;
          } else {
            break;
          }
        }
      }

      ImGui::SetNextItemWidth(-1);
      ImGui::InputTextWithHint("##HierarchySearch", "Search entities...",
                               hierarchy_search_, sizeof(hierarchy_search_));

      ImGuiTreeNodeFlags scene_flags =
          ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow |
          ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding |
          ImGuiTreeNodeFlags_Framed;
      std::string scene_label = "Scene";
      if (!current_scene_path_.empty()) {
        scene_label = VirtualFileSystem::Stem(current_scene_path_);
      }
      bool scene_open = ImGui::TreeNodeEx("##SceneRoot", scene_flags, "%s",
                                          scene_label.c_str());

      if (ImGui::BeginPopupContextItem("scene_root_context")) {
        if (ImGui::BeginMenu("Add")) {
          RenderAddEntityMenu(scene().get(), scene_dirty_);
          ImGui::EndMenu();
        }
        ImGui::EndPopup();
        ignoreMenu = true;
      }

      if (scene_open) {
        for (const auto& entityId : scene()->GetSceneHierarchy()) {
          Entity entity = {entityId, scene().get()};
          if (entity.GetParent()) {
            continue;
          }
          RenderEntity(entity, entityId, 0, ignoreMenu);
        }
        ImGui::TreePop();
      }

      UpdateHierarchyOrder();

      ImVec2 avail = ImGui::GetContentRegionAvail();
      if (avail.y > 0) {
        ImGui::InvisibleButton("##HierarchyDropZone", ImVec2(avail.x, avail.y));
        if (ImGui::BeginDragDropTarget()) {
          if (const ImGuiPayload* payload =
                  ImGui::AcceptDragDropPayload("SceneHierarchy Entity")) {
            entt::entity dropped = *static_cast<entt::entity*>(payload->Data);
            Entity dropped_entity = {dropped, scene().get()};
            if (dropped_entity.parent_handle() != entt::null) {
              scene()->UnlinkEntities(dropped_entity.parent_handle(), dropped);
              scene_dirty_ = true;
            }
          }
          ImGui::EndDragDropTarget();
        }

        if (ImGui::IsItemClicked(0)) {
          has_selected_entity_ = false;
        }

        if (ImGui::IsItemClicked(1)) {
          ImGui::OpenPopup("right_click_hierarchy");
        }
      } else {
        if (!ignoreMenu && ImGui::IsWindowHovered() &&
            ImGui::IsMouseClicked(1, false)) {
          ImGui::OpenPopup("right_click_hierarchy");
        }
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0) &&
            !ImGui::IsAnyItemHovered()) {
          has_selected_entity_ = false;
        }
      }

      if (ImGui::BeginPopup("right_click_hierarchy")) {
        if (ImGui::BeginMenu("Add")) {
          RenderAddEntityMenu(scene().get(), scene_dirty_);
          ImGui::EndMenu();
        }
        ImGui::EndPopup();
      }
    }
    ImGui::End();
  }
}

void EditorLayer::RenderEntityInspectorPanel() {
  bool& components_open = panel_components_;
  if (components_open) {
    if (ImGui::Begin("Entity Inspector", &components_open)) {
      if (!has_selected_entity_) {
        ImGui::TextDisabled("No entity selected");
      } else {
        RenderEntityInspector(selected_entity_);
      }
    }
    ImGui::End();
  }
}

void EditorLayer::RenderAssetBrowserPanel() {
  asset_browser_panel_.current_scene_path = current_scene_path_;
  asset_browser_panel_.Render(panel_asset_browser_);
  current_scene_path_ = asset_browser_panel_.current_scene_path;
}

void EditorLayer::RenderDeveloperConsolePanel() {
  {
    bool& console_open = panel_console_;
    if (console_open) {
      static std::vector<std::string> history;
      static int history_pos = -1;  // -1 = new line, 0..N = browsing history
      static char input_buf[512] = "";

      auto HistoryCallback = [](ImGuiInputTextCallbackData* data) -> int {
        if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
          if (history.empty()) {
            return 0;
          }
          if (data->EventKey == ImGuiKey_UpArrow) {
            if (history_pos == -1) {
              history_pos = static_cast<int>(history.size()) - 1;
            } else if (history_pos > 0) {
              history_pos--;
            }
          } else if (data->EventKey == ImGuiKey_DownArrow) {
            if (history_pos != -1) {
              history_pos++;
              if (history_pos >= static_cast<int>(history.size())) {
                history_pos = -1;
              }
            }
          }
          const char* text =
              (history_pos >= 0) ? history[history_pos].c_str() : "";
          data->DeleteChars(0, data->BufTextLen);
          data->InsertChars(0, text);
        }
        return 0;
      };

      if (ImGui::Begin(ICON_CONSOLE " Developer Console", &console_open)) {
        auto& cmd = Engine::console();
        const auto& log = cmd.GetLog();

        // Toolbar
        if (ImGui::Button("Clear")) {
          cmd.Clear();
        }
        ImGui::SameLine();
        static bool auto_scroll = true;
        ImGui::Checkbox("Auto-scroll", &auto_scroll);

        ImGui::Separator();

        // Log output
        float footer_height = ImGui::GetStyle().ItemSpacing.y +
                              ImGui::GetFrameHeightWithSpacing();
        if (ImGui::BeginChild("ConsoleLog", ImVec2(0, -footer_height),
                              ImGuiChildFlags_None,
                              ImGuiWindowFlags_HorizontalScrollbar)) {
          for (const auto& line : log) {
            ImVec4 color;
            switch (line.level) {
              case ConsoleLogLevel::Warning:
                color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
                break;
              case ConsoleLogLevel::Error:
                color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
                break;
              default:
                color = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
                break;
            }
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextUnformatted(line.text.c_str());
            ImGui::PopStyleColor();
          }
          if (auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
          }
        }
        ImGui::EndChild();

        // Input line with history
        ImGuiInputTextFlags input_flags = ImGuiInputTextFlags_EnterReturnsTrue |
                                          ImGuiInputTextFlags_CallbackHistory;
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##ConsoleInput", input_buf, sizeof(input_buf),
                             input_flags, HistoryCallback)) {
          if (input_buf[0] != '\0') {
            // Don't duplicate consecutive identical commands
            if (history.empty() || history.back() != input_buf) {
              history.push_back(input_buf);
            }
            cmd.Execute(input_buf);
            input_buf[0] = '\0';
          }
          history_pos = -1;
          ImGui::SetKeyboardFocusHere(-1);
        }
      }
      ImGui::End();
    }
  }
}

void EditorLayer::RenderRenderStatsPanel() {
  {
    bool& stats_open = panel_stats_;
    if (stats_open) {
      if (ImGui::Begin("Render Stats", &stats_open)) {
        std::shared_ptr<Renderer> renderer = Engine::renderer();
        const auto& stats = renderer->GetStats();

        ImGui::SeparatorText("Performance");
        ImGui::Text("FPS: %.1f", app_.GetFPS());
        ImGui::Text("Frame Time: %.2f ms", stats.frame_time_ms);
        ImGui::Text("Delta Time: %.4f s", app_.GetDeltaTime());

        ImGui::SeparatorText("Draw Stats");
        ImGui::Text("Draw Calls: %u", stats.draw_calls);
        ImGui::Text("Models: %u", stats.models);
        ImGui::Text("Meshes: %u", stats.meshes);
        ImGui::Text("Vertices: %u", stats.vertices);
        ImGui::Text("Triangles: %u", stats.triangles);

        ImGui::SeparatorText("Renderer");
        ImGui::Text("MSAA: %s",
                    renderer->options().msaa_mode == SamplingMode::DISABLED
                        ? "Off"
                    : renderer->options().msaa_mode == SamplingMode::X2 ? "2x"
                    : renderer->options().msaa_mode == SamplingMode::X4 ? "4x"
                                                                        : "8x");
        ImGui::Text("VSync: %s", renderer->options().vsync ? "On" : "Off");
        ImGui::Text(
            "AA Mode: %s",
            renderer->options().aa_mode == AntiAliasingMode::None   ? "None"
            : renderer->options().aa_mode == AntiAliasingMode::FXAA ? "FXAA"
                                                                    : "TAA");
        ImGui::Text("Swap Chain Images: %u", stats.swap_chain_images);
        ImGui::Text("Frames in Flight: %u", stats.frames_in_flight);

        ImGui::SeparatorText("Assets");
        auto asset_stats = Engine::asset_manager().GetStats();
        ImGui::Text("Total: %zu", asset_stats.total);
        ImGui::Text("Loaded: %zu", asset_stats.loaded);
        if (asset_stats.loading > 0) {
          ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Loading: %zu",
                             asset_stats.loading);
        } else {
          ImGui::Text("Loading: %zu", asset_stats.loading);
        }
        ImGui::Text("Unloaded: %zu", asset_stats.unloaded);
        if (asset_stats.failed > 0) {
          ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Failed: %zu",
                             asset_stats.failed);
        }

        {
          ImGui::SeparatorText("Shadow Cascades");
          auto cam = renderer->GetCameraData();
          if (cam) {
            ImGui::Text("Shadows: %s", cam->does_shadow_pass ? "ON" : "OFF");
            for (int i = 0; i < WIESEL_SHADOW_CASCADE_COUNT; i++) {
              ImGui::Text("Cascade %d: split Z = %.2f", i,
                          cam->shadow_map_cascades[i].SplitDepth);
            }
          }
        }

        if (scene()) {
          ImGui::SeparatorText("Render Pipelines");

          // Collect unique pipelines and their cameras
          struct PipelineInfo {
            RenderPipeline* pipeline;
            std::vector<std::string> cameras;
            bool is_default;
          };

          std::map<RenderPipeline*, PipelineInfo> pipeline_map;

          auto default_pipeline = scene()->GetDefaultPipeline();
          if (default_pipeline) {
            pipeline_map[default_pipeline.get()] = {
                default_pipeline.get(), {}, true};
          }

          for (const auto& entity :
               scene()->GetAllEntitiesWith<CameraComponent, TagComponent>()) {
            auto& cam = scene()->GetComponent<CameraComponent>(entity);
            auto& tag = scene()->GetComponent<TagComponent>(entity);
            RenderPipeline* pl = cam.render_pipeline ? cam.render_pipeline.get()
                                                     : default_pipeline.get();
            if (pl) {
              auto& info = pipeline_map[pl];
              info.pipeline = pl;
              info.cameras.push_back(tag.name);
              if (pl == default_pipeline.get()) {
                info.is_default = true;
              }
            }
          }

          for (const auto& [ptr, info] : pipeline_map) {
            std::string label =
                info.is_default ? "Default Pipeline" : "Custom Pipeline";
            if (ImGui::TreeNodeEx(label.c_str(),
                                  ImGuiTreeNodeFlags_DefaultOpen)) {
              // Cameras using this pipeline
              ImGui::TextDisabled("Cameras:");
              if (info.cameras.empty()) {
                ImGui::SameLine();
                ImGui::TextDisabled("(none)");
              }
              for (const auto& cam_name : info.cameras) {
                ImGui::SameLine();
                ImGui::Text("%s", cam_name.c_str());
              }

              // Feature list with timings
              ImGui::TextDisabled("Features:");
              const auto& features = info.pipeline->GetFeatures();

              // Collect pass timings: prefer external render graph (editor camera)
              // in edit mode since ECS camera graphs may have stale data.
              std::vector<PassTimingResult> timings;
              if (editor_state_ == EditorState::Edit) {
                auto ext_graph = scene()->GetExternalRenderGraph();
                if (ext_graph) {
                  timings = ext_graph->GetPassTimings();
                }
              } else {
                for (const auto& entity :
                     scene()->GetAllEntitiesWith<CameraComponent>()) {
                  auto& cam = scene()->GetComponent<CameraComponent>(entity);
                  RenderPipeline* cam_pl = cam.render_pipeline
                                               ? cam.render_pipeline.get()
                                               : default_pipeline.get();
                  if (cam_pl != ptr) {
                    continue;
                  }

                  auto graph = scene()->GetRenderGraph(entity);
                  if (graph) {
                    timings = graph->GetPassTimings();
                  }
                  break;
                }
                // Fallback to external render graph if no ECS camera graph found
                if (timings.empty()) {
                  auto ext_graph = scene()->GetExternalRenderGraph();
                  if (ext_graph) {
                    timings = ext_graph->GetPassTimings();
                  }
                }
              }

              for (const auto& feature : features) {
                const auto& fname = feature->GetName();
                float cpu_total = 0.0f;
                int pass_count = 0;
#ifdef WIESEL_GPU_PROFILING
                float gpu_total = 0.0f;
#endif
                for (const auto& t : timings) {
                  if (t.name.size() >= fname.size() &&
                      t.name.compare(0, fname.size(), fname) == 0) {
                    cpu_total += t.cpu_time_ms;
#ifdef WIESEL_GPU_PROFILING
                    gpu_total += t.gpu_time_ms;
#endif
                    pass_count++;
                  }
                }
                if (pass_count > 0) {
#ifdef WIESEL_GPU_PROFILING
                  ImGui::BulletText("%-20s  CPU %.3f ms  GPU %.3f ms",
                                    fname.c_str(), cpu_total, gpu_total);
#else
                  ImGui::BulletText("%-20s  CPU %.3f ms", fname.c_str(),
                                    cpu_total);
#endif
                } else {
                  ImGui::BulletText("%s", fname.c_str());
                }
              }

              if (!timings.empty()) {
                float total_cpu = 0.0f;
#ifdef WIESEL_GPU_PROFILING
                float total_gpu = 0.0f;
#endif
                for (const auto& t : timings) {
                  total_cpu += t.cpu_time_ms;
#ifdef WIESEL_GPU_PROFILING
                  total_gpu += t.gpu_time_ms;
#endif
                }
#ifdef WIESEL_GPU_PROFILING
                ImGui::Text("  Total: CPU %.3f ms  GPU %.3f ms", total_cpu,
                            total_gpu);
#else
                ImGui::Text("  Total: CPU %.3f ms", total_cpu);
#endif
              }
              ImGui::TreePop();
            }
          }
        }
      }
      ImGui::End();
    }
  }
}

void EditorLayer::RenderSceneViewportPanel() {
  Renderer* renderer = Engine::renderer().get();

  bool& scene_view_open = panel_scene_view_;
  if (scene_view_open) {
    ImGuiWindowFlags sceneFlags =
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar;
    scene_panel_visible_ = ImGui::Begin(ICON_CAMERA " Scene", &scene_view_open, sceneFlags);
    if (scene_panel_visible_) {
      // Play/Stop buttons + gizmo controls
      DrawPlayStopButtons();
      ImGui::SameLine();
      ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
      ImGui::SameLine();
      if (ImGui::RadioButton("Translate", current_op_ == ImGuizmo::TRANSLATE)) {
        current_op_ = ImGuizmo::TRANSLATE;
      }
      ImGui::SameLine();
      if (ImGui::RadioButton("Rotate", current_op_ == ImGuizmo::ROTATE)) {
        current_op_ = ImGuizmo::ROTATE;
      }
      ImGui::SameLine();
      if (ImGui::RadioButton("Scale", current_op_ == ImGuizmo::SCALE)) {
        current_op_ = ImGuizmo::SCALE;
      }
      ImGui::SameLine();
      ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
      ImGui::SameLine();
      if (ImGui::RadioButton("Local", current_mode_ == ImGuizmo::LOCAL)) {
        current_mode_ = ImGuizmo::LOCAL;
      }
      ImGui::SameLine();
      if (ImGui::RadioButton("World", current_mode_ == ImGuizmo::WORLD)) {
        current_mode_ = ImGuizmo::WORLD;
      }
      ImGui::SameLine();
      ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
      ImGui::SameLine();
      if (ImGui::RadioButton("Free",
                             editor_camera_mode_ == EditorCameraMode::Free)) {
        editor_camera_mode_ = EditorCameraMode::Free;
        editor_camera_.projection_mode = ProjectionMode::Perspective;
        editor_camera_.view_changed = true;
        editor_camera_.resources_dirty = true;
        piloting_camera_ = entt::null;
      }
      ImGui::SameLine();
      if (ImGui::RadioButton("2D",
                             editor_camera_mode_ == EditorCameraMode::Mode2D)) {
        editor_camera_mode_ = EditorCameraMode::Mode2D;
        editor_camera_.projection_mode = ProjectionMode::Orthographic;
        editor_camera_.ortho_size = editor_2d_zoom_;
        // Look straight down +Z, position Z behind sprites
        editor_pitch_ = 0.0f;
        editor_yaw_ = 0.0f;
        editor_camera_transform_.SetPosition(glm::vec3(0.0f, 180.0f, -5.0f));
        editor_camera_transform_.SetRotation(glm::vec3(0.0f, 0.0f, 0.0f));
        editor_camera_transform_.MarkChanged();
        editor_camera_.view_changed = true;
        editor_camera_.resources_dirty = true;
        piloting_camera_ = entt::null;
      }
      {
        float rightEdge = ImGui::GetWindowContentRegionMax().x;
        ImGui::SameLine(rightEdge - kSettingsButtonWidth);
        if (ImGui::Button("...##SceneSettings")) {
          ImGui::OpenPopup("SceneCameraSettings");
        }
        if (ImGui::BeginPopup("SceneCameraSettings")) {
          ImGui::SeparatorText("Camera");
          ImGui::DragFloat("Speed", &camera_speed_, 0.5f, 0.1f, 100.0f);
          ImGui::DragFloat("Sensitivity", &mouse_sensitivity_, 1.0f, 10.0f,
                           500.0f);

          if (editor_camera_mode_ == EditorCameraMode::Free) {
            ImGui::DragFloat("FOV", &editor_camera_.field_of_view, 1.0f, 1.0f,
                             179.0f);
          }

          ImGui::SeparatorText("Snap to Camera");
          for (auto entity : scene()
                                 ->GetAllEntitiesWith<CameraComponent,
                                                      TransformComponent>()) {
            auto& tag = scene()->GetComponent<TagComponent>(entity);
            bool is_piloting = (piloting_camera_ == entity);
            if (ImGui::Selectable(tag.name.c_str(), is_piloting)) {
              auto& cam = scene()->GetComponent<CameraComponent>(entity);
              auto& tc = scene()->GetComponent<TransformComponent>(entity);

              // Snap editor camera to this entity
              editor_camera_transform_.SetPosition(tc.GetPosition());
              editor_camera_transform_.SetRotation(tc.GetRotation());
              editor_yaw_ = tc.GetRotation().y;
              editor_pitch_ = tc.GetRotation().x;

              // Match projection
              if (cam.projection_mode == ProjectionMode::Orthographic) {
                editor_camera_mode_ = EditorCameraMode::Mode2D;
                editor_camera_.projection_mode = ProjectionMode::Orthographic;
                editor_camera_.ortho_size = cam.ortho_size;
                editor_2d_zoom_ = cam.ortho_size;
              } else {
                editor_camera_mode_ = EditorCameraMode::Free;
                editor_camera_.projection_mode = ProjectionMode::Perspective;
                editor_camera_.field_of_view = cam.field_of_view;
              }
              editor_camera_.near_plane = cam.near_plane;
              editor_camera_.far_plane = cam.far_plane;
              editor_camera_.background_color = cam.background_color;
              editor_camera_.view_changed = true;
              editor_camera_.resources_dirty = true;

              piloting_camera_ = is_piloting ? entt::null : entity;
            }
          }
          if (piloting_camera_ != entt::null) {
            if (ImGui::Button("Stop Piloting")) {
              piloting_camera_ = entt::null;
            }
          }

          ImGui::SeparatorText("Overlays");
          ImGui::EndPopup();
        }
      }

      // Editor camera output from its own resource pool
      auto editor_desc = editor_camera_.resource_pool.GetDescriptor(
          "PipelineOutputDescriptor");
      auto editor_image =
          editor_camera_.resource_pool.GetTexture("PipelineOutput");

      // Handle viewport resize - editor camera always tracks panel size
      ImVec2 avail = ImGui::GetContentRegionAvail();
      {
        uint32_t newW = static_cast<uint32_t>(avail.x);
        uint32_t newH = static_cast<uint32_t>(avail.y);
        if (newW > 0 && newH > 0 &&
            (newW != editor_camera_.viewport_size.x ||
             newH != editor_camera_.viewport_size.y)) {
          editor_camera_.viewport_size = {newW, newH};
          editor_camera_.aspect_ratio =
              static_cast<float>(newW) / static_cast<float>(newH);
          editor_camera_.view_changed = true;
          editor_camera_.resources_dirty = true;
        }
      }
      if (editor_desc && editor_image) {
        ImTextureID desc =
            reinterpret_cast<ImTextureID>(editor_desc->descriptor_set_);

        float image_aspect =
            (float)editor_image->width_ / (float)editor_image->height_;
        float avail_aspect = avail.x / avail.y;

        ImVec2 image_size;
        if (avail_aspect > image_aspect) {
          image_size.y = avail.y;
          image_size.x = image_size.y * image_aspect;
        } else {
          image_size.x = avail.x;
          image_size.y = image_size.x / image_aspect;
        }

        ImGui::Image(desc, image_size);

        ImVec2 image_min = ImGui::GetItemRectMin();
        ImVec2 image_max = ImGui::GetItemRectMax();
        bool scene_hovered = ImGui::IsItemHovered();

        // FPS overlay (top-left)
        ImVec2 text_pos = ImVec2(image_min.x + 6, image_min.y + 6);
        std::string fps_str =
            std::format("FPS: {}", static_cast<int>(app_.GetFPS()));
        ImGui::GetWindowDrawList()->AddText(text_pos, IM_COL32(0, 255, 0, 255),
                                            fps_str.c_str());

        // Resolution overlay (top-right)
        std::string res_str =
            std::format("{}x{}", (int)editor_camera_.viewport_size.x,
                        (int)editor_camera_.viewport_size.y);
        ImVec2 res_text_size = ImGui::CalcTextSize(res_str.c_str());
        ImVec2 res_pos =
            ImVec2(image_max.x - res_text_size.x - 6, image_min.y + 6);
        ImGui::GetWindowDrawList()->AddText(res_pos, IM_COL32(0, 255, 0, 255),
                                            res_str.c_str());

        // Right-click mouse look
        static bool scene_right_active = false;
        bool scene_focused = ImGui::IsWindowFocused();
        if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
          if (scene_hovered && !scene_right_active) {
            scene_right_active = true;
            Engine::window()->SetCursorCaptureSource(CursorCaptureSource::Editor);
            Engine::window()->SetCursorMode(CursorModeRelative);
          }
        } else {
          if (scene_right_active) {
            scene_right_active = false;
            Engine::window()->SetCursorMode(CursorModeNormal);
            Engine::window()->SetCursorCaptureSource(CursorCaptureSource::None);
          }
        }

        // Mouse look is handled in OnMouseMoved via the event system

        // Camera movement (only when no other widget wants keyboard input)
        if ((scene_focused || scene_right_active) &&
            !ImGui::GetIO().WantTextInput) {
          ImGuiIO& io = ImGui::GetIO();
          float dt = io.DeltaTime;
          float speed = camera_speed_ * dt;
          if (io.KeyShift) {
            speed *= 3.0f;
          }

          if (editor_camera_mode_ == EditorCameraMode::Mode2D) {
            // 2D: WASD = pan on XY plane
            if (ImGui::IsKeyDown(ImGuiKey_W)) {
              editor_camera_transform_.Move(0, speed, 0);
            }
            if (ImGui::IsKeyDown(ImGuiKey_S)) {
              editor_camera_transform_.Move(0, -speed, 0);
            }
            if (ImGui::IsKeyDown(ImGuiKey_D)) {
              editor_camera_transform_.Move(speed, 0, 0);
            }
            if (ImGui::IsKeyDown(ImGuiKey_A)) {
              editor_camera_transform_.Move(-speed, 0, 0);
            }
          } else {
            // Free: WASD + QE = 3D fly camera
            glm::quat q =
                glm::quat(glm::radians(editor_camera_transform_.GetRotation()));
            glm::mat4 R = glm::toMat4(q);
            glm::vec3 cam_right = glm::vec3(R[0]);
            glm::vec3 cam_forward = glm::vec3(R[2]);

            if (ImGui::IsKeyDown(ImGuiKey_W)) {
              editor_camera_transform_.Move(cam_forward * speed);
            }
            if (ImGui::IsKeyDown(ImGuiKey_S)) {
              editor_camera_transform_.Move(-cam_forward * speed);
            }
            if (ImGui::IsKeyDown(ImGuiKey_D)) {
              editor_camera_transform_.Move(cam_right * speed);
            }
            if (ImGui::IsKeyDown(ImGuiKey_A)) {
              editor_camera_transform_.Move(-cam_right * speed);
            }
            if (ImGui::IsKeyDown(ImGuiKey_E)) {
              editor_camera_transform_.Move(0, speed, 0);
            }
            if (ImGui::IsKeyDown(ImGuiKey_Q)) {
              editor_camera_transform_.Move(0, -speed, 0);
            }
          }
        }

        // Scroll to zoom
        if (scene_hovered) {
          float scroll = ImGui::GetIO().MouseWheel;
          if (std::abs(scroll) > 0.01f) {
            if (editor_camera_mode_ == EditorCameraMode::Mode2D) {
              // 2D: scroll = zoom (adjust ortho size)
              editor_2d_zoom_ -= scroll * editor_2d_zoom_ * 0.1f;
              editor_2d_zoom_ = glm::clamp(editor_2d_zoom_, 0.1f, 1000.0f);
              editor_camera_.ortho_size = editor_2d_zoom_;
              editor_camera_.view_changed = true;
            } else {
              // Free: scroll = dolly forward/back
              glm::quat q = glm::quat(
                  glm::radians(editor_camera_transform_.GetRotation()));
              glm::mat4 R = glm::toMat4(q);
              glm::vec3 cam_forward = glm::vec3(R[2]);
              editor_camera_transform_.Move(cam_forward * scroll *
                                            camera_speed_ * 0.3f);
            }
          }
        }

        // Follow piloted camera (read-only — editor follows entity, doesn't modify it)
        if (piloting_camera_ != entt::null &&
            scene()->HasEntity(piloting_camera_)) {
          auto& tc =
              scene()->GetComponent<TransformComponent>(piloting_camera_);
          editor_camera_transform_.SetPosition(tc.GetPosition());
          editor_camera_transform_.SetRotation(tc.GetRotation());
          editor_yaw_ = tc.GetRotation().y;
          editor_pitch_ = tc.GetRotation().x;

          auto& cam = scene()->GetComponent<CameraComponent>(piloting_camera_);
          editor_camera_.background_color = cam.background_color;
          editor_camera_.near_plane = cam.near_plane;
          editor_camera_.far_plane = cam.far_plane;

          if (cam.projection_mode != editor_camera_.projection_mode) {
            editor_camera_.projection_mode = cam.projection_mode;
            if (cam.projection_mode == ProjectionMode::Orthographic) {
              editor_camera_mode_ = EditorCameraMode::Mode2D;
            } else {
              editor_camera_mode_ = EditorCameraMode::Free;
            }
            editor_camera_.resources_dirty = true;
          }

          if (cam.projection_mode == ProjectionMode::Orthographic) {
            editor_camera_.ortho_size = cam.ortho_size;
            editor_2d_zoom_ = cam.ortho_size;
          } else {
            editor_camera_.field_of_view = cam.field_of_view;
          }
          editor_camera_.view_changed = true;
        }

        // ImGuizmo (uses editor camera matrices, disabled during right-click camera)
        if (has_selected_entity_ && !scene_right_active) {
          glm::mat4 view = editor_camera_.view_matrix;
          glm::mat4 proj = editor_camera_.projection;
          proj[1][1] *= -1;
          TransformComponent& transform =
              scene()->GetComponent<TransformComponent>(selected_entity_);
          glm::mat4 model = transform.GetTransformMatrix();
          ImGuizmo::SetOrthographic(false);
          ImGuizmo::SetDrawlist();
          ImGuizmo::SetRect(image_min.x, image_min.y, image_size.x,
                            image_size.y);
          if (ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj),
                                   current_op_, current_mode_,
                                   glm::value_ptr(model))) {
            // ImGuizmo returns a world-space matrix. If the entity has a parent,
            // convert back to local space before setting position/rotation/scale.
            Entity selected{selected_entity_, scene().get()};
            Entity parent = selected.GetParent();
            if (parent && parent.HasComponent<TransformComponent>()) {
              glm::mat4 parent_world = parent.GetComponent<TransformComponent>()
                                           .GetTransformMatrix();
              model = glm::inverse(parent_world) * model;
            }

            glm::vec3 translation, rotation, scale;
            ImGuizmo::DecomposeMatrixToComponents(
                glm::value_ptr(model), glm::value_ptr(translation),
                glm::value_ptr(rotation), glm::value_ptr(scale));

            transform.SetPosition(translation);
            transform.SetRotation(rotation);
            transform.SetScale(scale);
            scene_dirty_ = true;
          }

          // Draw collider wireframes for selected entity
          glm::mat4 vp = proj * view;
          ImDrawList* drawList = ImGui::GetWindowDrawList();

          auto ProjectPoint = [&](glm::vec3 worldPos) -> ImVec2 {
            glm::vec4 clip = vp * glm::vec4(worldPos, 1.0f);
            if (clip.w <= 0.001f) {
              return ImVec2(-9999, -9999);
            }
            glm::vec3 ndc = glm::vec3(clip) / clip.w;
            return ImVec2(image_min.x + (ndc.x * 0.5f + 0.5f) * image_size.x,
                          image_min.y + (-ndc.y * 0.5f + 0.5f) * image_size.y);
          };

          auto DrawLine3D = [&](glm::vec3 a, glm::vec3 b, ImU32 color) {
            ImVec2 sa = ProjectPoint(a);
            ImVec2 sb = ProjectPoint(b);
            drawList->AddLine(sa, sb, color, 1.5f);
          };

          if (scene()->HasComponent<BoxColliderComponent>(selected_entity_)) {
            auto& box =
                scene()->GetComponent<BoxColliderComponent>(selected_entity_);
            glm::vec3 center = transform.GetWorldPosition() + box.offset;
            glm::vec3 h = box.half_extents;
            glm::vec3 corners[8] = {
                center + glm::vec3(-h.x, -h.y, -h.z),
                center + glm::vec3(h.x, -h.y, -h.z),
                center + glm::vec3(h.x, h.y, -h.z),
                center + glm::vec3(-h.x, h.y, -h.z),
                center + glm::vec3(-h.x, -h.y, h.z),
                center + glm::vec3(h.x, -h.y, h.z),
                center + glm::vec3(h.x, h.y, h.z),
                center + glm::vec3(-h.x, h.y, h.z),
            };
            ImU32 col = IM_COL32(0, 255, 0, 200);
            DrawLine3D(corners[0], corners[1], col);
            DrawLine3D(corners[1], corners[2], col);
            DrawLine3D(corners[2], corners[3], col);
            DrawLine3D(corners[3], corners[0], col);
            DrawLine3D(corners[4], corners[5], col);
            DrawLine3D(corners[5], corners[6], col);
            DrawLine3D(corners[6], corners[7], col);
            DrawLine3D(corners[7], corners[4], col);
            DrawLine3D(corners[0], corners[4], col);
            DrawLine3D(corners[1], corners[5], col);
            DrawLine3D(corners[2], corners[6], col);
            DrawLine3D(corners[3], corners[7], col);
          }

          if (scene()->HasComponent<SphereColliderComponent>(
                  selected_entity_)) {
            auto& sphere = scene()->GetComponent<SphereColliderComponent>(
                selected_entity_);
            glm::vec3 center = transform.GetWorldPosition() + sphere.offset;
            float r = sphere.radius;
            ImU32 col = IM_COL32(0, 255, 0, 200);
            constexpr int segments = 32;
            for (int ring = 0; ring < 3; ring++) {
              for (int i = 0; i < segments; i++) {
                float a0 = (float)i / segments * 2.0f * glm::pi<float>();
                float a1 = (float)(i + 1) / segments * 2.0f * glm::pi<float>();
                glm::vec3 p0, p1;
                if (ring == 0) {
                  p0 = center + glm::vec3(cosf(a0), sinf(a0), 0) * r;
                  p1 = center + glm::vec3(cosf(a1), sinf(a1), 0) * r;
                } else if (ring == 1) {
                  p0 = center + glm::vec3(cosf(a0), 0, sinf(a0)) * r;
                  p1 = center + glm::vec3(cosf(a1), 0, sinf(a1)) * r;
                } else {
                  p0 = center + glm::vec3(0, cosf(a0), sinf(a0)) * r;
                  p1 = center + glm::vec3(0, cosf(a1), sinf(a1)) * r;
                }
                DrawLine3D(p0, p1, col);
              }
            }
          }

        }  // end has_selected_entity_

        // Canvas borders drawn by canvas render feature.
        // Camera frustums drawn by debug collider render feature.

        // Entity picking: click on Scene panel to select (only when not right-clicking)
        if (!scene_right_active && ImGui::IsMouseClicked(0) && scene_hovered &&
            !ImGuizmo::IsUsing() && !ImGuizmo::IsOver()) {
          ImVec2 mouse = ImGui::GetIO().MousePos;
          float rel_x = mouse.x - image_min.x;
          float rel_y = mouse.y - image_min.y;
          if (rel_x >= 0 && rel_y >= 0 && rel_x < image_size.x &&
              rel_y < image_size.y) {
            uint32_t render_w = editor_image->width_;
            uint32_t render_h = editor_image->height_;
            uint32_t px =
                static_cast<uint32_t>(rel_x * render_w / image_size.x);
            uint32_t py =
                static_cast<uint32_t>(rel_y * render_h / image_size.y);
            auto entity_id_tex = editor_camera_.resource_pool.GetTexture(
                "geometry.entity_id_resolve");
            auto canvas_entity_id_tex = editor_camera_.resource_pool.GetTexture(
                "canvas_world.entity_id");
            if (entity_id_tex) {
              renderer->RequestEntityPick(px, py, entity_id_tex,
                                          canvas_entity_id_tex);
            }
            // Store NDC for fallback sprite/canvas picking
            pending_pick_ndc_ = {
                (rel_x / image_size.x) * 2.0f - 1.0f,
                1.0f - (rel_y / image_size.y) * 2.0f  // flip Y
            };
          }
        }
        // Shift+R: open quick-add menu at camera position
        if (scene_hovered && ImGui::IsKeyPressed(ImGuiKey_R, false) &&
            ImGui::GetIO().KeyShift) {
          ImGui::OpenPopup("##QuickAdd");
        }
        if (ImGui::BeginPopup("##QuickAdd")) {
          glm::vec3 cam_pos = editor_camera_transform_.GetPosition();
          RenderAddEntityMenu(scene().get(), scene_dirty_, entt::null,
                              &cam_pos);
          ImGui::EndPopup();
        }
      }
    }
    ImGui::End();
  }
}

void EditorLayer::RenderGameViewportPanel() {
  Renderer* renderer = Engine::renderer().get();

  bool& game_view_open = panel_game_view_;
  if (game_view_open) {
    {
      bool gameVisible = ImGui::Begin(ICON_CAMERA " Game", &game_view_open);
      game_panel_visible_ = gameVisible;
      game_panel_focused_ = ImGui::IsWindowFocused();
      if (gameVisible) {
        DrawPlayStopButtons();
        {
          float comboWidth = kResolutionComboWidth;
          float rightEdge = ImGui::GetWindowContentRegionMax().x;
          ImGui::SameLine(rightEdge - comboWidth);
          ImGui::SetNextItemWidth(comboWidth);
          if (ImGui::BeginCombo(
                  "##GameResolution",
                  kResolutionPresets[resolution_preset_index_].label)) {
            for (int i = 0; i < kResolutionPresetCount; i++) {
              bool selected = (i == resolution_preset_index_);
              if (ImGui::Selectable(kResolutionPresets[i].label, selected)) {
                resolution_preset_index_ = i;
                scene()->SetRenderResolution(kResolutionPresets[i].size);
              }
              if (selected) {
                ImGui::SetItemDefaultFocus();
              }
            }
            ImGui::EndCombo();
          }
        }

        {
          // Check if any camera exists
          bool has_camera = false;
          for (auto entity : scene()->GetAllEntitiesWith<CameraComponent>()) {
            auto& cam = scene()->GetComponent<CameraComponent>(entity);
            if (cam.enabled) {
              has_camera = true;
              break;
            }
          }

          if (!has_camera) {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            const char* text = "No camera in scene";
            ImVec2 textSize = ImGui::CalcTextSize(text);
            ImGui::SetCursorPos(
                ImVec2(ImGui::GetCursorPosX() + (avail.x - textSize.x) * 0.5f,
                       ImGui::GetCursorPosY() + (avail.y - textSize.y) * 0.5f));
            ImGui::TextDisabled("%s", text);
          } else {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            if (scene()->GetRenderResolution().x <= 0) {
              // Free Aspect: game camera tracks panel size
              for (auto entity :
                   scene()->GetAllEntitiesWith<CameraComponent>()) {
                auto& cam = scene()->GetComponent<CameraComponent>(entity);
                if (!cam.enabled) {
                  continue;
                }
                uint32_t w = static_cast<uint32_t>(avail.x);
                uint32_t h = static_cast<uint32_t>(avail.y);
                if (w > 0 && h > 0 &&
                    (w != static_cast<uint32_t>(cam.viewport_size.x) ||
                     h != static_cast<uint32_t>(cam.viewport_size.y))) {
                  cam.viewport_size = {w, h};
                  cam.aspect_ratio =
                      static_cast<float>(w) / static_cast<float>(h);
                  cam.view_changed = true;
                  cam.resources_dirty = true;
                }
                break;
              }
            }

            auto final_output_desc = renderer->GetFinalOutputDescriptor();
            auto final_output_image = renderer->GetFinalOutputImage();
            if (final_output_desc && final_output_image) {
              ImTextureID gameDesc = reinterpret_cast<ImTextureID>(
              final_output_desc->descriptor_set_);

              float image_aspect = static_cast<float>(final_output_image->width_) /
                                  static_cast<float>(final_output_image->height_);
              float avail_aspect = avail.x / avail.y;

              ImVec2 drawSize;
              if (avail_aspect > image_aspect) {
                drawSize.y = avail.y;
                drawSize.x = drawSize.y * image_aspect;
              } else {
                drawSize.x = avail.x;
                drawSize.y = drawSize.x / image_aspect;
              }
              ImGui::Image(gameDesc, drawSize);

              ImVec2 imageMin = ImGui::GetItemRectMin();
              ImVec2 imageMax = ImGui::GetItemRectMax();

              // Set viewport origin and display size for UI hit testing
              scene()->SetViewportOrigin({imageMin.x, imageMin.y});
              scene()->SetViewportDisplaySize({drawSize.x, drawSize.y});

              // FPS overlay (top-left)
              ImVec2 textPos = ImVec2(imageMin.x + 6, imageMin.y + 6);
              std::string fpsStr =
                  std::format("FPS: {}", static_cast<int>(app_.GetFPS()));
              ImGui::GetWindowDrawList()->AddText(
                  textPos, IM_COL32(0, 255, 0, 255), fpsStr.c_str());

              // Resolution overlay (top-right)
              std::string resStr = std::format(
                  "{}x{}", final_output_image->width_, final_output_image->height_);
              ImVec2 resTextSize = ImGui::CalcTextSize(resStr.c_str());
              ImVec2 resPos =
                  ImVec2(imageMax.x - resTextSize.x - 6, imageMin.y + 6);
              ImGui::GetWindowDrawList()->AddText(
                  resPos, IM_COL32(0, 255, 0, 255), resStr.c_str());
            }
          }
        }
      }
      ImGui::End();
    }
  }
}

bool EditorLayer::DrawPlayStopButtons() {
  bool changed = false;
  if (editor_state_ == EditorState::Edit) {
    bool compiling = Engine::script_manager().IsCompiling();
    if (compiling) {
      ImGui::BeginDisabled();
    }
    if (ImGui::Button(compiling ? "Compiling..." : "Play")) {
      AutoSave();
      TakeSnapshot();
      editor_state_ = EditorState::Playing;
      scene()->ResetFirstUpdate();
      ImGui::SetWindowFocus(ICON_CAMERA " Game");
      changed = true;
    }
    if (compiling) {
      ImGui::EndDisabled();
    }
  } else {
    if (ImGui::Button("Stop")) {
      deferred_action_ = DeferredAction::StopPlaying;
      changed = true;
    }
  }
  return changed;
}

void EditorLayer::OnPostPresent() {
  // Execute pending entity pick readback (GPU is idle after EndPresent fence)
  Renderer* renderer = Engine::renderer().get();
  entt::entity picked;
  if (renderer->ExecuteEntityPick(picked)) {
    if (picked != entt::null && scene()->GetRegistry().valid(picked)) {
      selected_entity_ = picked;
      has_selected_entity_ = true;
      scroll_to_selected_ = true;
    } else if (pending_pick_ndc_.x >= -1.0f) {
      // GPU pick missed — fallback: check sprites by projecting to screen
      glm::mat4 vp = editor_camera_.projection * editor_camera_.view_matrix;
      // Vulkan flips Y in projection, undo for NDC comparison
      vp[1][1] *= -1.0f;

      entt::entity best = entt::null;
      float best_depth = std::numeric_limits<float>::max();

      for (auto entity :
           scene()->GetAllEntitiesWith<SpriteRendererComponent, TransformComponent>()) {
        auto& tc = scene()->GetComponent<TransformComponent>(entity);
        glm::vec3 world_pos = tc.GetWorldPosition();
        glm::vec4 clip = vp * glm::vec4(world_pos, 1.0f);
        if (clip.w <= 0.0f) {
          continue;
        }
        glm::vec3 ndc = glm::vec3(clip) / clip.w;

        // Approximate sprite screen size from scale
        glm::vec3 world_scale = tc.GetWorldScale();
        float half_w = world_scale.x * 0.5f;
        float half_h = world_scale.y * 0.5f;
        glm::vec4 corner =
            vp * glm::vec4(world_pos + glm::vec3(half_w, half_h, 0), 1.0f);
        if (corner.w <= 0.0f) {
          continue;
        }
        glm::vec3 corner_ndc = glm::vec3(corner) / corner.w;
        float extent_x = std::abs(corner_ndc.x - ndc.x);
        float extent_y = std::abs(corner_ndc.y - ndc.y);

        if (std::abs(pending_pick_ndc_.x - ndc.x) <= extent_x &&
            std::abs(pending_pick_ndc_.y - ndc.y) <= extent_y) {
          if (ndc.z < best_depth) {
            best = entity;
            best_depth = ndc.z;
          }
        }
      }

      if (best != entt::null) {
        selected_entity_ = best;
        has_selected_entity_ = true;
        scroll_to_selected_ = true;
      } else {
        has_selected_entity_ = false;
      }
    } else {
      has_selected_entity_ = false;
    }
    pending_pick_ndc_ = {-2, -2};  // reset
  }

  Engine::scene_manager().EndFrame();
}

void EditorLayer::TakeSnapshot() {
  SceneSerializer serializer(scene());
  play_mode_snapshot_ = serializer.SerializeToString();
}

void EditorLayer::RestoreSnapshot() {
  Engine::renderer()->WaitForGPU();
  has_selected_entity_ = false;

  ClearScene();

  SceneSerializer serializer(scene());
  serializer.DeserializeFromString(play_mode_snapshot_);
  play_mode_snapshot_.clear();

  scene()->InvalidateRenderGraphs();

  // Setup camera components that were deserialized
  for (auto entity : scene()->GetAllEntitiesWith<CameraComponent>()) {
    auto& cam = scene()->GetComponent<CameraComponent>(entity);
    Engine::renderer()->SetupCameraComponent(cam);
  }

  scene()->ResetPhysicsWorld();
  scene()->ResetScriptStates();
  scene()->ResetFirstUpdate();
}

void EditorLayer::OnPrePresent() {
  PROFILE_ZONE_SCOPED_N("Editor::OnPrePresent");

  Renderer* renderer = Engine::renderer().get();
  VkCommandBuffer cmd = renderer->GetCommandBuffer().handle_;

  if (editor_state_ == EditorState::Playing) {
    // PLAY MODE: Only render viewports that are actually visible.
    // This avoids double rendering when Scene and Game are tabbed.

    if (scene_panel_visible_) {
      // Transition editor PipelineOutput back to COLOR_ATTACHMENT
      // (it was left in SHADER_READ from our manual transition last frame)
      auto editor_output =
          editor_camera_.resource_pool.GetTexture("PipelineOutput");
      if (editor_output) {
        renderer->TransitionImageLayout(
            cmd, editor_output->images_[0],
            editor_output->format_,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1, 0, 1);
      }

      // Render editor camera
      PROFILE_PLOT("Scene Width",
                   static_cast<double>(editor_camera_.viewport_size.x));
      PROFILE_PLOT("Scene Height",
                   static_cast<double>(editor_camera_.viewport_size.y));
      scene()->RenderFromExternal(editor_camera_, editor_camera_transform_,
                                  show_grid_);
      PROFILE_FRAME_MARK_NAMED("Scene");

      // Transition editor PipelineOutput to SHADER_READ for ImGui sampling
      editor_output = editor_camera_.resource_pool.GetTexture("PipelineOutput");
      if (editor_output) {
        renderer->TransitionImageLayout(
            cmd, editor_output->images_[0],
            editor_output->format_,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 0, 1);
      }
    }

    if (game_panel_visible_) {
      PROFILE_FRAME_MARK_NAMED("Game");
      // Render ECS cameras (sets camera_ to ECS camera for BeginPresent)
      scene()->Render();
    }
  } else {
    // EDIT MODE: only render viewports that are visible.
    if (scene_panel_visible_) {
      PROFILE_FRAME_MARK_NAMED("Scene");
      scene()->RenderFromExternal(editor_camera_, editor_camera_transform_,
                                  show_grid_);
    }

    if (game_panel_visible_) {
      PROFILE_FRAME_MARK_NAMED("Game");
      scene()->Render();
    }
  }
}

void EditorLayer::UpdateHierarchyOrder() {
  if (hierarchy_data_.move_from == entt::null ||
      hierarchy_data_.move_to == entt::null) {
    return;
  }
  Entity from_entity = {hierarchy_data_.move_from, scene().get()};
  Entity to_entity = {hierarchy_data_.move_to, scene().get()};
  auto& hierarchy = scene()->GetSceneHierarchy();
  if (hierarchy_data_.bottom_part) {
    // todo move hierarchy order on childs
    if (from_entity.parent_handle() != entt::null) {
      scene()->UnlinkEntities(from_entity.parent_handle(),
                              hierarchy_data_.move_from);
    }
    std::erase(hierarchy, hierarchy_data_.move_from);
    auto insert_pos = std::ranges::find(hierarchy, hierarchy_data_.move_to) + 1;
    if (hierarchy.end() < insert_pos) {
      hierarchy.push_back(hierarchy_data_.move_from);
    } else {
      hierarchy.insert(insert_pos, hierarchy_data_.move_from);
    }
  } else {
    scene()->LinkEntities(hierarchy_data_.move_to, hierarchy_data_.move_from);
  }
  hierarchy_data_.move_from = entt::null;
  hierarchy_data_.move_to = entt::null;
}

// Main Menu Bar & Project/Scene Management

void EditorLayer::RenderCreateSkyboxPopup() {
  if (show_create_skybox_) {
    ImGui::OpenPopup("Create Skybox");
    show_create_skybox_ = false;
  }

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

  bool popup_open = true;
  if (ImGui::BeginPopupModal("Create Skybox", &popup_open,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    static char name_buf[128] = "skybox";
    static int skybox_type = 0;
    static AssetHandle source_handle;
    static std::array<AssetHandle, 6> face_handles;

    ImGui::InputText("Name", name_buf, sizeof(name_buf));

    const char* type_names[] = {"Panorama (2:1)", "Cubemap (6 faces)",
                                "Cross (4x3)"};
    ImGui::Combo("Type", &skybox_type, type_names, 3);

    if (skybox_type == 0 || skybox_type == 2) {
      AssetCombo("Source Image", AssetType::Texture, source_handle);
    } else {
      const char* face_labels[] = {"Right (+X)",  "Left (-X)",  "Top (+Y)",
                                   "Bottom (-Y)", "Front (+Z)", "Back (-Z)"};
      for (int i = 0; i < 6; i++) {
        ImGui::PushID(i);
        AssetCombo(face_labels[i], AssetType::Texture, face_handles[i]);
        ImGui::PopID();
      }
    }

    ImGui::Separator();

    bool can_create = name_buf[0] != '\0';
    if (skybox_type == 0 || skybox_type == 2) {
      can_create = can_create && source_handle.IsValid();
    } else {
      for (int i = 0; i < 6; i++) {
        if (!face_handles[i].IsValid()) {
          can_create = false;
          break;
        }
      }
    }

    if (ImGui::Button("Create") && can_create) {
      auto data = std::make_shared<SkyboxAssetData>();
      if (skybox_type == 0) {
        data->type = SkyboxType::Panorama;
        data->source_handle = source_handle;
      } else if (skybox_type == 1) {
        data->type = SkyboxType::Cubemap;
        for (int i = 0; i < 6; i++) {
          data->face_handles[i] = face_handles[i];
        }
      } else {
        data->type = SkyboxType::Cross;
        data->source_handle = source_handle;
      }

      std::string vfs_path = asset_browser_panel_.browser().CurrentVfsDir() +
                             std::string(name_buf) + ".wskybox";
      AssetHandle new_handle = AssetSerializerRegistry::Create<SkyboxAssetData>(
          name_buf, AssetType::Skybox, vfs_path, data);
      if (new_handle.IsValid()) {
        ScanProjectAssets();
        scene()->SetSkyboxAsset(new_handle);
        scene_dirty_ = true;
      }

      name_buf[0] = '\0';
      source_handle = {};
      face_handles = {};
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      name_buf[0] = '\0';
      source_handle = {};
      face_handles = {};
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

static AssetHandle CreateSpriteAsset(const std::string& vfs_path,
                                     const AssetHandle& texture_handle,
                                     float x, float y, float w, float h,
                                     float pivot_x, float pivot_y) {
  auto data = std::make_shared<SpriteAssetData>();
  data->texture_handle = texture_handle;
  data->rect = {x, y, w, h};
  data->pivot = {pivot_x, pivot_y};
  std::string name = VirtualFileSystem::Stem(vfs_path);
  return AssetSerializerRegistry::Create<SpriteAssetData>(
      name, AssetType::Sprite, vfs_path, data);
}

void EditorLayer::RenderCreateSpritePopup() {
  if (show_create_sprite_) {
    ImGui::OpenPopup("Create Sprite");
    show_create_sprite_ = false;
  }

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_Appearing);

  if (ImGui::BeginPopupModal("Create Sprite", nullptr,
                             ImGuiWindowFlags_None)) {
    static char name_buf[128] = "sprite";
    static AssetHandle texture_handle;
    static float rect[4] = {0, 0, 64, 64};  // x, y, w, h in pixels
    static float pivot[2] = {0.5f, 0.5f};

    ImGui::InputText("Name", name_buf, sizeof(name_buf));
    AssetCombo("Texture", AssetType::Texture, texture_handle, false);

    // Show texture preview with rect overlay
    if (texture_handle.IsValid()) {
      auto tex = Engine::asset_manager().Get<Texture>(texture_handle);
      if (!tex) {
        Engine::asset_manager().LoadSync(texture_handle);
        tex = Engine::asset_manager().Get<Texture>(texture_handle);
      }
      if (tex) {
        float tex_w = static_cast<float>(tex->width_);
        float tex_h = static_cast<float>(tex->height_);

        ImGui::Text("Texture: %dx%d", static_cast<int>(tex_w),
                     static_cast<int>(tex_h));

        ImGui::DragFloat4("Rect (x, y, w, h)", rect, 1.0f, 0.0f,
                          std::max(tex_w, tex_h));
        ImGui::DragFloat2("Pivot", pivot, 0.01f, 0.0f, 1.0f);

        // Clamp rect to texture bounds
        rect[0] = std::clamp(rect[0], 0.0f, tex_w);
        rect[1] = std::clamp(rect[1], 0.0f, tex_h);
        rect[2] = std::clamp(rect[2], 1.0f, tex_w - rect[0]);
        rect[3] = std::clamp(rect[3], 1.0f, tex_h - rect[1]);

        // Preview
        ImGui::Separator();
        ImGui::Text("Preview:");
        VkDescriptorSet desc = tex->GetImGuiDescriptor();
        if (desc) {
          // Show just the selected sub-region
          ImVec2 uv0(rect[0] / tex_w, rect[1] / tex_h);
          ImVec2 uv1((rect[0] + rect[2]) / tex_w,
                      (rect[1] + rect[3]) / tex_h);
          float aspect = rect[2] / rect[3];
          float preview_h = 128.0f;
          float preview_w = preview_h * aspect;
          ImGui::Image(reinterpret_cast<ImTextureID>(desc),
                       ImVec2(preview_w, preview_h), uv0, uv1);
        }
      }
    }

    ImGui::Separator();
    bool can_create = name_buf[0] != '\0' && texture_handle.IsValid();
    if (ImGui::Button("Create") && can_create) {
      std::string vfs_path = asset_browser_panel_.browser().CurrentVfsDir() +
                             std::string(name_buf) + ".wsprite";
      CreateSpriteAsset(vfs_path, texture_handle,
                      rect[0], rect[1], rect[2], rect[3],
                      pivot[0], pivot[1]);
      ScanProjectAssets();
      name_buf[0] = '\0';
      texture_handle = {};
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      name_buf[0] = '\0';
      texture_handle = {};
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void EditorLayer::RenderSliceSpritesPopup() {
  if (show_slice_sprites_) {
    ImGui::OpenPopup("Slice into Sprites");
    show_slice_sprites_ = false;
  }

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(700, 600), ImGuiCond_Appearing);

  if (ImGui::BeginPopupModal("Slice into Sprites", nullptr,
                             ImGuiWindowFlags_None)) {
    static char prefix_buf[128] = "sprite";
    static int columns = 6;
    static int rows = 1;
    static float pivot[2] = {0.5f, 0.5f};

    auto tex = slice_texture_handle_.IsValid()
                   ? Engine::asset_manager().Get<Texture>(slice_texture_handle_)
                   : nullptr;
    if (!tex) {
      ImGui::Text("Texture not loaded.");
      if (ImGui::Button("Close")) {
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
      return;
    }

    float tex_w = static_cast<float>(tex->width_);
    float tex_h = static_cast<float>(tex->height_);
    const auto* meta =
        Engine::asset_manager().GetMetadata(slice_texture_handle_);
    ImGui::Text("Texture: %s (%dx%d)", meta ? meta->name.c_str() : "?",
                static_cast<int>(tex_w), static_cast<int>(tex_h));

    ImGui::InputText("Name Prefix", prefix_buf, sizeof(prefix_buf));
    ImGui::InputInt("Columns", &columns);
    ImGui::InputInt("Rows", &rows);
    ImGui::DragFloat2("Pivot", pivot, 0.01f, 0.0f, 1.0f);

    columns = std::max(1, columns);
    rows = std::max(1, rows);

    float cell_w = tex_w / static_cast<float>(columns);
    float cell_h = tex_h / static_cast<float>(rows);
    int total = columns * rows;

    ImGui::Text("Cell size: %.0fx%.0f | Total sprites: %d", cell_w, cell_h,
                total);

    // Texture preview with grid overlay
    ImGui::Separator();
    VkDescriptorSet desc = tex->GetImGuiDescriptor();
    if (desc) {
      // Fit preview to available width
      float avail_w = ImGui::GetContentRegionAvail().x;
      float avail_h = ImGui::GetContentRegionAvail().y - 50.0f;
      float scale = std::min(avail_w / tex_w, avail_h / tex_h);
      scale = std::min(scale, 1.0f);  // don't upscale
      float preview_w = tex_w * scale;
      float preview_h = tex_h * scale;

      ImVec2 cursor = ImGui::GetCursorScreenPos();
      ImGui::Image(reinterpret_cast<ImTextureID>(desc),
                   ImVec2(preview_w, preview_h));

      // Draw grid lines
      ImDrawList* dl = ImGui::GetWindowDrawList();
      ImU32 grid_col = IM_COL32(255, 255, 0, 180);

      for (int c = 1; c < columns; c++) {
        float x = cursor.x + (static_cast<float>(c) / columns) * preview_w;
        dl->AddLine(ImVec2(x, cursor.y),
                    ImVec2(x, cursor.y + preview_h), grid_col);
      }
      for (int r = 1; r < rows; r++) {
        float y = cursor.y + (static_cast<float>(r) / rows) * preview_h;
        dl->AddLine(ImVec2(cursor.x, y),
                    ImVec2(cursor.x + preview_w, y), grid_col);
      }

      // Draw border
      dl->AddRect(cursor, ImVec2(cursor.x + preview_w, cursor.y + preview_h),
                  grid_col);

      // Draw cell index labels
      for (int r = 0; r < rows; r++) {
        for (int c = 0; c < columns; c++) {
          int idx = r * columns + c;
          float cx = cursor.x + (c + 0.5f) / columns * preview_w;
          float cy = cursor.y + (r + 0.5f) / rows * preview_h;
          std::string label = std::to_string(idx);
          ImVec2 text_sz = ImGui::CalcTextSize(label.c_str());
          dl->AddText(ImVec2(cx - text_sz.x * 0.5f, cy - text_sz.y * 0.5f),
                      IM_COL32(255, 255, 255, 200), label.c_str());
        }
      }
    }

    ImGui::Separator();
    bool can_create = prefix_buf[0] != '\0';
    if (ImGui::Button("Slice") && can_create) {
      std::string base_vfs = asset_browser_panel_.browser().CurrentVfsDir();
      for (int r = 0; r < rows; r++) {
        for (int c = 0; c < columns; c++) {
          int idx = r * columns + c;
          std::string name =
              std::string(prefix_buf) + "_" + std::to_string(idx);
          std::string vfs_path = base_vfs + name + ".wsprite";
          CreateSpriteAsset(vfs_path, slice_texture_handle_,
                          c * cell_w, r * cell_h, cell_w, cell_h,
                          pivot[0], pivot[1]);
        }
      }
      ScanProjectAssets();
      prefix_buf[0] = '\0';
      slice_texture_handle_ = {};
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      prefix_buf[0] = '\0';
      slice_texture_handle_ = {};
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void EditorLayer::RenderCreateSpriteAnimPopup() {
  // TODO: rewrite without SpriteSheet references
}

void EditorLayer::RenderCreateSpriteControllerPopup() {
  if (show_create_spritecontroller_) {
    ImGui::OpenPopup("Create Sprite Controller");
    show_create_spritecontroller_ = false;
  }

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_Appearing);

  bool popup_open = true;
  if (ImGui::BeginPopupModal("Create Sprite Controller", &popup_open,
                             ImGuiWindowFlags_None)) {
    static char name_buf[128] = "controller";
    static std::string default_state_name;

    struct StateEntry {
      char name[64] = "";
      AssetHandle anim_handle;
      float speed = 1.0f;
    };

    static std::vector<StateEntry> state_entries;

    ImGui::InputText("Name", name_buf, sizeof(name_buf));

    ImGui::SeparatorText("States");

    if (ImGui::BeginTable("##states", 4,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable)) {
      ImGui::TableSetupColumn("State Name", ImGuiTableColumnFlags_WidthFixed,
                              120);
      ImGui::TableSetupColumn("Animation");
      ImGui::TableSetupColumn("Speed", ImGuiTableColumnFlags_WidthFixed, 60);
      ImGui::TableSetupColumn("##del", ImGuiTableColumnFlags_WidthFixed, 20);
      ImGui::TableHeadersRow();

      int to_remove = -1;
      for (int i = 0; i < static_cast<int>(state_entries.size()); i++) {
        ImGui::PushID(i);
        auto& state = state_entries[i];

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##name", state.name, sizeof(state.name));

        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1);
        AssetCombo("##anim", AssetType::SpriteAnim, state.anim_handle, false);

        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1);
        ImGui::InputFloat("##speed", &state.speed, 0.1f);

        ImGui::TableNextColumn();
        if (ImGui::SmallButton("X")) {
          to_remove = i;
        }

        ImGui::PopID();
      }
      ImGui::EndTable();

      if (to_remove >= 0) {
        state_entries.erase(state_entries.begin() + to_remove);
      }
    }

    if (ImGui::Button("+ Add State")) {
      state_entries.push_back({});
    }

    // Default state selector
    if (ImGui::BeginCombo("Default State",
                          default_state_name.empty()
                              ? "(None)"
                              : default_state_name.c_str())) {
      for (auto& s : state_entries) {
        if (s.name[0] != '\0') {
          if (ImGui::Selectable(s.name, default_state_name == s.name)) {
            default_state_name = s.name;
          }
        }
      }
      ImGui::EndCombo();
    }

    ImGui::Separator();

    bool can_create = name_buf[0] != '\0' && !state_entries.empty();
    if (ImGui::Button("Create") && can_create) {
      auto data = std::make_shared<SpriteControllerAssetData>();
      data->default_state = default_state_name;
      for (auto& s : state_entries) {
        if (s.name[0] == '\0') {
          continue;
        }
        SpriteControllerAssetData::State state;
        state.name = s.name;
        state.animation_handle = s.anim_handle;
        state.speed = s.speed;
        data->states.push_back(std::move(state));
      }

      std::string vfs_path = asset_browser_panel_.browser().CurrentVfsDir() +
                             std::string(name_buf) + ".wspritecontroller";
      AssetSerializerRegistry::Create<SpriteControllerAssetData>(
          name_buf, AssetType::SpriteController, vfs_path, data);
      ScanProjectAssets();

      name_buf[0] = '\0';
      default_state_name.clear();
      state_entries.clear();
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      name_buf[0] = '\0';
      default_state_name.clear();
      state_entries.clear();
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

void EditorLayer::RenderEntityInspector(entt::entity handle) {
  Entity entity = {handle, scene().get()};
  TagComponent& tag = entity.GetComponent<TagComponent>();
  if (ImGui::InputText("##", &tag.name,
                       ImGuiInputTextFlags_AutoSelectAll)) {
    if (tag.name[0] == ' ') {
      TrimLeft(tag.name);
    }

    if (tag.name.empty()) {
      tag.name = "Entity";
    }
  }
  // Game tags
  {
    std::string tags_display;
    for (size_t i = 0; i < tag.tags.size(); i++) {
      if (i > 0) {
        tags_display += ", ";
      }
      tags_display += tag.tags[i];
    }
    if (tags_display.empty()) {
      tags_display = "(no tags)";
    }
    ImGui::TextDisabled("Tags: %s", tags_display.c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton("+##addtag")) {
      ImGui::OpenPopup("add_tag_popup");
    }
    if (ImGui::BeginPopup("add_tag_popup")) {
      static char tag_buf[64] = "";
      ImGui::InputText("##tagname", tag_buf, sizeof(tag_buf));
      ImGui::SameLine();
      if (ImGui::Button("Add") && tag_buf[0] != '\0') {
        tag.AddTag(tag_buf);
        tag_buf[0] = '\0';
        ImGui::CloseCurrentPopup();
      }
      // Show existing tags with remove buttons
      for (size_t i = 0; i < tag.tags.size(); i++) {
        ImGui::PushID(static_cast<int>(i));
        ImGui::Text("%s", tag.tags[i].c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) {
          tag.RemoveTag(tag.tags[i]);
          ImGui::PopID();
          break;
        }
        ImGui::PopID();
      }
      ImGui::EndPopup();
    }
  }

  ImGui::SameLine();
  if (ImGui::Button("Add")) {
    ImGui::OpenPopup("add_component_popup");
  }
  RenderModals(entity);
  if (ImGui::BeginPopup("add_component_popup")) {
    RenderAddPopup(entity);
    ImGui::EndPopup();
  }
  RenderExistingComponents(entity);
}

void EditorLayer::RenderProjectSettingsPopup() {
  if (!project()) {
    return;
  }

  if (show_project_settings_) {
    ImGui::OpenPopup("Project Settings");
    show_project_settings_ = false;
  }

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(720, 500), ImGuiCond_Appearing);

  bool popup_open = true;
  if (ImGui::BeginPopupModal("Project Settings", &popup_open,
                             ImGuiWindowFlags_NoScrollbar)) {
    auto& proj_settings = project()->GetSettings();
    auto& game_info = project()->GetGameInfo();
    bool changed = false;

    const char* categories[] = {"Scene", "Rendering", "Input"};
    constexpr int kCategoryCount = 3;

    // Left panel: category list
    // ItemSpacing.x = 2*WindowPadding so selectable highlight extends to child edges,
    // while text remains indented by WindowPadding (the cursor offset).
    float pad = ImGui::GetStyle().WindowPadding.x;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(pad * 2.0f, ImGui::GetStyle().ItemSpacing.y));
    ImGui::BeginChild("##categories", ImVec2(140, 0), ImGuiChildFlags_Borders);
    ImGui::PopStyleVar();
    for (int i = 0; i < kCategoryCount; i++) {
      if (ImGui::Selectable(categories[i], project_settings_category_ == i)) {
        project_settings_category_ = i;
      }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // Right panel: settings
    ImGui::BeginChild("##settings", ImVec2(0, 0), ImGuiChildFlags_Borders);

    if (project_settings_category_ == 0) {
      // ---- Scene ----
      ImGui::SeparatorText("Project");
      char name_buf[128];
      strncpy(name_buf, proj_settings.name.c_str(), sizeof(name_buf) - 1);
      name_buf[sizeof(name_buf) - 1] = '\0';
      if (ImGui::InputText(PrefixLabel("Project Name").c_str(), name_buf,
                           sizeof(name_buf))) {
        proj_settings.name = name_buf;
        changed = true;
      }

      ImGui::SeparatorText("Game");
      {
        char game_name_buf[128];
        strncpy(game_name_buf, game_info.name.c_str(),
                sizeof(game_name_buf) - 1);
        game_name_buf[sizeof(game_name_buf) - 1] = '\0';
        if (ImGui::InputText(PrefixLabel("Game Name").c_str(), game_name_buf,
                             sizeof(game_name_buf))) {
          game_info.name = game_name_buf;
          changed = true;
        }
      }
      {
        char version_buf[64];
        strncpy(version_buf, game_info.version.c_str(),
                sizeof(version_buf) - 1);
        version_buf[sizeof(version_buf) - 1] = '\0';
        if (ImGui::InputText(PrefixLabel("Version").c_str(), version_buf,
                             sizeof(version_buf))) {
          game_info.version = version_buf;
          changed = true;
        }
      }
      if (AssetCombo("Icon", AssetType::Texture, game_info.icon, true,
                     "(None)")) {
        changed = true;
      }

      // Start scene selector
      if (AssetCombo(PrefixLabel("Start Scene").c_str(), AssetType::Scene,
                     game_info.start_scene, true, "(None)")) {
        changed = true;
      }

      ImGui::SeparatorText("Skybox");
      {
        AssetHandle skybox_handle = scene()->GetSkyboxAsset();
        if (AssetCombo(PrefixLabel("Skybox").c_str(), AssetType::Skybox,
                       skybox_handle, true, "(Default)")) {
          scene()->SetSkyboxAsset(skybox_handle);
          scene_dirty_ = true;
        }
      }

      ImGui::SeparatorText("Asset Loading");
      {
        bool keep_loaded = scene()->GetKeepAssetsLoaded();
        if (ImGui::Checkbox(PrefixLabel("Keep Assets Loaded").c_str(),
                            &keep_loaded)) {
          scene()->SetKeepAssetsLoaded(keep_loaded);
          scene_dirty_ = true;
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip(
              "When enabled, assets from this scene are not unloaded\n"
              "when switching to another scene. Useful for loading screens.");
        }

        bool preload = scene()->GetPreloadAssets();
        if (ImGui::Checkbox(PrefixLabel("Preload Assets").c_str(), &preload)) {
          scene()->SetPreloadAssets(preload);
          scene_dirty_ = true;
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip(
              "When enabled, assets for this scene are loaded\n"
              "when the project opens, even if the scene is not active.\n"
              "Useful for scenes that need instant transitions.");
        }
      }

      ImGui::SeparatorText("Physics");
      {
        auto& physics = scene()->GetPhysicsWorld();
        glm::vec3 gravity = physics.GetGravity();
        if (ImGui::DragFloat3(PrefixLabel("Gravity").c_str(), &gravity.x,
                              0.1f)) {
          physics.SetGravity(gravity);
        }
      }

    } else if (project_settings_category_ == 1) {
      // ---- Rendering ----
      Renderer* renderer = Engine::renderer().get();
      auto& settings = renderer->options();

      ImGui::SeparatorText("Ambient");
      ImGui::ColorEdit3(PrefixLabel("Ambient Color").c_str(),
                        &settings.ambient_color.x);
      ImGui::SliderFloat(PrefixLabel("Ambient Intensity").c_str(),
                         &settings.ambient_intensity, 0.0f, 1.0f);

      ImGui::SeparatorText("General");
      ImGui::Checkbox(PrefixLabel("Enable SSAO").c_str(),
                      &settings.ssao_enabled);
      ImGui::Checkbox(PrefixLabel("Enable IBL").c_str(),
                      &settings.ibl_enabled);
      ImGui::Checkbox(PrefixLabel("Enable Vsync").c_str(), &settings.vsync);
      ImGui::Checkbox(PrefixLabel("Shadows").c_str(),
                      &settings.shadows_enabled);
      if (settings.shadows_enabled && renderer->IsRayTracingSupported()) {
        ImGui::Checkbox(PrefixLabel("RT Shadows").c_str(),
                        &settings.rt_shadows_enabled);
      }

      {
        const char* aa_labels[] = {"None", "FXAA", "TAA"};
        int aa_current =
            static_cast<int>(static_cast<AntiAliasingMode>(settings.aa_mode));
        if (ImGui::Combo(PrefixLabel("Anti-Aliasing").c_str(), &aa_current,
                         aa_labels, IM_ARRAYSIZE(aa_labels))) {
          settings.aa_mode = static_cast<AntiAliasingMode>(aa_current);
        }
      }

      {
        std::vector<SamplingMode> supported_values =
            renderer->GetSupportedSamplingModes();
        std::vector<const char*> labels;
        SamplingMode current_mode = settings.msaa_mode;
        int selected_index = 0;
        for (size_t i = 0; i < supported_values.size(); i++) {
          labels.push_back(ToString(supported_values[i]));
          if (supported_values[i] == current_mode) {
            selected_index = static_cast<int>(i);
          }
        }
        if (ImGui::Combo(PrefixLabel("MSAA Samples").c_str(), &selected_index,
                         labels.data(), labels.size())) {
          settings.msaa_mode = supported_values[selected_index];
        }
      }

      ImGui::SeparatorText("Post Processing");
      ImGui::Checkbox(PrefixLabel("Enable Bloom").c_str(),
                      &settings.bloom_enabled);
      if (settings.bloom_enabled) {
        ImGui::SliderFloat(PrefixLabel("Bloom Threshold").c_str(),
                           &settings.bloom_threshold, 0.0f, 1.0f);
        ImGui::SliderFloat(PrefixLabel("Bloom Intensity").c_str(),
                           &settings.bloom_intensity, 0.0f, 2.0f);
      }
      ImGui::Checkbox(PrefixLabel("Enable Motion Blur").c_str(),
                      &settings.motion_blur_enabled);
      if (settings.motion_blur_enabled) {
        ImGui::SliderFloat(PrefixLabel("MB Strength").c_str(),
                           &settings.motion_blur_strength, 0.0f, 3.0f);
        ImGui::SliderInt(PrefixLabel("MB Samples").c_str(),
                         &settings.motion_blur_samples, 2, 16);
      }

      // Sync live renderer options back to project for saving
      GameLoader::CaptureRenderOptions(game_info.render_options);
      changed = true;

    } else if (project_settings_category_ == 2) {
      // ---- Input ----
      auto& input = game_info.input;
      bool input_changed = false;

      ImGui::SeparatorText("Mouse");
      input_changed |=
          ImGui::DragFloat(PrefixLabel("Sensitivity X").c_str(),
                           &input.mouse_sensitivity_x, 1.0f, 1.0f, 500.0f);
      input_changed |=
          ImGui::DragFloat(PrefixLabel("Sensitivity Y").c_str(),
                           &input.mouse_sensitivity_y, 1.0f, 1.0f, 500.0f);

      int gp_count = InputManager::GetConnectedGamepadCount();
      if (gp_count > 0) {
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f),
                           "Gamepads: %d connected", gp_count);
      }

      ImGui::SeparatorText("Contexts");

      // Validate selected context still exists
      if (!selected_input_context_.empty() &&
          input.contexts.find(selected_input_context_) ==
              input.contexts.end()) {
        selected_input_context_.clear();
        selected_input_item_ = -1;
      }

      // Left: context list
      ImGui::BeginChild("##ctx_list", ImVec2(130, 0), ImGuiChildFlags_Borders);
      for (auto& [ctx_name, ctx] : input.contexts) {
        if (ImGui::Selectable(ctx_name.c_str(),
                              selected_input_context_ == ctx_name)) {
          selected_input_context_ = ctx_name;
          selected_input_item_ = -1;
        }
      }
      ImGui::Spacing();
      static char new_ctx_name[64] = "";
      ImGui::SetNextItemWidth(-1);
      ImGui::InputTextWithHint("##newctx", "new context...", new_ctx_name,
                               sizeof(new_ctx_name));
      if (ImGui::Button("Add", ImVec2(-1, 0)) && new_ctx_name[0] != '\0') {
        std::string cname = new_ctx_name;
        if (input.contexts.find(cname) == input.contexts.end()) {
          InputContext nc;
          nc.name = cname;
          input.contexts[cname] = std::move(nc);
          selected_input_context_ = cname;
          input_changed = true;
          new_ctx_name[0] = '\0';
        }
      }
      ImGui::EndChild();

      ImGui::SameLine();

      // Right: selected context content
      ImGui::BeginChild("##ctx_content", ImVec2(0, 0));
      if (!selected_input_context_.empty() &&
          input.contexts.find(selected_input_context_) !=
              input.contexts.end()) {
        auto& ctx = input.contexts[selected_input_context_];

        // Context header with delete
        ImGui::Text("Context: %s", selected_input_context_.c_str());
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 80);
        if (ImGui::SmallButton("Delete")) {
          input.contexts.erase(selected_input_context_);
          selected_input_context_.clear();
          selected_input_item_ = -1;
          input_changed = true;
          // Skip rendering rest since ctx is gone
          ImGui::EndChild();
          if (input_changed) {
            InputManager::LoadFromSettings(input);
            changed = true;
          }
          // Early out handled below
          goto input_done;
        }
        ImGui::Separator();

        static int input_tab = 0;  // 0 = Actions, 1 = Axes
        if (ImGui::BeginTabBar("##input_tabs")) {
          if (ImGui::BeginTabItem("Actions")) {
            input_tab = 0;

            // Table: Name | Keys | Buttons | Delete
            if (ImGui::BeginTable("##actions_table", 4,
                                  ImGuiTableFlags_Borders |
                                      ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_Resizable)) {
              ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed,
                                      100);
              ImGui::TableSetupColumn("Keys");
              ImGui::TableSetupColumn("Buttons");
              ImGui::TableSetupColumn("##del", ImGuiTableColumnFlags_WidthFixed,
                                      20);
              ImGui::TableHeadersRow();

              int action_to_remove = -1;
              for (int i = 0; i < (int)ctx.actions.size(); i++) {
                ImGui::PushID(i);
                auto& action = ctx.actions[i];

                ImGui::TableNextRow();

                // Name
                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-1);
                char abuf[128];
                strncpy(abuf, action.name.c_str(), sizeof(abuf) - 1);
                abuf[sizeof(abuf) - 1] = '\0';
                if (ImGui::InputText("##name", abuf, sizeof(abuf))) {
                  action.name = abuf;
                  input_changed = true;
                }

                // Keys (tags + add combo)
                ImGui::TableNextColumn();
                int key_rm = -1;
                for (int k = 0; k < (int)action.keys.size(); k++) {
                  if (k > 0) {
                    ImGui::SameLine();
                  }
                  ImGui::PushID(k);
                  ImGui::SmallButton(KeyCodeToString(action.keys[k]));
                  if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                    key_rm = k;
                  }
                  if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Right-click to remove");
                  }
                  ImGui::PopID();
                }
                if (key_rm >= 0) {
                  action.keys.erase(action.keys.begin() + key_rm);
                  input_changed = true;
                }
                if (!action.keys.empty()) {
                  ImGui::SameLine();
                }
                ImGui::PushID("addkey");
                ImGui::SetNextItemWidth(50);
                if (ImGui::BeginCombo("##addkey", "+",
                                      ImGuiComboFlags_NoPreview)) {
                  for (auto code : GetAllKeyCodes()) {
                    if (ImGui::Selectable(KeyCodeToString(code))) {
                      action.keys.push_back(code);
                      input_changed = true;
                    }
                  }
                  ImGui::EndCombo();
                }
                ImGui::PopID();

                // Buttons (tags + add combo)
                ImGui::TableNextColumn();
                int btn_rm = -1;
                for (int b = 0; b < (int)action.buttons.size(); b++) {
                  if (b > 0) {
                    ImGui::SameLine();
                  }
                  ImGui::PushID(b + 200);
                  ImGui::SmallButton(GamepadButtonToString(action.buttons[b]));
                  if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                    btn_rm = b;
                  }
                  if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Right-click to remove");
                  }
                  ImGui::PopID();
                }
                if (btn_rm >= 0) {
                  action.buttons.erase(action.buttons.begin() + btn_rm);
                  input_changed = true;
                }
                if (!action.buttons.empty()) {
                  ImGui::SameLine();
                }
                ImGui::PushID("addbtn");
                ImGui::SetNextItemWidth(50);
                if (ImGui::BeginCombo("##addbtn", "+",
                                      ImGuiComboFlags_NoPreview)) {
                  for (auto btn : GetAllGamepadButtons()) {
                    if (ImGui::Selectable(GamepadButtonToString(btn))) {
                      action.buttons.push_back(btn);
                      input_changed = true;
                    }
                  }
                  ImGui::EndCombo();
                }
                ImGui::PopID();

                // Delete
                ImGui::TableNextColumn();
                if (ImGui::SmallButton("X")) {
                  action_to_remove = i;
                }

                ImGui::PopID();
              }
              ImGui::EndTable();

              if (action_to_remove >= 0) {
                ctx.actions.erase(ctx.actions.begin() + action_to_remove);
                input_changed = true;
              }
            }
            if (ImGui::Button("+ Add Action")) {
              ctx.actions.push_back({"New Action", {}, {}});
              input_changed = true;
            }

            ImGui::EndTabItem();
          }

          if (ImGui::BeginTabItem("Axes")) {
            input_tab = 1;

            // Table: Name | +Keys | -Keys | Stick | Smooth | Delete
            if (ImGui::BeginTable("##axes_table", 6,
                                  ImGuiTableFlags_Borders |
                                      ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_Resizable)) {
              ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed,
                                      90);
              ImGui::TableSetupColumn("+Keys");
              ImGui::TableSetupColumn("-Keys");
              ImGui::TableSetupColumn("Stick", ImGuiTableColumnFlags_WidthFixed,
                                      100);
              ImGui::TableSetupColumn("Smooth", ImGuiTableColumnFlags_WidthFixed,
                                      80);
              ImGui::TableSetupColumn("##del", ImGuiTableColumnFlags_WidthFixed,
                                      20);
              ImGui::TableHeadersRow();

              int axis_to_remove = -1;
              for (int i = 0; i < (int)ctx.axes.size(); i++) {
                ImGui::PushID(i + 1000);
                auto& axis = ctx.axes[i];

                ImGui::TableNextRow();

                // Name
                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-1);
                char abuf[128];
                strncpy(abuf, axis.name.c_str(), sizeof(abuf) - 1);
                abuf[sizeof(abuf) - 1] = '\0';
                if (ImGui::InputText("##name", abuf, sizeof(abuf))) {
                  axis.name = abuf;
                  input_changed = true;
                }

                // Positive keys
                ImGui::TableNextColumn();
                int pk_rm = -1;
                for (int k = 0; k < (int)axis.positive_keys.size(); k++) {
                  if (k > 0) {
                    ImGui::SameLine();
                  }
                  ImGui::PushID(k);
                  ImGui::SmallButton(KeyCodeToString(axis.positive_keys[k]));
                  if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                    pk_rm = k;
                  }
                  if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Right-click to remove");
                  }
                  ImGui::PopID();
                }
                if (pk_rm >= 0) {
                  axis.positive_keys.erase(axis.positive_keys.begin() + pk_rm);
                  input_changed = true;
                }
                if (!axis.positive_keys.empty()) {
                  ImGui::SameLine();
                }
                ImGui::PushID("addpos");
                ImGui::SetNextItemWidth(50);
                if (ImGui::BeginCombo("##addpos", "+",
                                      ImGuiComboFlags_NoPreview)) {
                  for (auto code : GetAllKeyCodes()) {
                    if (ImGui::Selectable(KeyCodeToString(code))) {
                      axis.positive_keys.push_back(code);
                      input_changed = true;
                    }
                  }
                  ImGui::EndCombo();
                }
                ImGui::PopID();

                // Negative keys
                ImGui::TableNextColumn();
                int nk_rm = -1;
                for (int k = 0; k < (int)axis.negative_keys.size(); k++) {
                  if (k > 0) {
                    ImGui::SameLine();
                  }
                  ImGui::PushID(k + 500);
                  ImGui::SmallButton(KeyCodeToString(axis.negative_keys[k]));
                  if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                    nk_rm = k;
                  }
                  if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Right-click to remove");
                  }
                  ImGui::PopID();
                }
                if (nk_rm >= 0) {
                  axis.negative_keys.erase(axis.negative_keys.begin() + nk_rm);
                  input_changed = true;
                }
                if (!axis.negative_keys.empty()) {
                  ImGui::SameLine();
                }
                ImGui::PushID("addneg");
                ImGui::SetNextItemWidth(50);
                if (ImGui::BeginCombo("##addneg", "+",
                                      ImGuiComboFlags_NoPreview)) {
                  for (auto code : GetAllKeyCodes()) {
                    if (ImGui::Selectable(KeyCodeToString(code))) {
                      axis.negative_keys.push_back(code);
                      input_changed = true;
                    }
                  }
                  ImGui::EndCombo();
                }
                ImGui::PopID();

                // Stick
                ImGui::TableNextColumn();
                const char* stick_label =
                    axis.gamepad_axis >= 0
                        ? GamepadAxisToString(axis.gamepad_axis)
                        : "None";
                ImGui::SetNextItemWidth(-1);
                ImGui::PushID("gpaxis");
                if (ImGui::BeginCombo("##stick", stick_label)) {
                  if (ImGui::Selectable("None", axis.gamepad_axis < 0)) {
                    axis.gamepad_axis = -1;
                    input_changed = true;
                  }
                  for (auto ga : GetAllGamepadAxes()) {
                    if (ImGui::Selectable(GamepadAxisToString(ga),
                                          axis.gamepad_axis == ga)) {
                      axis.gamepad_axis = ga;
                      input_changed = true;
                    }
                  }
                  ImGui::EndCombo();
                }
                ImGui::PopID();

                // Smooth
                ImGui::TableNextColumn();
                ImGui::PushID("smooth");
                if (ImGui::Checkbox("##smooth", &axis.smooth)) {
                  input_changed = true;
                }
                if (axis.smooth) {
                  ImGui::SetNextItemWidth(60);
                  if (ImGui::DragFloat("Grav", &axis.gravity, 0.1f, 0.1f,
                                       50.0f)) {
                    input_changed = true;
                  }
                  ImGui::SetNextItemWidth(60);
                  if (ImGui::DragFloat("Sens", &axis.sensitivity, 0.1f, 0.1f,
                                       50.0f)) {
                    input_changed = true;
                  }
                }
                ImGui::PopID();

                // Delete
                ImGui::TableNextColumn();
                if (ImGui::SmallButton("X")) {
                  axis_to_remove = i;
                }

                ImGui::PopID();
              }
              ImGui::EndTable();

              if (axis_to_remove >= 0) {
                ctx.axes.erase(ctx.axes.begin() + axis_to_remove);
                input_changed = true;
              }
            }
            if (ImGui::Button("+ Add Axis")) {
              ctx.axes.push_back({"New Axis", {}, {}});
              input_changed = true;
            }

            ImGui::EndTabItem();
          }
          ImGui::EndTabBar();
        }
      } else {
        ImGui::TextDisabled("Select a context from the list");
      }
      ImGui::EndChild();

    input_done:
      if (input_changed) {
        InputManager::LoadFromSettings(input);
        changed = true;
      }
    }

    ImGui::EndChild();

    if (changed) {
      project()->Save();
    }

    ImGui::EndPopup();
  }
}

void EditorLayer::RenderMainMenuBar() {
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::BeginMenu("Project")) {
        if (ImGui::MenuItem("New Project...")) {
          NewProject();
        }
        if (ImGui::MenuItem("Open Project...")) {
          OpenProject();
        }
        if (ImGui::MenuItem("Save Project", nullptr, false,
                            project() != nullptr)) {
          SaveProject();
        }
        auto recent = RecentProjects::Load();
        if (!recent.empty()) {
          ImGui::Separator();
          for (const auto& path : recent) {
            std::string label = std::filesystem::path(path).stem().string();
            if (ImGui::MenuItem(label.c_str())) {
              if (std::filesystem::exists(path)) {
                deferred_action_ = DeferredAction::OpenProject;
                deferred_path_ = path;
              }
            }
            if (ImGui::IsItemHovered()) {
              ImGui::SetTooltip("%s", path.c_str());
            }
          }
        }
        ImGui::EndMenu();
      }

      ImGui::Separator();

      if (ImGui::MenuItem("New Scene", nullptr, false, project() != nullptr)) {
        NewScene();
      }
      if (ImGui::MenuItem("Save Scene", "Ctrl+S", false,
                          !current_scene_path_.empty())) {
        SaveScene();
      }
      if (ImGui::MenuItem("Save Scene As...", nullptr, false,
                          project() != nullptr)) {
        SaveSceneAs();
      }
      ImGui::Separator();

      if (ImGui::MenuItem("Export Game...", nullptr, false,
                          project() != nullptr)) {
        ExportGame();
      }

      ImGui::Separator();

      if (ImGui::MenuItem("Exit")) {
        app_.Close();
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
      if (ImGui::MenuItem("Clear Scene")) {
        ClearScene();
      }
      if (ImGui::MenuItem("Project Settings", nullptr, false,
                          project() != nullptr)) {
        show_project_settings_ = true;
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Window")) {
      ImGui::MenuItem(ICON_CAMERA " Scene", nullptr, &panel_scene_view_);
      ImGui::MenuItem(ICON_CAMERA " Game", nullptr, &panel_game_view_);
      ImGui::MenuItem(ICON_HIERARCHY " Scene Hierarchy", nullptr, &panel_scene_hierarchy_);
      ImGui::MenuItem("Entity Inspector", nullptr, &panel_components_);
      ImGui::MenuItem(ICON_BROWSER " Asset Browser", nullptr, &panel_asset_browser_);
      ImGui::MenuItem(ICON_CONSOLE " Console", nullptr, &panel_console_);
      ImGui::MenuItem("Stats", nullptr, &panel_stats_);
      ImGui::MenuItem("LSP Debug", nullptr, &panel_lsp_debug_);
      ImGui::Separator();
      if (ImGui::MenuItem("Reset Layout")) {
        panel_scene_hierarchy_ = true;
        panel_components_ = true;
        panel_asset_browser_ = true;
        panel_console_ = true;
        panel_stats_ = true;
        panel_scene_view_ = true;
        panel_game_view_ = true;
        layout_initialized_ = false;
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Debug")) {
      auto& settings = Engine::renderer()->options();

      ImGui::Checkbox(PrefixLabel("Wireframe Mode").c_str(),
                      &settings.wireframe_enabled);
      ImGui::Checkbox(PrefixLabel("Only SSAO").c_str(), &settings.only_ssao);
      {
        const char* debug_modes[] = {"Off",     "Cascades",  "Material",
                                     "Normals", "World Pos", "Raw Normal",
                                     "Albedo",  "Depth",     "Vertex Normal"};
        int debug_mode = settings.debug_cascades;
        if (ImGui::Combo(PrefixLabel("Debug View").c_str(), &debug_mode,
                         debug_modes, 9)) {
          settings.debug_cascades = debug_mode;
        }
      }

      ImGui::SeparatorText("Overlays");
      ImGui::Checkbox(PrefixLabel("Colliders").c_str(),
                      &settings.show_colliders);
      ImGui::Checkbox(PrefixLabel("Triggers").c_str(), &settings.show_triggers);
      ImGui::Checkbox(PrefixLabel("Reverb Zones").c_str(),
                      &settings.show_reverb_zones);
      ImGui::Checkbox(PrefixLabel("Cameras").c_str(), &settings.show_cameras);

      ImGui::SeparatorText("Actions");
      if (ImGui::MenuItem("Reload Scripts")) {
        Engine::script_manager().ReloadAsync();
      }
      if (ImGui::MenuItem("Reload All (+ Core)")) {
        Engine::script_manager().ReloadAsync(true);
      }
      if (ImGui::MenuItem("Recreate Pipeline")) {
        Engine::renderer()->SetRecreatePipeline(true);
      }

      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help")) {
      if (ImGui::MenuItem("About")) {
        show_about_popup_ = true;
      }
      ImGui::EndMenu();
    }

    // Right-aligned status bar: asset stats + project info
    {
      auto asset_stats = Engine::asset_manager().GetStats();

      std::string status_text;
      bool has_activity =
          asset_stats.loading > 0 || Engine::script_manager().IsCompiling();
      bool has_compile_error = false;
      if (Engine::script_manager().IsCompiling()) {
        status_text = "Compiling scripts...";
      } else if (!Engine::script_manager().last_compile_result().output.empty()
                 && !Engine::script_manager().last_compile_result().success) {
        has_compile_error = true;
      }

      if (status_text.empty() && asset_stats.loading > 0) {
        status_text = std::format("Loading {} asset{}...", asset_stats.loading,
                                  asset_stats.loading > 1 ? "s" : "");
      } else if (status_text.empty() && asset_stats.failed > 0) {
        status_text = std::format("{} failed", asset_stats.failed);
      }
      std::string asset_summary =
          std::format("[{}/{}]", asset_stats.loaded, asset_stats.total);

      std::string info;
      if (project()) {
        info = project()->GetSettings().name;
        if (!current_scene_path_.empty()) {
          info += " - " + VirtualFileSystem::Stem(current_scene_path_);
        }
        if (scene_dirty_) {
          info += " *";
        }
      } else {
        info = "No Project";
      }

      float spacing = 12.0f;
      float info_width = ImGui::CalcTextSize(info.c_str()).x;
      float summary_width = ImGui::CalcTextSize(asset_summary.c_str()).x;
      float status_width =
          status_text.empty()
              ? 0.0f
              : ImGui::CalcTextSize(status_text.c_str()).x + spacing;
      float error_width =
          has_compile_error
              ? ImGui::CalcTextSize("Compile Error").x + spacing
              : 0.0f;
      bool has_pending_reload = script_reload_pending_ &&
                                editor_state_ == EditorState::Playing;
      float pending_width =
          has_pending_reload
              ? ImGui::CalcTextSize("Reload Pending").x + spacing
              : 0.0f;
      float total_right = pending_width + error_width + status_width +
                          summary_width + spacing + info_width + 16.0f;

      ImGui::SameLine(ImGui::GetWindowWidth() - total_right);

      if (has_pending_reload) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
        ImGui::Text("Reload Pending");
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("Script changes detected.\n"
                            "Reload will happen when you stop playing.");
        }
        ImGui::PopStyleColor();
        ImGui::SameLine(0, spacing);
      }

      if (has_compile_error) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        if (ImGui::SmallButton("Compile Error")) {
          ImGui::OpenPopup("compile_error_popup");
        }
        ImGui::PopStyleColor();
        ImGui::SameLine(0, spacing);
      }

      if (!status_text.empty()) {
        if (has_activity) {
          ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "%s",
                             status_text.c_str());
        } else {
          ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s",
                             status_text.c_str());
        }
        ImGui::SameLine(0, spacing);
      }

      ImGui::TextDisabled("%s", asset_summary.c_str());
      ImGui::SameLine(0, spacing);
      ImGui::TextDisabled("%s", info.c_str());
    }

    // Compile error popup
    ImGui::SetNextWindowSizeConstraints(ImVec2(800, 400), ImVec2(800, 400));
    if (ImGui::BeginPopup("compile_error_popup")) {
      const auto& result = Engine::script_manager().last_compile_result();
      if (result.success) {
        ImGui::CloseCurrentPopup();
      } else {
        ImGui::Text("Compilation failed (exit code %d)", result.exit_code);
        ImGui::Separator();
        ImGui::BeginChild("compile_output",
                           ImVec2(0, -ImGui::GetFrameHeightWithSpacing()),
                           ImGuiChildFlags_None,
                           ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        ImGui::TextUnformatted(result.output.c_str());
        ImGui::PopStyleColor();
        ImGui::EndChild();
        if (!result.command.empty()) {
          ImGui::TextWrapped("Command: %s", result.command.c_str());
        }
      }
      ImGui::EndPopup();
    }

    ImGui::EndMainMenuBar();
  }

  // About popup
  if (show_about_popup_) {
    ImGui::OpenPopup("About Wiesel");
    show_about_popup_ = false;
  }
  if (ImGui::BeginPopupModal("About Wiesel", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Wiesel Engine");
    ImGui::Separator();

    ImGui::Text("Git Branch: %s", WIESEL_GIT_BRANCH);
    ImGui::Text("Git Commit: %s", WIESEL_GIT_COMMIT);
    ImGui::Text("Build Type: %s", WIESEL_BUILD_TYPE);

#ifdef WIESEL_BACKEND_SDL3
    ImGui::Text("Window Backend: SDL3");
#else
    ImGui::Text("Window Backend: GLFW");
#endif

    auto props = Engine::renderer()->GetPhysicalDeviceProperties();
    uint32_t vk_major = VK_API_VERSION_MAJOR(props.apiVersion);
    uint32_t vk_minor = VK_API_VERSION_MINOR(props.apiVersion);
    uint32_t vk_patch = VK_API_VERSION_PATCH(props.apiVersion);
    ImGui::Text("GPU: %s", props.deviceName);
    ImGui::Text("Vulkan: %u.%u.%u", vk_major, vk_minor, vk_patch);
    ImGui::Text("FPS: %.1f", app_.GetFPS());

    ImGui::Separator();
    if (ImGui::Button("Close", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  // Keyboard shortcuts (skip when code editor or other text input has focus)
  ImGuiIO& io = ImGui::GetIO();
  bool text_input_active = code_editor_focused_ || io.WantTextInput;
  if (!text_input_active && io.KeyCtrl &&
      ImGui::IsKeyPressed(ImGuiKey_S, false)) {
    if (!current_scene_path_.empty()) {
      SaveScene();
    } else {
      SaveSceneAs();
    }
  }

  // Entity copy (Ctrl+C)
  if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false) &&
      has_selected_entity_ && !ImGui::GetIO().WantTextInput) {
    Entity entity{selected_entity_, scene().get()};
    nlohmann::json j;
    j["name"] = entity.GetName();
    ComponentSerializerRegistry::SerializeAll(entity, j);
    entity_clipboard_ = j.dump();
  }

  // Entity paste (Ctrl+V)
  if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V, false) &&
      !entity_clipboard_.empty() && !ImGui::GetIO().WantTextInput) {
    try {
      nlohmann::json j = nlohmann::json::parse(entity_clipboard_);
      std::string name = j.value("name", "Entity") + " (Copy)";
      Entity new_entity = scene()->CreateEntity(name);
      ComponentSerializerRegistry::DeserializeAll(new_entity, j, nullptr);
      selected_entity_ = new_entity.handle();
      has_selected_entity_ = true;
      scroll_to_selected_ = true;
      scene_dirty_ = true;
    } catch (const std::exception& e) {
      LOG_ERROR("Failed to paste entity: {}", e.what());
    }
  }

  // Entity duplicate (Ctrl+D)
  if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false) &&
      has_selected_entity_ && !ImGui::GetIO().WantTextInput) {
    Entity entity{selected_entity_, scene().get()};
    nlohmann::json j;
    j["name"] = entity.GetName();
    ComponentSerializerRegistry::SerializeAll(entity, j);
    std::string name = j.value("name", "Entity") + " (Copy)";
    Entity new_entity = scene()->CreateEntity(name);
    ComponentSerializerRegistry::DeserializeAll(new_entity, j, nullptr);
    // Parent to same parent as original
    Entity parent = entity.GetParent();
    if (parent) {
      scene()->LinkEntities(parent.handle(), new_entity);
    }
    selected_entity_ = new_entity.handle();
    has_selected_entity_ = true;
    scroll_to_selected_ = true;
    scene_dirty_ = true;
  }

  // Delete selected entity
  if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) && has_selected_entity_ &&
      !ImGui::GetIO().WantTextInput) {
    Entity entity{selected_entity_, scene().get()};
    scene()->RemoveEntity(entity);
    has_selected_entity_ = false;
    scene_dirty_ = true;
  }
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
      auto proj = Project::Load(dir / (name + ".wiesel"));
      if (proj) {
        active_project_ = std::move(proj);
        Engine::SetGameInfo(std::make_shared<GameInfo>(
            active_project_->GetGameInfo()));
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
        LOG_INFO("Created project: {} at {}", name, folder);
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
    LOG_INFO("Project saved");
  }
}

void EditorLayer::NewScene() {
  if (!project()) {
    return;
  }

  AutoSave();

  Engine::vfs()->CreateDirectory("app://scenes");

  std::string base_name = "new_scene";
  std::string vfs_path = "app://scenes/" + base_name + ".wscene";
  int counter = 1;
  while (Engine::vfs()->FileExists(vfs_path)) {
    vfs_path = "app://scenes/" + base_name + "_" +
               std::to_string(counter++) + ".wscene";
  }

  ClearScene();
  current_scene_path_ = vfs_path;
  SaveScene();
  ScanProjectAssets();
  UpdateWindowTitle();
}

void EditorLayer::OpenScene(const std::string& vfs_path) {
  AutoSave();

  Engine::scene_manager().LoadSceneFromPath(vfs_path);

  current_scene_path_ = vfs_path;
  scene_dirty_ = false;

  // Track last opened scene in project
  if (std::shared_ptr<Project> p = project()) {
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
  Engine::vfs()->CreateDirectory(VirtualFileSystem::Parent(current_scene_path_));

  // Resolve VFS to physical for SaveSceneToFile (needs std::ofstream)
  auto physical = Engine::vfs()->ResolvePhysicalPath(current_scene_path_);
  if (!physical) {
    LOG_ERROR("Cannot resolve scene path: {}", current_scene_path_);
    return;
  }

  if (SaveSceneToFile(scene(), *physical)) {
    scene_dirty_ = false;
    UpdateWindowTitle();
    auto_save_timer_ = 0.0f;

    if (project()) {
      project()->Save();

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
  // Stop playing if active
  if (editor_state_ == EditorState::Playing) {
    editor_state_ = EditorState::Edit;
  }

  // Wait for GPU to finish before destroying resources
  Engine::renderer()->WaitForGPU();

  has_selected_entity_ = false;
  CleanupThumbnailCache();

  // Remove all entities
  auto& hierarchy = scene()->GetSceneHierarchy();
  std::vector<entt::entity> to_remove(hierarchy.begin(), hierarchy.end());
  for (auto entity_id : to_remove) {
    Entity entity{entity_id, scene().get()};
    scene()->RemoveEntity(entity);
  }
  scene()->ProcessDestroyQueue();

  scene()->ResetPhysicsWorld();
  scene()->InvalidateRenderGraphs();
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

  std::unique_ptr<Project> proj = Project::Load(path);
  if (!proj) {
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
  std::optional<std::filesystem::path> app_dir = Engine::vfs()->GetPhysicalPath("app://");
  if (app_dir.has_value()) {
    script_watcher_.SetExtensionFilter(".cs");
    script_watcher_.Watch(*app_dir, true);

    // Watch for UI file hot reload (.rml/.rcss)
    ui_file_watcher_.SetPatternFilter("\\.(rml|rcss)$");
    ui_file_watcher_.Watch(*app_dir, true);
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
    editor_camera_.resources_dirty = true;
  }

  RecentProjects::Add(fs::absolute(path).string());
  UpdateWindowTitle();
#ifdef WIESEL_DISCORD_RPC
  Engine::discord_rpc().SetPresence("Working on " + project->GetSettings().name,
                                    "Editing", "wiesel_logo", "Wiesel Engine");
#endif
  LOG_INFO("Opened project: {}", project->GetSettings().name);
}

void EditorLayer::StartLsp() {
  if (lsp_initialized_ || !active_project_) {
    return;
  }

  std::filesystem::path project_dir = active_project_->GetProjectDirectory();
  std::filesystem::path assets_dir = active_project_->GetAssetsDirectory();

  // Generate a .csproj for the LSP server to discover the project
  DotNetProject lsp_project("App");
  lsp_project.SetOutputPath((project_dir / "App.dll").string());

  // Collect all .cs files from the assets directory
  for (const auto& entry :
       std::filesystem::recursive_directory_iterator(assets_dir)) {
    if (entry.is_regular_file() && entry.path().extension() == ".cs") {
      lsp_project.AddSource(entry.path().string());
    }
  }

  // Reference Core.dll from the working directory (build output)
  std::filesystem::path core_dll = std::filesystem::absolute("Core.dll");
  if (std::filesystem::exists(core_dll)) {
    lsp_project.AddReference(core_dll.string());
  } else {
    LOG_WARN("LSP: Core.dll not found, engine types won't be available");
  }

  lsp_project.Save();
  LOG_INFO("LSP: generated .csproj at {}", lsp_project.GetCsprojPath().string());

  std::string command = "csharp-ls";
  if (lsp_client_.Start(command, project_dir)) {
    lsp_client_.Initialize(project_dir);
    lsp_initialized_ = true;
    lsp_autocomplete_ = std::make_unique<LspAutocompleteProvider>(lsp_client_);
    text_editor_.SetAutocompleteProvider(lsp_autocomplete_.get());
    LOG_INFO("LSP: csharp-ls started");
  } else {
    LOG_WARN("LSP: Failed to start csharp-ls. Intellisense unavailable.");
  }
}

void EditorLayer::StopLsp() {
  if (!lsp_initialized_) {
    return;
  }
  lsp_client_.Stop();
  lsp_initialized_ = false;
}

void EditorLayer::OpenCodeEditor(const std::filesystem::path& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    LOG_ERROR("Failed to open file: {}", path.string());
    return;
  }
  std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());

  // Close previous file in LSP
  if (!code_editor_uri_.empty() && lsp_initialized_) {
    lsp_client_.DidClose(code_editor_uri_);
  }

  code_editor_path_ = path;
  code_editor_uri_ = LspClient::PathToUri(path);
  text_editor_.SetText(content);
  text_editor_.SetFilePath(path.string());

  // Pick language definition based on file extension
  auto ext = path.extension().string();
  if (ext == ".rml") {
    static auto rml_lang = CreateRmlLanguageDefinition();
    text_editor_.SetLanguageDefinition(rml_lang);
  } else if (ext == ".rcss") {
    static auto rcss_lang = CreateRcssLanguageDefinition();
    text_editor_.SetLanguageDefinition(rcss_lang);
  } else {
    static auto csharp_lang = CreateCSharpLanguageDefinition();
    text_editor_.SetLanguageDefinition(csharp_lang);
  }

  text_editor_.SetShowWhitespaces(false);
  code_editor_unsaved_ = false;
  code_editor_open_ = true;

  // Start LSP only for C# files
  if (ext == ".cs") {
    StartLsp();
    semantic_tokens_received_ = false;
    if (lsp_initialized_) {
      lsp_client_.DidOpen(code_editor_uri_, content);
    }
  }
}

void EditorLayer::SaveCodeEditorFile() {
  if (code_editor_path_.empty()) {
    return;
  }
  std::ofstream file(code_editor_path_);
  if (!file.is_open()) {
    LOG_ERROR("Failed to save file: {}", code_editor_path_.string());
    return;
  }
  file << text_editor_.GetText();
  code_editor_unsaved_ = false;

  // Re-request semantic tokens after save
  if (lsp_initialized_) {
    lsp_client_.RequestSemanticTokens(code_editor_uri_);
  }
}

void EditorLayer::RenderCodeEditor() {
  if (!code_editor_open_) {
    return;
  }

  std::string title = code_editor_path_.filename().string();
  if (code_editor_unsaved_) {
    title += " *";
  }
  title += "###CodeEditor";

  ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin(title.c_str(), &code_editor_open_,
                    ImGuiWindowFlags_MenuBar)) {
    ImGui::End();
    return;
  }

  // Menu bar
  if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Save", "Ctrl+S", false, code_editor_unsaved_)) {
        SaveCodeEditorFile();
      }
      ImGui::EndMenu();
    }

    // Right-aligned buttons
    float avail = ImGui::GetContentRegionAvail().x;
    float button_width = ImGui::CalcTextSize("Open in VS Code").x + 20;
    ImGui::SameLine(avail - button_width + ImGui::GetCursorPosX());
    if (ImGui::SmallButton("Open in VS Code")) {
      std::string cmd = "code \"" + code_editor_path_.string() + "\"";
      std::system(cmd.c_str());
    }

    ImGui::EndMenuBar();
  }

  // Status bar
  auto cpos = text_editor_.GetCursorPosition();
  ImGui::Text("Ln %d, Col %d | %s", cpos.mLine + 1, cpos.mColumn + 1,
              code_editor_path_.filename().string().c_str());

  // Request semantic tokens if we haven't received them yet (throttled)
  if (lsp_initialized_ && !semantic_tokens_received_) {
    static float retry_timer = 0.0f;
    retry_timer += ImGui::GetIO().DeltaTime;
    if (retry_timer >= 1.0f) {
      retry_timer = 0.0f;
      lsp_client_.RequestSemanticTokens(code_editor_uri_);
    }
  }

  // Apply LSP semantic tokens as identifier highlights
  if (lsp_initialized_ && lsp_client_.HasSemanticTokens()) {
    semantic_tokens_received_ = true;
    std::vector<LspSemanticToken> tokens = lsp_client_.TakeSemanticTokens();
    const std::vector<std::string>& legend = lsp_client_.GetTokenTypeLegend();
    std::vector<std::string> lines = text_editor_.GetTextLines();

    int added_count = 0;
    for (const LspSemanticToken& token : tokens) {
      if (token.line >= static_cast<int>(lines.size())) {
        continue;
      }
      const std::string& line = lines[token.line];
      if (token.column + token.length > static_cast<int>(line.size())) {
        continue;
      }
      std::string token_text = line.substr(token.column, token.length);

      std::string type_name;
      if (token.token_type < static_cast<int>(legend.size())) {
        type_name = legend[token.token_type];
      }

      if (type_name == "class" || type_name == "struct" ||
          type_name == "interface" || type_name == "enum" ||
          type_name == "type" || type_name == "namespace") {
        text_editor_.AddIdentifier(token_text);
        added_count++;
      }
    }
    if (added_count > 0) {
      text_editor_.InvalidateColorize();
    }
  }

  // Apply LSP diagnostics as error markers
  if (lsp_initialized_ && lsp_client_.HasDiagnostics(code_editor_uri_)) {
    std::vector<LspDiagnostic> diags = lsp_client_.TakeDiagnostics(code_editor_uri_);
    TextEditor::ErrorMarkers markers;
    for (const LspDiagnostic& d : diags) {
      markers[d.line + 1] = d.message;  // TextEditor uses 1-based lines
    }
    text_editor_.SetErrorMarkers(markers);

    // Request semantic tokens now that the server has analyzed the file
    if (!lsp_client_.HasSemanticTokens()) {
      lsp_client_.RequestSemanticTokens(code_editor_uri_);
    }
  }

  // Editor (monospace font)
  if (code_editor_font_) {
    ImGui::PushFont(code_editor_font_);
  }

  bool was_modified = text_editor_.IsTextChanged();

  text_editor_.Render("##editor");

  if (text_editor_.IsTextChanged() && !was_modified) {
    code_editor_unsaved_ = true;
  }

  if (code_editor_font_) {
    ImGui::PopFont();
  }

  code_editor_focused_ =
      ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);

  // Ctrl+S to save within the editor window
  if (code_editor_focused_ && ImGui::GetIO().KeyCtrl &&
      ImGui::IsKeyPressed(ImGuiKey_S, false)) {
    SaveCodeEditorFile();
  }

  ImGui::End();
}

void EditorLayer::RenderLspDebugPanel() {
  if (!panel_lsp_debug_) {
    return;
  }

  ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("LSP Debug", &panel_lsp_debug_)) {
    ImGui::End();
    return;
  }

  ImGui::Text("Status: %s",
              lsp_initialized_ ? (lsp_client_.IsRunning() ? "Running" : "Died")
                               : "Not started");

  ImGui::Separator();

  std::vector<LspClient::LogEntry> log = lsp_client_.GetLog();
  if (ImGui::BeginChild("##lsp_log", ImVec2(0, 0), ImGuiChildFlags_Borders)) {
    for (const LspClient::LogEntry& entry : log) {
      ImVec4 color = entry.outgoing ? ImVec4(0.4f, 0.8f, 0.4f, 1.0f)
                                    : ImVec4(0.5f, 0.7f, 1.0f, 1.0f);
      ImGui::PushStyleColor(ImGuiCol_Text, color);
      ImGui::TextWrapped("%s%s", entry.outgoing ? ">> " : "<< ",
                         entry.summary.c_str());
      ImGui::PopStyleColor();
      ImGui::Separator();
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
      ImGui::SetScrollHereY(1.0f);
    }
  }
  ImGui::EndChild();

  ImGui::End();
}

void EditorLayer::RenderAssetPropertiesPanel() {
  static bool panel_open = true;
  if (!panel_open) {
    return;
  }

  if (ImGui::Begin("Asset Properties", &panel_open)) {
    const AssetMetadata* meta =
        Engine::asset_manager().GetMetadata(properties_asset_handle_);
    if (!meta || !properties_asset_handle_.IsValid()) {
      ImGui::TextDisabled("No asset selected");
      ImGui::End();
      return;
    }
    ImGui::Text("Name: %s", meta->name.c_str());
    ImGui::TextDisabled("Path: %s", meta->virtual_source_path.c_str());
    ImGui::Separator();

    // Large preview for texture/sprite assets
    if (meta->type == AssetType::Texture || meta->type == AssetType::Sprite) {
      ThumbnailEntry thumb =
          GetOrCreateThumbnail(properties_asset_handle_, *meta);
      if (thumb.texture_id) {
        float avail_width = ImGui::GetContentRegionAvail().x;
        float max_preview = std::min(avail_width, 256.0f);
        ImVec2 preview_size = thumb.FitSize(max_preview);
        // Center the preview
        float indent = (avail_width - preview_size.x) * 0.5f;
        if (indent > 0.0f) {
          ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
        }
        ImGui::Image(reinterpret_cast<ImTextureID>(thumb.texture_id),
                     preview_size, thumb.uv0, thumb.uv1);
        if (thumb.width > 0 && thumb.height > 0) {
          uint32_t display_w = static_cast<uint32_t>(
              thumb.width * (thumb.uv1.x - thumb.uv0.x));
          uint32_t display_h = static_cast<uint32_t>(
              thumb.height * (thumb.uv1.y - thumb.uv0.y));
          ImGui::TextDisabled("%u x %u", display_w, display_h);
        }
        ImGui::Separator();
      }
    }

    const AssetPropertyDesc* desc = AssetPropertyRegistry::Get(meta->type);
    if (desc && meta->properties) {
      bool changed = desc->RenderImGui(meta->properties.get());
      if (changed) {
        // Write properties back to .meta file
        std::optional<std::filesystem::path> physical =
            Engine::vfs()->GetPhysicalPath(meta->virtual_source_path);
        if (physical.has_value()) {
          std::filesystem::path meta_path = physical->string() + ".meta";
          GameLoader::WriteMetaFile(meta_path, meta->handle, meta->type,
                                       meta->properties.get());
        }
      }
    }

    ImGui::Separator();
    if (ImGui::Button("Reimport")) {
      // Wait for GPU so no in-flight frames reference the old resources
      Engine::renderer()->WaitForGPU();

      if (meta->type == AssetType::Font) {
        FontCache::Invalidate(properties_asset_handle_);
        std::shared_ptr<Scene> s = scene();
        if (s) {
          for (entt::entity e : s->GetAllEntitiesWith<TextComponent>()) {
            auto& tc = s->GetComponent<TextComponent>(e);
            if (tc.font_handle == properties_asset_handle_) {
              tc.gpu_dirty_ = true;
              tc.glyph_gpu_.clear();
            }
          }
        }
      }

      Engine::asset_manager().Unload(properties_asset_handle_);
      // Use sync load since we already waited for GPU
      Engine::asset_manager().LoadSync(properties_asset_handle_);
      // Rebuild render graphs with new resources
      scene()->InvalidateRenderGraphs();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Reload this asset with the current properties.");
    }
  }
  ImGui::End();
}

void EditorLayer::ScanProjectAssets() {
  std::shared_ptr<Wiesel::Project> project = active_project_;
  if (project) {
    ProjectLoader::ScanAssets(*project);
    project->Save();

    thumbnail_cache_instance_.RemoveStale();

    // Clear stale skybox reference if its asset was deleted
    AssetHandle sky = scene()->GetSkyboxAsset();
    if (sky.IsValid() && !Engine::asset_manager().HasAsset(sky)) {
      scene()->SetSkyboxAsset({});
    }

    // Recompile scripts if any .cs files changed
    Engine::script_manager().ReloadAsync();
  }
}

void EditorLayer::RenderStartupDialog() {
  ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImVec2 center = ImVec2(viewport->Pos.x + viewport->Size.x * 0.5f,
                         viewport->Pos.y + viewport->Size.y * 0.5f);

  ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(420, 0));
  ImGui::Begin("Welcome to Wiesel", nullptr,
               ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking);

  ImGui::Text("Get started:");
  ImGui::Spacing();

  float width = ImGui::GetContentRegionAvail().x;
  if (ImGui::Button("New Project...", ImVec2(width, 30))) {
    NewProject();
  }
  if (ImGui::Button("Open Project...", ImVec2(width, 30))) {
    OpenProject();
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  std::vector<std::string> recent = RecentProjects::Load();
  if (!recent.empty()) {
    ImGui::Text("Recent Projects:");
    ImGui::Spacing();
    for (const std::string& path : recent) {
      namespace fs = std::filesystem;
      std::string name = fs::path(path).stem().string();
      std::string dir = fs::path(path).parent_path().string();
      std::string label = name + "  (" + dir + ")";
      if (ImGui::Selectable(label.c_str())) {
        if (fs::exists(path)) {
          deferred_action_ = DeferredAction::OpenProject;
          deferred_path_ = path;
        }
      }
    }
  } else {
    ImGui::TextDisabled("No recent projects.");
  }

  ImGui::End();
}

void EditorLayer::OpenPrefabForEditing(const std::string& vfs_path) {
  namespace fs = std::filesystem;

  // Save current scene state
  AutoSave();
  prefab_return_scene_path_ = current_scene_path_;

  // Clear scene and load prefab as a temporary scene
  ClearScene();
  Entity root = Prefab::InstantiateFromFile(scene(), vfs_path);
  if (root.handle() == entt::null) {
    LOG_ERROR("Failed to open prefab for editing: {}", vfs_path);
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
  scene()->InvalidateRenderGraphs();

  // Setup camera components
  for (entt::entity entity : scene()->GetAllEntitiesWith<CameraComponent>()) {
    auto& cam = scene()->GetComponent<CameraComponent>(entity);
    Engine::renderer()->SetupCameraComponent(cam);
  }

  LOG_INFO("Editing prefab: {}", vfs_path);
}

void EditorLayer::SavePrefab() {
  if (!editing_prefab_ || editing_prefab_path_.empty()) {
    return;
  }

  // Find the root entity (first in hierarchy - the prefab root)
  std::vector<entt::entity>& hierarchy = scene()->GetSceneHierarchy();
  if (hierarchy.empty()) {
    LOG_ERROR("Cannot save prefab: scene is empty");
    return;
  }

  Entity root = {hierarchy[0], scene().get()};
  if (Prefab::SaveToFile(root, editing_prefab_path_)) {
    scene_dirty_ = false;
    LOG_INFO("Prefab saved: {}", editing_prefab_path_);
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

  LOG_INFO("Exporting game to: {}", export_dir.string());

  // Copy gameinfo.wgame
  fs::path src_gameinfo = active_project_->GetGameInfoPath();
  if (fs::exists(src_gameinfo)) {
    fs::copy_file(src_gameinfo, export_dir / "gameinfo.wgame",
                  fs::copy_options::overwrite_existing);
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

      Wpak::Status status =
          Wpak::WriteArchive(export_dir / "assets.pak", filtered);
      if (!status.success) {
        LOG_ERROR("Export: Failed to pack app assets: {}",
                  status.error.message);
        return;
      }
      LOG_INFO("Export: Packed {} assets ({} excluded)", filtered.size(),
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
        LOG_ERROR("Export: Core compilation failed:\n{}", result.output);
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
        LOG_ERROR("Export: App compilation failed:\n{}", result.output);
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
        Wpak::Status status =
            Wpak::WriteArchive(export_dir / "engine.pak", files.value);
        if (!status.success) {
          LOG_ERROR("Export: Failed to pack engine assets: {}",
                    status.error.message);
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
      LOG_INFO("Export: Copied runtime as {}", dst_name.filename().string());
    } else {
      LOG_WARN("Export: wiesel-runtime not found. Build the 'wiesel-runtime' "
               "target first.");
    }
  }

  LOG_INFO("Export complete: {}", export_dir.string());

  // Open the export directory
  OpenFileInDefaultEditor(export_dir);
}
}  // namespace Wiesel::Editor
