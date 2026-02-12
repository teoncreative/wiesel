//
// Created by Metehan Gezer on 18/04/2025.
//

#include "w_editor.hpp"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <ImGuizmo.h>

#include "asset/w_asset_manager.hpp"
#include "imgui_internal.h"
#include "util/w_dialogs.hpp"
#include "layer/w_layerscene.hpp"
#include "scene/w_componentutil.hpp"
#include "script/w_scriptmanager.hpp"
#include "util/imgui/w_imguiutil.hpp"
#include "w_engine.hpp"

namespace Wiesel::Editor {

EditorLayer::EditorLayer(Application& app, Ref<Scene> scene)
    : app_(app), scene_(scene), Layer("Demo Overlay") {}

EditorLayer::~EditorLayer() = default;

void EditorLayer::OnAttach() {
  LOG_DEBUG("OnAttach");
}

void EditorLayer::OnDetach() {
  LOG_DEBUG("OnDetach");
}

void EditorLayer::OnUpdate(float_t delta_time) {
  scene_->OnUpdate(delta_time);
}

void EditorLayer::OnEvent(Event& event) {
  scene_->OnEvent(event);
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
    ImGui::DockBuilderDockWindow("Viewport", dock_center);
    ImGui::DockBuilderDockWindow("Scene Properties", dock_right);
    ImGui::DockBuilderDockWindow("Gizmo Tools", dock_right);
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

    VkSampleCountFlags supported = renderer->GetSupportedMsaaFlags();

    const VkSampleCountFlagBits values[] = {
      VK_SAMPLE_COUNT_1_BIT,
      VK_SAMPLE_COUNT_2_BIT,
      VK_SAMPLE_COUNT_4_BIT,
      VK_SAMPLE_COUNT_8_BIT,
      VK_SAMPLE_COUNT_16_BIT,
      VK_SAMPLE_COUNT_32_BIT,
      VK_SAMPLE_COUNT_64_BIT
  };

    std::vector<const char*> labels;
    std::vector<VkSampleCountFlagBits> available;

    int selected_index = 0;
    int i = 0;
    for (auto v : values) {
      if (!(supported & v)) {
        i++;
        continue;
      }
      if (renderer->options().msaa_samples == v) {
        selected_index = i;
      }
      available.push_back(v);

      static char buffer[8];
      snprintf(buffer, sizeof(buffer), "%dx", (int)v);
      labels.push_back(strdup(buffer));  // or store properly
      i++;
    }


    if (ImGui::Combo("MSAA Samples", &selected_index, labels.data(),
                     labels.size())) {
      VkSampleCountFlagBits selected = available[selected_index];
      renderer->options().msaa_samples = selected;
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
  }
  ImGui::End();

  if (ImGui::Begin("Gizmo Tools")) {
    if (ImGui::RadioButton("Translate", current_op_ == ImGuizmo::TRANSLATE)) current_op_ = ImGuizmo::TRANSLATE;
    ImGui::SameLine();
    if (ImGui::RadioButton("Rotate",    current_op_ == ImGuizmo::ROTATE))    current_op_ = ImGuizmo::ROTATE;
    ImGui::SameLine();
    if (ImGui::RadioButton("Scale",     current_op_ == ImGuizmo::SCALE))     current_op_ = ImGuizmo::SCALE;
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

    if (!ignoreMenu && ImGui::IsMouseClicked(1, false))
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
      if (!meta || meta->source_path.empty()) continue;

      const auto& path = meta->source_path;

      // Check if this asset is under current_dir
      if (!current_dir.empty() && path.rfind(current_dir, 0) != 0) continue;

      // Get the remainder after current_dir prefix
      std::string remainder = current_dir.empty()
                                  ? path
                                  : path.substr(current_dir.size());

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
    // Resolves an imported file: if outside the project, copies into dest_dir.
    // For models (copy_folder=true): copies the entire parent folder to preserve
    // relative texture references (e.g. .gltf + textures, .obj + .mtl).
    // Returns a forward-slash relative path to the file.
    static auto ResolveImportPath = [](const std::string& file,
                                       const std::string& dest_dir,
                                       bool copy_folder) -> std::string {
      namespace fs = std::filesystem;
      fs::path abs = fs::absolute(file);
      fs::path cwd = fs::current_path();

      // Check if already under the project directory
      auto rel = fs::relative(abs, cwd);
      std::string relStr = rel.string();
      if (relStr.find("..") == std::string::npos) {
        // Already inside project, use as-is
        std::ranges::replace(relStr, '\\', '/');
        return relStr;
      }

      // Outside project - need to copy
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
        fs::path dest_file = dest / abs.filename();
        std::string result = fs::relative(dest_file, cwd).string();
        std::ranges::replace(result, '\\', '/');
        return result;
      } else {
        // Copy single file
        fs::path dest = fs::path(dest_dir) / abs.filename();
        fs::create_directories(dest_dir);
        fs::copy_file(abs, dest, fs::copy_options::skip_existing, ec);
        if (ec) {
          LOG_ERROR("Failed to copy asset '{}' to '{}': {}", file, dest.string(), ec.message());
          return "";
        }
        std::string result = fs::relative(dest, cwd).string();
        std::ranges::replace(result, '\\', '/');
        return result;
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
                          bool is_folder) -> bool {
        bool clicked = false;
        ImGui::PushID(label);

        ImVec2 cursor = ImGui::GetCursorScreenPos();
        ImVec2 icon_min = cursor;
        ImVec2 icon_max = ImVec2(cursor.x + tile_size, cursor.y + tile_size);

        // Invisible button for interaction
        if (ImGui::InvisibleButton("##tile", ImVec2(tile_size, tile_size + 20))) {
          clicked = true;
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

        // Icon rectangle
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

        // Tooltip with full name on hover
        if (hovered) {
          ImGui::SetTooltip("%s", label);
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
          auto slash = trimmed.find_last_of("/\\");
          current_dir = (slash != std::string::npos)
                            ? trimmed.substr(0, slash + 1)
                            : "";
        }
        NextColumn();
      }

      // Subdirectory tiles (single click to enter)
      for (const auto& dir : subdirs) {
        if (DrawTile(dir.c_str(), ImVec4(0.3f, 0.35f, 0.45f, 1.0f), "Directory",
                     false, true)) {
          current_dir += dir + "/";
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
          default:                  return "?";
        }
      };

      // Asset tiles
      for (auto& handle : visible_assets) {
        const auto* meta = mgr.GetMetadata(handle);
        if (!meta) continue;

        bool is_sel = selected_asset == handle;
        if (DrawTile(meta->name.c_str(), GetAssetColor(meta->type),
                     GetAssetAbbrev(meta->type), is_sel, false)) {
          selected_asset = handle;
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

  static bool viewportOpen = true;
  if (ImGui::Begin("Viewport", &viewportOpen)) {
    auto finalOutputDesc = renderer->GetFinalOutputDescriptor();
    auto finalOutputImage = renderer->GetFinalOutputImage();
    ImTextureID desc =
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

    ImGui::Image(desc, drawSize);
    //ImGui::Image(desc, ImVec2(texture->m_Width, texture->m_Height));

    ImVec2 imageMin = ImGui::GetItemRectMin(); // top-left of last item (the image)

    ImVec2 textPos = ImVec2(imageMin.x + 6, imageMin.y + 6);
    std::string fpsStr = fmt::format("FPS: {}", static_cast<int>(app_.GetFPS()));

    ImGui::GetWindowDrawList()->AddText(textPos, IM_COL32(0, 255, 0, 255), fpsStr.c_str());

    ImGuizmo::SetRect(6, 6, drawSize.x, drawSize.y);

    if (has_selected_entity_) {
      Ref<CameraData> cam = renderer->GetCameraData();
      glm::mat4 view = cam->view_matrix;
      glm::mat4 proj = cam->projection;
      proj[1][1] *= -1;
      TransformComponent& transform = scene_->GetComponent<TransformComponent>(selected_entity_);
      glm::mat4& model = transform.transform_matrix;
      ImGuizmo::SetOrthographic(false);
      ImGuizmo::SetDrawlist();
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
    }
  }
  ImGui::End();
}

void EditorLayer::OnPostPresent() {
  scene_->ProcessDestroyQueue();
}

void EditorLayer::OnPrePresent() {
  scene_->Render();
}

void EditorLayer::UpdateHierarchyOrder() {
  if (hierarchy_data_.move_from == entt::null || hierarchy_data_.move_to == entt::null) {
    return;
  }
  Entity fromEntity = {hierarchy_data_.move_from, scene_.get()};
  Entity toEntity = {hierarchy_data_.move_to, scene_.get()};
  auto& hierarcry = scene_->GetSceneHierarchy();
  if (hierarchy_data_.bottom_part) {
    // todo move hierarchy order on childs
    if (fromEntity.parent_handle() != entt::null) {
      scene_->UnlinkEntities(fromEntity.parent_handle(), hierarchy_data_.move_from);
    }
    hierarcry.erase(
        std::remove(hierarcry.begin(), hierarcry.end(), hierarchy_data_.move_from),
        hierarcry.end());
    auto insertPos = std::find(hierarcry.begin(), hierarcry.end(), hierarchy_data_.move_to) + 1;
    if (hierarcry.end() < insertPos) {
      hierarcry.push_back(hierarchy_data_.move_from);
    } else {
      hierarcry.insert(insertPos, hierarchy_data_.move_from);
    }
  } else {
    scene_->LinkEntities(hierarchy_data_.move_to, hierarchy_data_.move_from);
  }
}

}
