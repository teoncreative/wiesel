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

#include "w_editor.hpp"
#include <unordered_set>
#include "util/w_discord_rpc.hpp"

// clang-format off
// Import order important
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>
#include <ImGuizmo.h>
#include <misc/cpp/imgui_stdlib.h>
// clang-format off

#include "util/w_tracy.hpp"

#include "asset/w_asset_manager.hpp"
#include "asset/w_asset_property_registry.hpp"
#include "imgui_internal.h"
#include "input/w_input.hpp"
#include "layer/w_layerscene.hpp"
#include "physics/w_collider.hpp"
#include "physics/w_physics_world.hpp"
#include "w_project_loader.hpp"
#include "asset/w_asset_utils.hpp"
#include "rendering/w_material.hpp"
#include "rendering/w_sprite.hpp"
#include "rendering/w_sprite_asset.hpp"
#include "../include/w_thumbnail_cache.hpp"
#include "rendering/w_texture.hpp"
#include "scene/w_component_serializer.hpp"
#include "../include/w_editor_components.hpp"
#include "scene/w_prefab.hpp"
#include "scene/w_scene_manager.hpp"
#include "scene/w_scene_serializer.hpp"
#include "script/w_scriptmanager.hpp"
#include "ui/w_font.hpp"
#include "util/imgui/w_imguiutil.hpp"
#include "util/w_dialogs.hpp"
#include "util/w_filewatcher.hpp"
#include "util/w_gamepadcodes.hpp"
#include "util/w_natural_sort.hpp"
#include "util/w_platform.hpp"
#include "w_engine.hpp"

namespace Wiesel::Editor {

std::shared_ptr<Wiesel::Scene> scene() {
  return Wiesel::Engine::scene_manager().GetActiveScene();
}

static std::shared_ptr<Wiesel::Project> active_project_;

std::shared_ptr<Wiesel::Project> project() {
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
  } catch (...) {}
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
static std::unordered_set<entt::entity> open_ancestors_;
static std::string entity_clipboard_;
static ImGuizmo::OPERATION current_op_ = ImGuizmo::TRANSLATE;

// Panel visibility (toggled via Window menu)
static bool panel_scene_hierarchy_ = true;
static bool panel_components_ = true;
static bool panel_asset_browser_ = true;
static bool panel_console_ = true;
static bool panel_stats_ = true;
static bool panel_scene_view_ = true;
static bool panel_game_view_ = true;
static bool panel_scene_properties_ = true;
static bool layout_initialized_ = false;

static struct SceneHierarchyData {
  entt::entity move_from = entt::null;
  entt::entity move_to = entt::null;
  bool bottom_part = false;
} hierarchy_data_;

static FileWatcher script_watcher_;
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
      } catch (...) {}
    }
  }

  std::ofstream file(path);
  if (!file.is_open()) {
    return false;
  }
  file << root.dump(2);
  return true;
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
    auto logo = Engine::vfs()->Open("/engine/textures/logo.png");
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
  editor_camera_transform_.SetRotation(
      glm::vec3(editor_pitch_, editor_yaw_, 0.0f));
  editor_yaw_ =
      180.0f;  // facing +Z (quat look = -sin(y),-cos(y) so 180 gives +Z)
  editor_pitch_ = -15.0f;  // slightly looking down

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
}

void EditorLayer::OnDetach() {
  LOG_DEBUG("OnDetach");
  CleanupThumbnailCache();
  editor_camera_.resource_pool.Clear();
  editor_camera_.render_pipeline = nullptr;
  Engine::scene_manager().Cleanup();
}

void EditorLayer::ProcessDeferredActions() {
  if (deferred_action_ == DeferredAction::None) {
    return;
  }

  auto action = deferred_action_;
  auto path = std::move(deferred_path_);
  deferred_action_ = DeferredAction::None;
  deferred_path_.clear();

  switch (action) {
    case DeferredAction::OpenScene:
      OpenSceneFromPath(path);
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
      RestoreSnapshot();
      ImGui::SetWindowFocus("Scene");
      break;
    default:
      break;
  }
}

void EditorLayer::OnUpdate(float_t delta_time) {
  PROFILE_ZONE_SCOPED_N("Editor::OnUpdate");

  // Process deferred scene switches (safe point between frames)
  ProcessDeferredActions();

#ifdef WIESEL_DISCORD_RPC
  Engine::discord_rpc().RunCallbacks();
#endif

  // Poll async script compilation
  Engine::script_manager().FinishReloadIfReady();

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
  if (script_reload_pending_ && window_focused_) {
    script_reload_pending_ = false;
    LOG_INFO("Script changes detected, reloading...");
    Engine::script_manager().ReloadAsync();
  }
}

