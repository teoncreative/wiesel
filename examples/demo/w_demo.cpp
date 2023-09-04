
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
  {
    Entity entity = m_Scene->CreateEntity("Sponza");
    auto& transform = entity.GetComponent<TransformComponent>();
    transform.Scale = {0.01f, 0.01f, 0.01f};
    auto& model = entity.AddComponent<ModelComponent>();
    Engine::LoadModel(transform, model, "assets/models/sponza/sponza.gltf");
    auto& behaviors = entity.AddComponent<BehaviorsComponent>();
    behaviors.AddBehavior<MonoBehavior>(entity, "TestBehavior");
  }
  {
    auto entity = m_Scene->CreateEntity("Directional Light");
    auto& transform = entity.GetComponent<TransformComponent>();
    transform.Position = glm::vec3(1.0f, 1.0f, 1.0f);
    entity.AddComponent<LightDirectComponent>();
  }
  {
    auto entity = m_Scene->CreateEntity("Point Light");
    auto& transform = entity.GetComponent<TransformComponent>();
    transform.Position = glm::vec3{0.0f, 1.0f, 0.0f};
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
  }
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

static entt::entity selectedEntity;
static bool hasSelectedEntity = false;

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

    for (const auto& item :
         m_App.GetScene()->GetAllEntitiesWith<TagComponent>()) {
      Entity entity = {item, &*m_App.GetScene()};
      auto& tagComponent = entity.GetComponent<TagComponent>();
      if (ImGui::Selectable(tagComponent.Tag.c_str(),
                            hasSelectedEntity && selectedEntity == item,
                            ImGuiSelectableFlags_None, ImVec2(0, 0))) {
        selectedEntity = item;
        hasSelectedEntity = true;
      }
      if (ImGui::BeginPopupContextItem()) {
        selectedEntity = item;
        if (ImGui::Button("Remove Entity")) {
          m_App.GetScene()->DestroyEntity(entity);
          hasSelectedEntity = false;
        }
        ImGui::EndPopup();
        ignoreMenu = true;
      }
    }

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
  if (ImGui::Begin("Components", &componentsOpen) && hasSelectedEntity) {
    Entity entity = {selectedEntity, &*m_App.GetScene()};
    TagComponent& tag = entity.GetComponent<TagComponent>();
    if (ImGui::InputText("##", &tag.Tag, ImGuiInputTextFlags_AutoSelectAll)) {
      bool hasSpace = false;
      for (int i = 0; i < tag.Tag.size(); i++) {
        const char& c = tag.Tag[i];
        if (c == ' ') {
          hasSpace = true;
        }
        break;
      }
      if (hasSpace) {
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
      /*if (ImGui::BeginMenu("Sub-menu")) {
					ImGui::MenuItem("Click me");
					ImGui::EndMenu();
				}*/
      //	ImGui::Separator();

      ImGui::EndPopup();
    }

    GENERATE_COMPONENT_EDITORS(entity);
  }
  ImGui::End();
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
