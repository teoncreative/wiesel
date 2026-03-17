//
// Created by Metehan Gezer on 18/04/2025.
//

#include "w_editor.hpp"
#include "util/w_discord_rpc.hpp"

#include <imgui.h>
#include "util/w_tracy.hpp"
#include <misc/cpp/imgui_stdlib.h>
#include <backends/imgui_impl_vulkan.h>
#include <ImGuizmo.h>

#include "asset/w_asset_manager.hpp"
#include "imgui_internal.h"
#include "physics/w_collider.hpp"
#include "physics/w_physics_world.hpp"
#include "rendering/w_material.hpp"
#include "rendering/w_sprite.hpp"
#include "rendering/w_texture.hpp"
#include "util/w_dialogs.hpp"
#include "util/w_filewatcher.hpp"
#include "util/w_platform.hpp"
#include "layer/w_layerscene.hpp"
#include "scene/w_componentutil.hpp"
#include "project/w_project_loader.hpp"
#include "scene/w_scene_serializer.hpp"
#include "scene/w_scene_manager.hpp"
#include "scene/w_prefab.hpp"
#include "script/w_scriptmanager.hpp"
#include "util/imgui/w_imguiutil.hpp"
#include "input/w_input.hpp"
#include "util/w_gamepadcodes.hpp"
#include "w_engine.hpp"

namespace Wiesel::Editor {

// --- RecentProjects ---

std::filesystem::path RecentProjects::GetConfigPath() {
  namespace fs = std::filesystem;
#ifdef _WIN32
  const char* appdata = std::getenv("APPDATA");
  if (appdata) return fs::path(appdata) / "Wiesel" / "recent_projects.json";
  return fs::path(".wiesel") / "recent_projects.json";
#else
  const char* home = std::getenv("HOME");
  if (home) return fs::path(home) / ".wiesel" / "recent_projects.json";
  return fs::path(".wiesel") / "recent_projects.json";
#endif
}

std::vector<std::string> RecentProjects::Load() {
  std::vector<std::string> result;
  auto path = GetConfigPath();
  if (!std::filesystem::exists(path)) return result;
  std::ifstream file(path);
  if (!file.is_open()) return result;
  try {
    nlohmann::json j;
    file >> j;
    if (j.is_array()) {
      for (const auto& item : j) {
        if (item.is_string()) result.push_back(item.get<std::string>());
      }
    }
  } catch (...) {}
  return result;
}

void RecentProjects::Save(const std::vector<std::string>& paths) {
  auto config_path = GetConfigPath();
  std::filesystem::create_directories(config_path.parent_path());
  std::ofstream file(config_path);
  if (!file.is_open()) return;
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
  if (recent.size() > kMaxRecent) recent.resize(kMaxRecent);
  Save(recent);
}

// Todo move these to the editor overlay instead
static entt::entity selected_entity_;
static bool has_selected_entity_ = false;
static ImGuizmo::OPERATION current_op_ = ImGuizmo::TRANSLATE;
static struct SceneHierarchyData {
  entt::entity move_from = entt::null;
  entt::entity move_to = entt::null;
  bool bottom_part = false;
} hierarchy_data_;

static FileWatcher script_watcher_;
static float script_watch_timer_ = 0.0f;
static constexpr float kScriptWatchInterval = 1.0f;

struct ResolutionPreset {
  const char* label;
  glm::vec2 size;  // {0,0} = Free Aspect
};
static const ResolutionPreset kResolutionPresets[] = {
    {"2560x1440", {2560, 1440}},
    {"1920x1080", {1920, 1080}},
    {"1600x900",  {1600, 900}},
    {"1280x720",  {1280, 720}},
    {"854x480",   {854, 480}},
    {"Free Aspect", {0, 0}},
};
static constexpr int kResolutionPresetCount = sizeof(kResolutionPresets) / sizeof(kResolutionPresets[0]);

struct ThumbnailEntry {
  VkDescriptorSet texture_id = nullptr;
  bool attempted = false;
};

static std::unordered_map<AssetHandle, ThumbnailEntry> thumbnail_cache_;

static void CleanupThumbnailCache() {
  for (auto& [handle, entry] : thumbnail_cache_) {
    if (entry.texture_id) {
      ImGui_ImplVulkan_RemoveTexture(entry.texture_id);
    }
  }
  thumbnail_cache_.clear();
}

EditorLayer::EditorLayer(Application& app, std::shared_ptr<Scene> scene)
    : app_(app), scene_(scene), Layer("Demo Overlay") {}

EditorLayer::~EditorLayer() = default;

void EditorLayer::OnAttach() {
  LOG_DEBUG("OnAttach");

#ifdef WIESEL_DISCORD_RPC
  Engine::discord_rpc().Initialize("1483104533247688866");
  Engine::discord_rpc().SetPresence("Wiesel Editor", "Idle", "wiesel_logo", "Wiesel Engine");
#endif

  // Initialize editor free camera
  editor_camera_transform_.position = glm::vec3(0.0f, 5.0f, -10.0f);
  editor_camera_transform_.scale = glm::vec3(1.0f);
  editor_yaw_ = 180.0f;  // facing +Z (quat look = -sin(y),-cos(y) so 180 gives +Z)
  editor_pitch_ = -15.0f; // slightly looking down
  editor_camera_transform_.rotation = glm::vec3(editor_pitch_, editor_yaw_, 0.0f);

  editor_camera_.viewport_size = {1920, 1080};
  editor_camera_.far_plane = 500.0f;
  editor_camera_.field_of_view = 60.0f;
  Engine::renderer()->SetupCameraComponent(editor_camera_);
  scene_->SetRenderResolution(kResolutionPresets[resolution_preset_index_].size);

  // Register this scene as the active scene for SceneManager
  SceneManager::Get().SetActiveScene(scene_);

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

  // Start watching app scripts directory for hot reload
  if (Engine::properties().dev_mode) {
    std::optional<std::filesystem::path> scripts_dir =
        Engine::vfs()->GetPhysicalPath("/app/scripts");
    if (scripts_dir.has_value() && std::filesystem::exists(*scripts_dir)) {
      script_watcher_.Watch(*scripts_dir, true);
      LOG_INFO("Watching scripts directory: {}", scripts_dir->string());
    }
  }
}

void EditorLayer::OnDetach() {
  LOG_DEBUG("OnDetach");
  CleanupThumbnailCache();
  editor_camera_.resource_pool.Clear();
  editor_camera_.render_pipeline = nullptr;
  scene_->Cleanup();
}

void EditorLayer::ProcessDeferredActions() {
  if (deferred_action_ == DeferredAction::None) return;

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
  InputManager::SetEnabled(editor_state_ == EditorState::Playing && game_panel_focused_);

  if (editor_state_ == EditorState::Playing) {
    // Process pending scene loads from scripts
    if (SceneManager::Get().HasPendingSceneLoad()) {
      SceneManager::Get().ProcessPendingLoad(scene_);
    }
    scene_->OnUpdate(delta_time);
  } else {
    scene_->OnUpdateEditor(delta_time);

    // Auto-save in edit mode
    if (scene_dirty_ && !current_scene_path_.empty()) {
      auto_save_timer_ += delta_time;
      if (auto_save_timer_ >= kAutoSaveInterval) {
        AutoSave();
      }
    }
  }

  // Poll script file watcher for hot reload
  if (script_watcher_.IsWatching()) {
    script_watch_timer_ += delta_time;
    if (script_watch_timer_ >= kScriptWatchInterval) {
      script_watch_timer_ = 0.0f;
      if (script_watcher_.Poll()) {
        LOG_INFO("Script changes detected, reloading...");
        CleanupThumbnailCache();
        Engine::script_manager().ReloadAsync();
      }
    }
  }
}

bool EditorLayer::OnMouseMoved(MouseMovedEvent& event) {
  if (event.GetCursorMode() == CursorModeRelative) {
    editor_yaw_ -= event.GetDeltaX() * mouse_sensitivity_;
    editor_pitch_ -= event.GetDeltaY() * mouse_sensitivity_;
    editor_pitch_ = glm::clamp(editor_pitch_, -89.0f, 89.0f);
    editor_camera_transform_.rotation = glm::vec3(editor_pitch_, editor_yaw_, 0.0f);
  }
  return false;
}

void EditorLayer::OnEvent(Event& event) {
  EventDispatcher dispatcher(event);
  dispatcher.Dispatch<MouseMovedEvent>(WIESEL_BIND_FN(OnMouseMoved));

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
    scene_->OnEvent(event);
  } else {
    auto type = event.GetEventType();
    if (type == WindowResizeEvent::GetStaticType() ||
        type == PipelineRecreatedEvent::GetStaticType()) {
      scene_->OnEvent(event);
    }
  }
}

