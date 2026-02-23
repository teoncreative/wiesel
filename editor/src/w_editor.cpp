//
// Created by Metehan Gezer on 18/04/2025.
//

#include "w_editor.hpp"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <backends/imgui_impl_vulkan.h>
#include <ImGuizmo.h>

#include "asset/w_asset_manager.hpp"
#include "imgui_internal.h"
#include "physics/w_collider.hpp"
#include "physics/w_physics_world.hpp"
#include "rendering/w_sprite.hpp"
#include "rendering/w_texture.hpp"
#include "util/w_dialogs.hpp"
#include "util/w_filewatcher.hpp"
#include "util/w_platform.hpp"
#include "layer/w_layerscene.hpp"
#include "scene/w_componentutil.hpp"
#include "script/w_scriptmanager.hpp"
#include "util/imgui/w_imguiutil.hpp"
#include "input/w_input.hpp"
#include "w_engine.hpp"

namespace Wiesel::Editor {

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

EditorLayer::EditorLayer(Application& app, Ref<Scene> scene)
    : app_(app), scene_(scene), Layer("Demo Overlay") {}

EditorLayer::~EditorLayer() = default;

void EditorLayer::OnAttach() {
  LOG_DEBUG("OnAttach");

  // Initialize editor free camera
  editor_camera_transform_.position = glm::vec3(0.0f, 5.0f, -10.0f);
  editor_camera_transform_.scale = glm::vec3(1.0f);
  editor_yaw_ = 180.0f;  // facing +Z (quat look = -sin(y),-cos(y) so 180 gives +Z)
  editor_pitch_ = -15.0f; // slightly looking down
  editor_camera_transform_.rotation = glm::vec3(editor_pitch_, editor_yaw_, 0.0f);

  editor_camera_.viewport_size = {2560, 1440};
  editor_camera_.far_plane = 500.0f;
  editor_camera_.field_of_view = 60.0f;
  Engine::GetRenderer()->SetupCameraComponent(editor_camera_);

  // Start watching app scripts directory for hot reload
  if (Engine::GetEngineProperties().dev_mode) {
    std::optional<std::filesystem::path> scripts_dir =
        Engine::GetVirtualFileSystem()->GetPhysicalPath("/app/scripts");
    if (scripts_dir.has_value() && std::filesystem::exists(*scripts_dir)) {
      script_watcher_.Watch(*scripts_dir, true);
      LOG_INFO("Watching scripts directory: {}", scripts_dir->string());
    }
  }
}

void EditorLayer::OnDetach() {
  LOG_DEBUG("OnDetach");
  CleanupThumbnailCache();
}

void EditorLayer::OnUpdate(float_t delta_time) {
  // Only let scripts read input when the Game panel is focused during play
  InputManager::SetEnabled(editor_state_ == EditorState::Playing && game_panel_focused_);

  if (editor_state_ == EditorState::Playing) {
    scene_->OnUpdate(delta_time);
  } else {
    scene_->OnUpdateEditor(delta_time);
  }

  // Poll script file watcher for hot reload
  if (script_watcher_.IsWatching()) {
    script_watch_timer_ += delta_time;
    if (script_watch_timer_ >= kScriptWatchInterval) {
      script_watch_timer_ = 0.0f;
      if (script_watcher_.Poll()) {
        LOG_INFO("Script changes detected, reloading...");
        CleanupThumbnailCache();
        ScriptManager::Reload();
      }
    }
  }
}

void EditorLayer::OnEvent(Event& event) {
  if (editor_state_ == EditorState::Playing) {
    // Only forward input events (keyboard/mouse) when the Game panel is focused
    bool is_input = event.IsInCategory(EventCategory::kEventCategoryInput);
    if (is_input && !game_panel_focused_) {
      return;
    }
    scene_->OnEvent(event);
  } else {
    // Only forward structural events in edit mode (window resize, pipeline recreated)
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
  AssetManager& mgr = AssetManager::Get();

  if (meta.type == AssetType::Texture || meta.type == AssetType::Skybox) {
    Ref<Texture> texture = mgr.Get<Texture>(handle);
    if (!texture || !texture->is_allocated_ || !texture->image_view_) {
      return entry;  // Not loaded yet, retry next frame
    }
    entry.texture_id = ImGui_ImplVulkan_AddTexture(
        texture->sampler_, texture->image_view_->handle_,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    entry.attempted = true;
  } else if (meta.type == AssetType::Sprite) {
    Ref<SpriteAsset> sprite = mgr.Get<SpriteAsset>(handle);
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
  std::string tag = "";
  for (int i = 0; i < depth; i++) {
    tag += "\t";
  }
  tag += tag_component.tag;
  tag += "##";
  tag += (uint32_t) entity_id;

  if (ImGui::Selectable(tag.c_str(),
                        has_selected_entity_ && selected_entity_ == entity_id,
                        ImGuiSelectableFlags_None, ImVec2(0, 0))) {
    selected_entity_ = entity_id;
    has_selected_entity_ = true;
  }

  ImGuiDragDropFlags src_flags = 0;
  src_flags |= ImGuiDragDropFlags_SourceNoDisableHover;     // Keep the source displayed as hovered
  //src_flags |= ImGuiDragDropFlags_SourceNoHoldToOpenOthers; // Because our dragging is local, we disable the feature of opening foreign treenodes/tabs while dragging
  //src_flags |= ImGuiDragDropFlags_SourceNoPreviewTooltip; // Hide the tooltip

  ImGuiDragDropFlags target_flags = 0;
  target_flags |= ImGuiDragDropFlags_AcceptBeforeDelivery;    // Don't wait until the delivery (release mouse button on a target) to do something

  if (ImGui::BeginDragDropSource(src_flags)) {
    if (!(src_flags & ImGuiDragDropFlags_SourceNoPreviewTooltip)) {
      ImGui::Text("%s", tag.c_str());
    }
    ImGui::SetDragDropPayload("SceneHierarchy Entity", &entity_id, sizeof(entt::entity));
    ImGui::EndDragDropSource();
  }

  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SceneHierarchy Entity", target_flags)) {
      entt::entity* new_data = static_cast<entt::entity*>(payload->Data);
      hierarchy_data_.move_from = *new_data;
      hierarchy_data_.move_to = entity_id;
      hierarchy_data_.bottom_part = false;
    }
    ImGui::EndDragDropTarget();
  }

  if (ImGui::BeginDragDropSource(src_flags)) {
    if (!(src_flags & ImGuiDragDropFlags_SourceNoPreviewTooltip)) {
      ImGui::Text("%s", tag.c_str());
    }
    ImGui::SetDragDropPayload("SceneHierarchy Entity", &entity_id, sizeof(entt::entity));
    ImGui::EndDragDropSource();
  }

  if (ImGui::BeginDragDropTarget()) {
    target_flags |= ImGuiDragDropFlags_AcceptNoDrawDefaultRect; // Don't display the yellow rectangle
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SceneHierarchy Entity", target_flags)) {
      entt::entity newData = *(entt::entity*)payload->Data;
      hierarchy_data_.move_from = newData;
      hierarchy_data_.move_to = entity_id;
      hierarchy_data_.bottom_part = true;
    }
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImGuiContext& g = *GImGui;
    ImRect r = g.DragDropTargetRect;
    ImVec2 min = r.Min;
    ImVec2 max = r.Max;
    min.y += 2.0f;
    max.y -= 2.0f;
    window->DrawList->AddRect(min, max, ImGui::GetColorU32(ImGuiCol_DragDropTarget), 0.0f, 0, 1.0f);

    ImGui::EndDragDropTarget();
  }

  // Accept asset drops onto entities in the hierarchy
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("AssetHandle")) {
      AssetHandle dropped = *static_cast<const AssetHandle*>(payload->Data);
      const AssetMetadata* meta = AssetManager::Get().GetMetadata(dropped);
      if (meta) {
        if (meta->type == AssetType::Model) {
          if (!entity.HasComponent<ModelComponent>()) {
            entity.AddComponent<ModelComponent>();
          }
          auto& model = entity.GetComponent<ModelComponent>();
          model.model_handle = dropped;
        }
        // Future: handle AssetType::Script, AssetType::Texture, etc.
      }
    }
    ImGui::EndDragDropTarget();
  }

  if (ImGui::BeginPopupContextItem()) {
    selected_entity_ = entity_id;
    if (ImGui::Button("Remove Entity")) {
      scene_->RemoveEntity(entity);
      has_selected_entity_ = false;
    }
    ImGui::EndPopup();
    ignore_menu = true;
  }
  if (entity.child_handles()) {
    for (const auto& child_entity_id : *entity.child_handles()) {
      Entity child = {child_entity_id, scene_.get()};
      RenderEntity(child, child_entity_id, depth + 1, ignore_menu);
    }
  }
}

