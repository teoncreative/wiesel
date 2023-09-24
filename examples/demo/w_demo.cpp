
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "w_demo.hpp"
#include "imgui_internal.h"
#include "input/w_input.hpp"
#include "layer/w_layerimgui.hpp"
#include "scene/w_componentutil.hpp"
#include "script/mono/w_monobehavior.hpp"
#include "systems/w_canvas_system.hpp"
#include "util/w_keycodes.hpp"
#include "util/w_math.hpp"
#include "w_engine.hpp"
#include "w_entrypoint.hpp"

#include <random>

using namespace Wiesel;

namespace WieselDemo {

DemoLayer::DemoLayer(DemoApplication& app) : m_App(app), Layer("Demo Layer") {
  m_Scene = app.GetScene();
  m_Renderer = Engine::GetRenderer();
}

DemoLayer::~DemoLayer() = default;

uint32_t nLehmer = 0;

uint32_t Lehmer32() {
  nLehmer += 0xe120fc15;
  uint64_t tmp;
  tmp = (uint64_t)nLehmer * 0x4a39b70d;
  uint32_t m1 = (tmp >> 32) ^ tmp;
  tmp = (uint64_t)m1 * 0x12fad5c9;
  uint32_t m2 = (tmp >> 32) ^ tmp;
  return m2;
}

void DemoLayer::OnAttach() {
  LOG_DEBUG("OnAttach");
  // Loading a model to the scene
  entt::entity sponzaEntity;
  entt::entity pointLightEntity;
  {
    Entity entity = m_Scene->CreateEntity("Sponza");
    sponzaEntity = entity.GetHandle();
    auto& transform = entity.GetComponent<TransformComponent>();
    transform.Scale = {0.01f, 0.01f, 0.01f};
    transform.Position = {5.0f, 2.0f, 0.0f};
    auto& model = entity.AddComponent<ModelComponent>();
    Engine::LoadModel(transform, model, "assets/models/sponza/sponza.gltf");
    auto& behaviors = entity.AddComponent<BehaviorsComponent>();
//    behaviors.AddBehavior<MonoBehavior>(entity, "TestBehavior");
  }
  {
    Entity entity = m_Scene->CreateEntity("Sponza 2");
    sponzaEntity = entity.GetHandle();
    auto& transform = entity.GetComponent<TransformComponent>();
    transform.Scale = {0.01f, 0.01f, 0.01f};
    transform.Position = {5.0f, 10.0f, 0.0f};
    auto& model = entity.AddComponent<ModelComponent>();
    Engine::LoadModel(transform, model, "assets/models/sponza/sponza.gltf");
    auto& behaviors = entity.AddComponent<BehaviorsComponent>();
    //    behaviors.AddBehavior<MonoBehavior>(entity, "TestBehavior");
  }
  {
    Entity entity = m_Scene->CreateEntity("Sponza 3");
    sponzaEntity = entity.GetHandle();
    auto& transform = entity.GetComponent<TransformComponent>();
    transform.Scale = {0.01f, 0.01f, 0.01f};
    transform.Position = {5.0f, 20.0f, 0.0f};
    auto& model = entity.AddComponent<ModelComponent>();
    Engine::LoadModel(transform, model, "assets/models/sponza/sponza.gltf");
    auto& behaviors = entity.AddComponent<BehaviorsComponent>();
    //    behaviors.AddBehavior<MonoBehavior>(entity, "TestBehavior");
  }
  {
    Entity entity = m_Scene->CreateEntity("Sponza 4");
    sponzaEntity = entity.GetHandle();
    auto& transform = entity.GetComponent<TransformComponent>();
    transform.Scale = {0.01f, 0.01f, 0.01f};
    transform.Position = {5.0f, 30.0f, 0.0f};
    auto& model = entity.AddComponent<ModelComponent>();
    Engine::LoadModel(transform, model, "assets/models/sponza/sponza.gltf");
    auto& behaviors = entity.AddComponent<BehaviorsComponent>();
    //    behaviors.AddBehavior<MonoBehavior>(entity, "TestBehavior");
  }
  {
    auto entity = m_Scene->CreateEntity("Directional Light");
    auto& transform = entity.GetComponent<TransformComponent>();
    transform.Position = glm::vec3(1.0f, 1.0f, 1.0f);
    entity.AddComponent<LightDirectComponent>();
  }
  {
    auto entity = m_Scene->CreateEntity("Point Light");
    pointLightEntity = entity.GetHandle();
    auto& transform = entity.GetComponent<TransformComponent>();
    transform.Position = glm::vec3{0.0f, 5.0f, 0.0f};
    entity.AddComponent<LightPointComponent>();
  }
  {
    auto entity = m_Scene->CreateEntity("Camera");
    auto& camera = entity.AddComponent<CameraComponent>();
    auto& transform = entity.GetComponent<TransformComponent>();
    transform.Position = glm::vec3(0.0f, 1.0f, 0.0f);
    camera.m_Camera.m_AspectRatio = Engine::GetRenderer()->GetAspectRatio();
    camera.m_Camera.m_IsPrimary = true;
    camera.m_Camera.m_IsChanged = true;
    auto& behaviors = entity.AddComponent<BehaviorsComponent>();
    behaviors.AddBehavior<MonoBehavior>(entity, "CameraScript");
  }
  //m_Scene->LinkEntities(sponzaEntity, pointLightEntity);
  /*{
    Entity entity = m_Scene->CreateEntity("Canvas");
    auto& ui = entity.AddComponent<CanvasComponent>();
  }*/

  m_Renderer->SetVsync(false);
}

void DemoLayer::OnDetach() {
  LOG_DEBUG("OnDetach");
}

void DemoLayer::OnUpdate(float_t deltaTime) {
  //LOG_INFO("OnUpdate {}", deltaTime);
  if (!m_Scene->GetPrimaryCamera()) {
    return;
  }
}

void DemoLayer::OnEvent(Event& event) {
  EventDispatcher dispatcher(event);

  dispatcher.Dispatch<KeyPressedEvent>(WIESEL_BIND_FN(OnKeyPress));
  dispatcher.Dispatch<KeyReleasedEvent>(WIESEL_BIND_FN(OnKeyReleased));
  dispatcher.Dispatch<MouseMovedEvent>(WIESEL_BIND_FN(OnMouseMoved));
}

bool DemoLayer::OnKeyPress(KeyPressedEvent& event) {
  if (event.GetKeyCode() == KeyF1) {
    m_App.Close();
    return true;
  } else if (event.GetKeyCode() == KeyEscape) {
    // todo add Input::GetCursorMode to C# api
    if (m_App.GetWindow()->GetCursorMode() == CursorModeRelative) {
      m_App.GetWindow()->SetCursorMode(CursorModeNormal);
    } else {
      m_App.GetWindow()->SetCursorMode(CursorModeRelative);
    }
    return false;
  }
  return false;
}

bool DemoLayer::OnKeyReleased(KeyReleasedEvent& event) {
  return false;
}

bool DemoLayer::OnMouseMoved(MouseMovedEvent& event) {
  return false;
}

DemoOverlay::DemoOverlay(DemoApplication& app, Ref<DemoLayer> demoLayer)
    : m_App(app), m_DemoLayer(demoLayer), Layer("Demo Overlay") {}

DemoOverlay::~DemoOverlay() = default;

void DemoOverlay::OnAttach() {
  LOG_DEBUG("OnAttach");
}

void DemoOverlay::OnDetach() {
  LOG_DEBUG("OnDetach");
}

void DemoOverlay::OnUpdate(float_t deltaTime) {}

void DemoOverlay::OnEvent(Wiesel::Event& event) {}

static entt::entity SelectedEntity;
static bool HasSelectedEntity = false;
static struct SceneHierarchyData {
  entt::entity MoveFrom = entt::null;
  entt::entity MoveTo = entt::null;
  bool BottomPart = false;
} HierarchyData;

void DemoOverlay::RenderEntity(Entity& entity, entt::entity entityId, int depth, bool& ignoreMenu) {
  auto& tagComponent = entity.GetComponent<TagComponent>();
  std::string tag = "";
  for (int i = 0; i < depth; i++) {
    tag += "\t";
  }
  tag += tagComponent.Tag;
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
      entt::entity newData = *(entt::entity*)payload->Data;
      HierarchyData.MoveFrom = newData;
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

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 4));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 1));
  ImGui::Selectable(tag.c_str(),
                        false,
                        ImGuiSelectableFlags_None | ImGuiSelectableFlags_Disabled, ImVec2(0, 1));
  ImGui::PopStyleVar(2);

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