bool EditorLayer::OnMouseMoved(MouseMovedEvent& event) {
  if (event.GetCursorMode() == CursorModeRelative) {
    if (editor_camera_mode_ == EditorCameraMode::Mode2D) {
      // 2D mode: right-click drag = pan
      float pan_speed = editor_2d_zoom_ * 0.003f;
      editor_camera_transform_.Move(event.GetDeltaX() * pan_speed, 0, 0);
      editor_camera_transform_.Move(0, -event.GetDeltaX() * pan_speed, 0);
      editor_camera_transform_.MarkChanged();
    } else {
      // Free mode: right-click drag = look
      editor_yaw_ -= event.GetDeltaX() * mouse_sensitivity_;
      editor_pitch_ -= event.GetDeltaY() * mouse_sensitivity_;
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
        type == PipelineRecreatedEvent::GetStaticType()) {
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

  // Accept asset drops onto entities in the hierarchy
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload* payload =
            ImGui::AcceptDragDropPayload("AssetHandle")) {
      AssetHandle dropped = *static_cast<const AssetHandle*>(payload->Data);
      const AssetMetadata* meta = Engine::asset_manager().GetMetadata(dropped);
      if (meta) {
        if (meta->type == AssetType::Model) {
          if (!entity.HasComponent<ModelComponent>()) {
            entity.AddComponent<ModelComponent>();
          }
          entity.GetComponent<ModelComponent>().model_handle = dropped;
          scene_dirty_ = true;
        }
      }
    }
    ImGui::EndDragDropTarget();
  }

  // Context menu
  if (ImGui::BeginPopupContextItem()) {
    selected_entity_ = entity_id;
    has_selected_entity_ = true;
    if (ImGui::BeginMenu("Add Child")) {
      entt::entity parent_id = entity_id;
      if (ImGui::MenuItem("Empty Entity")) {
        app_.SubmitToMainThread([this, parent_id]() {
          Entity child = scene()->CreateEntity();
          scene()->LinkEntities(parent_id, child);
          scene_dirty_ = true;
        });
      }
      if (ImGui::BeginMenu("3D Shape")) {
        const char* shapes[] = {"Cube", "Sphere", "Plane", "Cylinder",
                                "Capsule"};
        for (const char* shape : shapes) {
          if (ImGui::MenuItem(shape)) {
            std::string shape_name = shape;
            app_.SubmitToMainThread([this, parent_id, shape_name]() {
              Entity child = scene()->CreateEntity(shape_name);
              auto& mc = child.AddComponent<ModelComponent>();
              mc.model_handle = Engine::GetPrimitive(shape_name);
              scene()->LinkEntities(parent_id, child);
              scene_dirty_ = true;
            });
          }
        }
        ImGui::EndMenu();
      }
      ImGui::EndMenu();
    }
    if (ImGui::MenuItem("Save as Prefab...")) {
      Dialogs::SaveFileDialog({{"Wiesel Prefab", "wprefab"}},
                              [this, entity_id](const std::string& file) {
                                if (file.empty()) {
                                  return;
                                }
                                std::filesystem::path path(file);
                                if (path.extension() != ".wprefab") {
                                  path += ".wprefab";
                                }
                                Entity ent{entity_id, scene().get()};
                                Prefab::SaveToFile(ent, path);
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
  Renderer* renderer = Engine::renderer().get();

  RenderMainMenuBar();

  // Show startup dialog if no project loaded
  if (!project()) {
    ImGui::DockSpaceOverViewport();
    RenderStartupDialog();
    return;
  }

  ImGuiID dockspace_id = ImGui::DockSpaceOverViewport();

  // Build initial layout once
  if (!layout_initialized_) {
    layout_initialized_ = true;

    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

    // Split: left panel (20%) | center+right remainder
    ImGuiID dock_left, dock_remainder;
    ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, kLeftPanelRatio,
                                &dock_left, &dock_remainder);

    // Split left into top (hierarchy) and bottom (components)
    ImGuiID dock_left_top, dock_left_bottom;
    ImGui::DockBuilderSplitNode(dock_left, ImGuiDir_Up, kHierarchySplitRatio,
                                &dock_left_top, &dock_left_bottom);

    // Split remainder: bottom (asset browser) | center+right
    ImGuiID dock_bottom, dock_center_right;
    ImGui::DockBuilderSplitNode(dock_remainder, ImGuiDir_Down,
                                kAssetBrowserRatio, &dock_bottom,
                                &dock_center_right);

    // Split center_right: right panel (scene props) | center (viewport)
    ImGuiID dock_right, dock_center;
    ImGui::DockBuilderSplitNode(dock_center_right, ImGuiDir_Right,
                                kRightPanelRatio, &dock_right, &dock_center);

    // Dock windows
    ImGui::DockBuilderDockWindow("Scene Hierarchy", dock_left_top);
    ImGui::DockBuilderDockWindow("Components", dock_left_bottom);
    ImGui::DockBuilderDockWindow("Game", dock_center);
    ImGui::DockBuilderDockWindow("Scene", dock_center);
    // Select Scene tab by default
    ImGuiID scene_window_id = ImHashStr("Scene");
    ImGui::DockBuilderGetNode(dock_center)->SelectedTabId = scene_window_id;
    ImGui::DockBuilderDockWindow("Scene Properties", dock_right);
    ImGui::DockBuilderDockWindow("Asset Properties", dock_right);
    ImGui::DockBuilderDockWindow("Asset Browser", dock_bottom);
    ImGui::DockBuilderDockWindow("Developer Console", dock_bottom);
    ImGui::DockBuilderDockWindow("Render Stats", dock_right);

    ImGui::DockBuilderFinish(dockspace_id);
  }

  bool& scene_properties_open = panel_scene_properties_;
  if (scene_properties_open) {
    if (ImGui::Begin("Scene Properties", &scene_properties_open)) {
      auto& settings = renderer->options();

      ImGui::SeparatorText("Debug");
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
      ImGui::Checkbox(PrefixLabel("Colliders").c_str(),
                      &settings.show_colliders);
      ImGui::Checkbox(PrefixLabel("Triggers").c_str(), &settings.show_triggers);
      ImGui::Checkbox(PrefixLabel("Reverb Zones").c_str(),
                      &settings.show_reverb_zones);
      ImGui::Checkbox(PrefixLabel("Cameras").c_str(), &settings.show_cameras);

      ImGui::SeparatorText("Shadow Cascades");
      auto cam = renderer->GetCameraData();
      if (cam) {
        ImGui::Text("Shadows: %s", cam->does_shadow_pass ? "ON" : "OFF");
        for (int i = 0; i < WIESEL_SHADOW_CASCADE_COUNT; i++) {
          ImGui::Text("Cascade %d: split Z = %.2f", i,
                      cam->shadow_map_cascades[i].SplitDepth);
        }
      }

      ImGui::Separator();
      if (ImGui::Button("Reload Scripts")) {
        Engine::script_manager().ReloadAsync();
      }
      if (ImGui::Button("Recreate Pipeline")) {
        renderer->SetRecreatePipeline(true);
      }
    }
    ImGui::End();
  }

  RenderProjectSettingsPopup();
  RenderAssetPropertiesPanel();
  RenderCreateSkyboxPopup();
  RenderCreateSpritePopup();
  RenderSliceSpritesPopup();
  RenderCreateSpriteSheetPopup();
  RenderCreateSpriteAnimPopup();

  bool& scene_open = panel_scene_hierarchy_;
  if (scene_open) {
    if (ImGui::Begin("Scene Hierarchy", &scene_open)) {
      bool ignoreMenu = false;

      // Prefab editing banner
      if (editing_prefab_) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              ImVec4(0.3f, 0.6f, 0.9f, 1.0f));
        float width = ImGui::GetContentRegionAvail().x;
        ImGui::Text("Editing: %s",
                    editing_prefab_path_.filename().string().c_str());
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

      // When scrolling to selected, build set of ancestors to force-open
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

      // Search bar
      ImGui::SetNextItemWidth(-1);
      ImGui::InputTextWithHint("##HierarchySearch", "Search entities...",
                               hierarchy_search_, sizeof(hierarchy_search_));

      // Scene root node (always open, not collapsible)
      ImGuiTreeNodeFlags scene_flags =
          ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow |
          ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding |
          ImGuiTreeNodeFlags_Framed;
      std::string scene_label = "Scene";
      if (!current_scene_path_.empty()) {
        scene_label = current_scene_path_.stem().string();
      }
      bool scene_open = ImGui::TreeNodeEx("##SceneRoot", scene_flags, "%s",
                                          scene_label.c_str());

      // Right-click on scene root to add entities
      if (ImGui::BeginPopupContextItem("scene_root_context")) {
        if (ImGui::BeginMenu("Add")) {
          if (ImGui::MenuItem("Empty Entity")) {
            scene()->CreateEntity();
            scene_dirty_ = true;
          }
          if (ImGui::BeginMenu("3D Shape")) {
            const char* shapes[] = {"Cube", "Sphere", "Plane", "Cylinder",
                                    "Capsule"};
            for (const char* shape : shapes) {
              if (ImGui::MenuItem(shape)) {
                Entity e = scene()->CreateEntity(shape);
                auto& mc = e.AddComponent<ModelComponent>();
                mc.model_handle = Engine::GetPrimitive(shape);
                scene_dirty_ = true;
              }
            }
            ImGui::EndMenu();
          }
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

      // Invisible drop zone covering remaining empty space to unparent entities
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

        // Click empty space to deselect
        if (ImGui::IsItemClicked(0)) {
          has_selected_entity_ = false;
        }

        // Right-click on empty space
        if (ImGui::IsItemClicked(1)) {
          ImGui::OpenPopup("right_click_hierarchy");
        }
      } else {
        // No remaining space - still handle right-click and deselect
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
          if (ImGui::MenuItem("Empty Entity")) {
            scene()->CreateEntity();
            scene_dirty_ = true;
            ImGui::CloseCurrentPopup();
          }
          if (ImGui::BeginMenu("3D Shape")) {
            const char* shapes[] = {"Cube", "Sphere", "Plane", "Cylinder",
                                    "Capsule"};
            for (const char* shape : shapes) {
              if (ImGui::MenuItem(shape)) {
                Entity e = scene()->CreateEntity(shape);
                auto& mc = e.AddComponent<ModelComponent>();
                mc.model_handle = Engine::GetPrimitive(shape);
                scene_dirty_ = true;
                ImGui::CloseCurrentPopup();
              }
            }
            ImGui::EndMenu();
          }
          ImGui::EndMenu();
        }
        ImGui::EndPopup();
      }
    }
    ImGui::End();
  }

  bool& components_open = panel_components_;
  if (components_open) {
    if (ImGui::Begin("Components", &components_open) && has_selected_entity_) {
      Entity entity = {selected_entity_, scene().get()};
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
    ImGui::End();
  }

  bool& asset_browser_open = panel_asset_browser_;
  if (asset_browser_open) {
    if (ImGui::Begin("Asset Browser", &asset_browser_open)) {
      auto& mgr = Engine::asset_manager();

      static std::string
          current_dir;  // relative to assets dir, e.g. "" or "models/" or "scenes/"
      browser_current_dir_ = current_dir;
      static std::string selected_file;
      static float tile_size = 80.0f;

      // Scan the physical filesystem for the current directory
      struct FileEntry {
        std::string name;
        bool is_dir;
        std::filesystem::path physical_path;
        AssetType asset_type;
      };

      std::vector<FileEntry> entries;

      auto physical_app = Engine::vfs()->GetPhysicalPath("/app");
      if (physical_app.has_value()) {
        namespace fs = std::filesystem;
        fs::path browse_dir = fs::absolute(*physical_app) / current_dir;
        if (fs::exists(browse_dir) && fs::is_directory(browse_dir)) {
          for (auto& entry : fs::directory_iterator(browse_dir)) {
            // Hide .meta files from the browser
            if (entry.is_regular_file() &&
                entry.path().extension() == ".meta") {
              continue;
            }

            FileEntry fe;
            fe.name = entry.path().filename().string();
            fe.is_dir = entry.is_directory();
            fe.physical_path = entry.path();
            fe.asset_type = AssetType::None;

            if (!fe.is_dir) {
              auto ext = entry.path().extension().string();
              fe.asset_type = ExtToAssetType(ext);
              if (fe.asset_type == AssetType::None) {
                if (ext == ".cs") {
                  fe.asset_type = AssetType::Script;
                }
              }
            }

            entries.push_back(fe);
          }
        }
        // Sort: directories first, then files, both alphabetically
        std::ranges::sort(entries, [](const FileEntry& a, const FileEntry& b) {
          if (a.is_dir != b.is_dir) {
            return a.is_dir > b.is_dir;
          }
          return NaturalLess(a.name, b.name);
        });
      }

      // Breadcrumb bar
      {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        if (ImGui::Button("Assets")) {
          current_dir.clear();
        }

        if (!current_dir.empty()) {
          // Split current_dir into parts
          std::string accumulated;
          std::string remaining = current_dir;
          while (!remaining.empty()) {
            auto slash = remaining.find_first_of("/\\");
            std::string part;
            if (slash != std::string::npos) {
              part = remaining.substr(0, slash);
              remaining = remaining.substr(slash + 1);
            } else {
              part = remaining;
              remaining.clear();
            }
            if (part.empty()) {
              continue;
            }
            accumulated += part + "/";

            ImGui::SameLine(0, 2);
            ImGui::TextUnformatted("/");
            ImGui::SameLine(0, 2);

            std::string btn_id = part + "##bc_" + accumulated;
            if (ImGui::Button(btn_id.c_str())) {
              current_dir = accumulated;
            }
          }
        }
        ImGui::PopStyleColor();
      }

      // Import button
      ImGui::SameLine();
      if (ImGui::Button("+ Import")) {
        ImGui::OpenPopup("ImportAssetPopup");
      }
      ImGui::SameLine();
      if (ImGui::Button("+ Folder")) {
        ImGui::OpenPopup("NewFolderPopup");
      }

      // New folder popup
      static char new_folder_name[128] = "";
      if (ImGui::BeginPopup("NewFolderPopup")) {
        ImGui::Text("Folder name:");
        ImGui::InputText("##foldername", new_folder_name,
                         sizeof(new_folder_name));
        if (ImGui::Button("Create") && new_folder_name[0] != '\0') {
          namespace fs = std::filesystem;
          auto physical_app = Engine::vfs()->GetPhysicalPath("/app");
          if (physical_app.has_value()) {
            fs::path base = fs::absolute(*physical_app);
            if (!current_dir.empty()) {
              std::string rel = current_dir;
              if (rel.rfind("/app/", 0) == 0) {
                rel = rel.substr(5);
              } else if (rel.rfind("/", 0) == 0) {
                rel = rel.substr(1);
              }
              base = base / rel;
            }
            fs::create_directories(base / new_folder_name);
            new_folder_name[0] = '\0';
          }
          ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
          new_folder_name[0] = '\0';
          ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
      }

      // Create skybox popup

      // Import file into the current asset browser directory
      auto ImportFileToCurrentDir = [](const std::string& file,
                                       AssetType type) {
        namespace fs = std::filesystem;
        if (file.empty()) {
          return;
        }

        fs::path abs = fs::absolute(file);
        auto physical_app = Engine::vfs()->GetPhysicalPath("/app");
        if (!physical_app.has_value()) {
          LOG_ERROR("No /app mount point – open a project first");
          return;
        }
        fs::path app_assets = fs::absolute(*physical_app);

        // Determine destination directory from current browser path
        fs::path dest_dir = app_assets;
        if (!current_dir.empty()) {
          // current_dir is like "/app/models/" - strip "/app/" prefix
          std::string rel = current_dir;
          if (rel.rfind("/app/", 0) == 0) {
            rel = rel.substr(5);
          } else if (rel.rfind("/", 0) == 0) {
            rel = rel.substr(1);
          }
          dest_dir = app_assets / rel;
        }

        std::error_code ec;
        fs::create_directories(dest_dir, ec);

        // For model files, copy all sibling files from the source directory
        // (glTF needs .bin + textures, FBX may have companion files)
        if (type == AssetType::Model) {
          fs::path source_dir = abs.parent_path();
          fs::path model_dest_dir = dest_dir / abs.stem();
          fs::create_directories(model_dest_dir, ec);
          // Copy all files from the source directory
          for (const auto& entry : fs::directory_iterator(source_dir)) {
            if (!entry.is_regular_file()) {
              continue;
            }
            fs::path file_dest = model_dest_dir / entry.path().filename();
            fs::copy_file(entry.path(), file_dest,
                          fs::copy_options::skip_existing, ec);
            if (ec) {
              LOG_WARN("Failed to copy '{}': {}", entry.path().string(),
                       ec.message());
              ec.clear();
            }
          }
          // Also copy subdirectories (some models have texture subfolders)
          for (const auto& entry :
               fs::recursive_directory_iterator(source_dir)) {
            if (entry.is_directory()) {
              continue;
            }
            auto rel_to_source = fs::relative(entry.path(), source_dir);
            fs::path file_dest = model_dest_dir / rel_to_source;
            fs::create_directories(file_dest.parent_path(), ec);
            fs::copy_file(entry.path(), file_dest,
                          fs::copy_options::skip_existing, ec);
            ec.clear();
          }
          // Register the main model file
          auto vfs_rel =
              fs::relative(model_dest_dir / abs.filename(), app_assets);
          std::string vfs_path = "/app/" + vfs_rel.generic_string();
          std::string name = abs.stem().string();
          Engine::asset_manager().Register(name, type, vfs_path);
          LOG_INFO("Imported model directory {} to {}", name, vfs_path);
        } else {
          fs::path dest = dest_dir / abs.filename();
          fs::copy_file(abs, dest, fs::copy_options::skip_existing, ec);
          if (ec) {
            LOG_ERROR("Failed to import '{}' to '{}': {}", file, dest.string(),
                      ec.message());
            return;
          }
          auto vfs_rel = fs::relative(dest, app_assets);
          std::string vfs_path = "/app/" + vfs_rel.generic_string();
          std::string name = abs.stem().string();
          Engine::asset_manager().Register(name, type, vfs_path);
          LOG_INFO("Imported {} to {}", name, vfs_path);
        }
      };

      if (ImGui::BeginPopup("ImportAssetPopup")) {
        if (ImGui::MenuItem("Model...")) {
          Dialogs::OpenFileDialog(
              {{"Model file", "obj,gltf,glb,fbx"}},
              [ImportFileToCurrentDir](const std::string& file) {
                ImportFileToCurrentDir(file, AssetType::Model);
              });
        }
        if (ImGui::MenuItem("Texture...")) {
          Dialogs::OpenFileDialog(
              {{"Image file", "png,jpg,jpeg,tga,bmp"}},
              [ImportFileToCurrentDir](const std::string& file) {
                ImportFileToCurrentDir(file, AssetType::Texture);
              });
        }
        ImGui::EndPopup();
      }

      // Tile size slider
      ImGui::SameLine(ImGui::GetContentRegionAvail().x - 100);
      ImGui::SetNextItemWidth(100);
      ImGui::SliderFloat("##tilesize", &tile_size, 48.0f, 128.0f, "");

      ImGui::Separator();

      static bool open_script_popup = false;

      // Content area
      if (ImGui::BeginChild("asset_content", ImVec2(0, 0),
                            ImGuiChildFlags_None)) {
        float panel_width = ImGui::GetContentRegionAvail().x;
        float cell_size = tile_size + 8.0f;
        int columns = std::max(1, (int)(panel_width / cell_size));
        int col = 0;

        auto DrawTile = [&](const char* label, ImVec4 icon_color,
                            const char* type_abbrev, bool is_selected,
                            bool is_folder,
                            const ThumbnailEntry* thumbnail = nullptr,
                            const AssetMetadata* asset_meta = nullptr,
                            bool* double_clicked = nullptr) -> bool {
          bool clicked = false;
          ImGui::PushID(label);

          ImVec2 cursor = ImGui::GetCursorScreenPos();
          ImVec2 icon_min = cursor;
          ImVec2 icon_max = ImVec2(cursor.x + tile_size, cursor.y + tile_size);

          // Invisible button for interaction
          if (ImGui::InvisibleButton("##tile",
                                     ImVec2(tile_size, tile_size + 20))) {
            clicked = true;
          }
          if (double_clicked && ImGui::IsItemHovered() &&
              ImGui::IsMouseDoubleClicked(0)) {
            *double_clicked = true;
          }
          bool hovered = ImGui::IsItemHovered();

          ImDrawList* dl = ImGui::GetWindowDrawList();

          // Selection/hover highlight
          if (is_selected) {
            dl->AddRectFilled(ImVec2(icon_min.x - 2, icon_min.y - 2),
                              ImVec2(icon_max.x + 2, icon_max.y + 22),
                              IM_COL32(60, 100, 160, 180), 4.0f);
          } else if (hovered) {
            dl->AddRectFilled(ImVec2(icon_min.x - 2, icon_min.y - 2),
                              ImVec2(icon_max.x + 2, icon_max.y + 22),
                              IM_COL32(70, 70, 70, 120), 4.0f);
          }

          // Icon: thumbnail image or colored rectangle
          if (thumbnail && thumbnail->texture_id) {
            ImVec2 img_size = thumbnail->FitSize(tile_size);
            float ox = (tile_size - img_size.x) * 0.5f;
            float oy = (tile_size - img_size.y) * 0.5f;
            ImVec2 img_min(icon_min.x + ox, icon_min.y + oy);
            ImVec2 img_max(img_min.x + img_size.x, img_min.y + img_size.y);
            dl->AddImageRounded(
                reinterpret_cast<ImTextureID>(thumbnail->texture_id), img_min,
                img_max, thumbnail->uv0, thumbnail->uv1, IM_COL32_WHITE,
                6.0f);
          } else {
            ImU32 col32 = ImGui::ColorConvertFloat4ToU32(icon_color);
            dl->AddRectFilled(icon_min, icon_max, col32,
                              is_folder ? 2.0f : 6.0f);

            // Type abbreviation centered in icon
            if (type_abbrev && type_abbrev[0]) {
              ImVec2 text_sz = ImGui::CalcTextSize(type_abbrev);
              ImVec2 text_pos(icon_min.x + (tile_size - text_sz.x) * 0.5f,
                              icon_min.y + (tile_size - text_sz.y) * 0.5f);
              dl->AddText(text_pos, IM_COL32(255, 255, 255, 200), type_abbrev);
            }
          }

          // Label below icon (truncated)
          float max_text_w = tile_size;
          ImVec2 label_pos(icon_min.x, icon_max.y + 2);
          std::string display_label = label;
          ImVec2 label_sz = ImGui::CalcTextSize(display_label.c_str());
          if (label_sz.x > max_text_w) {
            while (display_label.size() > 3) {
              display_label.pop_back();
              std::string truncated = display_label + "..";
              if (ImGui::CalcTextSize(truncated.c_str()).x <= max_text_w) {
                display_label = truncated;
                break;
              }
            }
          }
          dl->AddText(label_pos, IM_COL32(220, 220, 220, 255),
                      display_label.c_str());

          // Tooltip on hover
          if (hovered) {
            if (asset_meta) {
              ImGui::BeginTooltip();
              ImGui::TextUnformatted(asset_meta->name.c_str());
              ImGui::Separator();
              ImGui::Text("Type: %s", AssetTypeToString(asset_meta->type));
              ImGui::Text("Handle: %s", asset_meta->handle.ToString().c_str());
              ImGui::Text("Path: %s", asset_meta->virtual_source_path.c_str());

              AssetLoadState load_state = asset_meta->load_state.load();
              const char* state_str = "Unknown";
              switch (load_state) {
                case AssetLoadState::Unloaded:
                  state_str = "Unloaded";
                  break;
                case AssetLoadState::Loading:
                  state_str = "Loading";
                  break;
                case AssetLoadState::Loaded:
                  state_str = "Loaded";
                  break;
                case AssetLoadState::Failed:
                  state_str = "Failed";
                  break;
              }
              ImGui::Text("State: %s", state_str);

              auto physical_path = Engine::vfs()->GetPhysicalPath(
                  asset_meta->virtual_source_path);
              ImGui::Text("Source: %s",
                          physical_path.has_value() ? "Filesystem" : "Archive");

              ImGui::EndTooltip();
            } else {
              ImGui::SetTooltip("%s", label);
            }
          }

          ImGui::PopID();
          return clicked;
        };

        auto NextColumn = [&]() {
          col++;
          if (col < columns) {
            ImGui::SameLine(0, 8.0f);
          } else {
            col = 0;
          }
        };

        // Asset type colors
        auto GetAssetColor = [](AssetType type) -> ImVec4 {
          switch (type) {
            case AssetType::Texture:
              return {0.25f, 0.45f, 0.72f, 1.0f};
            case AssetType::Model:
              return {0.30f, 0.62f, 0.35f, 1.0f};
            case AssetType::Material:
              return {0.72f, 0.50f, 0.20f, 1.0f};
            case AssetType::Shader:
              return {0.55f, 0.30f, 0.68f, 1.0f};
            case AssetType::Sprite:
              return {0.20f, 0.60f, 0.65f, 1.0f};
            case AssetType::Skybox:
              return {0.25f, 0.55f, 0.55f, 1.0f};
            case AssetType::Font:
              return {0.65f, 0.60f, 0.25f, 1.0f};
            case AssetType::Script:
              return {0.55f, 0.70f, 0.30f, 1.0f};
            case AssetType::Scene:
              return {0.72f, 0.35f, 0.35f, 1.0f};
            case AssetType::Prefab:
              return {0.45f, 0.55f, 0.72f, 1.0f};
            case AssetType::Audio:
              return {0.72f, 0.45f, 0.60f, 1.0f};
            case AssetType::SpriteSheet:
              return {0.20f, 0.65f, 0.55f, 1.0f};
            case AssetType::SpriteAnim:
              return {0.30f, 0.70f, 0.45f, 1.0f};
            default:
              return {0.40f, 0.40f, 0.40f, 1.0f};
          }
        };

        auto GetAssetAbbrev = [](AssetType type) -> const char* {
          switch (type) {
            case AssetType::Texture:
              return "TEX";
            case AssetType::Model:
              return "MDL";
            case AssetType::Material:
              return "MAT";
            case AssetType::Shader:
              return "SHD";
            case AssetType::Sprite:
              return "SPR";
            case AssetType::Skybox:
              return "SKY";
            case AssetType::Font:
              return "FNT";
            case AssetType::Script:
              return "CS";
            case AssetType::Scene:
              return "SCN";
            case AssetType::Prefab:
              return "PFB";
            case AssetType::Audio:
              return "SND";
            case AssetType::SpriteSheet:
              return "SPR";
            case AssetType::SpriteAnim:
              return "ANM";
            default:
              return "?";
          }
        };

        // Rename state
        static std::string renaming_file;
        static char rename_buf[256] = "";

        // ".." back folder
        if (!current_dir.empty()) {
          if (DrawTile("..", ImVec4(0.35f, 0.35f, 0.4f, 1.0f), "..", false,
                       true)) {
            std::string trimmed = current_dir;
            if (!trimmed.empty() && trimmed.back() == '/') {
              trimmed.pop_back();
            }
            size_t slash = trimmed.find_last_of('/');
            if (slash == std::string::npos) {
              current_dir = "";
            } else {
              current_dir = trimmed.substr(0, slash + 1);
            }
          }
          // Drop target on ".." to move files to parent directory
          if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload =
                    ImGui::AcceptDragDropPayload("BrowserFile")) {
              namespace fs = std::filesystem;
              std::string src_str(static_cast<const char*>(payload->Data));
              fs::path src(src_str);
              fs::path parent = src.parent_path().parent_path();
              fs::path dest = parent / src.filename();
              std::error_code ec;
              fs::rename(src, dest, ec);
              if (!ec) {
                if (current_scene_path_ == fs::absolute(src)) {
                  current_scene_path_ = fs::absolute(dest);
                  UpdateWindowTitle();
                }
                ScanProjectAssets();
              }
            }
            ImGui::EndDragDropTarget();
          }
          NextColumn();
        }

        // File and directory tiles
        for (auto& fe : entries) {
          bool is_sel = selected_file == fe.name;
          bool dbl_clicked = false;

          AssetHandle handle;
          const AssetMetadata* meta = nullptr;

          if (fe.is_dir) {
            if (DrawTile(fe.name.c_str(), ImVec4(0.3f, 0.35f, 0.45f, 1.0f),
                         "DIR", is_sel, true, nullptr, nullptr, &dbl_clicked)) {
              selected_file = fe.name;
            }
            if (dbl_clicked) {
              current_dir += fe.name + "/";
            }
          } else {
            // Look up asset in AssetManager for thumbnails
            std::string vfs_path = "/app/" + current_dir + fe.name;
            for (auto& h : mgr.GetAll()) {
              const auto* m = mgr.GetMetadata(h);
              if (m && m->virtual_source_path == vfs_path) {
                handle = h;
                meta = m;
                break;
              }
            }

            const ThumbnailEntry* thumbnail = nullptr;
            ThumbnailEntry thumb_entry;
            if (meta && (meta->type == AssetType::Texture ||
                         meta->type == AssetType::Sprite)) {
              thumb_entry = GetOrCreateThumbnail(handle, *meta);
              if (thumb_entry.texture_id) {
                thumbnail = &thumb_entry;
              }
            }

            bool is_imported = handle.IsValid();
            ImVec4 tile_color = GetAssetColor(fe.asset_type);
            if (!is_imported && fe.asset_type != AssetType::None) {
              // Dim unimported assets
              tile_color.x *= 0.4f;
              tile_color.y *= 0.4f;
              tile_color.z *= 0.4f;
            }
            if (DrawTile(fe.name.c_str(), tile_color,
                         GetAssetAbbrev(fe.asset_type), is_sel, false,
                         thumbnail, meta, &dbl_clicked)) {
              selected_file = fe.name;
              // Auto-show in Asset Properties panel
              if (handle.IsValid()) {
                properties_asset_handle_ = handle;
              }
            }

            if (dbl_clicked) {
              if (fe.asset_type == AssetType::Scene) {
                deferred_action_ = DeferredAction::OpenScene;
                deferred_path_ = fe.physical_path;
              } else if (fe.asset_type == AssetType::Prefab) {
                deferred_action_ = DeferredAction::OpenPrefab;
                deferred_path_ = fe.physical_path;
              } else if (fe.asset_type == AssetType::Script) {
                OpenFileInDefaultEditor(fe.physical_path);
              }
            }

            // Drag source for asset files
            if (handle.IsValid() && ImGui::BeginDragDropSource(
                                        ImGuiDragDropFlags_SourceAllowNullID)) {
              ImGui::SetDragDropPayload("AssetHandle", &handle,
                                        sizeof(AssetHandle));
              ImGui::Text("%s", fe.name.c_str());
              ImGui::EndDragDropSource();
            }
          }

          // Drag source for moving files/folders in the browser
          if (ImGui::BeginDragDropSource(
                  ImGuiDragDropFlags_SourceAllowNullID)) {
            std::string path_str = fe.physical_path.string();
            ImGui::SetDragDropPayload("BrowserFile", path_str.c_str(),
                                      path_str.size() + 1);
            ImGui::Text("%s %s", fe.is_dir ? "[DIR]" : "", fe.name.c_str());
            ImGui::EndDragDropSource();
          }

          // Drop target on directories to move files into them
          if (fe.is_dir && ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload =
                    ImGui::AcceptDragDropPayload("BrowserFile")) {
              namespace fs = std::filesystem;
              std::string src_str(static_cast<const char*>(payload->Data));
              fs::path src(src_str);
              fs::path dest = fe.physical_path / src.filename();
              std::error_code ec;
              fs::rename(src, dest, ec);
              if (!ec) {
                if (current_scene_path_ == fs::absolute(src)) {
                  current_scene_path_ = fs::absolute(dest);
                  UpdateWindowTitle();
                }
                ScanProjectAssets();
              }
            }
            ImGui::EndDragDropTarget();
          }

          // Right-click context menu for files and folders
          if (is_sel &&
              ImGui::BeginPopupContextItem(("##ctx_" + fe.name).c_str())) {
            // Import option for unimported files
            if (!fe.is_dir && !handle.IsValid() &&
                fe.asset_type != AssetType::None) {
              if (ImGui::MenuItem("Import")) {
                std::string import_vfs = "/app/" + current_dir + fe.name;
                AssetHandle new_handle = ProjectLoader::ImportAsset(
                    fe.name, fe.asset_type, import_vfs);
                if (new_handle.IsValid()) {
                  if (fe.asset_type == AssetType::Prefab ||
                      fe.asset_type == AssetType::Scene) {
                    mgr.SetLoadState(new_handle, AssetLoadState::Unloaded,
                                     AssetLoadState::Loaded);
                  }
                }
              }
              ImGui::Separator();
            }
            if (!fe.is_dir && fe.asset_type == AssetType::Texture &&
                handle.IsValid()) {
              if (ImGui::MenuItem("Slice into Sprites")) {
                slice_texture_handle_ = handle;
                show_slice_sprites_ = true;
                ImGui::CloseCurrentPopup();
              }
              ImGui::Separator();
            }
            if (ImGui::MenuItem("Rename")) {
              renaming_file = fe.name;
              auto stem = fe.physical_path.stem().string();
              snprintf(rename_buf, sizeof(rename_buf), "%s", stem.c_str());
            }
            if (!fe.is_dir && ImGui::MenuItem("Duplicate")) {
              namespace fs = std::filesystem;
              std::string stem = fe.physical_path.stem().string();
              std::string ext = fe.physical_path.extension().string();
              fs::path copy_path =
                  fe.physical_path.parent_path() / (stem + "_copy" + ext);
              int n = 1;
              while (fs::exists(copy_path)) {
                copy_path = fe.physical_path.parent_path() /
                            (stem + "_copy" + std::to_string(n++) + ext);
              }
              std::error_code ec;
              fs::copy_file(fe.physical_path, copy_path, ec);
              if (!ec) {
                ScanProjectAssets();
              }
            }
            if (fe.is_dir && ImGui::MenuItem("Duplicate")) {
              namespace fs = std::filesystem;
              std::string name = fe.name + "_copy";
              fs::path copy_path = fe.physical_path.parent_path() / name;
              int n = 1;
              while (fs::exists(copy_path)) {
                copy_path = fe.physical_path.parent_path() /
                            (fe.name + "_copy" + std::to_string(n++));
              }
              std::error_code ec;
              fs::copy(fe.physical_path, copy_path, fs::copy_options::recursive,
                       ec);
              if (!ec) {
                ScanProjectAssets();
              }
            }
            if (!fe.is_dir && handle.IsValid() &&
                AssetPropertyRegistry::HasProperties(fe.asset_type)) {
              // Properties panel auto-selects on click, no menu item needed.
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Delete")) {
              namespace fs = std::filesystem;
              std::error_code ec;
              fs::remove_all(fe.physical_path, ec);
              if (!ec) {
                selected_file.clear();
                ScanProjectAssets();
              }
            }
            ImGui::EndPopup();
          }

          NextColumn();
        }

        // Rename popup
        if (!renaming_file.empty()) {
          ImGui::OpenPopup("RenamePopup");
        }
        if (ImGui::BeginPopup("RenamePopup")) {
          ImGui::Text("Rename:");
          ImGui::InputText("##rename", rename_buf, sizeof(rename_buf));
          if (ImGui::Button("OK") && rename_buf[0] != '\0') {
            namespace fs = std::filesystem;
            auto physical_app_path = Engine::vfs()->GetPhysicalPath("/app");
            if (physical_app_path.has_value()) {
              fs::path old_path = fs::absolute(*physical_app_path) /
                                  current_dir / renaming_file;
              std::string ext = old_path.extension().string();
              fs::path new_path =
                  old_path.parent_path() / (std::string(rename_buf) + ext);
              std::error_code ec;
              fs::rename(old_path, new_path, ec);
              if (!ec) {
                // If renaming the current scene, update the path
                if (current_scene_path_ == fs::absolute(old_path)) {
                  current_scene_path_ = fs::absolute(new_path);
                  UpdateWindowTitle();
                }
                ScanProjectAssets();
              }
            }
            renaming_file.clear();
            ImGui::CloseCurrentPopup();
          }
          ImGui::SameLine();
          if (ImGui::Button("Cancel")) {
            renaming_file.clear();
            ImGui::CloseCurrentPopup();
          }
          ImGui::EndPopup();
        }

        // Right-click on empty space
        static bool open_folder_popup = false;
        if (ImGui::BeginPopupContextWindow("##browser_ctx",
                                           ImGuiPopupFlags_NoOpenOverItems)) {
          if (ImGui::BeginMenu("Create")) {
            if (ImGui::MenuItem("Scene")) {
              NewScene();
            }
            if (ImGui::MenuItem("Folder")) {
              open_folder_popup = true;
              ImGui::CloseCurrentPopup();
            }
            if (ImGui::MenuItem("C# Script")) {
              open_script_popup = true;
              ImGui::CloseCurrentPopup();
            }
            if (ImGui::MenuItem("Skybox")) {
              show_create_skybox_ = true;
              ImGui::CloseCurrentPopup();
            }
            if (ImGui::MenuItem("Sprite")) {
              show_create_sprite_ = true;
              ImGui::CloseCurrentPopup();
            }
            if (ImGui::MenuItem("Sprite Sheet")) {
              show_create_spritesheet_ = true;
              ImGui::CloseCurrentPopup();
            }
            if (ImGui::MenuItem("Sprite Animation")) {
              show_create_spriteanim_ = true;
              ImGui::CloseCurrentPopup();
            }
            ImGui::EndMenu();
          }
          ImGui::EndPopup();
        }
        if (open_folder_popup) {
          ImGui::OpenPopup("NewFolderPopup");
          open_folder_popup = false;
        }
      }
      ImGui::EndChild();

      // New C# script popup (must be in same scope as OpenPopup)
      if (open_script_popup) {
        ImGui::OpenPopup("NewScriptPopup");
        open_script_popup = false;
      }
      static char new_script_name[128] = "NewScript";
      if (ImGui::BeginPopup("NewScriptPopup")) {
        ImGui::Text("Script name:");
        ImGui::InputText("##scriptname", new_script_name,
                         sizeof(new_script_name));
        if (ImGui::Button("Create") && new_script_name[0] != '\0') {
          namespace fs = std::filesystem;
          auto physical_app = Engine::vfs()->GetPhysicalPath("/app");
          if (physical_app.has_value()) {
            fs::path base = fs::absolute(*physical_app);
            if (!browser_current_dir_.empty()) {
              base = base / browser_current_dir_;
            }
            fs::path script_path =
                base / (std::string(new_script_name) + ".cs");
            if (!fs::exists(script_path)) {
              VfsFile tmpl =
                  Engine::vfs()->Open("/engine/templates/script.cs.template");
              std::string content;
              if (tmpl) {
                content = std::string(
                    reinterpret_cast<const char*>(tmpl.Data()), tmpl.Size());
              } else {
                content =
                    "using WieselEngine;\n\npublic class {{CLASS_NAME}} : "
                    "MonoBehavior\n{\n}\n";
              }
              std::string class_name = new_script_name;
              size_t pos = 0;
              while ((pos = content.find("{{CLASS_NAME}}", pos)) !=
                     std::string::npos) {
                content.replace(pos, 14, class_name);
                pos += class_name.length();
              }
              std::ofstream out(script_path);
              if (out.is_open()) {
                out << content;
              }
              ScanProjectAssets();
            }
          }
          new_script_name[0] = '\0';
          ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
          new_script_name[0] = '\0';
          ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
      }
    }
    ImGui::End();
  }

  // Developer Console Panel
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

      if (ImGui::Begin("Developer Console", &console_open)) {
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

  // Render Stats Panel
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

  // Helper lambda: draws Play/Stop buttons, returns true if state changed
  auto DrawPlayStopButtons = [&]() -> bool {
    bool changed = false;
    if (editor_state_ == EditorState::Edit) {
      if (ImGui::Button("Play")) {
        AutoSave();
        TakeSnapshot();
        editor_state_ = EditorState::Playing;
        scene()->ResetFirstUpdate();
        ImGui::SetWindowFocus("Game");
        changed = true;
      }
    } else {
      if (ImGui::Button("Stop")) {
        deferred_action_ = DeferredAction::StopPlaying;
        changed = true;
      }
    }
    return changed;
  };

  //
  // Scene Panel (editor free camera)
  //

  bool& scene_view_open = panel_scene_view_;
  if (scene_view_open) {
    ImGuiWindowFlags sceneFlags =
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar;
    scene_panel_visible_ = ImGui::Begin("Scene", &scene_view_open, sceneFlags);
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
        // Look straight down -Z, position Z behind sprites
        editor_pitch_ = 0.0f;
        editor_yaw_ = 180.0f;
        editor_camera_transform_.SetPosition(glm::vec3(0.0f, 180.0f, 5.0f));
        editor_camera_transform_.SetRotation(glm::vec3(0.0f, 180.0f, 0.0f));
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
          ImGui::SetNextItemWidth(kResolutionComboWidth);
          if (ImGui::BeginCombo(
                  "Resolution",
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

      // Handle viewport resize
      ImVec2 avail = ImGui::GetContentRegionAvail();
      if (scene()->GetRenderResolution().x <= 0) {
        // Free Aspect: editor camera tracks panel size
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

      // When a preset is active, RenderFromExternal applies render_resolution_ automatically
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
            Engine::window()->SetCursorMode(CursorModeRelative);
          }
        } else {
          if (scene_right_active) {
            scene_right_active = false;
            Engine::window()->SetCursorMode(CursorModeNormal);
          }
        }

        // Mouse look is handled in OnMouseMoved via the event system

        // Camera movement
        if (scene_focused || scene_right_active) {
          ImGui::GetIO().WantCaptureKeyboard = false;
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
            glm::vec3 cam_forward = -glm::vec3(R[2]);

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
              glm::vec3 cam_forward = -glm::vec3(R[2]);
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
                                   current_op_, ImGuizmo::WORLD,
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
      }
    }
    ImGui::End();
  }

  // ======== Game Panel (primary scene camera, only when playing) ========
  bool& game_view_open = panel_game_view_;
  if (game_view_open) {
    {
      bool gameVisible = ImGui::Begin("Game", &game_view_open);
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

            auto finalOutputDesc = renderer->GetFinalOutputDescriptor();
            auto finalOutputImage = renderer->GetFinalOutputImage();
            if (finalOutputDesc && finalOutputImage) {
              ImTextureID gameDesc = reinterpret_cast<ImTextureID>(
                  finalOutputDesc->descriptor_set_);

              float imageAspect = static_cast<float>(finalOutputImage->width_) /
                                  static_cast<float>(finalOutputImage->height_);
              float availAspect = avail.x / avail.y;

              ImVec2 drawSize;
              if (availAspect > imageAspect) {
                drawSize.y = avail.y;
                drawSize.x = drawSize.y * imageAspect;
              } else {
                drawSize.x = avail.x;
                drawSize.y = drawSize.x / imageAspect;
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
                  "{}x{}", finalOutputImage->width_, finalOutputImage->height_);
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
           scene()->GetAllEntitiesWith<SpriteComponent, TransformComponent>()) {
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
      auto editorOutput =
          editor_camera_.resource_pool.GetTexture("PipelineOutput");
      if (editorOutput) {
        renderer->TransitionImageLayout(
            editorOutput->images_[0], editorOutput->format_,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1, cmd, 0, 1);
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
      editorOutput = editor_camera_.resource_pool.GetTexture("PipelineOutput");
      if (editorOutput) {
        renderer->TransitionImageLayout(
            editorOutput->images_[0], editorOutput->format_,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, cmd, 0, 1);
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

    // Resolve handle to VFS path
    auto resolve = [](const AssetHandle& h) -> std::string {
      if (!h.IsValid()) {
        return "";
      }
      const auto* meta = Engine::asset_manager().GetMetadata(h);
      return meta ? meta->virtual_source_path : "";
    };

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
      nlohmann::json j;
      j["asset_handle"] = AssetHandle::Generate().ToString();

      if (skybox_type == 0) {
        j["type"] = "panorama";
        j["source"] = resolve(source_handle);
      } else if (skybox_type == 1) {
        j["type"] = "cubemap";
        j["faces"] = {
            {"right", resolve(face_handles[0])},
            {"left", resolve(face_handles[1])},
            {"top", resolve(face_handles[2])},
            {"bottom", resolve(face_handles[3])},
            {"front", resolve(face_handles[4])},
            {"back", resolve(face_handles[5])},
        };
      } else {
        j["type"] = "cross";
        j["source"] = resolve(source_handle);
      }

      auto physical_app = Engine::vfs()->GetPhysicalPath("/app");
      if (physical_app.has_value()) {
        namespace fs = std::filesystem;
        fs::path base = fs::absolute(*physical_app);
        if (!browser_current_dir_.empty()) {
          base = base / browser_current_dir_;
        }
        fs::path file_path = base / (std::string(name_buf) + ".wskybox");
        {
          std::ofstream out(file_path);
          if (out.is_open()) {
            out << j.dump(2);
          }
        }
        ScanProjectAssets();

        // Auto-select the new skybox on the scene
        std::string handle_str = j["asset_handle"].get<std::string>();
        AssetHandle new_handle = AssetHandle::FromString(handle_str);
        if (new_handle.IsValid()) {
          scene()->SetSkyboxAsset(new_handle);
          scene_dirty_ = true;
        }
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

// Helper: write a .wsprite JSON file and rescan assets
static void WriteSpriteFile(const std::filesystem::path& file_path,
                            const AssetHandle& texture_handle,
                            float x, float y, float w, float h,
                            float pivot_x, float pivot_y) {
  nlohmann::json j;
  j["asset_handle"] = AssetHandle::Generate().ToString();
  j["texture"] = texture_handle.ToString();
  j["rect"] = {x, y, w, h};
  j["pivot"] = {pivot_x, pivot_y};
  std::ofstream out(file_path);
  if (out.is_open()) {
    out << j.dump(2);
  }
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
      auto physical_app = Engine::vfs()->GetPhysicalPath("/app");
      if (physical_app.has_value()) {
        namespace fs = std::filesystem;
        fs::path base = fs::absolute(*physical_app);
        if (!browser_current_dir_.empty()) {
          base = base / browser_current_dir_;
        }
        fs::path file_path = base / (std::string(name_buf) + ".wsprite");
        WriteSpriteFile(file_path, texture_handle,
                        rect[0], rect[1], rect[2], rect[3],
                        pivot[0], pivot[1]);
        ScanProjectAssets();
      }
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
      auto physical_app = Engine::vfs()->GetPhysicalPath("/app");
      if (physical_app.has_value()) {
        namespace fs = std::filesystem;
        fs::path base = fs::absolute(*physical_app);
        if (!browser_current_dir_.empty()) {
          base = base / browser_current_dir_;
        }

        for (int r = 0; r < rows; r++) {
          for (int c = 0; c < columns; c++) {
            int idx = r * columns + c;
            std::string name =
                std::string(prefix_buf) + "_" + std::to_string(idx);
            fs::path file_path = base / (name + ".wsprite");
            WriteSpriteFile(file_path, slice_texture_handle_,
                            c * cell_w, r * cell_h, cell_w, cell_h,
                            pivot[0], pivot[1]);
          }
        }
        ScanProjectAssets();
      }
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

void EditorLayer::RenderCreateSpriteSheetPopup() {
  if (show_create_spritesheet_) {
    ImGui::OpenPopup("Create Sprite Sheet");
    show_create_spritesheet_ = false;
  }

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

  bool popup_open = true;
  if (ImGui::BeginPopupModal("Create Sprite Sheet", &popup_open,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    static char name_buf[128] = "spritesheet";
    static int sheet_mode = 0;  // 0 = single atlas, 1 = multiple images
    static AssetHandle texture_handle;
    static int cell_w = 64;
    static int cell_h = 64;
    static int frame_count = 0;
    static std::vector<AssetHandle> multi_textures;
    static float multi_duration = 0.1f;

    ImGui::InputText("Name", name_buf, sizeof(name_buf));

    const char* mode_names[] = {"Single Atlas", "Multiple Images"};
    ImGui::Combo("Mode", &sheet_mode, mode_names, 2);

    if (sheet_mode == 0) {
      AssetCombo("Texture", AssetType::Texture, texture_handle, false);
      ImGui::InputInt("Cell Width", &cell_w);
      ImGui::InputInt("Cell Height", &cell_h);
      ImGui::InputInt("Frame Count (0 = auto)", &frame_count);

      if (cell_w < 1) {
        cell_w = 1;
      }
      if (cell_h < 1) {
        cell_h = 1;
      }
      if (frame_count < 0) {
        frame_count = 0;
      }
    } else {
      ImGui::InputFloat("Frame Duration", &multi_duration, 0.01f);
      ImGui::Text("Frames: %d", static_cast<int>(multi_textures.size()));

      int to_remove = -1;
      for (int i = 0; i < static_cast<int>(multi_textures.size()); i++) {
        ImGui::PushID(i);
        const auto* meta =
            Engine::asset_manager().GetMetadata(multi_textures[i]);
        std::string label = meta ? meta->name : "(Unknown)";
        ImGui::Text("%d: %s", i, label.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) {
          to_remove = i;
        }
        ImGui::PopID();
      }
      if (to_remove >= 0) {
        multi_textures.erase(multi_textures.begin() + to_remove);
      }

      // Add texture button
      static AssetHandle add_tex;
      AssetCombo("Add Frame", AssetType::Texture, add_tex);
      if (add_tex.IsValid()) {
        multi_textures.push_back(add_tex);
        add_tex = {};
      }
    }

    ImGui::Separator();

    bool can_create = name_buf[0] != '\0';
    if (sheet_mode == 0) {
      can_create = can_create && texture_handle.IsValid();
    } else {
      can_create = can_create && !multi_textures.empty();
    }

    if (ImGui::Button("Create") && can_create) {
      nlohmann::json j;
      j["asset_handle"] = AssetHandle::Generate().ToString();

      if (sheet_mode == 0) {
        const auto* meta = Engine::asset_manager().GetMetadata(texture_handle);
        j["texture"] = meta ? meta->virtual_source_path : "";
        j["cell_size"] = {cell_w, cell_h};
        j["frame_count"] = frame_count;
      } else {
        nlohmann::json tex_arr = nlohmann::json::array();
        for (auto& h : multi_textures) {
          const auto* meta = Engine::asset_manager().GetMetadata(h);
          if (meta) {
            tex_arr.push_back(meta->virtual_source_path);
          }
        }
        j["textures"] = tex_arr;
        j["frame_duration"] = multi_duration;
      }

      auto physical_app = Engine::vfs()->GetPhysicalPath("/app");
      if (physical_app.has_value()) {
        namespace fs = std::filesystem;
        fs::path base = fs::absolute(*physical_app);
        if (!browser_current_dir_.empty()) {
          base = base / browser_current_dir_;
        }
        fs::path file_path = base / (std::string(name_buf) + ".wspritesheet");
        {
          std::ofstream out(file_path);
          if (out.is_open()) {
            out << j.dump(2);
          }
        }
        ScanProjectAssets();
      }

      name_buf[0] = '\0';
      texture_handle = {};
      multi_textures.clear();
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      name_buf[0] = '\0';
      texture_handle = {};
      multi_textures.clear();
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

void EditorLayer::RenderCreateSpriteAnimPopup() {
  if (show_create_spriteanim_) {
    ImGui::OpenPopup("Create Sprite Animation");
    show_create_spriteanim_ = false;
  }

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_Appearing);

  bool popup_open = true;
  if (ImGui::BeginPopupModal("Create Sprite Animation", &popup_open,
                             ImGuiWindowFlags_NoScrollbar)) {
    static char name_buf[128] = "animation";
    static AssetHandle sheet_handle;
    static std::string default_state;

    struct ClipEntry {
      char name[64] = "";
      int start = 0;
      int count = 1;
      float duration = 0.1f;
      bool loop = true;
    };

    static std::vector<ClipEntry> clip_entries;

    ImGui::InputText("Name", name_buf, sizeof(name_buf));
    AssetCombo("Sprite Sheet", AssetType::SpriteSheet, sheet_handle, false);

    ImGui::SeparatorText("Clips");

    if (ImGui::BeginTable("##clips", 6,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable)) {
      ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 100);
      ImGui::TableSetupColumn("Start");
      ImGui::TableSetupColumn("Count");
      ImGui::TableSetupColumn("Duration");
      ImGui::TableSetupColumn("Loop", ImGuiTableColumnFlags_WidthFixed, 40);
      ImGui::TableSetupColumn("##del", ImGuiTableColumnFlags_WidthFixed, 20);
      ImGui::TableHeadersRow();

      int to_remove = -1;
      for (int i = 0; i < static_cast<int>(clip_entries.size()); i++) {
        ImGui::PushID(i);
        auto& clip = clip_entries[i];

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##name", clip.name, sizeof(clip.name));

        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1);
        ImGui::InputInt("##start", &clip.start);

        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1);
        ImGui::InputInt("##count", &clip.count);

        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1);
        ImGui::InputFloat("##dur", &clip.duration, 0.01f);

        ImGui::TableNextColumn();
        ImGui::Checkbox("##loop", &clip.loop);

        ImGui::TableNextColumn();
        if (ImGui::SmallButton("X")) {
          to_remove = i;
        }

        ImGui::PopID();
      }
      ImGui::EndTable();

      if (to_remove >= 0) {
        clip_entries.erase(clip_entries.begin() + to_remove);
      }
    }

    if (ImGui::Button("+ Add Clip")) {
      clip_entries.push_back({});
    }

    ImGui::Separator();

    bool can_create =
        name_buf[0] != '\0' && sheet_handle.IsValid() && !clip_entries.empty();
    if (ImGui::Button("Create") && can_create) {
      nlohmann::json j;
      j["asset_handle"] = AssetHandle::Generate().ToString();
      j["sprite_sheet"] = sheet_handle.ToString();

      nlohmann::json clips_json = nlohmann::json::array();
      for (auto& clip : clip_entries) {
        if (clip.name[0] == '\0') {
          continue;
        }
        nlohmann::json cj;
        cj["name"] = clip.name;
        cj["start"] = clip.start;
        cj["count"] = clip.count;
        cj["duration"] = clip.duration;
        cj["loop"] = clip.loop;
        clips_json.push_back(cj);
      }
      j["clips"] = clips_json;

      auto physical_app = Engine::vfs()->GetPhysicalPath("/app");
      if (physical_app.has_value()) {
        namespace fs = std::filesystem;
        fs::path base = fs::absolute(*physical_app);
        if (!browser_current_dir_.empty()) {
          base = base / browser_current_dir_;
        }
        fs::path file_path = base / (std::string(name_buf) + ".wspriteanim");
        {
          std::ofstream out(file_path);
          if (out.is_open()) {
            out << j.dump(2);
          }
        }
        ScanProjectAssets();
      }

      name_buf[0] = '\0';
      sheet_handle = {};
      clip_entries.clear();
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      name_buf[0] = '\0';
      sheet_handle = {};
      clip_entries.clear();
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
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
      auto& ro = game_info.render_options;
      ro.ambient_color = {settings.ambient_color.x, settings.ambient_color.y,
                          settings.ambient_color.z};
      ro.ambient_intensity = settings.ambient_intensity;
      ro.ssao_enabled = settings.ssao_enabled;
      ro.bloom_enabled = settings.bloom_enabled;
      ro.bloom_threshold = settings.bloom_threshold;
      ro.bloom_intensity = settings.bloom_intensity;
      ro.motion_blur_enabled = settings.motion_blur_enabled;
      ro.motion_blur_strength = settings.motion_blur_strength;
      ro.motion_blur_samples = settings.motion_blur_samples;
      ro.shadows_enabled = settings.shadows_enabled;
      ro.vsync = settings.vsync;
      ro.aa_mode = static_cast<int>(settings.aa_mode.Get());
      ro.msaa_mode = static_cast<int>(settings.msaa_mode.Get());
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
      input_changed |=
          ImGui::DragFloat(PrefixLabel("Y Axis Limit").c_str(),
                           &input.mouse_axis_limit_y, 1.0f, 1.0f, 90.0f);

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

            // Table: Name | +Keys | -Keys | Stick | Delete
            if (ImGui::BeginTable("##axes_table", 5,
                                  ImGuiTableFlags_Borders |
                                      ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_Resizable)) {
              ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed,
                                      90);
              ImGui::TableSetupColumn("+Keys");
              ImGui::TableSetupColumn("-Keys");
              ImGui::TableSetupColumn("Stick", ImGuiTableColumnFlags_WidthFixed,
                                      100);
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
      ImGui::MenuItem("Scene", nullptr, &panel_scene_view_);
      ImGui::MenuItem("Game", nullptr, &panel_game_view_);
      ImGui::MenuItem("Scene Hierarchy", nullptr, &panel_scene_hierarchy_);
      ImGui::MenuItem("Components", nullptr, &panel_components_);
      ImGui::MenuItem("Asset Browser", nullptr, &panel_asset_browser_);
      ImGui::MenuItem("Scene Properties", nullptr, &panel_scene_properties_);
      ImGui::MenuItem("Console", nullptr, &panel_console_);
      ImGui::MenuItem("Stats", nullptr, &panel_stats_);
      ImGui::Separator();
      if (ImGui::MenuItem("Reset Layout")) {
        panel_scene_hierarchy_ = true;
        panel_components_ = true;
        panel_asset_browser_ = true;
        panel_console_ = true;
        panel_stats_ = true;
        panel_scene_view_ = true;
        panel_game_view_ = true;
        panel_scene_properties_ = true;
        layout_initialized_ = false;
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
          info += " - " + current_scene_path_.filename().string();
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
      float total_right = error_width + status_width + summary_width + spacing
                          + info_width + 16.0f;

      ImGui::SameLine(ImGui::GetWindowWidth() - total_right);

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

  // Keyboard shortcuts
  ImGuiIO& io = ImGui::GetIO();
  if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
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
    } catch (...) {}
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
        vfs->Unmount("/app");
        vfs->Mount("/app", project->GetAssetsDirectory().string());

        // Create the default scene file
        ClearScene();
        current_scene_path_ = project->GetScenesDirectory() / "main.wscene";
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
    auto& settings = Engine::renderer()->options();
    auto& opts = project->GetGameInfo().render_options;
    opts.ambient_color = settings.ambient_color;
    opts.ambient_intensity = settings.ambient_intensity;
    opts.ssao_enabled = settings.ssao_enabled;
    opts.bloom_enabled = settings.bloom_enabled;
    opts.bloom_threshold = settings.bloom_threshold;
    opts.bloom_intensity = settings.bloom_intensity;
    opts.motion_blur_enabled = settings.motion_blur_enabled;
    opts.motion_blur_strength = settings.motion_blur_strength;
    opts.motion_blur_samples = settings.motion_blur_samples;
    opts.shadows_enabled = settings.shadows_enabled;
    opts.vsync = settings.vsync;
    opts.aa_mode =
        static_cast<int>(static_cast<AntiAliasingMode>(settings.aa_mode));

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
  auto project = active_project_;
  if (!project) {
    return;
  }

  // Auto-save current scene before creating new one
  AutoSave();

  // Generate a unique name
  namespace fs = std::filesystem;
  fs::path scenes_dir = project->GetScenesDirectory();
  fs::create_directories(scenes_dir);

  std::string base_name = "new_scene";
  fs::path scene_path = scenes_dir / (base_name + ".wscene");
  int counter = 1;
  while (fs::exists(scene_path)) {
    scene_path =
        scenes_dir / (base_name + "_" + std::to_string(counter++) + ".wscene");
  }

  ClearScene();
  current_scene_path_ = fs::absolute(scene_path);
  SaveScene();
  ScanProjectAssets();
  UpdateWindowTitle();
}

void EditorLayer::OpenSceneFromPath(const std::filesystem::path& path) {
  // Auto-save current scene before switching
  AutoSave();

  // Queue scene load - SceneManager::BeginFrame handles clearing,
  // deserializing, camera setup, and asset unloading.
  Engine::scene_manager().LoadSceneFromPath(path);

  // Editor-specific state updated after BeginFrame processes the load
  current_scene_path_ = std::filesystem::absolute(path);
  scene_dirty_ = false;

  // Track last opened scene in project
  if (auto p = project()) {
    auto rel =
        std::filesystem::relative(current_scene_path_, p->GetAssetsDirectory());
    std::string vfs_path = "/app/" + rel.generic_string();
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
  std::filesystem::create_directories(current_scene_path_.parent_path());

  if (SaveSceneToFile(scene(), current_scene_path_)) {
    scene_dirty_ = false;
    UpdateWindowTitle();

    auto_save_timer_ = 0.0f;

    // Register with SceneManager
    auto project = active_project_;
    if (project) {
      project->Save();

      auto rel = std::filesystem::relative(current_scene_path_,
                                           project->GetAssetsDirectory());
      std::string scene_name = current_scene_path_.stem().string();
      Engine::scene_manager().RegisterScene(scene_name, current_scene_path_);

      // Register scene asset if not already in asset browser
      auto vfs_path = "/app/" + rel.generic_string();
      auto& mgr = Engine::asset_manager();
      bool found = false;
      for (auto& h : mgr.GetAll()) {
        const auto* meta = mgr.GetMetadata(h);
        if (meta && meta->virtual_source_path == vfs_path) {
          found = true;
          break;
        }
      }
      if (!found) {
        mgr.Register(scene_name, AssetType::Scene, vfs_path);
      }
    }
  }
}

void EditorLayer::SaveSceneAs() {
  Dialogs::SaveFileDialog(
      {{"Wiesel Scene", "wscene"}}, [this](const std::string& file) {
        if (file.empty()) {
          return;
        }

        std::filesystem::path path(file);
        if (path.extension() != ".wscene") {
          path += ".wscene";
        }

        // If a project is open, ensure the scene is saved inside the assets dir
        auto project = active_project_;
        if (project) {
          namespace fs = std::filesystem;
          fs::path abs = fs::absolute(path);
          fs::path assets = fs::absolute(project->GetAssetsDirectory());
          auto rel = fs::relative(abs, assets);
          if (rel.string().find("..") != std::string::npos) {
            // Outside project assets - redirect into assets/scenes/
            path = project->GetScenesDirectory() / path.filename();
          }
        }

        current_scene_path_ = std::filesystem::absolute(path);
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
    title += " - [Prefab] " + editing_prefab_path_.filename().string();
  } else if (!current_scene_path_.empty()) {
    title += " - " + current_scene_path_.filename().string();
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

  if (SaveSceneToFile(scene(), current_scene_path_)) {
    scene_dirty_ = false;
    auto_save_timer_ = 0.0f;
    UpdateWindowTitle();

    LOG_DEBUG("Auto-saved scene: {}", current_scene_path_.filename().string());
  }
}

void EditorLayer::LoadProjectFromPath(const std::filesystem::path& path) {
  namespace fs = std::filesystem;

  auto proj = Project::Load(path);
  if (!proj) {
    return;
  }

  active_project_ = std::move(proj);
  Engine::SetGameInfo(
      std::make_shared<GameInfo>(active_project_->GetGameInfo()));
  auto project = active_project_;

  // Remove startup FPS cap now that a project is loaded
  app_.SetMaxFPS(0.0f);

  ProjectLoader::MountProject(*project);
  ProjectLoader::ScanAssets(*project);
  Engine::script_manager().Reload();
  ProjectLoader::ApplyRenderOptions(*project);
  ProjectLoader::ApplyInputSettings(*project);

  // Start watching scripts directory for hot reload
  auto scripts_dir = Engine::vfs()->GetPhysicalPath("/app/scripts");
  if (scripts_dir.has_value()) {
    fs::create_directories(*scripts_dir);
    script_watcher_.Watch(*scripts_dir, true);
  }

  // Open last scene or start scene (prefer last_scene, fall back to start)
  auto resolve_scene_path = [&](const AssetHandle& handle)
      -> std::optional<std::filesystem::path> {
    if (!handle.IsValid()) {
      return std::nullopt;
    }
    const auto* meta = Engine::asset_manager().GetMetadata(handle);
    if (!meta) {
      return std::nullopt;
    }
    auto physical =
        Engine::vfs()->GetPhysicalPath(meta->virtual_source_path);
    if (physical.has_value() && fs::exists(*physical)) {
      return physical;
    }
    return std::nullopt;
  };

  auto scene_to_open = resolve_scene_path(project->GetSettings().last_scene);
  if (!scene_to_open.has_value()) {
    scene_to_open = resolve_scene_path(project->GetGameInfo().start_scene);
  }

  if (scene_to_open.has_value()) {
    OpenSceneFromPath(*scene_to_open);
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

void EditorLayer::RenderAssetPropertiesPanel() {
  static bool panel_open = true;
  if (!panel_open) {
    return;
  }

  if (ImGui::Begin("Asset Properties", &panel_open)) {
    const auto* meta =
        Engine::asset_manager().GetMetadata(properties_asset_handle_);
    if (!meta || !properties_asset_handle_.IsValid()) {
      ImGui::TextDisabled("No asset selected");
      ImGui::TextDisabled("Right-click an asset in the browser");
      ImGui::TextDisabled("and select Properties.");
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

    const auto* desc = AssetPropertyRegistry::Get(meta->type);
    if (desc && meta->properties) {
      bool changed = desc->RenderImGui(meta->properties.get());
      if (changed) {
        // Write properties back to .meta file
        auto physical =
            Engine::vfs()->GetPhysicalPath(meta->virtual_source_path);
        if (physical.has_value()) {
          std::filesystem::path meta_path = physical->string() + ".meta";
          ProjectLoader::WriteMetaFile(meta_path, meta->handle, meta->type,
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
        auto s = scene();
        if (s) {
          for (auto e : s->GetAllEntitiesWith<TextComponent>()) {
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
  auto project = active_project_;
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

void EditorLayer::OpenPrefabForEditing(const std::filesystem::path& path) {
  namespace fs = std::filesystem;

  // Save current scene state
  AutoSave();
  prefab_return_scene_path_ = current_scene_path_;

  // Clear scene and load prefab as a temporary scene
  ClearScene();
  Entity root = Prefab::InstantiateFromFile(scene(), path);
  if (root.handle() == entt::null) {
    LOG_ERROR("Failed to open prefab for editing: {}", path.string());
    // Restore previous scene
    if (!prefab_return_scene_path_.empty()) {
      OpenSceneFromPath(prefab_return_scene_path_);
    }
    return;
  }

  editing_prefab_ = true;
  editing_prefab_path_ = fs::absolute(path);
  current_scene_path_.clear();
  scene_dirty_ = false;
  UpdateWindowTitle();
  scene()->InvalidateRenderGraphs();

  // Setup camera components
  for (auto entity : scene()->GetAllEntitiesWith<CameraComponent>()) {
    auto& cam = scene()->GetComponent<CameraComponent>(entity);
    Engine::renderer()->SetupCameraComponent(cam);
  }

  LOG_INFO("Editing prefab: {}", path.string());
}

void EditorLayer::SavePrefab() {
  if (!editing_prefab_ || editing_prefab_path_.empty()) {
    return;
  }

  // Find the root entity (first in hierarchy - the prefab root)
  auto& hierarchy = scene()->GetSceneHierarchy();
  if (hierarchy.empty()) {
    LOG_ERROR("Cannot save prefab: scene is empty");
    return;
  }

  Entity root = {hierarchy[0], scene().get()};
  if (Prefab::SaveToFile(root, editing_prefab_path_)) {
    scene_dirty_ = false;
    LOG_INFO("Prefab saved: {}", editing_prefab_path_.string());
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
    OpenSceneFromPath(prefab_return_scene_path_);
    prefab_return_scene_path_.clear();
  } else {
    ClearScene();
  }
}

}  // namespace Wiesel::Editor
