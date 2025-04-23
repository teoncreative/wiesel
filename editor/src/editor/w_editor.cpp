//
// Created by Metehan Gezer on 18/04/2025.
//

#include "editor/w_editor.hpp"
#include "script/w_scriptmanager.hpp"
#include "imgui_internal.h"
#include "scene/w_componentutil.hpp"
#include "w_engine.hpp"

namespace Wiesel::Editor {

EditorOverlay::EditorOverlay(Application& app, Ref<Scene> scene)
    : m_App(app), m_Scene(scene), Layer("Demo Overlay") {}

EditorOverlay::~EditorOverlay() = default;

void EditorOverlay::OnAttach() {
  LOG_DEBUG("OnAttach");
}

void EditorOverlay::OnDetach() {
  LOG_DEBUG("OnDetach");
}

void EditorOverlay::OnUpdate(float_t deltaTime) {}

void EditorOverlay::OnEvent(Wiesel::Event& event) {}

static entt::entity SelectedEntity;
static bool HasSelectedEntity = false;
static struct SceneHierarchyData {
  entt::entity MoveFrom = entt::null;
  entt::entity MoveTo = entt::null;
  bool BottomPart = false;
} HierarchyData;

void EditorOverlay::RenderEntity(Entity& entity, entt::entity entityId, int depth, bool& ignoreMenu) {
  auto& tagComponent = entity.GetComponent<TagComponent>();
  std::string tag = "";
  for (int i = 0; i < depth; i++) {
    tag += "\t";
  }
  tag += tagComponent.Tag;
  tag += "##";
  tag += (uint32_t) entityId;

  if (ImGui::Selectable(tag.c_str(),
                        HasSelectedEntity && SelectedEntity == entityId,
                        ImGuiSelectableFlags_None, ImVec2(0, 0))) {
    SelectedEntity = entityId;
    HasSelectedEntity = true;
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
    ImGui::SetDragDropPayload("SceneHierarchy Entity", &entityId, sizeof(entt::entity));
    ImGui::EndDragDropSource();
  }

  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SceneHierarchy Entity", target_flags)) {
      entt::entity* newData = static_cast<entt::entity*>(payload->Data);
      HierarchyData.MoveFrom = *newData;
      HierarchyData.MoveTo = entityId;
      HierarchyData.BottomPart = false;
    }
    ImGui::EndDragDropTarget();
  }

  if (ImGui::BeginDragDropSource(src_flags)) {
    if (!(src_flags & ImGuiDragDropFlags_SourceNoPreviewTooltip)) {
      ImGui::Text("%s", tag.c_str());
    }
    ImGui::SetDragDropPayload("SceneHierarchy Entity", &entityId, sizeof(entt::entity));
    ImGui::EndDragDropSource();
  }

  if (ImGui::BeginDragDropTarget()) {
    target_flags |= ImGuiDragDropFlags_AcceptNoDrawDefaultRect; // Don't display the yellow rectangle
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SceneHierarchy Entity", target_flags)) {
      entt::entity newData = *(entt::entity*)payload->Data;
      HierarchyData.MoveFrom = newData;
      HierarchyData.MoveTo = entityId;
      HierarchyData.BottomPart = true;
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
    SelectedEntity = entityId;
    if (ImGui::Button("Remove Entity")) {
      m_App.GetScene()->DestroyEntity(entity);
      HasSelectedEntity = false;
    }
    ImGui::EndPopup();
    ignoreMenu = true;
  }
  if (entity.GetChildHandles()) {
    for (const auto& childEntityId : *entity.GetChildHandles()) {
      Entity child = {childEntityId, &*m_App.GetScene()};
      RenderEntity(child, childEntityId, depth + 1, ignoreMenu);
    }
  }
}

