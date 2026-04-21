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
#include "util/imgui/w_imguiutil.h"
#include "w_editor_components.h"
#include "w_editor_entity_factory.h"
#include "w_editor_icons.h"
#include "w_engine.h"

namespace wiesel::editor {

Scene* scene();

static bool RenderAddEntityMenu(Scene& scene, Entity entity, bool& dirty,
                                CommandStack& commands,
                                const glm::vec3* spawn_pos = nullptr) {
  Entity created =
      RenderEntityFactoryMenu(scene, commands, entity, spawn_pos);
  if (created) {
    dirty = true;
    return true;
  }
  return false;
}

// Check if an entity or any of its descendants match the search filter.
static bool EntityMatchesSearch(Entity& entity,
                                const std::string& filter) {
  if (filter.empty()) {
    return true;
  }
  auto& tag = entity.GetComponent<TagComponent>();
  std::string name_lower = tag.name;
  std::ranges::transform(name_lower, name_lower.begin(), ::tolower);
  if (name_lower.find(filter) != std::string::npos) {
    return true;
  }
  // Check children recursively
  if (entity.HasComponent<TreeComponent>()) {
    auto& tree = entity.GetComponent<TreeComponent>();
    for (entt::entity child : tree.children) {
      Entity child_entity{child, entity.GetScene()};
      if (EntityMatchesSearch(child_entity, filter)) {
        return true;
      }
    }
  }
  return false;
}

void EditorLayer::RenderEntity(Entity& entity, int depth,
                               bool& ignore_menu) {
  auto& tag_component = entity.GetComponent<TagComponent>();

  // Filter by search
  std::string filter(hierarchy_search_);
  std::ranges::transform(filter, filter.begin(), ::tolower);
  if (!filter.empty() &&
      !EntityMatchesSearch(entity, filter)) {
    return;
  }

  bool has_children =
      entity.child_handles() && !entity.child_handles()->empty();
  bool is_selected = selected_entity_ == entity;
  bool is_renaming = renaming_entity_ == entity;

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
      renaming_entity_ = {};
    }
    // Cancel on escape
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
      renaming_entity_ = {};
    }
    // Lose focus = confirm
    if (!ImGui::IsItemActive() && !ImGui::IsItemDeactivated()) {
      // Not yet focused - set focus
      ImGui::SetKeyboardFocusHere(-1);
    } else if (ImGui::IsItemDeactivated() && renaming_entity_) {
      if (rename_entity_buf_[0] != '\0') {
        tag_component.name = rename_entity_buf_;
        scene_dirty_ = true;
      }
      renaming_entity_ = {};
    }
    return;
  }

  // Prefix the row with the entity's highest-priority component icon.
  std::string label;
  std::string_view ent_icon = GetEntityIcon(entity);
  if (!ent_icon.empty()) {
    label.append(ent_icon);
    label.append("  ");
  }
  label.append(tag_component.name);
  label.append("##");
  label.append(std::to_string(static_cast<uint32_t>(entity.handle())));

  ImGuiTreeNodeFlags flags =
      ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth;
  if (is_selected) {
    flags |= ImGuiTreeNodeFlags_Selected;
  }
  if (!has_children) {
    flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
      ImGuiTreeNodeFlags_KeepArrowSpaceOnLeaf;
  }
  // Auto-open nodes when searching or scrolling to a descendant
  if (!filter.empty() || open_ancestors_.contains(entity.ToRef())) {
    ImGui::SetNextItemOpen(true);
  }

  bool node_open = ImGui::HierarchyTreeNodeEx(label.c_str(), flags);

  // Scroll to selected entity when requested (e.g., after viewport click)
  if (is_selected && scroll_to_selected_) {
    ImGui::ScrollToItem();
    scroll_to_selected_ = false;
    open_ancestors_.clear();
  }

  // Selection on release (not during drag)
  if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(0) &&
      !ImGui::IsItemToggledOpen() && !ImGui::IsDragDropActive()) {
    selected_entity_ = entity.ToRef();
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
    renaming_entity_ = entity.ToRef();
    strncpy(rename_entity_buf_, tag_component.name.c_str(),
            sizeof(rename_entity_buf_) - 1);
    rename_entity_buf_[sizeof(rename_entity_buf_) - 1] = '\0';
  }

  // Drag & drop source
  ImGuiDragDropFlags src_flags = ImGuiDragDropFlags_SourceNoDisableHover;
  if (ImGui::BeginDragDropSource(src_flags)) {
    ImGui::Text("%s", tag_component.name.c_str());
    HierarchyDragPayload drag_payload{entity.ToRef()};
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
      hierarchy_data_.move_from = drop_data->entity_ref;
      hierarchy_data_.move_to = entity.ToRef();
      hierarchy_data_.bottom_part = false;
    }
    ImGui::EndDragDropTarget();
  }

  // Context menu
  if (ImGui::BeginPopupContextItem()) {
    ImGui::PopStyleVar();
    selected_entity_ = entity.ToRef();
    ImGui::SetNextMenuItemIcon(ICON_LC_PLUS);
    if (ImGui::BeginMenu("Add Child")) {
      RenderAddEntityMenu(*entity.GetScene(), entity, scene_dirty_, command_stack_);
      ImGui::EndMenu();
    }
    ImGui::SetNextMenuItemIcon(ICON_LC_PACKAGE);
    if (ImGui::MenuItem("Save as Prefab...")) {
      EntityRef ref = entity.ToRef();
      file_picker_.OpenSave(
          "Save as Prefab", ".wprefab",
          [this, ref](const std::string& vfs_path) {
            auto physical = Engine::vfs()->ResolvePhysicalPath(vfs_path);
            if (!physical) {
              return;
            }
            Entity entity = ref.Resolve();
            Prefab::SaveToFile(entity, *physical);
            ScanProjectAssets();
          });
    }
    ImGui::Separator();
    ImGui::SetNextMenuItemIcon(ICON_LC_TRASH);
    if (ImGui::MenuItem("Delete")) {
      command_stack_.Execute(
          std::make_unique<EntityDeleteCommand>(entity));
      selected_entity_ = EntityRef{};
      scene_dirty_ = true;
    }
    ImGui::EndPopup();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0f));
    ignore_menu = true;
  }

  // Recurse into children if the tree node is open
  if (has_children && node_open) {
    for (const auto& child_entity_id : *entity.child_handles()) {
      Entity child = {child_entity_id, entity.GetScene()};
      RenderEntity(child, depth + 1,
                   ignore_menu);
    }
    ImGui::TreePop();
  }
}

