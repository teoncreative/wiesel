
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
#include "w_editor.hpp"
#include "w_engine.hpp"
#include "w_entrypoint.hpp"

#include <random>

using namespace Wiesel;
using namespace Wiesel::Editor;

namespace WieselDemo {

DemoLayer::DemoLayer(DemoApplication& app) : m_App(app), Layer("Demo Layer") {
  scene_ = app.GetScene();
  m_Renderer = Engine::GetRenderer();
}

DemoLayer::~DemoLayer() = default;

void DemoLayer::OnAttach() {
  LOG_DEBUG("OnAttach");
  // Loading a model to the scene
  entt::entity sponzaEntity;
  entt::entity pointLightEntity;
  {
    Entity entity = scene_->CreateEntity("Sponza");
    sponzaEntity = entity.handle();
    auto& transform = entity.GetComponent<TransformComponent>();
    transform.scale = {0.01f, 0.01f, 0.01f};
    transform.position = {5.0f, 2.0f, 0.0f};
    auto& model = entity.AddComponent<ModelComponent>();
    Engine::LoadModel(transform, model, "assets/models/sponza/sponza.gltf");
    auto& behaviors = entity.AddComponent<BehaviorsComponent>();
    behaviors.AddBehavior<MonoBehavior>(entity, "TestBehavior");
  }
  {
    auto entity = scene_->CreateEntity("Directional Light");
    auto& transform = entity.GetComponent<TransformComponent>();
    transform.position = glm::vec3(1.0f, 1.0f, 1.0f);
    entity.AddComponent<LightDirectComponent>();
  }
  {
    auto entity = scene_->CreateEntity("Point Light");
    pointLightEntity = entity.handle();
    auto& transform = entity.GetComponent<TransformComponent>();
    transform.position = glm::vec3{0.0f, 5.0f, 0.0f};
    entity.AddComponent<LightPointComponent>();
  }
  {
    auto entity = scene_->CreateEntity("Camera");
    auto& camera = entity.AddComponent<CameraComponent>();
    auto& transform = entity.GetComponent<TransformComponent>();
    transform.position = glm::vec3(0.0f, 1.0f, 0.0f);
    Engine::GetRenderer()->SetupCameraComponent(camera);
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
}

void DemoLayer::OnEvent(Event& event) {
  EventDispatcher dispatcher(event);

  dispatcher.Dispatch<KeyPressedEvent>(WIESEL_BIND_FN(OnKeyPress));
  dispatcher.Dispatch<KeyReleasedEvent>(WIESEL_BIND_FN(OnKeyReleased));
  dispatcher.Dispatch<MouseMovedEvent>(WIESEL_BIND_FN(OnMouseMoved));
  dispatcher.Dispatch<WindowResizeEvent>(WIESEL_BIND_FN(OnWindowResize));
}

bool DemoLayer::OnKeyPress(KeyPressedEvent& event) {
  if (event.GetKeyCode() == KeyF1) {
    m_App.Close();
    return true;
  }
  return false;
}

bool DemoLayer::OnKeyReleased(KeyReleasedEvent& event) {
  return false;
}

bool DemoLayer::OnMouseMoved(MouseMovedEvent& event) {
  return false;
}

bool DemoLayer::OnWindowResize(Wiesel::WindowResizeEvent& event) {
  m_App.SubmitToMainThread([this]() {
    for (const auto& entity : scene_->GetAllEntitiesWith<CameraComponent>()) {
      CameraComponent& component = scene_->GetComponent<CameraComponent>(entity);
      Engine::GetRenderer()->SetupCameraComponent(component);
    }
  });
  return false;
}

void DemoApplication::Init() {
  LOG_DEBUG("Init");
  PushLayer(CreateReference<DemoLayer>(*this));
  PushOverlay(CreateReference<EditorOverlay>(*this, scene_));
}

DemoApplication::DemoApplication() : Application({"Wiesel Demo"}, {}) {
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