void EditorOverlay::OnImGuiRender() {
  ImGui::DockSpaceOverViewport();

  static bool scenePropertiesOpen = true;
  //ImGui::ShowDemoWindow(&scenePropertiesOpen);
  if (ImGui::Begin("Scene Properties", &scenePropertiesOpen)) {
    ImGui::SeparatorText("Controls");
    if (ImGui::Checkbox(PrefixLabel("Wireframe Mode").c_str(),
                        Engine::GetRenderer()->IsWireframeEnabledPtr())) {
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

  static bool sceneOpen = true;
  if (ImGui::Begin("Scene Hierarchy", &sceneOpen)) {
    bool ignoreMenu = false;

    for (const auto& entityId : m_App.GetScene()->GetSceneHierarchy()) {
      Entity entity = {entityId, &*m_App.GetScene()};
      if (entity.GetParent()) {
        continue;
      }

      RenderEntity(entity, entityId, 0, ignoreMenu);
    }

    UpdateHierarchyOrder();

    if (!ignoreMenu && ImGui::IsMouseClicked(1, false))
      ImGui::OpenPopup("right_click_hierarcy");
    if (ImGui::BeginPopup("right_click_hierarcy")) {
      if (ImGui::BeginMenu("Add")) {
        if (ImGui::MenuItem("Empty Object")) {
          m_App.GetScene()->CreateEntity();
          ImGui::CloseCurrentPopup();
        }
        ImGui::EndMenu();
      }
      ImGui::EndPopup();
    }
  }
  ImGui::End();

  static bool componentsOpen = true;
  if (ImGui::Begin("Components", &componentsOpen) && HasSelectedEntity) {
    Entity entity = {SelectedEntity, &*m_App.GetScene()};
    TagComponent& tag = entity.GetComponent<TagComponent>();
    if (ImGui::InputText("##", &tag.Tag, ImGuiInputTextFlags_AutoSelectAll)) {
      if (tag.Tag[0] == ' ') {
        TrimLeft(tag.Tag);
      }

      if (tag.Tag.empty()) {
        tag.Tag = "Entity";
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
  Ref<AttachmentTexture> texture = Engine::GetRenderer()->GetCompositeColorResolveImage();
  if (ImGui::Begin("Viewport", &viewportOpen)) {
    if (!texture->m_Descriptors) {
      texture->m_Descriptors = Engine::GetRenderer()->CreateDescriptors(texture);
    }
    ImTextureID desc =
        reinterpret_cast<ImTextureID>(texture->m_Descriptors->m_DescriptorSet);

    ImVec2 avail = ImGui::GetContentRegionAvail();
    float imageAspect = (float)texture->m_Width / (float)texture->m_Height;
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
    std::string fpsStr = fmt::format("FPS: {}", static_cast<int>(m_App.GetFPS()));

    ImGui::GetWindowDrawList()->AddText(textPos, IM_COL32(0, 255, 0, 255), fpsStr.c_str());
  }
  ImGui::End();
}

void EditorOverlay::UpdateHierarchyOrder() {
  if (HierarchyData.MoveFrom == entt::null || HierarchyData.MoveTo == entt::null) {
    return;
  }
  Entity fromEntity = {HierarchyData.MoveFrom, &*m_App.GetScene()};
  Entity toEntity = {HierarchyData.MoveTo, &*m_App.GetScene()};
  auto& hierarcry = m_App.GetScene()->GetSceneHierarchy();
  if (HierarchyData.BottomPart) {
    // todo move hierarchy order on childs
    if (fromEntity.GetParentHandle() != entt::null) {
      m_App.GetScene()->UnlinkEntities(fromEntity.GetParentHandle(), HierarchyData.MoveFrom);
    }
    hierarcry.erase(
        std::remove(hierarcry.begin(), hierarcry.end(), HierarchyData.MoveFrom),
        hierarcry.end());
    auto insertPos = std::find(hierarcry.begin(), hierarcry.end(), HierarchyData.MoveTo) + 1;
    if (hierarcry.end() < insertPos) {
      hierarcry.push_back(HierarchyData.MoveFrom);
    } else {
      hierarcry.insert(insertPos, HierarchyData.MoveFrom);
    }
  } else {
    m_App.GetScene()->LinkEntities(HierarchyData.MoveTo, HierarchyData.MoveFrom);
  }
}

}
