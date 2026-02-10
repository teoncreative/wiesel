
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

DemoLayer::DemoLayer(DemoApplication& app, std::shared_ptr<Scene> scene) : app_(app), scene_(scene), Layer("Demo Layer") {
  renderer_ = Engine::GetRenderer();
}

DemoLayer::~DemoLayer() = default;

void DemoLayer::OnAttach() {
  LOG_DEBUG("OnAttach");
  {
    Entity entity = scene_->CreateEntity("Sponza");
    auto& transform = entity.GetComponent<TransformComponent>();
    transform.scale = {0.01f, 0.01f, 0.01f};
    transform.position = {5.0f, 2.0f, 0.0f};
    auto& model = entity.AddComponent<ModelComponent>();
    Engine::LoadModel(transform, model, "assets/models/sponza/sponza.gltf");
    auto& behaviors = entity.AddComponent<BehaviorsComponent>();
    behaviors.AddBehavior<MonoBehavior>(entity, "TestBehavior");
  }
  {
    auto entity = scene_->CreateEntity("Sun");
    auto& transform = entity.GetComponent<TransformComponent>();
    transform.rotation = glm::vec3{55.0f, 30.0f, 0.0f};
    auto& light = entity.AddComponent<LightDirectComponent>();
    light.light_data.base.color = glm::vec3(1.0f, 0.98f, 0.95f);
    light.light_data.base.ambient = 0.15f;
    light.light_data.base.diffuse = 1.0f;
    light.light_data.base.specular = 0.5f;
    light.light_data.base.density = 1.0f;
  }
  {
    auto entity = scene_->CreateEntity("Light Center");
    auto& transform = entity.GetComponent<TransformComponent>();
    transform.position = {5.0f, 3.5f, 0.0f};
    auto& light = entity.AddComponent<LightPointComponent>();
    light.light_data.base.color = glm::vec3(1.0f, 0.85f, 0.6f);
    light.light_data.base.ambient = 0.05f;
    light.light_data.base.diffuse = 1.2f;
    light.light_data.base.specular = 0.8f;
    light.light_data.base.density = 1.5f;
    light.light_data.constant = 1.0f;
    light.light_data.linear = 0.045f;
    light.light_data.exp = 0.016f;
  }
  {
    auto entity = scene_->CreateEntity("Light Left");
    auto& transform = entity.GetComponent<TransformComponent>();
    transform.position = {5.0f, 2.5f, 3.0f};
    auto& light = entity.AddComponent<LightPointComponent>();
    light.light_data.base.color = glm::vec3(1.0f, 0.9f, 0.7f);
    light.light_data.base.ambient = 0.03f;
    light.light_data.base.diffuse = 1.0f;
    light.light_data.base.specular = 0.6f;
    light.light_data.base.density = 1.0f;
    light.light_data.constant = 1.0f;
    light.light_data.linear = 0.07f;
    light.light_data.exp = 0.025f;
  }
  {
    auto entity = scene_->CreateEntity("Light Right");
    auto& transform = entity.GetComponent<TransformComponent>();
    transform.position = {5.0f, 2.5f, -3.0f};
    auto& light = entity.AddComponent<LightPointComponent>();
    light.light_data.base.color = glm::vec3(1.0f, 0.9f, 0.7f);
    light.light_data.base.ambient = 0.03f;
    light.light_data.base.diffuse = 1.0f;
    light.light_data.base.specular = 0.6f;
    light.light_data.base.density = 1.0f;
    light.light_data.constant = 1.0f;
    light.light_data.linear = 0.07f;
    light.light_data.exp = 0.025f;
  }
  {
    auto entity = scene_->CreateEntity("Camera");
    auto& camera = entity.AddComponent<CameraComponent>();
    camera.viewport_size = {1280, 720};
    auto& transform = entity.GetComponent<TransformComponent>();
    transform.position = glm::vec3(0.0f, 1.0f, 0.0f);
    Engine::GetRenderer()->SetupCameraComponent(camera);
    auto& behaviors = entity.AddComponent<BehaviorsComponent>();
    behaviors.AddBehavior<MonoBehavior>(entity, "CameraScript");
  }
  renderer_->render_settings().vsync = false;
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
}

bool DemoLayer::OnKeyPress(KeyPressedEvent& event) {
  if (event.GetKeyCode() == KeyF1) {
    app_.Close();
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

void DemoApplication::Init() {
  LOG_DEBUG("Init");
  std::shared_ptr<Scene> scene = std::make_shared<Scene>();
  PushLayer(CreateReference<ImGuiLayer>());
  PushLayer(CreateReference<DemoLayer>(*this, scene));
  PushLayer(CreateReference<EditorLayer>(*this, scene));
}

DemoApplication::DemoApplication() : Application({"Wiesel Demo"}, {}) {
  LOG_DEBUG("DemoApp constructor");
}

DemoApplication::~DemoApplication() {
  LOG_DEBUG("DemoApp destructor");
}
}  // namespace WieselDemo

// Called from entrypoint
Application* Wiesel::CreateApp(int argc, char** argv) {
  return new WieselDemo::DemoApplication();
}