void DemoOverlay::OnImGuiRender() {
  static bool scenePropertiesOpen = true;
  //ImGui::ShowDemoWindow(&scenePropertiesOpen);
  if (ImGui::Begin("Scene Properties", &scenePropertiesOpen)) {
    ImGui::SeparatorText("Controls");
    if (ImGui::Checkbox(PrefixLabel("Wireframe Mode").c_str(),
                        Engine::GetRenderer()->IsWireframeEnabledPtr())) {
      Engine::GetRenderer()->SetRecreateGraphicsPipeline(true);
    }
    if (ImGui::Button("Recreate Pipeline")) {
      Engine::GetRenderer()->SetRecreateGraphicsPipeline(true);
    }
    if (ImGui::Button("Recreate Shaders")) {
      Engine::GetRenderer()->SetRecreateShaders(true);
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
    if (ImGui::Button("Add Component"))
      ImGui::OpenPopup("add_component_popup");
    if (ImGui::BeginPopup("add_component_popup")) {
      GENERATE_COMPONENT_ADDERS(entity);
      ImGui::EndPopup();
    }

    GENERATE_COMPONENT_EDITORS(entity);
  }
  ImGui::End();
}

void DemoOverlay::UpdateHierarchyOrder() {
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

void DemoApplication::Init() {
  LOG_DEBUG("Init");
  Ref<DemoLayer> demoLayer = CreateReference<DemoLayer>(*this);
  PushLayer(demoLayer);
  PushOverlay(CreateReference<DemoOverlay>(*this, demoLayer));
}

DemoApplication::DemoApplication() : Application({"Wiesel Demo"}) {
  LOG_DEBUG("DemoApp constructor");
}

DemoApplication::~DemoApplication() {
  LOG_DEBUG("DemoApp destructor");
}
}  // namespace WieselDemo

// Called from entrypoint
Application* Wiesel::CreateApp() {
  return new WieselDemo::DemoApplication();
}
