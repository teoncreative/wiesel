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

#include <imgui.h>

#include "asset/w_asset_manager.h"
#include "imgui_internal.h"
#include "scene/w_lights.h"
#include "scene/w_prefab.h"
#include "scene/w_scene_manager.h"
#include "w_editor_icons.h"
#include "w_engine.h"

namespace Wiesel::Editor {

// Defined in w_editor.cc
std::shared_ptr<Scene> scene();

static bool RenderAddEntityMenu(Scene& scene, bool& dirty,
                                CommandStack& commands,
                                entt::entity parent = entt::null,
                                const glm::vec3* spawn_pos = nullptr) {
  Entity created{entt::null, nullptr};

  if (ImGui::MenuItem("Empty Entity")) {
    created = scene.CreateEntity();
  }

  if (ImGui::BeginMenu("3D Shape")) {
    const char* shapes[] = {"Cube", "Sphere", "Plane", "Cylinder", "Capsule"};
    for (const char* shape : shapes) {
      if (ImGui::MenuItem(shape)) {
        created = scene.CreateEntity(shape);
        auto& mc = created.AddComponent<MeshRendererComponent>();
        mc.model_handle = Engine::GetPrimitive(shape);
        mc.mesh_index = 0;
      }
    }
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("Light")) {
    if (ImGui::MenuItem("Directional Light")) {
      created = scene.CreateEntity("Directional Light");
      created.AddComponent<LightDirectComponent>();
    }
    if (ImGui::MenuItem("Point Light")) {
      created = scene.CreateEntity("Point Light");
      created.AddComponent<LightPointComponent>();
    }
    ImGui::EndMenu();
  }

  if (ImGui::MenuItem("Camera")) {
    created = scene.CreateEntity("Camera");
    created.AddComponent<CameraComponent>();
  }

  if (created.handle() != entt::null) {
    if (parent != entt::null) {
      scene.LinkEntities(parent, created);
    }
    if (spawn_pos) {
      auto& tc = created.GetComponent<TransformComponent>();
      tc.SetPosition(*spawn_pos);
    }
    {
      auto shared_scene = Engine::scene_manager().FindSceneByPtr(&scene);
      if (shared_scene) {
        commands.Execute(std::make_unique<EntityCreateCommand>(
            shared_scene, created.handle()));
      }
    }
    dirty = true;
    return true;
  }

  return false;
}

// Check if an entity or any of its descendants match the search filter.
static bool EntityMatchesSearch(Scene& scene, entt::entity entity_id,
                                const std::string& filter) {
  if (filter.empty()) {
    return true;
  }
  auto& tag = scene.GetComponent<TagComponent>(entity_id);
  std::string name_lower = tag.name;
  std::ranges::transform(name_lower, name_lower.begin(), ::tolower);
  if (name_lower.find(filter) != std::string::npos) {
    return true;
  }
  // Check children recursively
  if (scene.HasComponent<TreeComponent>(entity_id)) {
    auto& tree = scene.GetComponent<TreeComponent>(entity_id);
    for (auto child : tree.childs) {
      if (EntityMatchesSearch(scene, child, filter)) {
        return true;
      }
    }
  }
  return false;
}

// Helper to find the index of a scene in the loaded scenes vector.
static int FindSceneIndex(const std::shared_ptr<Scene>& target) {
  const auto& loaded = Engine::scene_manager().GetLoadedScenes();
  for (size_t i = 0; i < loaded.size(); ++i) {
    if (loaded[i] == target) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

void EditorLayer::RenderEntity(Entity& entity, entt::entity entity_id,
                               std::shared_ptr<Scene> entity_scene, int depth,
                               bool& ignore_menu) {
  auto& tag_component = entity.GetComponent<TagComponent>();

  // Filter by search
  std::string filter(hierarchy_search_);
  std::ranges::transform(filter, filter.begin(), ::tolower);
  if (!filter.empty() &&
      !EntityMatchesSearch(*entity_scene, entity_id, filter)) {
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
      ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
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
    selected_entity_scene_ = entity_scene;
    has_selected_entity_ = true;
    inspector_mode_ = InspectorMode::Entity;
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
    HierarchyDragPayload drag_payload{entity_id, FindSceneIndex(entity_scene)};
    ImGui::SetDragDropPayload("SceneHierarchy Entity", &drag_payload,
                              sizeof(HierarchyDragPayload));
    ImGui::EndDragDropSource();
  }

  // Drag & drop target: drop ON entity to make it a child
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload* payload =
            ImGui::AcceptDragDropPayload("SceneHierarchy Entity")) {
      HierarchyDragPayload* drop_data =
          static_cast<HierarchyDragPayload*>(payload->Data);
      const auto& loaded = Engine::scene_manager().GetLoadedScenes();
      hierarchy_data_.move_from = drop_data->entity;
      if (drop_data->scene_index >= 0 &&
          drop_data->scene_index < static_cast<int>(loaded.size())) {
        hierarchy_data_.move_from_scene = loaded[drop_data->scene_index];
      }
      hierarchy_data_.move_to = entity_id;
      hierarchy_data_.move_to_scene = entity_scene;
      hierarchy_data_.bottom_part = false;
    }
    ImGui::EndDragDropTarget();
  }

  // Context menu
  if (ImGui::BeginPopupContextItem()) {
    selected_entity_ = entity_id;
    selected_entity_scene_ = entity_scene;
    has_selected_entity_ = true;
    if (ImGui::BeginMenu("Add Child")) {
      RenderAddEntityMenu(*entity_scene, scene_dirty_, command_stack_,
                          entity_id);
      ImGui::EndMenu();
    }
    if (ImGui::MenuItem("Save as Prefab...")) {
      file_picker_.OpenSave(
          "Save as Prefab", ".wprefab",
          [this, entity_id, entity_scene](const std::string& vfs_path) {
            auto physical = Engine::vfs()->ResolvePhysicalPath(vfs_path);
            if (!physical) {
              return;
            }
            Entity ent{entity_id, entity_scene.get()};
            Prefab::SaveToFile(ent, *physical);
            ScanProjectAssets();
          });
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Delete")) {
      command_stack_.Execute(
          std::make_unique<EntityDeleteCommand>(entity_scene, entity_id));
      has_selected_entity_ = false;
      scene_dirty_ = true;
    }
    ImGui::EndPopup();
    ignore_menu = true;
  }

  // Recurse into children if the tree node is open
  if (has_children && node_open) {
    for (const auto& child_entity_id : *entity.child_handles()) {
      Entity child = {child_entity_id, entity_scene.get()};
      RenderEntity(child, child_entity_id, entity_scene, depth + 1,
                   ignore_menu);
    }
    ImGui::TreePop();
  }
}

void EditorLayer::RenderSceneHierarchyPanel() {
  bool& scene_open = panel_scene_hierarchy_;
  if (scene_open) {
    if (ImGui::Begin(CODICON_SYMBOL_RULER " Scene Hierarchy", &scene_open)) {
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

      if (scroll_to_selected_ && has_selected_entity_ &&
          selected_entity_scene_) {
        open_ancestors_.clear();
        entt::entity walk = selected_entity_;
        while (walk != entt::null) {
          if (selected_entity_scene_->HasComponent<TreeComponent>(walk)) {
            entt::entity parent =
                selected_entity_scene_->GetComponent<TreeComponent>(walk)
                    .parent;
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

      const std::vector<std::shared_ptr<Scene>>& loaded_scenes =
          Engine::scene_manager().GetLoadedScenes();
      for (size_t scene_idx = 0; scene_idx < loaded_scenes.size();
           ++scene_idx) {
        const std::shared_ptr<Scene>& current_scene = loaded_scenes[scene_idx];
        ImGuiTreeNodeFlags scene_flags =
            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow |
            ImGuiTreeNodeFlags_SpanAvailWidth |
            ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_Framed;
        std::string scene_label = current_scene->GetName();
        if (scene_label.empty()) {
          const std::string& src = current_scene->GetSourcePath();
          if (!src.empty()) {
            scene_label = VirtualFileSystem::Stem(src);
          } else {
            scene_label = "Scene";
          }
        }
        std::string scene_id = "##SceneRoot_" + std::to_string(scene_idx);
        bool scene_node_open = ImGui::TreeNodeEx(scene_id.c_str(), scene_flags,
                                                 "%s", scene_label.c_str());

        std::string ctx_id = "scene_root_context_" + std::to_string(scene_idx);
        if (ImGui::BeginPopupContextItem(ctx_id.c_str())) {
          if (ImGui::BeginMenu("Add")) {
            RenderAddEntityMenu(*current_scene, scene_dirty_, command_stack_);
            ImGui::EndMenu();
          }
          ImGui::EndPopup();
          ignoreMenu = true;
        }

        if (scene_node_open) {
          for (const auto& entity_id : current_scene->GetSceneHierarchy()) {
            Entity entity = {entity_id, current_scene.get()};
            if (entity.GetParent()) {
              continue;
            }
            RenderEntity(entity, entity_id, current_scene, 0, ignoreMenu);
          }
          ImGui::TreePop();
        }
      }

      UpdateHierarchyOrder();

      ImVec2 avail = ImGui::GetContentRegionAvail();
      if (avail.y > 0) {
        ImGui::InvisibleButton("##HierarchyDropZone", ImVec2(avail.x, avail.y));
        if (ImGui::BeginDragDropTarget()) {
          if (const ImGuiPayload* payload =
                  ImGui::AcceptDragDropPayload("SceneHierarchy Entity")) {
            HierarchyDragPayload* drop_data =
                static_cast<HierarchyDragPayload*>(payload->Data);
            const auto& loaded = Engine::scene_manager().GetLoadedScenes();
            if (drop_data->scene_index >= 0 &&
                drop_data->scene_index < static_cast<int>(loaded.size())) {
              Scene* drop_scene = loaded[drop_data->scene_index].get();
              Entity dropped_entity = {drop_data->entity, drop_scene};
              if (dropped_entity.parent_handle() != entt::null) {
                drop_scene->UnlinkEntities(dropped_entity.parent_handle(),
                                           drop_data->entity);
                scene_dirty_ = true;
              }
            }
          }
          // Accept model asset drop to instantiate
          if (const ImGuiPayload* payload =
                  ImGui::AcceptDragDropPayload("AssetHandle")) {
            AssetHandle handle =
                *static_cast<const AssetHandle*>(payload->Data);
            InstantiateModelAsset(handle);
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
          RenderAddEntityMenu(*scene(), scene_dirty_, command_stack_);
          ImGui::EndMenu();
        }
        ImGui::EndPopup();
      }
    }
    ImGui::End();
  }
}

void EditorLayer::UpdateHierarchyOrder() {
  if (hierarchy_data_.move_from == entt::null ||
      hierarchy_data_.move_to == entt::null) {
    return;
  }

  std::shared_ptr<Scene> from_scene = hierarchy_data_.move_from_scene;
  std::shared_ptr<Scene> to_scene = hierarchy_data_.move_to_scene;

  // Cross-scene move: transfer entity to the target scene
  if (from_scene != to_scene) {
    Entity from_entity = {hierarchy_data_.move_from, from_scene.get()};
    Entity moved =
        Engine::scene_manager().MoveEntityToScene(from_entity, to_scene);
    if (moved) {
      to_scene->LinkEntities(hierarchy_data_.move_to, moved.handle());
    }
  } else {
    // Same-scene move
    Entity from_entity = {hierarchy_data_.move_from, from_scene.get()};
    if (hierarchy_data_.bottom_part) {
      // Reorder in hierarchy
      if (from_entity.parent_handle() != entt::null) {
        from_scene->UnlinkEntities(from_entity.parent_handle(),
                                   hierarchy_data_.move_from);
      }
      auto& hierarchy = to_scene->GetSceneHierarchy();
      std::erase(hierarchy, hierarchy_data_.move_from);
      auto insert_pos =
          std::ranges::find(hierarchy, hierarchy_data_.move_to) + 1;
      if (hierarchy.end() < insert_pos) {
        hierarchy.push_back(hierarchy_data_.move_from);
      } else {
        hierarchy.insert(insert_pos, hierarchy_data_.move_from);
      }
    } else {
      from_scene->LinkEntities(hierarchy_data_.move_to,
                               hierarchy_data_.move_from);
    }
  }

  hierarchy_data_.move_from = entt::null;
  hierarchy_data_.move_from_scene.reset();
  hierarchy_data_.move_to = entt::null;
  hierarchy_data_.move_to_scene.reset();
}

}  // namespace Wiesel::Editor
