
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
#include <cxxopts.hpp>

#include <random>

#include "layer/w_layerscene.hpp"

using namespace Wiesel;
using namespace Wiesel::Editor;

namespace WieselDemo {

DemoLayer::DemoLayer(DemoApplication& app, std::shared_ptr<Scene> scene) : app_(app), scene_(scene), Layer("Demo Layer") {
  renderer_ = Engine::GetRenderer();
}

DemoLayer::~DemoLayer() = default;

void DemoLayer::OnAttach() {
  LOG_DEBUG("OnAttach");
  Entity cameraEntity = scene_->CreateEntity("Camera");
  {
    auto& transform = cameraEntity.GetComponent<TransformComponent>();
    auto& camera = cameraEntity.AddComponent<CameraComponent>();
    camera.far_plane = 45.0f;
    transform.position = glm::vec3(0.0f, 1.0f, 0.0f);
    Engine::GetRenderer()->SetupCameraComponent(camera);
    auto& behaviors = cameraEntity.AddComponent<BehaviorsComponent>();
    MonoBehavior& behavior = behaviors.AddBehavior<MonoBehavior>(cameraEntity, "CameraScript");
    //behavior.SetEnabled(false);
  }
  {
    Entity entity = scene_->CreateEntity("City");
    auto& transform = entity.GetComponent<TransformComponent>();
    transform.scale = {1, 1, 1};
    transform.position = {0.0f, 0.0f, 0.0f};
    auto& model = entity.AddComponent<ModelComponent>();
    Engine::LoadModel(transform, model, "assets/models/city/gmae.obj", false);
  }
  {
    Entity entity = scene_->CreateEntity("Car");
    auto& transform = entity.GetComponent<TransformComponent>();
    transform.scale = {0.06, 0.06, 0.06};
    transform.position = {0.0f, 0.0f, 0.0f};
    transform.rotation = {-90.0f, 0.0f, 0.0f};
    auto& model = entity.AddComponent<ModelComponent>();
    Engine::LoadModel(transform, model, "assets/models/bike/cyberpunk_bike.glb", false);
    //auto& behaviors = entity.AddComponent<BehaviorsComponent>();
    //MonoBehavior& behavior = behaviors.AddBehavior<MonoBehavior>(entity, "CarScript");
    //behavior.AttachExternComponent<TransformComponent>("CameraTransform", cameraEntity);
  }
  {
    auto entity = scene_->CreateEntity("Sun");
    auto& transform = entity.GetComponent<TransformComponent>();
    transform.rotation = glm::vec3{63.0f, 30.0f, 0.0f};
    auto& light = entity.AddComponent<LightDirectComponent>();
    light.light_data.base.color = glm::vec3(0.949f, 0.996f, 1.0f);
    light.light_data.base.ambient = 128.0f / 255.0f; // ≈ 0.502
    light.light_data.base.diffuse = 1.0f;
    light.light_data.base.specular = 8.0f / 255.0f; // ≈ 0.031
    light.light_data.base.density = 1.0f;
  }
  {
    auto entity = scene_->CreateEntity("Speedometer");
    auto& transform = entity.GetComponent<TransformComponent>();
    auto& sprite = entity.AddComponent<SpriteComponent>();
    SpriteBuilder builder{"assets/textures/speedometer_320.png", {320, 298}};
    builder.SetSampler(Engine::GetRenderer()->GetDefaultLinearSampler());
    builder.AddFrame(0, {0,0}, {320, 298});
    sprite.asset_handle_ = builder.Build();
  }
  scene_->SetSkybox(CreateReference<Skybox>(
      Engine::GetRenderer()->CreateCubemapTexture({
                                                      "assets/textures/skymap/right.jpg",
                                                      "assets/textures/skymap/left.jpg",
                                                      "assets/textures/skymap/top.jpg",
                                                      "assets/textures/skymap/bottom.jpg",
                                                      "assets/textures/skymap/front.jpg",
                                                      "assets/textures/skymap/back.jpg"
                                                  }, {}, {})
  ));

  renderer_->SetVsync(false);
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

bool DemoLayer::OnWindowResize(WindowResizeEvent& event) {
  app_.SubmitToMainThread([this]() {
    for (const auto& entity : scene_->GetAllEntitiesWith<CameraComponent>()) {
      CameraComponent& component = scene_->GetComponent<CameraComponent>(entity);
      Engine::GetRenderer()->SetupCameraComponent(component);
    }
  });
  return false;
}

void DemoApplication::Init() {
  LOG_DEBUG("Init");
  std::shared_ptr<Scene> scene = std::make_shared<Scene>();
  if (enable_editor_) {
    PushLayer(std::make_shared<ImGuiLayer>());
    PushLayer(std::make_shared<DemoLayer>(*this, scene));
    PushLayer(std::make_shared<EditorLayer>(*this, scene));
  } else {
    PushLayer(std::make_shared<DemoLayer>(*this, scene));
    PushLayer(std::make_shared<SceneLayer>(scene));
  }
}

DemoApplication::DemoApplication(bool enable_editor) : Application({"Wiesel Demo"}, {}), enable_editor_(enable_editor) {
}

DemoApplication::~DemoApplication() {
}
}  // namespace WieselDemo

// Called from entrypoint
Application* Wiesel::CreateApp(int argc, char** argv) {
  cxxopts::Options options("demo", "Wiesel demo application");

  options.add_options()
      ("e,enable_editor", "Enable the editor", cxxopts::value<bool>()->default_value("false"));

  auto result = options.parse(argc, argv);
  bool enable_editor = result["enable_editor"].as<bool>();

  return new WieselDemo::DemoApplication(enable_editor);
}