static ThumbnailEntry GetOrCreateThumbnail(AssetHandle handle, const AssetMetadata& meta) {
  auto it = thumbnail_cache_.find(handle);
  if (it != thumbnail_cache_.end()) {
    return it->second;
  }

  ThumbnailEntry entry;
  AssetManager& mgr = Engine::asset_manager();

  if (meta.type == AssetType::Texture || meta.type == AssetType::Skybox) {
    std::shared_ptr<Texture> texture = mgr.Get<Texture>(handle);
    if (!texture || !texture->is_allocated_ || !texture->image_view_) {
      return entry;  // Not loaded yet, retry next frame
    }
    entry.texture_id = ImGui_ImplVulkan_AddTexture(
        texture->sampler_, texture->image_view_->handle_,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    entry.attempted = true;
  } else if (meta.type == AssetType::Sprite) {
    std::shared_ptr<SpriteAsset> sprite = mgr.Get<SpriteAsset>(handle);
    if (!sprite || !sprite->IsAllocated() || sprite->GetFrames().empty()) {
      return entry;
    }
    const SpriteAsset::Frame& frame = sprite->GetFrames()[0];
    if (!frame.view || !sprite->GetSampler()) {
      return entry;
    }
    entry.texture_id = ImGui_ImplVulkan_AddTexture(
        sprite->GetSampler()->GetHandle(), frame.view->handle_,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    entry.attempted = true;
  }

  if (entry.attempted) {
    thumbnail_cache_[handle] = entry;
  }
  return entry;
}

void EditorLayer::RenderEntity(Entity& entity, entt::entity entity_id, int depth, bool& ignore_menu) {
  auto& tag_component = entity.GetComponent<TagComponent>();
  bool has_children = entity.child_handles() && !entity.child_handles()->empty();
  bool is_selected = has_selected_entity_ && selected_entity_ == entity_id;

  // Build unique label
  std::string label = tag_component.tag + "##" + std::to_string(static_cast<uint32_t>(entity_id));

  ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
      | ImGuiTreeNodeFlags_SpanAvailWidth
      | ImGuiTreeNodeFlags_FramePadding;
  if (is_selected) {
    flags |= ImGuiTreeNodeFlags_Selected;
  }
  if (!has_children) {
    flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
  }

  bool node_open = ImGui::TreeNodeEx(label.c_str(), flags);

  // Selection on release (not during drag)
  if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(0)
      && !ImGui::IsItemToggledOpen() && !ImGui::IsDragDropActive()) {
    selected_entity_ = entity_id;
    has_selected_entity_ = true;
  }

  // Drag & drop source
  ImGuiDragDropFlags src_flags = ImGuiDragDropFlags_SourceNoDisableHover;
  if (ImGui::BeginDragDropSource(src_flags)) {
    ImGui::Text("%s", tag_component.tag.c_str());
    ImGui::SetDragDropPayload("SceneHierarchy Entity", &entity_id, sizeof(entt::entity));
    ImGui::EndDragDropSource();
  }

  // Drag & drop target: drop ON entity to make it a child
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SceneHierarchy Entity")) {
      hierarchy_data_.move_from = *static_cast<entt::entity*>(payload->Data);
      hierarchy_data_.move_to = entity_id;
      hierarchy_data_.bottom_part = false;
    }
    ImGui::EndDragDropTarget();
  }

  // Accept asset drops onto entities in the hierarchy
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("AssetHandle")) {
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
    if (ImGui::MenuItem("Add Child")) {
      Entity child = scene_->CreateEntity();
      scene_->LinkEntities(entity_id, child);
      scene_dirty_ = true;
    }
    if (ImGui::MenuItem("Save as Prefab...")) {
      Dialogs::SaveFileDialog(
          {{"Wiesel Prefab", "wprefab"}},
          [this, entity_id](const std::string& file) {
            if (file.empty()) return;
            std::filesystem::path path(file);
            if (path.extension() != ".wprefab") {
              path += ".wprefab";
            }
            Entity ent{entity_id, scene_.get()};
            Prefab::SaveToFile(ent, path);
          });
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Delete")) {
      scene_->RemoveEntity(entity);
      has_selected_entity_ = false;
      scene_dirty_ = true;
    }
    ImGui::EndPopup();
    ignore_menu = true;
  }

  // Recurse into children if the tree node is open
  if (has_children && node_open) {
    for (const auto& child_entity_id : *entity.child_handles()) {
      Entity child = {child_entity_id, scene_.get()};
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
  if (!project_) {
    ImGui::DockSpaceOverViewport();
    RenderStartupDialog();
    return;
  }

  ImGuiID dockspace_id = ImGui::DockSpaceOverViewport();

  // Build initial layout once
  static bool layout_initialized = false;
  if (!layout_initialized) {
    layout_initialized = true;

    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

    // Split: left panel (20%) | center+right remainder
    ImGuiID dock_left, dock_remainder;
    ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.20f, &dock_left, &dock_remainder);

    // Split left into top (hierarchy) and bottom (components)
    ImGuiID dock_left_top, dock_left_bottom;
    ImGui::DockBuilderSplitNode(dock_left, ImGuiDir_Up, 0.45f, &dock_left_top, &dock_left_bottom);

    // Split remainder: bottom (asset browser 25%) | center+right
    ImGuiID dock_bottom, dock_center_right;
    ImGui::DockBuilderSplitNode(dock_remainder, ImGuiDir_Down, 0.25f, &dock_bottom, &dock_center_right);

    // Split center_right: right panel (scene props 20%) | center (viewport)
    ImGuiID dock_right, dock_center;
    ImGui::DockBuilderSplitNode(dock_center_right, ImGuiDir_Right, 0.20f, &dock_right, &dock_center);

    // Dock windows
    ImGui::DockBuilderDockWindow("Scene Hierarchy", dock_left_top);
    ImGui::DockBuilderDockWindow("Components", dock_left_bottom);
    ImGui::DockBuilderDockWindow("Game", dock_center);
    ImGui::DockBuilderDockWindow("Scene", dock_center);
    // Select Scene tab by default
    ImGuiID scene_window_id = ImHashStr("Scene");
    ImGui::DockBuilderGetNode(dock_center)->SelectedTabId = scene_window_id;
    ImGui::DockBuilderDockWindow("Scene Properties", dock_right);
    ImGui::DockBuilderDockWindow("Asset Browser", dock_bottom);
    ImGui::DockBuilderDockWindow("Developer Console", dock_bottom);
    ImGui::DockBuilderDockWindow("Render Stats", dock_right);

    ImGui::DockBuilderFinish(dockspace_id);
  }

  static bool scenePropertiesOpen = true;
  if (ImGui::Begin("Scene Properties", &scenePropertiesOpen)) {
    auto& settings = renderer->options();

    ImGui::SeparatorText("Debug");
    ImGui::Checkbox(PrefixLabel("Wireframe Mode").c_str(), &settings.wireframe_enabled);
    ImGui::Checkbox(PrefixLabel("Only SSAO").c_str(), &settings.only_ssao);
    {
      const char* debug_modes[] = {"Off", "Cascades", "Material", "Normals", "World Pos", "Raw Normal", "Albedo", "Depth", "Vertex Normal"};
      int debug_mode = settings.debug_cascades;
      if (ImGui::Combo(PrefixLabel("Debug View").c_str(), &debug_mode, debug_modes, 9)) {
        settings.debug_cascades = debug_mode;
      }
    }
    ImGui::Checkbox(PrefixLabel("Debug Colliders").c_str(), &settings.debug_colliders);

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

  RenderProjectSettingsPopup();

  static bool sceneOpen = true;
  if (ImGui::Begin("Scene Hierarchy", &sceneOpen)) {
    bool ignoreMenu = false;

    // Prefab editing banner
    if (editing_prefab_) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 0.9f, 1.0f));
      float width = ImGui::GetContentRegionAvail().x;
      ImGui::Text("Editing: %s", editing_prefab_path_.filename().string().c_str());
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

    // Scene root node (always open, not collapsible)
    ImGuiTreeNodeFlags scene_flags = ImGuiTreeNodeFlags_DefaultOpen
        | ImGuiTreeNodeFlags_OpenOnArrow
        | ImGuiTreeNodeFlags_SpanAvailWidth
        | ImGuiTreeNodeFlags_FramePadding
        | ImGuiTreeNodeFlags_Framed;
    bool scene_open = ImGui::TreeNodeEx("##SceneRoot", scene_flags, "Scene");

    // Right-click on scene root to add entities
    if (ImGui::BeginPopupContextItem("scene_root_context")) {
      if (ImGui::MenuItem("Add Empty Entity")) {
        scene_->CreateEntity();
        scene_dirty_ = true;
      }
      ImGui::EndPopup();
      ignoreMenu = true;
    }

    if (scene_open) {
      for (const auto& entityId : scene_->GetSceneHierarchy()) {
        Entity entity = {entityId, scene_.get()};
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
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SceneHierarchy Entity")) {
          entt::entity dropped = *static_cast<entt::entity*>(payload->Data);
          Entity dropped_entity = {dropped, scene_.get()};
          if (dropped_entity.parent_handle() != entt::null) {
            scene_->UnlinkEntities(dropped_entity.parent_handle(), dropped);
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
      if (!ignoreMenu && ImGui::IsWindowHovered() && ImGui::IsMouseClicked(1, false))
        ImGui::OpenPopup("right_click_hierarchy");
      if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0) && !ImGui::IsAnyItemHovered()) {
        has_selected_entity_ = false;
      }
    }

    if (ImGui::BeginPopup("right_click_hierarchy")) {
      if (ImGui::MenuItem("Add Empty Entity")) {
        scene_->CreateEntity();
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }
  }
  ImGui::End();

  static bool componentsOpen = true;
  if (ImGui::Begin("Components", &componentsOpen) && has_selected_entity_) {
    Entity entity = {selected_entity_, scene_.get()};
    TagComponent& tag = entity.GetComponent<TagComponent>();
    if (ImGui::InputText("##", &tag.tag, ImGuiInputTextFlags_AutoSelectAll)) {
      if (tag.tag[0] == ' ') {
        TrimLeft(tag.tag);
      }

      if (tag.tag.empty()) {
        tag.tag = "Entity";
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
  static bool assetBrowserOpen = true;
  if (ImGui::Begin("Asset Browser", &assetBrowserOpen)) {
    auto& mgr = Engine::asset_manager();

    static std::string current_dir;  // relative to assets dir, e.g. "" or "models/" or "scenes/"
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
          if (entry.is_regular_file() && entry.path().extension() == ".meta") continue;

          FileEntry fe;
          fe.name = entry.path().filename().string();
          fe.is_dir = entry.is_directory();
          fe.physical_path = entry.path();
          fe.asset_type = AssetType::None;

          if (!fe.is_dir) {
            auto ext = entry.path().extension().string();
            fe.asset_type = ProjectLoader::ExtToAssetType(ext);
            if (fe.asset_type == AssetType::None) {
              if (ext == ".cs") fe.asset_type = AssetType::Script;
            }
          }

          entries.push_back(fe);
        }
      }
      // Sort: directories first, then files, both alphabetically
      std::ranges::sort(entries, [](const FileEntry& a, const FileEntry& b) {
        if (a.is_dir != b.is_dir) return a.is_dir > b.is_dir;
        return a.name < b.name;
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
          if (part.empty()) continue;
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
      ImGui::InputText("##foldername", new_folder_name, sizeof(new_folder_name));
      if (ImGui::Button("Create") && new_folder_name[0] != '\0') {
        namespace fs = std::filesystem;
        auto physical_app = Engine::vfs()->GetPhysicalPath("/app");
        if (physical_app.has_value()) {
          fs::path base = fs::absolute(*physical_app);
          if (!current_dir.empty()) {
            std::string rel = current_dir;
            if (rel.rfind("/app/", 0) == 0) rel = rel.substr(5);
            else if (rel.rfind("/", 0) == 0) rel = rel.substr(1);
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
    // Import file into the current asset browser directory
    auto ImportFileToCurrentDir = [](const std::string& file,
                                     AssetType type) {
      namespace fs = std::filesystem;
      if (file.empty()) return;

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
        if (rel.rfind("/app/", 0) == 0) rel = rel.substr(5);
        else if (rel.rfind("/", 0) == 0) rel = rel.substr(1);
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
          if (!entry.is_regular_file()) continue;
          fs::path file_dest = model_dest_dir / entry.path().filename();
          fs::copy_file(entry.path(), file_dest, fs::copy_options::skip_existing, ec);
          if (ec) {
            LOG_WARN("Failed to copy '{}': {}", entry.path().string(), ec.message());
            ec.clear();
          }
        }
        // Also copy subdirectories (some models have texture subfolders)
        for (const auto& entry : fs::recursive_directory_iterator(source_dir)) {
          if (entry.is_directory()) continue;
          auto rel_to_source = fs::relative(entry.path(), source_dir);
          fs::path file_dest = model_dest_dir / rel_to_source;
          fs::create_directories(file_dest.parent_path(), ec);
          fs::copy_file(entry.path(), file_dest, fs::copy_options::skip_existing, ec);
          ec.clear();
        }
        // Register the main model file
        auto vfs_rel = fs::relative(model_dest_dir / abs.filename(), app_assets);
        std::string vfs_path = "/app/" + vfs_rel.generic_string();
        std::string name = abs.stem().string();
        Engine::asset_manager().Register(name, type, vfs_path);
        LOG_INFO("Imported model directory {} to {}", name, vfs_path);
      } else {
        fs::path dest = dest_dir / abs.filename();
        fs::copy_file(abs, dest, fs::copy_options::skip_existing, ec);
        if (ec) {
          LOG_ERROR("Failed to import '{}' to '{}': {}", file, dest.string(), ec.message());
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

    // Content area
    if (ImGui::BeginChild("asset_content", ImVec2(0, 0), ImGuiChildFlags_None)) {
      float panel_width = ImGui::GetContentRegionAvail().x;
      float cell_size = tile_size + 8.0f;
      int columns = std::max(1, (int)(panel_width / cell_size));
      int col = 0;

      auto DrawTile = [&](const char* label, ImVec4 icon_color,
                          const char* type_abbrev, bool is_selected,
                          bool is_folder,
                          VkDescriptorSet thumbnail = nullptr,
                          const AssetMetadata* asset_meta = nullptr,
                          bool* double_clicked = nullptr) -> bool {
        bool clicked = false;
        ImGui::PushID(label);

        ImVec2 cursor = ImGui::GetCursorScreenPos();
        ImVec2 icon_min = cursor;
        ImVec2 icon_max = ImVec2(cursor.x + tile_size, cursor.y + tile_size);

        // Invisible button for interaction
        if (ImGui::InvisibleButton("##tile", ImVec2(tile_size, tile_size + 20))) {
          clicked = true;
        }
        if (double_clicked && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
          *double_clicked = true;
        }
        bool hovered = ImGui::IsItemHovered();

        ImDrawList* dl = ImGui::GetWindowDrawList();

        // Selection/hover highlight
        if (is_selected) {
          dl->AddRectFilled(
              ImVec2(icon_min.x - 2, icon_min.y - 2),
              ImVec2(icon_max.x + 2, icon_max.y + 22),
              IM_COL32(60, 100, 160, 180), 4.0f);
        } else if (hovered) {
          dl->AddRectFilled(
              ImVec2(icon_min.x - 2, icon_min.y - 2),
              ImVec2(icon_max.x + 2, icon_max.y + 22),
              IM_COL32(70, 70, 70, 120), 4.0f);
        }

        // Icon: thumbnail image or colored rectangle
        if (thumbnail) {
          dl->AddImageRounded(reinterpret_cast<ImTextureID>(thumbnail), icon_min, icon_max,
                              ImVec2(0, 0), ImVec2(1, 1),
                              IM_COL32_WHITE, 6.0f);
        } else {
          ImU32 col32 = ImGui::ColorConvertFloat4ToU32(icon_color);
          dl->AddRectFilled(icon_min, icon_max, col32, is_folder ? 2.0f : 6.0f);

          // Type abbreviation centered in icon
          if (type_abbrev && type_abbrev[0]) {
            ImVec2 text_sz = ImGui::CalcTextSize(type_abbrev);
            ImVec2 text_pos(
                icon_min.x + (tile_size - text_sz.x) * 0.5f,
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
              case AssetLoadState::Unloaded: state_str = "Unloaded"; break;
              case AssetLoadState::Loading:  state_str = "Loading";  break;
              case AssetLoadState::Loaded:   state_str = "Loaded";   break;
              case AssetLoadState::Failed:   state_str = "Failed";   break;
            }
            ImGui::Text("State: %s", state_str);

            auto physical_path = Engine::vfs()->GetPhysicalPath(asset_meta->virtual_source_path);
            ImGui::Text("Source: %s", physical_path.has_value() ? "Filesystem" : "Archive");

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
          case AssetType::Texture:  return {0.25f, 0.45f, 0.72f, 1.0f};
          case AssetType::Model:    return {0.30f, 0.62f, 0.35f, 1.0f};
          case AssetType::Material: return {0.72f, 0.50f, 0.20f, 1.0f};
          case AssetType::Shader:   return {0.55f, 0.30f, 0.68f, 1.0f};
          case AssetType::Sprite:   return {0.20f, 0.60f, 0.65f, 1.0f};
          case AssetType::Skybox:   return {0.25f, 0.55f, 0.55f, 1.0f};
          case AssetType::Font:     return {0.65f, 0.60f, 0.25f, 1.0f};
          case AssetType::Script:   return {0.55f, 0.70f, 0.30f, 1.0f};
          case AssetType::Scene:    return {0.72f, 0.35f, 0.35f, 1.0f};
          case AssetType::Prefab:   return {0.45f, 0.55f, 0.72f, 1.0f};
          case AssetType::Audio:    return {0.72f, 0.45f, 0.60f, 1.0f};
          default:                  return {0.40f, 0.40f, 0.40f, 1.0f};
        }
      };

      auto GetAssetAbbrev = [](AssetType type) -> const char* {
        switch (type) {
          case AssetType::Texture:  return "TEX";
          case AssetType::Model:    return "MDL";
          case AssetType::Material: return "MAT";
          case AssetType::Shader:   return "SHD";
          case AssetType::Sprite:   return "SPR";
          case AssetType::Skybox:   return "SKY";
          case AssetType::Font:     return "FNT";
          case AssetType::Script:   return "CS";
          case AssetType::Scene:    return "SCN";
          case AssetType::Prefab:   return "PFB";
          case AssetType::Audio:    return "SND";
          default:                  return "?";
        }
      };

      // Rename state
      static std::string renaming_file;
      static char rename_buf[256] = "";

      // ".." back folder
      if (!current_dir.empty()) {
        if (DrawTile("..", ImVec4(0.35f, 0.35f, 0.4f, 1.0f), "..",
                     false, true)) {
          std::string trimmed = current_dir;
          if (!trimmed.empty() && trimmed.back() == '/')
            trimmed.pop_back();
          size_t slash = trimmed.find_last_of('/');
          if (slash == std::string::npos) {
            current_dir = "";
          } else {
            current_dir = trimmed.substr(0, slash + 1);
          }
        }
        // Drop target on ".." to move files to parent directory
        if (ImGui::BeginDragDropTarget()) {
          if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("BrowserFile")) {
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

          VkDescriptorSet thumbnail = nullptr;
          if (meta && (meta->type == AssetType::Texture || meta->type == AssetType::Sprite)) {
            ThumbnailEntry thumb = GetOrCreateThumbnail(handle, *meta);
            thumbnail = thumb.texture_id;
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
          if (handle.IsValid() && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            ImGui::SetDragDropPayload("AssetHandle", &handle, sizeof(AssetHandle));
            ImGui::Text("%s", fe.name.c_str());
            ImGui::EndDragDropSource();
          }
        }

        // Drag source for moving files/folders in the browser
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
          std::string path_str = fe.physical_path.string();
          ImGui::SetDragDropPayload("BrowserFile", path_str.c_str(), path_str.size() + 1);
          ImGui::Text("%s %s", fe.is_dir ? "[DIR]" : "", fe.name.c_str());
          ImGui::EndDragDropSource();
        }

        // Drop target on directories to move files into them
        if (fe.is_dir && ImGui::BeginDragDropTarget()) {
          if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("BrowserFile")) {
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
        if (is_sel && ImGui::BeginPopupContextItem(("##ctx_" + fe.name).c_str())) {
          // Import option for unimported files
          if (!fe.is_dir && !handle.IsValid() && fe.asset_type != AssetType::None) {
            if (ImGui::MenuItem("Import")) {
              std::string import_vfs = "/app/" + current_dir + fe.name;
              AssetHandle new_handle = mgr.Register(fe.name, fe.asset_type, import_vfs);
              if (new_handle.IsValid()) {
                namespace fs = std::filesystem;
                fs::path meta_path = fe.physical_path.string() + ".meta";
                ProjectLoader::WriteMetaFile(meta_path, new_handle);
                if (fe.asset_type == AssetType::Prefab || fe.asset_type == AssetType::Scene) {
                  mgr.SetLoadState(new_handle, AssetLoadState::Unloaded, AssetLoadState::Loaded);
                }
              }
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
            fs::path copy_path = fe.physical_path.parent_path() / (stem + "_copy" + ext);
            int n = 1;
            while (fs::exists(copy_path)) {
              copy_path = fe.physical_path.parent_path() / (stem + "_copy" + std::to_string(n++) + ext);
            }
            std::error_code ec;
            fs::copy_file(fe.physical_path, copy_path, ec);
            if (!ec) ScanProjectAssets();
          }
          if (fe.is_dir && ImGui::MenuItem("Duplicate")) {
            namespace fs = std::filesystem;
            std::string name = fe.name + "_copy";
            fs::path copy_path = fe.physical_path.parent_path() / name;
            int n = 1;
            while (fs::exists(copy_path)) {
              copy_path = fe.physical_path.parent_path() / (fe.name + "_copy" + std::to_string(n++));
            }
            std::error_code ec;
            fs::copy(fe.physical_path, copy_path, fs::copy_options::recursive, ec);
            if (!ec) ScanProjectAssets();
          }
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
            fs::path old_path = fs::absolute(*physical_app_path) / current_dir / renaming_file;
            std::string ext = old_path.extension().string();
            fs::path new_path = old_path.parent_path() / (std::string(rename_buf) + ext);
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
      if (ImGui::BeginPopupContextWindow("##browser_ctx", ImGuiPopupFlags_NoOpenOverItems)) {
        if (ImGui::MenuItem("New Scene")) {
          NewScene();
        }
        if (ImGui::MenuItem("New Folder")) {
          ImGui::CloseCurrentPopup();
          // Will be handled by the NewFolderPopup
        }
        ImGui::EndPopup();
      }
    }
    ImGui::EndChild();
  }
  ImGui::End();

  // Developer Console Panel
  {
    static bool console_open = true;
    static std::vector<std::string> history;
    static int history_pos = -1;  // -1 = new line, 0..N = browsing history
    static char input_buf[512] = "";

    auto HistoryCallback = [](ImGuiInputTextCallbackData* data) -> int {
      if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
        if (history.empty()) return 0;
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
        const char* text = (history_pos >= 0) ? history[history_pos].c_str() : "";
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
      float footer_height = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
      if (ImGui::BeginChild("ConsoleLog", ImVec2(0, -footer_height), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar)) {
        for (const auto& line : log) {
          ImVec4 color;
          switch (line.level) {
            case ConsoleLogLevel::Warning: color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f); break;
            case ConsoleLogLevel::Error:   color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f); break;
            default:                color = ImVec4(0.8f, 0.8f, 0.8f, 1.0f); break;
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
      ImGuiInputTextFlags input_flags = ImGuiInputTextFlags_EnterReturnsTrue
          | ImGuiInputTextFlags_CallbackHistory;
      ImGui::SetNextItemWidth(-1);
      if (ImGui::InputText("##ConsoleInput", input_buf, sizeof(input_buf), input_flags, HistoryCallback)) {
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

  // Render Stats Panel
  {
    static bool stats_open = true;
    if (ImGui::Begin("Render Stats", &stats_open)) {
      auto renderer = Engine::renderer();
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
      ImGui::Text("MSAA: %s", renderer->options().msaa_mode == SamplingMode::DISABLED ? "Off"
          : renderer->options().msaa_mode == SamplingMode::X2 ? "2x"
          : renderer->options().msaa_mode == SamplingMode::X4 ? "4x" : "8x");
      ImGui::Text("VSync: %s", renderer->options().vsync ? "On" : "Off");
      ImGui::Text("AA Mode: %s", renderer->options().aa_mode == AntiAliasingMode::None ? "None"
          : renderer->options().aa_mode == AntiAliasingMode::FXAA ? "FXAA" : "TAA");
      ImGui::Text("Swap Chain Images: %u", stats.swap_chain_images);
      ImGui::Text("Frames in Flight: %u", stats.frames_in_flight);

      ImGui::SeparatorText("Assets");
      auto asset_stats = Engine::asset_manager().GetStats();
      ImGui::Text("Total: %zu", asset_stats.total);
      ImGui::Text("Loaded: %zu", asset_stats.loaded);
      if (asset_stats.loading > 0) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Loading: %zu", asset_stats.loading);
      } else {
        ImGui::Text("Loading: %zu", asset_stats.loading);
      }
      ImGui::Text("Unloaded: %zu", asset_stats.unloaded);
      if (asset_stats.failed > 0) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Failed: %zu", asset_stats.failed);
      }

      if (scene_) {
        ImGui::SeparatorText("Render Pipelines");

        // Collect unique pipelines and their cameras
        struct PipelineInfo {
          RenderPipeline* pipeline;
          std::vector<std::string> cameras;
          bool is_default;
        };
        std::map<RenderPipeline*, PipelineInfo> pipeline_map;

        auto default_pipeline = scene_->GetDefaultPipeline();
        if (default_pipeline) {
          pipeline_map[default_pipeline.get()] = {default_pipeline.get(), {}, true};
        }

        for (const auto& entity : scene_->GetAllEntitiesWith<CameraComponent, TagComponent>()) {
          auto& cam = scene_->GetComponent<CameraComponent>(entity);
          auto& tag = scene_->GetComponent<TagComponent>(entity);
          RenderPipeline* pl = cam.render_pipeline
              ? cam.render_pipeline.get()
              : default_pipeline.get();
          if (pl) {
            auto& info = pipeline_map[pl];
            info.pipeline = pl;
            info.cameras.push_back(tag.tag);
            if (pl == default_pipeline.get()) {
              info.is_default = true;
            }
          }
        }

        for (const auto& [ptr, info] : pipeline_map) {
          std::string label = info.is_default ? "Default Pipeline" : "Custom Pipeline";
          if (ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
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
              auto ext_graph = scene_->GetExternalRenderGraph();
              if (ext_graph) {
                timings = ext_graph->GetPassTimings();
              }
            } else {
              for (const auto& entity : scene_->GetAllEntitiesWith<CameraComponent>()) {
                auto& cam = scene_->GetComponent<CameraComponent>(entity);
                RenderPipeline* cam_pl = cam.render_pipeline
                    ? cam.render_pipeline.get()
                    : default_pipeline.get();
                if (cam_pl != ptr) continue;

                auto graph = scene_->GetRenderGraph(entity);
                if (graph) {
                  timings = graph->GetPassTimings();
                }
                break;
              }
              // Fallback to external render graph if no ECS camera graph found
              if (timings.empty()) {
                auto ext_graph = scene_->GetExternalRenderGraph();
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
                ImGui::BulletText("%-20s  CPU %.3f ms",
                    fname.c_str(), cpu_total);
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
              ImGui::Text("  Total: CPU %.3f ms  GPU %.3f ms", total_cpu, total_gpu);
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

  // Helper lambda: draws Play/Stop buttons, returns true if state changed
  auto DrawPlayStopButtons = [&]() -> bool {
    bool changed = false;
    if (editor_state_ == EditorState::Edit) {
      if (ImGui::Button("Play")) {
        AutoSave();
        TakeSnapshot();
        editor_state_ = EditorState::Playing;
        scene_->ResetFirstUpdate();
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

  static bool sceneViewOpen = true;
  ImGuiWindowFlags sceneFlags = ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar;
  scene_panel_visible_ = ImGui::Begin("Scene", &sceneViewOpen, sceneFlags);
  if (scene_panel_visible_) {
    // Play/Stop buttons + gizmo controls
    DrawPlayStopButtons();
    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();
    if (ImGui::RadioButton("Translate", current_op_ == ImGuizmo::TRANSLATE)) current_op_ = ImGuizmo::TRANSLATE;
    ImGui::SameLine();
    if (ImGui::RadioButton("Rotate",    current_op_ == ImGuizmo::ROTATE))    current_op_ = ImGuizmo::ROTATE;
    ImGui::SameLine();
    if (ImGui::RadioButton("Scale",     current_op_ == ImGuizmo::SCALE))     current_op_ = ImGuizmo::SCALE;
    {
      float rightEdge = ImGui::GetWindowContentRegionMax().x;
      ImGui::SameLine(rightEdge - 24.0f);
      if (ImGui::Button("...##SceneSettings")) {
        ImGui::OpenPopup("SceneCameraSettings");
      }
      if (ImGui::BeginPopup("SceneCameraSettings")) {
        ImGui::SeparatorText("Camera");
        ImGui::SetNextItemWidth(130.0f);
        if (ImGui::BeginCombo("Resolution", kResolutionPresets[resolution_preset_index_].label)) {
          for (int i = 0; i < kResolutionPresetCount; i++) {
            bool selected = (i == resolution_preset_index_);
            if (ImGui::Selectable(kResolutionPresets[i].label, selected)) {
              resolution_preset_index_ = i;
              scene_->SetRenderResolution(kResolutionPresets[i].size);
            }
            if (selected) ImGui::SetItemDefaultFocus();
          }
          ImGui::EndCombo();
        }
        ImGui::DragFloat("Speed", &camera_speed_, 0.5f, 0.1f, 100.0f);
        ImGui::DragFloat("Sensitivity", &mouse_sensitivity_, 1.0f, 10.0f, 500.0f);
        ImGui::SeparatorText("Overlays");
        ImGui::EndPopup();
      }
    }

    // Editor camera output from its own resource pool
    auto editorDesc = editor_camera_.resource_pool.GetDescriptor("PipelineOutputDescriptor");
    auto editorImage = editor_camera_.resource_pool.GetTexture("PipelineOutput");

    // Handle viewport resize
    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (scene_->GetRenderResolution().x <= 0) {
      // Free Aspect: editor camera tracks panel size (old behavior)
      uint32_t newW = static_cast<uint32_t>(avail.x);
      uint32_t newH = static_cast<uint32_t>(avail.y);
      if (newW > 0 && newH > 0 &&
          (newW != editor_camera_.viewport_size.x || newH != editor_camera_.viewport_size.y)) {
        editor_camera_.viewport_size = {newW, newH};
        editor_camera_.aspect_ratio = static_cast<float>(newW) / static_cast<float>(newH);
        editor_camera_.view_changed = true;
        editor_camera_.resources_dirty = true;
      }
    }
    // When a preset is active, RenderFromExternal applies render_resolution_ automatically

    if (editorDesc && editorImage) {
      ImTextureID desc =
          reinterpret_cast<ImTextureID>(editorDesc->descriptor_set_);

      float imageAspect = (float)editorImage->width_ / (float)editorImage->height_;
      float availAspect = avail.x / avail.y;

      ImVec2 drawSize;
      if (availAspect > imageAspect) {
        drawSize.y = avail.y;
        drawSize.x = drawSize.y * imageAspect;
      } else {
        drawSize.x = avail.x;
        drawSize.y = drawSize.x / imageAspect;
      }

      ImGui::Image(desc, drawSize);

      ImVec2 imageMin = ImGui::GetItemRectMin();
      ImVec2 imageMax = ImGui::GetItemRectMax();
      bool scene_hovered = ImGui::IsItemHovered();

      // FPS overlay (top-left)
      ImVec2 textPos = ImVec2(imageMin.x + 6, imageMin.y + 6);
      std::string fpsStr = std::format("FPS: {}", static_cast<int>(app_.GetFPS()));
      ImGui::GetWindowDrawList()->AddText(textPos, IM_COL32(0, 255, 0, 255), fpsStr.c_str());

      // Resolution overlay (top-right)
      std::string resStr = std::format("{}x{}", (int)editor_camera_.viewport_size.x, (int)editor_camera_.viewport_size.y);
      ImVec2 resTextSize = ImGui::CalcTextSize(resStr.c_str());
      ImVec2 resPos = ImVec2(imageMax.x - resTextSize.x - 6, imageMin.y + 6);
      ImGui::GetWindowDrawList()->AddText(resPos, IM_COL32(0, 255, 0, 255), resStr.c_str());

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

      // WASD camera movement (works when scene panel is focused or right-click dragging)
      if (scene_focused || scene_right_active) {
        ImGui::GetIO().WantCaptureKeyboard = false;
        ImGuiIO& io = ImGui::GetIO();
        float dt = io.DeltaTime;

        glm::quat q = glm::quat(glm::radians(editor_camera_transform_.rotation));
        glm::mat4 R = glm::toMat4(q);
        glm::vec3 cam_right   =  glm::vec3(R[0]); // local +X
        glm::vec3 cam_forward = -glm::vec3(R[2]); // local -Z = look direction

        float speed = camera_speed_ * dt;
        if (io.KeyShift) speed *= 3.0f;

        if (ImGui::IsKeyDown(ImGuiKey_W)) editor_camera_transform_.position += cam_forward * speed;
        if (ImGui::IsKeyDown(ImGuiKey_S)) editor_camera_transform_.position -= cam_forward * speed;
        if (ImGui::IsKeyDown(ImGuiKey_D)) editor_camera_transform_.position += cam_right * speed;
        if (ImGui::IsKeyDown(ImGuiKey_A)) editor_camera_transform_.position -= cam_right * speed;
        if (ImGui::IsKeyDown(ImGuiKey_E)) editor_camera_transform_.position.y += speed;
        if (ImGui::IsKeyDown(ImGuiKey_Q)) editor_camera_transform_.position.y -= speed;
      }

      // Scroll to zoom (even without right-click)
      if (scene_hovered) {
        float scroll = ImGui::GetIO().MouseWheel;
        if (std::abs(scroll) > 0.01f) {
          glm::quat q = glm::quat(glm::radians(editor_camera_transform_.rotation));
          glm::mat4 R = glm::toMat4(q);
          glm::vec3 cam_forward = -glm::vec3(R[2]);
          editor_camera_transform_.position += cam_forward * scroll * camera_speed_ * 0.3f;
        }
      }

      // ImGuizmo (uses editor camera matrices, disabled during right-click camera)
      if (has_selected_entity_ && !scene_right_active) {
        glm::mat4 view = editor_camera_.view_matrix;
        glm::mat4 proj = editor_camera_.projection;
        proj[1][1] *= -1;
        TransformComponent& transform = scene_->GetComponent<TransformComponent>(selected_entity_);
        glm::mat4& model = transform.transform_matrix;
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(imageMin.x, imageMin.y, drawSize.x, drawSize.y);
        if (ImGuizmo::Manipulate(
                glm::value_ptr(view),
                glm::value_ptr(proj),
                current_op_,
                ImGuizmo::WORLD,
                glm::value_ptr(model))) {
          glm::vec3 translation, rotation, scale;
          ImGuizmo::DecomposeMatrixToComponents(
              glm::value_ptr(model),
              glm::value_ptr(translation),
              glm::value_ptr(rotation),
              glm::value_ptr(scale));

          transform.position = translation;
          transform.rotation = rotation;
          transform.scale = scale;
          scene_dirty_ = true;
        }

        // Draw collider wireframes for selected entity
        glm::mat4 vp = proj * view;
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        auto projectPoint = [&](glm::vec3 worldPos) -> ImVec2 {
          glm::vec4 clip = vp * glm::vec4(worldPos, 1.0f);
          if (clip.w <= 0.001f) return ImVec2(-9999, -9999);
          glm::vec3 ndc = glm::vec3(clip) / clip.w;
          return ImVec2(
              imageMin.x + (ndc.x * 0.5f + 0.5f) * drawSize.x,
              imageMin.y + (-ndc.y * 0.5f + 0.5f) * drawSize.y);
        };

        auto drawLine3D = [&](glm::vec3 a, glm::vec3 b, ImU32 color) {
          ImVec2 sa = projectPoint(a);
          ImVec2 sb = projectPoint(b);
          drawList->AddLine(sa, sb, color, 1.5f);
        };

        if (scene_->HasComponent<BoxColliderComponent>(selected_entity_)) {
          auto& box = scene_->GetComponent<BoxColliderComponent>(selected_entity_);
          glm::vec3 center = transform.position + box.offset;
          glm::vec3 h = box.half_extents;
          glm::vec3 corners[8] = {
              center + glm::vec3(-h.x, -h.y, -h.z),
              center + glm::vec3( h.x, -h.y, -h.z),
              center + glm::vec3( h.x,  h.y, -h.z),
              center + glm::vec3(-h.x,  h.y, -h.z),
              center + glm::vec3(-h.x, -h.y,  h.z),
              center + glm::vec3( h.x, -h.y,  h.z),
              center + glm::vec3( h.x,  h.y,  h.z),
              center + glm::vec3(-h.x,  h.y,  h.z),
          };
          ImU32 col = IM_COL32(0, 255, 0, 200);
          drawLine3D(corners[0], corners[1], col);
          drawLine3D(corners[1], corners[2], col);
          drawLine3D(corners[2], corners[3], col);
          drawLine3D(corners[3], corners[0], col);
          drawLine3D(corners[4], corners[5], col);
          drawLine3D(corners[5], corners[6], col);
          drawLine3D(corners[6], corners[7], col);
          drawLine3D(corners[7], corners[4], col);
          drawLine3D(corners[0], corners[4], col);
          drawLine3D(corners[1], corners[5], col);
          drawLine3D(corners[2], corners[6], col);
          drawLine3D(corners[3], corners[7], col);
        }

        if (scene_->HasComponent<SphereColliderComponent>(selected_entity_)) {
          auto& sphere = scene_->GetComponent<SphereColliderComponent>(selected_entity_);
          glm::vec3 center = transform.position + sphere.offset;
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
              drawLine3D(p0, p1, col);
            }
          }
        }
      }

      // 2D canvas element selection highlight
      if (has_selected_entity_ &&
          scene_->HasComponent<RectangleTransformComponent>(selected_entity_)) {
        auto& rt = scene_->GetComponent<RectangleTransformComponent>(selected_entity_);
        float renderW = static_cast<float>(editorImage->width_);
        float renderH = static_cast<float>(editorImage->height_);
        float scaleX = drawSize.x / renderW;
        float scaleY = drawSize.y / renderH;

        ImVec2 rMin(imageMin.x + rt.computed_position.x * scaleX,
                    imageMin.y + rt.computed_position.y * scaleY);
        ImVec2 rMax(imageMin.x + (rt.computed_position.x + rt.computed_size.x) * scaleX,
                    imageMin.y + (rt.computed_position.y + rt.computed_size.y) * scaleY);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImU32 outlineCol = IM_COL32(50, 150, 255, 230);
        ImU32 fillCol = IM_COL32(50, 150, 255, 30);
        drawList->AddRectFilled(rMin, rMax, fillCol);
        drawList->AddRect(rMin, rMax, outlineCol, 0.0f, 0, 2.0f);

        // Corner handles
        float handleSize = 4.0f;
        ImU32 handleCol = IM_COL32(255, 255, 255, 255);
        ImVec2 corners[4] = {rMin, {rMax.x, rMin.y}, rMax, {rMin.x, rMax.y}};
        for (auto& c : corners) {
          drawList->AddRectFilled(
              ImVec2(c.x - handleSize, c.y - handleSize),
              ImVec2(c.x + handleSize, c.y + handleSize),
              handleCol);
          drawList->AddRect(
              ImVec2(c.x - handleSize, c.y - handleSize),
              ImVec2(c.x + handleSize, c.y + handleSize),
              outlineCol, 0.0f, 0, 1.0f);
        }
      }

      // Entity picking: click on Scene panel to select (only when not right-clicking)
      if (!scene_right_active && ImGui::IsMouseClicked(0) && scene_hovered &&
          !ImGuizmo::IsUsing() && !ImGuizmo::IsOver()) {
        ImVec2 mouse = ImGui::GetIO().MousePos;
        float relX = mouse.x - imageMin.x;
        float relY = mouse.y - imageMin.y;
        if (relX >= 0 && relY >= 0 && relX < drawSize.x && relY < drawSize.y) {
          uint32_t renderW = editorImage->width_;
          uint32_t renderH = editorImage->height_;
          uint32_t px = static_cast<uint32_t>(relX * renderW / drawSize.x);
          uint32_t py = static_cast<uint32_t>(relY * renderH / drawSize.y);
          auto entity_id_tex = editor_camera_.resource_pool.GetTexture(
              "geometry.entity_id_resolve");
          if (entity_id_tex) {
            renderer->RequestEntityPick(px, py, entity_id_tex);
          }
        }
      }
    }
  }
  ImGui::End();

  // ======== Game Panel (primary scene camera, only when playing) ========
  static bool gameViewOpen = true;
  {
    bool gameVisible = ImGui::Begin("Game", &gameViewOpen);
    game_panel_visible_ = gameVisible;
    game_panel_focused_ = ImGui::IsWindowFocused();
    if (gameVisible) {
      DrawPlayStopButtons();
      {
        float comboWidth = 130.0f;
        float rightEdge = ImGui::GetWindowContentRegionMax().x;
        ImGui::SameLine(rightEdge - comboWidth);
        ImGui::SetNextItemWidth(comboWidth);
        if (ImGui::BeginCombo("##GameResolution", kResolutionPresets[resolution_preset_index_].label)) {
          for (int i = 0; i < kResolutionPresetCount; i++) {
            bool selected = (i == resolution_preset_index_);
            if (ImGui::Selectable(kResolutionPresets[i].label, selected)) {
              resolution_preset_index_ = i;
              scene_->SetRenderResolution(kResolutionPresets[i].size);
            }
            if (selected) ImGui::SetItemDefaultFocus();
          }
          ImGui::EndCombo();
        }
      }

      if (editor_state_ == EditorState::Playing) {
        // Check if any camera exists
        bool has_camera = false;
        for (auto entity : scene_->GetAllEntitiesWith<CameraComponent>()) {
          auto& cam = scene_->GetComponent<CameraComponent>(entity);
          if (cam.enabled) { has_camera = true; break; }
        }

        if (!has_camera) {
          ImVec2 avail = ImGui::GetContentRegionAvail();
          const char* text = "No camera in scene";
          ImVec2 textSize = ImGui::CalcTextSize(text);
          ImGui::SetCursorPos(ImVec2(
              ImGui::GetCursorPosX() + (avail.x - textSize.x) * 0.5f,
              ImGui::GetCursorPosY() + (avail.y - textSize.y) * 0.5f));
          ImGui::TextDisabled("%s", text);
        } else {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        if (scene_->GetRenderResolution().x <= 0) {
          // Free Aspect: game camera tracks panel size (old behavior)
          for (auto entity : scene_->GetAllEntitiesWith<CameraComponent>()) {
            auto& cam = scene_->GetComponent<CameraComponent>(entity);
            if (!cam.enabled) continue;
            uint32_t w = static_cast<uint32_t>(avail.x);
            uint32_t h = static_cast<uint32_t>(avail.y);
            if (w > 0 && h > 0 &&
                (w != static_cast<uint32_t>(cam.viewport_size.x) ||
                 h != static_cast<uint32_t>(cam.viewport_size.y))) {
              cam.viewport_size = {w, h};
              cam.aspect_ratio = static_cast<float>(w) / static_cast<float>(h);
              cam.view_changed = true;
              cam.resources_dirty = true;
            }
            break;  // only first enabled camera
          }
        }
        // When a preset is active, Scene::Render() applies render_resolution_ automatically

        auto finalOutputDesc = renderer->GetFinalOutputDescriptor();
        auto finalOutputImage = renderer->GetFinalOutputImage();
        if (finalOutputDesc && finalOutputImage) {
          ImTextureID gameDesc =
              reinterpret_cast<ImTextureID>(finalOutputDesc->descriptor_set_);

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

          // FPS overlay (top-left)
          ImVec2 textPos = ImVec2(imageMin.x + 6, imageMin.y + 6);
          std::string fpsStr = std::format("FPS: {}", static_cast<int>(app_.GetFPS()));
          ImGui::GetWindowDrawList()->AddText(textPos, IM_COL32(0, 255, 0, 255), fpsStr.c_str());

          // Resolution overlay (top-right)
          std::string resStr = std::format("{}x{}", finalOutputImage->width_, finalOutputImage->height_);
          ImVec2 resTextSize = ImGui::CalcTextSize(resStr.c_str());
          ImVec2 resPos = ImVec2(imageMax.x - resTextSize.x - 6, imageMin.y + 6);
          ImGui::GetWindowDrawList()->AddText(resPos, IM_COL32(0, 255, 0, 255), resStr.c_str());
        }
        } // has_camera
      } else {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        const char* text = "Not Playing";
        ImVec2 textSize = ImGui::CalcTextSize(text);
        ImGui::SetCursorPos(ImVec2(
            ImGui::GetCursorPosX() + (avail.x - textSize.x) * 0.5f,
            ImGui::GetCursorPosY() + (avail.y - textSize.y) * 0.5f));
        ImGui::TextDisabled("%s", text);
      }
    }
    ImGui::End();
  }
}

void EditorLayer::OnPostPresent() {
  // Execute pending entity pick readback (GPU is idle after EndPresent fence)
  Renderer* renderer = Engine::renderer().get();
  entt::entity picked;
  if (renderer->ExecuteEntityPick(picked)) {
    if (picked != entt::null && scene_->GetRegistry().valid(picked)) {
      selected_entity_ = picked;
      has_selected_entity_ = true;
    } else {
      has_selected_entity_ = false;
    }
  }

  scene_->ProcessDestroyQueue();
}

void EditorLayer::TakeSnapshot() {
  SceneSerializer serializer(scene_);
  play_mode_snapshot_ = serializer.SerializeToString();
}

void EditorLayer::RestoreSnapshot() {
  Engine::renderer()->WaitForGPU();
  has_selected_entity_ = false;

  ClearScene();

  SceneSerializer serializer(scene_);
  serializer.DeserializeFromString(play_mode_snapshot_);
  play_mode_snapshot_.clear();

  scene_->InvalidateRenderGraphs();

  // Setup camera components that were deserialized
  for (auto entity : scene_->GetAllEntitiesWith<CameraComponent>()) {
    auto& cam = scene_->GetComponent<CameraComponent>(entity);
    Engine::renderer()->SetupCameraComponent(cam);
  }

  scene_->ResetPhysicsWorld();
  scene_->ResetScriptStates();
  scene_->ResetFirstUpdate();
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
      auto editorOutput = editor_camera_.resource_pool.GetTexture("PipelineOutput");
      if (editorOutput) {
        renderer->TransitionImageLayout(
            editorOutput->images_[0], editorOutput->format_,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1, cmd, 0, 1);
      }

      // Render editor camera
      PROFILE_PLOT("Scene Width", static_cast<double>(editor_camera_.viewport_size.x));
      PROFILE_PLOT("Scene Height", static_cast<double>(editor_camera_.viewport_size.y));
      scene_->RenderFromExternal(editor_camera_, editor_camera_transform_, show_grid_);
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
      scene_->Render();
    }
  } else {
    PROFILE_FRAME_MARK_NAMED("Scene");
    // EDIT MODE: Only editor camera renders.
    // camera_ will point to editor camera after RenderFromExternal.
    // BeginPresent/EndPresent handle its transitions automatically.
    scene_->RenderFromExternal(editor_camera_, editor_camera_transform_, show_grid_);
  }
}

void EditorLayer::UpdateHierarchyOrder() {
  if (hierarchy_data_.move_from == entt::null || hierarchy_data_.move_to == entt::null) {
    return;
  }
  Entity from_entity = {hierarchy_data_.move_from, scene_.get()};
  Entity to_entity = {hierarchy_data_.move_to, scene_.get()};
  auto& hierarchy = scene_->GetSceneHierarchy();
  if (hierarchy_data_.bottom_part) {
    // todo move hierarchy order on childs
    if (from_entity.parent_handle() != entt::null) {
      scene_->UnlinkEntities(from_entity.parent_handle(), hierarchy_data_.move_from);
    }
    std::erase(hierarchy, hierarchy_data_.move_from);
    auto insert_pos = std::ranges::find(hierarchy, hierarchy_data_.move_to) + 1;
    if (hierarchy.end() < insert_pos) {
      hierarchy.push_back(hierarchy_data_.move_from);
    } else {
      hierarchy.insert(insert_pos, hierarchy_data_.move_from);
    }
  } else {
    scene_->LinkEntities(hierarchy_data_.move_to, hierarchy_data_.move_from);
  }
  hierarchy_data_.move_from = entt::null;
  hierarchy_data_.move_to = entt::null;
}

// ============================================================================
// Main Menu Bar & Project/Scene Management
// ============================================================================

void EditorLayer::RenderProjectSettingsPopup() {
  if (!project_) return;

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
    auto& proj_settings = project_->GetSettings();
    bool changed = false;

    const char* categories[] = {"Scene", "Rendering", "Input"};
    constexpr int kCategoryCount = 3;

    // Left panel: category list
    ImGui::BeginChild("##categories", ImVec2(140, 0), ImGuiChildFlags_Borders);
    for (int i = 0; i < kCategoryCount; i++) {
      if (ImGui::Selectable(categories[i], project_settings_category_ == i)) {
        project_settings_category_ = i;
      }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // Right panel: settings
    ImGui::BeginChild("##settings", ImVec2(0, 0));

    if (project_settings_category_ == 0) {
      // ---- Scene ----
      ImGui::SeparatorText("Project");
      char name_buf[128];
      strncpy(name_buf, proj_settings.name.c_str(), sizeof(name_buf) - 1);
      name_buf[sizeof(name_buf) - 1] = '\0';
      if (ImGui::InputText(PrefixLabel("Project Name").c_str(), name_buf, sizeof(name_buf))) {
        proj_settings.name = name_buf;
        changed = true;
      }

      // Start scene selector
      if (ImGui::BeginCombo(PrefixLabel("Start Scene").c_str(),
                             proj_settings.start_scene.empty()
                                 ? "(none)" : proj_settings.start_scene.c_str())) {
        for (const auto& scene_rel : proj_settings.scenes) {
          bool selected = (scene_rel == proj_settings.start_scene);
          if (ImGui::Selectable(scene_rel.c_str(), selected)) {
            proj_settings.start_scene = scene_rel;
            changed = true;
          }
        }
        ImGui::EndCombo();
      }

      ImGui::SeparatorText("Physics");
      {
        auto& physics = scene_->GetPhysicsWorld();
        glm::vec3 gravity = physics.GetGravity();
        if (ImGui::DragFloat3(PrefixLabel("Gravity").c_str(), &gravity.x, 0.1f)) {
          physics.SetGravity(gravity);
        }
      }

    } else if (project_settings_category_ == 1) {
      // ---- Rendering ----
      Renderer* renderer = Engine::renderer().get();
      auto& settings = renderer->options();

      ImGui::SeparatorText("General");
      ImGui::Checkbox(PrefixLabel("Enable SSAO").c_str(), &settings.ssao_enabled);
      ImGui::Checkbox(PrefixLabel("Enable Vsync").c_str(), &settings.vsync);
      ImGui::Checkbox(PrefixLabel("Shadows").c_str(), &settings.shadows_enabled);
      if (settings.shadows_enabled && renderer->IsRayTracingSupported()) {
        ImGui::Checkbox(PrefixLabel("RT Shadows").c_str(), &settings.rt_shadows_enabled);
      }

      {
        const char* aa_labels[] = {"None", "FXAA", "TAA"};
        int aa_current = static_cast<int>(static_cast<AntiAliasingMode>(settings.aa_mode));
        if (ImGui::Combo(PrefixLabel("Anti-Aliasing").c_str(), &aa_current, aa_labels, IM_ARRAYSIZE(aa_labels))) {
          settings.aa_mode = static_cast<AntiAliasingMode>(aa_current);
        }
      }

      {
        std::vector<SamplingMode> supported_values = renderer->GetSupportedSamplingModes();
        std::vector<const char*> labels;
        SamplingMode current_mode = settings.msaa_mode;
        int selected_index = 0;
        for (size_t i = 0; i < supported_values.size(); i++) {
          labels.push_back(ToString(supported_values[i]));
          if (supported_values[i] == current_mode) selected_index = static_cast<int>(i);
        }
        if (ImGui::Combo(PrefixLabel("MSAA Samples").c_str(), &selected_index,
                          labels.data(), labels.size())) {
          settings.msaa_mode = supported_values[selected_index];
        }
      }

      ImGui::SeparatorText("Post Processing");
      ImGui::Checkbox(PrefixLabel("Enable Bloom").c_str(), &settings.bloom_enabled);
      if (settings.bloom_enabled) {
        ImGui::SliderFloat(PrefixLabel("Bloom Threshold").c_str(),
                           &settings.bloom_threshold, 0.0f, 1.0f);
        ImGui::SliderFloat(PrefixLabel("Bloom Intensity").c_str(),
                           &settings.bloom_intensity, 0.0f, 2.0f);
      }
      ImGui::Checkbox(PrefixLabel("Enable Motion Blur").c_str(), &settings.motion_blur_enabled);
      if (settings.motion_blur_enabled) {
        ImGui::SliderFloat(PrefixLabel("MB Strength").c_str(),
                           &settings.motion_blur_strength, 0.0f, 3.0f);
        ImGui::SliderInt(PrefixLabel("MB Samples").c_str(),
                         &settings.motion_blur_samples, 2, 16);
      }

      // Sync live renderer options back to project for saving
      auto& ro = proj_settings.render_options;
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
      auto& input = proj_settings.input;
      bool input_changed = false;

      ImGui::SeparatorText("Mouse");
      input_changed |= ImGui::DragFloat(PrefixLabel("Sensitivity X").c_str(),
                                         &input.mouse_sensitivity_x, 1.0f, 1.0f, 500.0f);
      input_changed |= ImGui::DragFloat(PrefixLabel("Sensitivity Y").c_str(),
                                         &input.mouse_sensitivity_y, 1.0f, 1.0f, 500.0f);
      input_changed |= ImGui::DragFloat(PrefixLabel("Y Axis Limit").c_str(),
                                         &input.mouse_axis_limit_y, 1.0f, 1.0f, 90.0f);

      int gp_count = InputManager::GetConnectedGamepadCount();
      if (gp_count > 0) {
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Gamepads: %d connected", gp_count);
      }

      ImGui::SeparatorText("Contexts");

      // Validate selected context still exists
      if (!selected_input_context_.empty() &&
          input.contexts.find(selected_input_context_) == input.contexts.end()) {
        selected_input_context_.clear();
        selected_input_item_ = -1;
      }

      // Left: context list
      ImGui::BeginChild("##ctx_list", ImVec2(130, 0), ImGuiChildFlags_Borders);
      for (auto& [ctx_name, ctx] : input.contexts) {
        if (ImGui::Selectable(ctx_name.c_str(), selected_input_context_ == ctx_name)) {
          selected_input_context_ = ctx_name;
          selected_input_item_ = -1;
        }
      }
      ImGui::Spacing();
      static char new_ctx_name[64] = "";
      ImGui::SetNextItemWidth(-1);
      ImGui::InputTextWithHint("##newctx", "new context...", new_ctx_name, sizeof(new_ctx_name));
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
          input.contexts.find(selected_input_context_) != input.contexts.end()) {
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
          if (input_changed) { InputManager::LoadFromSettings(input); changed = true; }
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
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
              ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 100);
              ImGui::TableSetupColumn("Keys");
              ImGui::TableSetupColumn("Buttons");
              ImGui::TableSetupColumn("##del", ImGuiTableColumnFlags_WidthFixed, 20);
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
                  if (k > 0) ImGui::SameLine();
                  ImGui::PushID(k);
                  ImGui::SmallButton(KeyCodeToString(action.keys[k]));
                  if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) key_rm = k;
                  if (ImGui::IsItemHovered()) ImGui::SetTooltip("Right-click to remove");
                  ImGui::PopID();
                }
                if (key_rm >= 0) { action.keys.erase(action.keys.begin() + key_rm); input_changed = true; }
                if (!action.keys.empty()) ImGui::SameLine();
                ImGui::PushID("addkey");
                ImGui::SetNextItemWidth(50);
                if (ImGui::BeginCombo("##addkey", "+", ImGuiComboFlags_NoPreview)) {
                  for (auto code : GetAllKeyCodes()) {
                    if (ImGui::Selectable(KeyCodeToString(code))) {
                      action.keys.push_back(code); input_changed = true;
                    }
                  }
                  ImGui::EndCombo();
                }
                ImGui::PopID();

                // Buttons (tags + add combo)
                ImGui::TableNextColumn();
                int btn_rm = -1;
                for (int b = 0; b < (int)action.buttons.size(); b++) {
                  if (b > 0) ImGui::SameLine();
                  ImGui::PushID(b + 200);
                  ImGui::SmallButton(GamepadButtonToString(action.buttons[b]));
                  if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) btn_rm = b;
                  if (ImGui::IsItemHovered()) ImGui::SetTooltip("Right-click to remove");
                  ImGui::PopID();
                }
                if (btn_rm >= 0) { action.buttons.erase(action.buttons.begin() + btn_rm); input_changed = true; }
                if (!action.buttons.empty()) ImGui::SameLine();
                ImGui::PushID("addbtn");
                ImGui::SetNextItemWidth(50);
                if (ImGui::BeginCombo("##addbtn", "+", ImGuiComboFlags_NoPreview)) {
                  for (auto btn : GetAllGamepadButtons()) {
                    if (ImGui::Selectable(GamepadButtonToString(btn))) {
                      action.buttons.push_back(btn); input_changed = true;
                    }
                  }
                  ImGui::EndCombo();
                }
                ImGui::PopID();

                // Delete
                ImGui::TableNextColumn();
                if (ImGui::SmallButton("X")) action_to_remove = i;

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
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
              ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 90);
              ImGui::TableSetupColumn("+Keys");
              ImGui::TableSetupColumn("-Keys");
              ImGui::TableSetupColumn("Stick", ImGuiTableColumnFlags_WidthFixed, 100);
              ImGui::TableSetupColumn("##del", ImGuiTableColumnFlags_WidthFixed, 20);
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
                  axis.name = abuf; input_changed = true;
                }

                // Positive keys
                ImGui::TableNextColumn();
                int pk_rm = -1;
                for (int k = 0; k < (int)axis.positive_keys.size(); k++) {
                  if (k > 0) ImGui::SameLine();
                  ImGui::PushID(k);
                  ImGui::SmallButton(KeyCodeToString(axis.positive_keys[k]));
                  if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) pk_rm = k;
                  if (ImGui::IsItemHovered()) ImGui::SetTooltip("Right-click to remove");
                  ImGui::PopID();
                }
                if (pk_rm >= 0) { axis.positive_keys.erase(axis.positive_keys.begin() + pk_rm); input_changed = true; }
                if (!axis.positive_keys.empty()) ImGui::SameLine();
                ImGui::PushID("addpos");
                ImGui::SetNextItemWidth(50);
                if (ImGui::BeginCombo("##addpos", "+", ImGuiComboFlags_NoPreview)) {
                  for (auto code : GetAllKeyCodes()) {
                    if (ImGui::Selectable(KeyCodeToString(code))) {
                      axis.positive_keys.push_back(code); input_changed = true;
                    }
                  }
                  ImGui::EndCombo();
                }
                ImGui::PopID();

                // Negative keys
                ImGui::TableNextColumn();
                int nk_rm = -1;
                for (int k = 0; k < (int)axis.negative_keys.size(); k++) {
                  if (k > 0) ImGui::SameLine();
                  ImGui::PushID(k + 500);
                  ImGui::SmallButton(KeyCodeToString(axis.negative_keys[k]));
                  if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) nk_rm = k;
                  if (ImGui::IsItemHovered()) ImGui::SetTooltip("Right-click to remove");
                  ImGui::PopID();
                }
                if (nk_rm >= 0) { axis.negative_keys.erase(axis.negative_keys.begin() + nk_rm); input_changed = true; }
                if (!axis.negative_keys.empty()) ImGui::SameLine();
                ImGui::PushID("addneg");
                ImGui::SetNextItemWidth(50);
                if (ImGui::BeginCombo("##addneg", "+", ImGuiComboFlags_NoPreview)) {
                  for (auto code : GetAllKeyCodes()) {
                    if (ImGui::Selectable(KeyCodeToString(code))) {
                      axis.negative_keys.push_back(code); input_changed = true;
                    }
                  }
                  ImGui::EndCombo();
                }
                ImGui::PopID();

                // Stick
                ImGui::TableNextColumn();
                const char* stick_label = axis.gamepad_axis >= 0
                    ? GamepadAxisToString(axis.gamepad_axis) : "None";
                ImGui::SetNextItemWidth(-1);
                ImGui::PushID("gpaxis");
                if (ImGui::BeginCombo("##stick", stick_label)) {
                  if (ImGui::Selectable("None", axis.gamepad_axis < 0)) {
                    axis.gamepad_axis = -1; input_changed = true;
                  }
                  for (auto ga : GetAllGamepadAxes()) {
                    if (ImGui::Selectable(GamepadAxisToString(ga), axis.gamepad_axis == ga)) {
                      axis.gamepad_axis = ga; input_changed = true;
                    }
                  }
                  ImGui::EndCombo();
                }
                ImGui::PopID();

                // Delete
                ImGui::TableNextColumn();
                if (ImGui::SmallButton("X")) axis_to_remove = i;

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
      project_->Save();
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
        if (ImGui::MenuItem("Save Project", nullptr, false, project_ != nullptr)) {
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

      if (ImGui::MenuItem("New Scene", nullptr, false, project_ != nullptr)) {
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
      if (ImGui::MenuItem("Project Settings", nullptr, false, project_ != nullptr)) {
        show_project_settings_ = true;
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
      bool has_activity = asset_stats.loading > 0 || Engine::script_manager().IsCompiling();
      if (Engine::script_manager().IsCompiling()) {
        status_text = "Compiling scripts...";
      } else if (asset_stats.loading > 0) {
        status_text = std::format("Loading {} asset{}...",
            asset_stats.loading, asset_stats.loading > 1 ? "s" : "");
      } else if (asset_stats.failed > 0) {
        status_text = std::format("{} failed", asset_stats.failed);
      }
      std::string asset_summary = std::format("[{}/{}]", asset_stats.loaded, asset_stats.total);

      std::string info;
      if (project_) {
        info = project_->GetSettings().name;
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
      float status_width = status_text.empty() ? 0.0f : ImGui::CalcTextSize(status_text.c_str()).x + spacing;
      float total_right = status_width + summary_width + spacing + info_width + 16.0f;

      ImGui::SameLine(ImGui::GetWindowWidth() - total_right);

      if (!status_text.empty()) {
        if (has_activity) {
          ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "%s", status_text.c_str());
        } else {
          ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", status_text.c_str());
        }
        ImGui::SameLine(0, spacing);
      }

      ImGui::TextDisabled("%s", asset_summary.c_str());
      ImGui::SameLine(0, spacing);
      ImGui::TextDisabled("%s", info.c_str());
    }

    ImGui::EndMainMenuBar();
  }

  // About popup
  if (show_about_popup_) {
    ImGui::OpenPopup("About Wiesel");
    show_about_popup_ = false;
  }
  if (ImGui::BeginPopupModal("About Wiesel", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
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
}

void EditorLayer::NewProject() {
  Dialogs::SelectFolderDialog([this](const std::string& folder) {
    if (folder.empty()) return;

    // Ask for project name via a simple approach: use folder name
    std::filesystem::path dir(folder);
    std::string name = dir.filename().string();

    if (Project::Create(dir, name)) {
      auto proj = Project::Load(dir / (name + ".wiesel"));
      if (proj) {
        project_ = std::move(proj);
        Project::SetActive(project_.get());

        // Mount project assets
        auto* vfs = Engine::vfs().get();
        vfs->Unmount("/app");
        vfs->Mount("/app", project_->GetAssetsDirectory().string());

        // Create the default scene file
        ClearScene();
        current_scene_path_ = project_->GetScenesDirectory() / "main.wscene";
        SaveScene();

        ScanProjectAssets();
        Engine::script_manager().ReloadAsync();
        RecentProjects::Add(std::filesystem::absolute(dir / (name + ".wiesel")).string());
        UpdateWindowTitle();
        LOG_INFO("Created project: {} at {}", name, folder);
      }
    }
  });
}

void EditorLayer::OpenProject() {
  Dialogs::OpenFileDialog(
      {{"Wiesel Project", "wiesel"}}, [this](const std::string& file) {
        if (file.empty()) return;
        LoadProjectFromPath(file);
      });
}

void EditorLayer::SaveProject() {
  if (project_) {
    // Capture current render options into project settings
    auto& settings = Engine::renderer()->options();
    auto& opts = project_->GetSettings().render_options;
    opts.ssao_enabled = settings.ssao_enabled;
    opts.bloom_enabled = settings.bloom_enabled;
    opts.bloom_threshold = settings.bloom_threshold;
    opts.bloom_intensity = settings.bloom_intensity;
    opts.motion_blur_enabled = settings.motion_blur_enabled;
    opts.motion_blur_strength = settings.motion_blur_strength;
    opts.motion_blur_samples = settings.motion_blur_samples;
    opts.shadows_enabled = settings.shadows_enabled;
    opts.vsync = settings.vsync;
    opts.aa_mode = static_cast<int>(static_cast<AntiAliasingMode>(settings.aa_mode));

    project_->Save();
    LOG_INFO("Project saved");
  }
}

void EditorLayer::NewScene() {
  if (!project_) return;

  // Auto-save current scene before creating new one
  AutoSave();

  // Generate a unique name
  namespace fs = std::filesystem;
  fs::path scenes_dir = project_->GetScenesDirectory();
  fs::create_directories(scenes_dir);

  std::string base_name = "new_scene";
  fs::path scene_path = scenes_dir / (base_name + ".wscene");
  int counter = 1;
  while (fs::exists(scene_path)) {
    scene_path = scenes_dir / (base_name + "_" + std::to_string(counter++) + ".wscene");
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

  ClearScene();

  SceneSerializer serializer(scene_);
  if (serializer.Deserialize(path)) {
    current_scene_path_ = std::filesystem::absolute(path);
    scene_dirty_ = false;
    scene_->InvalidateRenderGraphs();

    // Setup camera components that were deserialized
    auto view = scene_->GetAllEntitiesWith<CameraComponent>();
    for (auto entity : view) {
      auto& cam = scene_->GetComponent<CameraComponent>(entity);
      Engine::renderer()->SetupCameraComponent(cam);
    }

    // Track last opened scene in project
    if (project_) {
      auto rel = std::filesystem::relative(current_scene_path_,
                                           project_->GetAssetsDirectory());
      project_->GetSettings().last_scene = rel.generic_string();
      project_->Save();
    }

    UpdateWindowTitle();
    LOG_INFO("Scene loaded: {}", path.string());
  }
}

void EditorLayer::SaveScene() {
  if (current_scene_path_.empty()) {
    SaveSceneAs();
    return;
  }

  // Ensure parent directory exists
  std::filesystem::create_directories(current_scene_path_.parent_path());

  SceneSerializer serializer(scene_);
  if (serializer.Serialize(current_scene_path_)) {
    scene_dirty_ = false;
    UpdateWindowTitle();

    auto_save_timer_ = 0.0f;

    // Add to project scene list and register with SceneManager
    if (project_) {
      auto rel = std::filesystem::relative(current_scene_path_,
                                           project_->GetAssetsDirectory());
      project_->AddScene(rel.generic_string());
      project_->GetSettings().last_scene = rel.generic_string();
      project_->Save();

      std::string scene_name = current_scene_path_.stem().string();
      SceneManager::Get().RegisterScene(scene_name, current_scene_path_);

      // Register scene asset if not already in asset browser
      auto vfs_path = "/app/" + rel.generic_string();
      auto& mgr = Engine::asset_manager();
      bool found = false;
      for (auto& h : mgr.GetAll()) {
        const auto* meta = mgr.GetMetadata(h);
        if (meta && meta->virtual_source_path == vfs_path) { found = true; break; }
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
        if (file.empty()) return;

        std::filesystem::path path(file);
        if (path.extension() != ".wscene") {
          path += ".wscene";
        }

        // If a project is open, ensure the scene is saved inside the assets dir
        if (project_) {
          namespace fs = std::filesystem;
          fs::path abs = fs::absolute(path);
          fs::path assets = fs::absolute(project_->GetAssetsDirectory());
          auto rel = fs::relative(abs, assets);
          if (rel.string().find("..") != std::string::npos) {
            // Outside project assets - redirect into assets/scenes/
            path = project_->GetScenesDirectory() / path.filename();
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
  auto& hierarchy = scene_->GetSceneHierarchy();
  std::vector<entt::entity> to_remove(hierarchy.begin(), hierarchy.end());
  for (auto entity_id : to_remove) {
    Entity entity{entity_id, scene_.get()};
    scene_->RemoveEntity(entity);
  }
  scene_->ProcessDestroyQueue();

  scene_->ResetPhysicsWorld();
  scene_->InvalidateRenderGraphs();
  scene_dirty_ = false;
}

void EditorLayer::UpdateWindowTitle() {
  std::string title = "Wiesel Editor";
  if (project_) {
    title += " - " + project_->GetSettings().name;
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
  if (!scene_dirty_ || current_scene_path_.empty()) return;

  SceneSerializer serializer(scene_);
  if (serializer.Serialize(current_scene_path_)) {
    scene_dirty_ = false;
    auto_save_timer_ = 0.0f;
    UpdateWindowTitle();

    if (project_) {
      auto rel = std::filesystem::relative(current_scene_path_,
                                           project_->GetAssetsDirectory());
      project_->AddScene(rel.generic_string());
      project_->GetSettings().last_scene = rel.generic_string();
      project_->Save();
    }

    LOG_DEBUG("Auto-saved scene: {}", current_scene_path_.filename().string());
  }
}

void EditorLayer::LoadProjectFromPath(const std::filesystem::path& path) {
  namespace fs = std::filesystem;

  auto proj = Project::Load(path);
  if (!proj) return;

  project_ = std::move(proj);
  Project::SetActive(project_.get());

  ProjectLoader::MountProject(*project_);
  ProjectLoader::ScanAssets(*project_);
  Engine::script_manager().Reload();
  ProjectLoader::ApplyRenderOptions(*project_);
  ProjectLoader::ApplyInputSettings(*project_);

  // Open last scene or start scene
  const auto& last = project_->GetSettings().last_scene;
  const auto& start = project_->GetSettings().start_scene;
  const auto& to_open = !last.empty() ? last : start;
  if (!to_open.empty()) {
    auto scene_path = project_->GetAssetsDirectory() / to_open;
    if (fs::exists(scene_path)) {
      OpenSceneFromPath(scene_path);
    }
  }

  RecentProjects::Add(fs::absolute(path).string());
  UpdateWindowTitle();
#ifdef WIESEL_DISCORD_RPC
  Engine::discord_rpc().SetPresence(
      "Working on " + project_->GetSettings().name, "Editing",
      "wiesel_logo", "Wiesel Engine");
#endif
  LOG_INFO("Opened project: {}", project_->GetSettings().name);
}

void EditorLayer::ScanProjectAssets() {
  if (project_) {
    ProjectLoader::ScanAssets(*project_);
    project_->Save();
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

  auto recent = RecentProjects::Load();
  if (!recent.empty()) {
    ImGui::Text("Recent Projects:");
    ImGui::Spacing();
    for (const auto& path : recent) {
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
  Entity root = Prefab::InstantiateFromFile(scene_, path);
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
  scene_->InvalidateRenderGraphs();

  // Setup camera components
  for (auto entity : scene_->GetAllEntitiesWith<CameraComponent>()) {
    auto& cam = scene_->GetComponent<CameraComponent>(entity);
    Engine::renderer()->SetupCameraComponent(cam);
  }

  LOG_INFO("Editing prefab: {}", path.string());
}

void EditorLayer::SavePrefab() {
  if (!editing_prefab_ || editing_prefab_path_.empty()) return;

  // Find the root entity (first in hierarchy - the prefab root)
  auto& hierarchy = scene_->GetSceneHierarchy();
  if (hierarchy.empty()) {
    LOG_ERROR("Cannot save prefab: scene is empty");
    return;
  }

  Entity root = {hierarchy[0], scene_.get()};
  if (Prefab::SaveToFile(root, editing_prefab_path_)) {
    scene_dirty_ = false;
    LOG_INFO("Prefab saved: {}", editing_prefab_path_.string());
  }
}

void EditorLayer::ClosePrefabEditor() {
  if (!editing_prefab_) return;

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

}
