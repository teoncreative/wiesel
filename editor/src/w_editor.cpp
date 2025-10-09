//
// Created by Metehan Gezer on 18/04/2025.
//

#include "w_editor.hpp"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <ImGuizmo.h>

#include "imgui_internal.h"
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
  ImGui::DockSpaceOverViewport();

  static bool scenePropertiesOpen = true;
  //ImGui::ShowDemoWindow(&scenePropertiesOpen);
  if (ImGui::Begin("Scene Properties", &scenePropertiesOpen)) {
    ImGui::SeparatorText("Controls");
    if (ImGui::Checkbox(PrefixLabel("Wireframe Mode").c_str(),
                        Engine::GetRenderer()->IsWireframeEnabledPtr())) {
      Engine::GetRenderer()->SetRecreatePipeline(true);
    }
    if (ImGui::Checkbox(PrefixLabel("Enable SSAO").c_str(),
                        Engine::GetRenderer()->IsSSAOEnabledPtr())) {
      Engine::GetRenderer()->SetRecreatePipeline(true);
    }
    if (ImGui::Checkbox(PrefixLabel("Only SSAO").c_str(),
                        Engine::GetRenderer()->IsOnlySSAOPtr())) {
      Engine::GetRenderer()->SetRecreatePipeline(true);
    }
    if (ImGui::Button("Recreate Pipeline")) {
      Engine::GetRenderer()->SetRecreatePipeline(true);
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
  static bool viewportOpen = true;
  if (ImGui::Begin("Viewport", &viewportOpen)) {
    ImTextureID desc =
        reinterpret_cast<ImTextureID>(Engine::GetRenderer()->GetCameraData()->composite_output_descriptor->descriptor_set_);

    ImVec2 avail = ImGui::GetContentRegionAvail();
    float imageAspect = (float)Engine::GetRenderer()->GetCameraData()->composite_color_image->width_ / (float)Engine::GetRenderer()->GetCameraData()->composite_color_image->height_;
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
      Ref<CameraData> cam = Engine::GetRenderer()->GetCameraData();
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