void EditorLayer::OnBeginPresent() {
  Renderer* renderer = Engine::GetRenderer().get();

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
    ImGui::DockBuilderDockWindow("Scene", dock_center);
    ImGui::DockBuilderDockWindow("Game", dock_center);
    ImGui::DockBuilderDockWindow("Scene Properties", dock_right);
    ImGui::DockBuilderDockWindow("Asset Browser", dock_bottom);

    ImGui::DockBuilderFinish(dockspace_id);
  }

  static bool scenePropertiesOpen = true;
  //ImGui::ShowDemoWindow(&scenePropertiesOpen);
  if (ImGui::Begin("Scene Properties", &scenePropertiesOpen)) {
    ImGui::SeparatorText("Controls");
    auto& settings = renderer->options();
    ImGui::Checkbox(PrefixLabel("Wireframe Mode").c_str(),
                        &settings.wireframe_enabled);
    ImGui::Checkbox(PrefixLabel("Enable SSAO").c_str(),
                    &settings.ssao_enabled);
    ImGui::Checkbox(PrefixLabel("Enable Vsync").c_str(),
                    &settings.vsync);
    ImGui::Checkbox(PrefixLabel("Only SSAO").c_str(),
                    &settings.only_ssao);
    ImGui::Checkbox(PrefixLabel("Debug Cascades").c_str(),
                    &settings.debug_cascades);
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

    {
      const char* aa_labels[] = {"None", "FXAA", "TAA"};
      int aa_current = static_cast<int>(static_cast<AntiAliasingMode>(settings.aa_mode));
      if (ImGui::Combo(PrefixLabel("Anti-Aliasing").c_str(), &aa_current, aa_labels, IM_ARRAYSIZE(aa_labels))) {
        settings.aa_mode = static_cast<AntiAliasingMode>(aa_current);
      }
    }

    std::vector<SamplingMode> supported_values =
        renderer->GetSupportedSamplingModes();
    std::vector<const char*> labels;

    // Find current selection
    SamplingMode current_mode = renderer->options().msaa_mode;
    int selected_index = 0;

    for (size_t i = 0; i < supported_values.size(); i++) {
      labels.push_back(ToString(supported_values[i]));

      if (supported_values[i] == current_mode) {
        selected_index = static_cast<int>(i);
      }
    }

    if (ImGui::Combo("MSAA Samples", &selected_index, labels.data(),
                     labels.size())) {
      renderer->options().msaa_mode = supported_values[selected_index];
    }

    if (ImGui::Button("Recreate Pipeline")) {
      renderer->SetRecreatePipeline(true);
    }

    ImGui::SeparatorText("Shadow Cascades");
    auto cam = renderer->GetCameraData();
    if (cam) {
      ImGui::Text("Shadows: %s", cam->does_shadow_pass ? "ON" : "OFF");
      for (int i = 0; i < WIESEL_SHADOW_CASCADE_COUNT; i++) {
        ImGui::Text("Cascade %d: split Z = %.2f", i,
                    cam->shadow_map_cascades[i].SplitDepth);
      }
    }
    if (ImGui::Button("Reload Scripts")) {
      ScriptManager::Reload();
    }

    ImGui::SeparatorText("Physics");
    {
      auto& physics = scene_->GetPhysicsWorld();
      glm::vec3 gravity = physics.GetGravity();
      if (ImGui::DragFloat3(PrefixLabel("Gravity").c_str(), &gravity.x, 0.1f)) {
        physics.SetGravity(gravity);
      }
    }
  }
  ImGui::End();

  static bool sceneOpen = true;
  if (ImGui::Begin("Scene Hierarchy", &sceneOpen)) {
    bool ignoreMenu = false;

    for (const auto& entityId : scene_->GetSceneHierarchy()) {
      Entity entity = {entityId, scene_.get()};
      if (entity.GetParent()) {
        continue;
      }

      RenderEntity(entity, entityId, 0, ignoreMenu);
    }

    UpdateHierarchyOrder();

    if (!ignoreMenu && ImGui::IsWindowHovered() && ImGui::IsMouseClicked(1, false))
      ImGui::OpenPopup("right_click_hierarchy");
    if (ImGui::BeginPopup("right_click_hierarchy")) {
      if (ImGui::BeginMenu("Add")) {
        if (ImGui::MenuItem("Empty Object")) {
          scene_->CreateEntity();
          ImGui::CloseCurrentPopup();
        }
        ImGui::EndMenu();
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
    auto& mgr = AssetManager::Get();

    static std::string current_dir;
    static AssetHandle selected_asset;
    static float tile_size = 80.0f;

    // Collect directories and assets visible in current_dir
    std::set<std::string> subdirs;
    std::vector<AssetHandle> visible_assets;

    for (auto& handle : mgr.GetAll()) {
      const auto* meta = mgr.GetMetadata(handle);
      if (!meta || meta->virtual_source_path.empty()) continue;

      const auto& path = meta->virtual_source_path;

      // Get the remainder after current_dir prefix
      // current_dir is stored as "/app/models/" or empty for root
      std::string remainder;
      if (current_dir.empty()) {
        // Strip leading / from VFS paths to get first-level dirs
        remainder = (!path.empty() && path[0] == '/') ? path.substr(1) : path;
      } else {
        if (path.rfind(current_dir, 0) != 0) continue;
        remainder = path.substr(current_dir.size());
      }

      size_t slash = remainder.find_first_of("/\\");
      if (slash != std::string::npos) {
        // There's a subdirectory - collect it
        subdirs.insert(remainder.substr(0, slash));
      } else {
        // File is directly in current_dir
        visible_assets.push_back(handle);
      }
    }

    // Breadcrumb bar
    {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
      if (ImGui::Button("Assets")) {
        current_dir.clear();
      }

      if (!current_dir.empty()) {
        // Split current_dir into parts, preserving leading /
        std::string accumulated = "/";
        std::string remaining = current_dir;
        if (!remaining.empty() && remaining[0] == '/') {
          remaining = remaining.substr(1);
        }
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
    // Resolves an imported file: if outside the project, copies into dest_dir.
    // For models (copy_folder=true): copies the entire parent folder to preserve
    // relative texture references (e.g. .gltf + textures, .obj + .mtl).
    // Returns a VFS path (e.g. /app/models/sponza/sponza.gltf).
    static auto ResolveImportPath = [](const std::string& file,
                                       const std::string& dest_dir,
                                       bool copy_folder) -> std::string {
      namespace fs = std::filesystem;
      fs::path abs = fs::absolute(file);
      fs::path app_assets = fs::absolute(Engine::GetEngineProperties().app_assets_path);

      auto ToVfsPath = [&](const fs::path& physical_path) -> std::string {
        auto rel = fs::relative(physical_path, app_assets);
        return "/app/" + rel.generic_string();
      };

      // Check if already under the app assets directory
      auto rel = fs::relative(abs, app_assets);
      std::string relStr = rel.string();
      if (relStr.find("..") == std::string::npos) {
        return ToVfsPath(abs);
      }

      // Outside project, need to copy
      std::error_code ec;
      if (copy_folder) {
        // Copy entire parent folder to preserve texture/material references
        fs::path src_dir = abs.parent_path();
        fs::path folder_name = src_dir.filename();
        fs::path dest = fs::path(dest_dir) / folder_name;
        fs::create_directories(dest);
        fs::copy(src_dir, dest, fs::copy_options::recursive | fs::copy_options::skip_existing, ec);
        if (ec) {
          LOG_ERROR("Failed to copy folder '{}' to '{}': {}", src_dir.string(), dest.string(), ec.message());
          return "";
        }
        return ToVfsPath(dest / abs.filename());
      } else {
        // Copy single file
        fs::path dest = fs::path(dest_dir) / abs.filename();
        fs::create_directories(dest_dir);
        fs::copy_file(abs, dest, fs::copy_options::skip_existing, ec);
        if (ec) {
          LOG_ERROR("Failed to copy asset '{}' to '{}': {}", file, dest.string(), ec.message());
          return "";
        }
        return ToVfsPath(dest);
      }
    };

    if (ImGui::BeginPopup("ImportAssetPopup")) {
      if (ImGui::MenuItem("Model...")) {
        Dialogs::OpenFileDialog(
            {{"Model file", "obj,gltf,glb"}}, [](const std::string& file) {
              if (file.empty()) return;
              std::string path = ResolveImportPath(file, "assets/models", true);
              if (path.empty()) return;
              std::string name = std::filesystem::path(file).stem().string();
              AssetManager::Get().Register(name, AssetType::Model, path);
            });
      }
      if (ImGui::MenuItem("Texture...")) {
        Dialogs::OpenFileDialog(
            {{"Image file", "png,jpg,jpeg,tga,bmp"}}, [](const std::string& file) {
              if (file.empty()) return;
              std::string path = ResolveImportPath(file, "assets/textures", false);
              if (path.empty()) return;
              std::string name = std::filesystem::path(file).stem().string();
              AssetManager::Get().Register(name, AssetType::Texture, path);
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

            auto physical_path = Engine::GetVirtualFileSystem()->GetPhysicalPath(asset_meta->virtual_source_path);
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

      // ".." back folder
      if (!current_dir.empty()) {
        if (DrawTile("..", ImVec4(0.35f, 0.35f, 0.4f, 1.0f), "..",
                     false, true)) {
          // Go up one level
          std::string trimmed = current_dir;
          if (!trimmed.empty() && (trimmed.back() == '/' || trimmed.back() == '\\'))
            trimmed.pop_back();
          size_t slash = trimmed.find_last_of("/\\");
          if (slash == std::string::npos || slash == 0) {
            current_dir = "";
          } else {
            current_dir = trimmed.substr(0, slash + 1);
          }
        }
        NextColumn();
      }

      // Subdirectory tiles (single click to enter)
      for (const auto& dir : subdirs) {
        if (DrawTile(dir.c_str(), ImVec4(0.3f, 0.35f, 0.45f, 1.0f), "Directory",
                     false, true)) {
          if (current_dir.empty()) {
            current_dir = "/" + dir + "/";
          } else {
            current_dir += dir + "/";
          }
        }
        NextColumn();
      }

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
          default:                  return {0.40f, 0.40f, 0.40f, 1.0f};
        }
      };

      auto GetAssetAbbrev = [](AssetType type) -> const char* {
        switch (type) {
          case AssetType::Texture:  return "Texture";
          case AssetType::Model:    return "Model";
          case AssetType::Material: return "Material";
          case AssetType::Shader:   return "Shader";
          case AssetType::Sprite:   return "Sprite";
          case AssetType::Skybox:   return "Skybox";
          case AssetType::Font:     return "Font";
          case AssetType::Script:   return "Script";
          default:                  return "?";
        }
      };

      // Asset tiles
      for (auto& handle : visible_assets) {
        const auto* meta = mgr.GetMetadata(handle);
        if (!meta) continue;

        VkDescriptorSet thumbnail = nullptr;
        if (meta->type == AssetType::Texture || meta->type == AssetType::Sprite ||
            meta->type == AssetType::Skybox) {
          ThumbnailEntry thumb = GetOrCreateThumbnail(handle, *meta);
          thumbnail = thumb.texture_id;
        }

        bool is_sel = selected_asset == handle;
        bool dbl_clicked = false;
        if (DrawTile(meta->name.c_str(), GetAssetColor(meta->type),
                     GetAssetAbbrev(meta->type), is_sel, false, thumbnail, meta, &dbl_clicked)) {
          selected_asset = handle;
        }

        if (dbl_clicked && meta->type == AssetType::Script) {
          std::optional<std::filesystem::path> physical =
              Engine::GetVirtualFileSystem()->GetPhysicalPath(meta->virtual_source_path);
          if (physical.has_value()) {
            OpenFileInDefaultEditor(*physical);
          }
        }

        // Drag source for asset tiles
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
          ImGui::SetDragDropPayload("AssetHandle", &handle, sizeof(AssetHandle));
          ImGui::Text("%s (%s)", meta->name.c_str(), GetAssetAbbrev(meta->type));
          ImGui::EndDragDropSource();
        }

        NextColumn();
      }
    }
    ImGui::EndChild();
  }
  ImGui::End();

  // Helper lambda: draws Play/Stop buttons, returns true if state changed
  auto DrawPlayStopButtons = [&]() -> bool {
    bool changed = false;
    if (editor_state_ == EditorState::Edit) {
      if (ImGui::Button("Play")) {
        TakeSnapshot();
        editor_state_ = EditorState::Playing;
        scene_->ResetFirstUpdate();
        ImGui::SetWindowFocus("Game");
        changed = true;
      }
    } else {
      if (ImGui::Button("Stop")) {
        editor_state_ = EditorState::Edit;
        RestoreSnapshot();
        ImGui::SetWindowFocus("Scene");
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
  if (ImGui::Begin("Scene", &sceneViewOpen, sceneFlags)) {
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

    // Editor camera output from its own resource pool
    auto editorDesc = editor_camera_.resource_pool.GetDescriptor("PipelineOutputDescriptor");
    auto editorImage = editor_camera_.resource_pool.GetTexture("PipelineOutput");

    // Handle viewport resize
    ImVec2 avail = ImGui::GetContentRegionAvail();
    uint32_t newW = static_cast<uint32_t>(avail.x);
    uint32_t newH = static_cast<uint32_t>(avail.y);
    if (newW > 0 && newH > 0 &&
        (newW != editor_camera_.viewport_size.x || newH != editor_camera_.viewport_size.y)) {
      editor_camera_.viewport_size = {newW, newH};
      editor_camera_.aspect_ratio = static_cast<float>(newW) / static_cast<float>(newH);
      editor_camera_.view_changed = true;
      editor_camera_.resources_dirty = true;
    }

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
      bool sceneHovered = ImGui::IsItemHovered();

      // FPS overlay
      ImVec2 textPos = ImVec2(imageMin.x + 6, imageMin.y + 6);
      std::string fpsStr = fmt::format("FPS: {}", static_cast<int>(app_.GetFPS()));
      ImGui::GetWindowDrawList()->AddText(textPos, IM_COL32(0, 255, 0, 255), fpsStr.c_str());

      // Right-click fly mode (Unity-style: hold right-click for mouse look + WASD)
      static bool scene_right_active = false;
      if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
        if (sceneHovered && !scene_right_active) {
          scene_right_active = true;
        }
      } else {
        scene_right_active = false;
      }

      if (scene_right_active) {
        ImGui::GetIO().WantCaptureKeyboard = true;
        ImGuiIO& io = ImGui::GetIO();
        float dt = io.DeltaTime;

        // Mouse look
        editor_yaw_ += io.MouseDelta.x * mouse_sensitivity_;
        editor_pitch_ -= io.MouseDelta.y * mouse_sensitivity_;
        editor_pitch_ = glm::clamp(editor_pitch_, -89.0f, 89.0f);
        editor_camera_transform_.rotation = glm::vec3(editor_pitch_, editor_yaw_, 0.0f);

        // Extract camera axes directly from the rotation quaternion
        // This guarantees movement matches the rendered view exactly
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
      if (sceneHovered) {
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

      // Entity picking: click on Scene panel to select (only when not right-clicking)
      if (!scene_right_active && ImGui::IsMouseClicked(0) && sceneHovered &&
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
    game_panel_focused_ = ImGui::IsWindowFocused();
    if (gameVisible) {
      DrawPlayStopButtons();

      if (editor_state_ == EditorState::Playing) {
        auto finalOutputDesc = renderer->GetFinalOutputDescriptor();
        auto finalOutputImage = renderer->GetFinalOutputImage();
        if (finalOutputDesc && finalOutputImage) {
          ImTextureID gameDesc =
              reinterpret_cast<ImTextureID>(finalOutputDesc->descriptor_set_);

          ImVec2 avail = ImGui::GetContentRegionAvail();
          float imageAspect = (float)finalOutputImage->width_ / (float)finalOutputImage->height_;
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
        }
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
  Renderer* renderer = Engine::GetRenderer().get();
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
  scene_snapshot_.clear();
  for (auto entity : scene_->GetAllEntitiesWith<TransformComponent>()) {
    auto& t = scene_->GetComponent<TransformComponent>(entity);
    scene_snapshot_[entity] = {t.position, t.rotation, t.scale};
  }
}

void EditorLayer::RestoreSnapshot() {
  // Restore transforms
  for (auto& [entity, snap] : scene_snapshot_) {
    if (!scene_->GetRegistry().valid(entity))
      continue;
    auto& t = scene_->GetComponent<TransformComponent>(entity);
    t.position = snap.position;
    t.rotation = snap.rotation;
    t.scale = snap.scale;
    t.is_changed = true;
  }

  // Reset physics and scripts
  scene_->ResetPhysicsWorld();
  scene_->ResetScriptStates();
  scene_->ResetFirstUpdate();
}

void EditorLayer::OnPrePresent() {
  Renderer* renderer = Engine::GetRenderer().get();
  VkCommandBuffer cmd = renderer->GetCommandBuffer().handle_;

  if (editor_state_ == EditorState::Playing) {
    // PLAY MODE: Both editor camera and ECS cameras render.
    // BeginPresent/EndPresent handle the ECS camera transitions.
    // We must manually transition the editor camera's output.

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
    scene_->RenderFromExternal(editor_camera_, editor_camera_transform_);

    // Transition editor PipelineOutput to SHADER_READ for ImGui sampling
    editorOutput = editor_camera_.resource_pool.GetTexture("PipelineOutput");
    if (editorOutput) {
      renderer->TransitionImageLayout(
          editorOutput->images_[0], editorOutput->format_,
          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, cmd, 0, 1);
    }

    // Render ECS cameras (sets camera_ to ECS camera for BeginPresent)
    scene_->Render();
  } else {
    // EDIT MODE: Only editor camera renders.
    // camera_ will point to editor camera after RenderFromExternal.
    // BeginPresent/EndPresent handle its transitions automatically.
    scene_->RenderFromExternal(editor_camera_, editor_camera_transform_);
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
}

}