void EditorLayer::RenderSceneHierarchyPanel() {
  bool& scene_open = panel_scene_hierarchy_;
  if (scene_open) {
    if (ImGui::Begin(ICON_LC_LAYERS " Scene Hierarchy", &scene_open)) {
      bool ignore_menu = false;

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
          selected_entity_ = EntityRef{};
        }
        ImGui::Separator();
      }

      if (scroll_to_selected_ && selected_entity_) {
        open_ancestors_.clear();
        Entity walk = selected_entity_.Resolve();
        while (!walk) {
          Entity parent = walk.GetParent();
          if (parent) {
            open_ancestors_.insert(parent.ToRef());
            walk = parent;
          } else {
            break;
          }
        }
      }

      ImGui::SetNextItemWidth(-1);
      ImGui::InputTextWithHint("##HierarchySearch",
                               ICON_LC_SEARCH "  Search entities...",
                               hierarchy_search_, sizeof(hierarchy_search_));

      ImGui::FullWidthSeparator();

      // Rows sit flush against each other. SameLine X spacing is preserved.
      ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                          ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0f));

      const auto& loaded_scenes = Engine::scene_manager().GetLoadedScenes();
      for (size_t scene_idx = 0; scene_idx < loaded_scenes.size();
           ++scene_idx) {
        ImGui::Dummy(ImVec2(0.0f, ImGui::GetStyle().WindowPadding.y));

        Scene* current_scene = loaded_scenes[scene_idx].get();
        ImGuiTreeNodeFlags scene_flags =
            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow |
            ImGuiTreeNodeFlags_SpanFullWidth;
        std::string scene_label = current_scene->GetName();
        if (scene_label.empty()) {
          const std::string& src = current_scene->GetSourcePath();
          if (!src.empty()) {
            scene_label = VirtualFileSystem::Stem(src);
          } else {
            scene_label = "Scene";
          }
        }
        std::string scene_node_label =
            std::string(ICON_LC_LAYERS_2 "  ") + scene_label + "##SceneRoot_" +
            std::to_string(scene_idx);
        bool scene_node_open = ImGui::HierarchyTreeNodeEx(
            scene_node_label.c_str(), scene_flags, /*static_tint=*/true);

        std::string ctx_id = "scene_root_context_" + std::to_string(scene_idx);
        if (ImGui::BeginPopupContextItem(ctx_id.c_str())) {
          ImGui::PopStyleVar();
          ImGui::SetNextMenuItemIcon(ICON_LC_PLUS);
          if (ImGui::BeginMenu("Add")) {
            RenderAddEntityMenu(*current_scene, kInvalidEntity, scene_dirty_, command_stack_);
            ImGui::EndMenu();
          }
          ImGui::EndPopup();
          ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                              ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0f));
          ignore_menu = true;
        }

        if (scene_node_open) {
          for (const auto& entity_id : current_scene->GetSceneHierarchy()) {
            Entity entity = {entity_id, current_scene};
            if (entity.GetParent()) {
              continue;
            }
            RenderEntity(entity, 0, ignore_menu);
          }
          ImGui::TreePop();
        }
      }

      // Pop the zero-Y ItemSpacing pushed above the scene loop.
      ImGui::PopStyleVar();

      UpdateHierarchyOrder();

      ImVec2 avail = ImGui::GetContentRegionAvail();
      if (avail.y > 0) {
        ImGui::InvisibleButton("##HierarchyDropZone", ImVec2(avail.x, avail.y));
        if (ImGui::BeginDragDropTarget()) {
          if (const ImGuiPayload* payload =
                  ImGui::AcceptDragDropPayload("SceneHierarchy Entity")) {
            HierarchyDragPayload* drop_data =
                static_cast<HierarchyDragPayload*>(payload->Data);
            if (drop_data->entity_ref) {
              // We gonna unparent the entity
              Entity target_entity = drop_data->entity_ref.Resolve();
              if (target_entity) {
                Entity parent_entity = target_entity.GetParent();
                if (parent_entity) {
                  Scene* scene = parent_entity.GetScene();
                  scene->UnlinkEntities(parent_entity,
                                             target_entity);
                  scene_dirty_ = true;
                }
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
          selected_entity_ = EntityRef{};
        }

        if (ImGui::IsItemClicked(1)) {
          ImGui::OpenPopup("right_click_hierarchy");
        }
      } else {
        if (!ignore_menu && ImGui::IsWindowHovered() &&
            ImGui::IsMouseClicked(1, false)) {
          ImGui::OpenPopup("right_click_hierarchy");
        }
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0) &&
            !ImGui::IsAnyItemHovered()) {
          selected_entity_ = EntityRef{};
        }
      }

      if (ImGui::BeginPopup("right_click_hierarchy")) {
        ImGui::SetNextMenuItemIcon(ICON_LC_PLUS);
        if (ImGui::BeginMenu("Add")) {
          RenderAddEntityMenu(*scene(), kInvalidEntity, scene_dirty_, command_stack_);
          ImGui::EndMenu();
        }
        ImGui::EndPopup();
      }
    }
    ImGui::End();
  }
}

void EditorLayer::UpdateHierarchyOrder() {
  Entity from_entity = hierarchy_data_.move_from.Resolve();
  Entity to_entity = hierarchy_data_.move_to.Resolve();
  if (!from_entity || !to_entity) {
    return;
  }
  Scene* from_scene = from_entity.GetScene();
  Scene* to_scene = to_entity.GetScene();

  // Cross-scene move: transfer entity to the target scene
  if (from_scene != to_scene) {
    Entity moved =
        Engine::scene_manager().MoveEntityToScene(from_entity, to_scene);
    if (moved) {
      to_scene->LinkEntities(hierarchy_data_.move_to, moved.handle());
    }
  } else {
    // Same-scene move
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
  hierarchy_data_.move_from = kInvalidEntityRef;
  hierarchy_data_.move_to = kInvalidEntityRef;
}

}  // namespace wiesel::editor
