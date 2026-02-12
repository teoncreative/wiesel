
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
#include "asset/w_asset_manager.hpp"
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

#include "cxxopts.hpp"
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
  auto& assets = AssetManager::Get();

  {
    Entity entity = scene_->CreateEntity("Sponza");
    auto& transform = entity.GetComponent<TransformComponent>();
    transform.scale = {0.01f, 0.01f, 0.01f};
    transform.position = {5.0f, 2.0f, 0.0f};
    auto& model = entity.AddComponent<ModelComponent>();
    model.model_handle = assets.Register("Sponza", AssetType::Model, "assets/models/sponza/sponza.gltf");
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
    camera.viewport_size = {2560, 1440};
    auto& transform = entity.GetComponent<TransformComponent>();
    transform.position = glm::vec3(0.0f, 1.0f, 0.0f);
    Engine::GetRenderer()->SetupCameraComponent(camera);
    auto& behaviors = entity.AddComponent<BehaviorsComponent>();
    behaviors.AddBehavior<MonoBehavior>(entity, "CameraScript");
  }
  {
    auto skyboxTexture = Engine::GetRenderer()->CreateCubemapTextureFromSingle("assets/textures/cubemap/Cubemap_Sky_03-512x512.png", {}, {});
    assets.RegisterAndStore<Texture>("Skybox Cubemap", AssetType::Skybox,
                                     "assets/textures/skymap/", skyboxTexture);
    scene_->SetSkybox(CreateReference<Skybox>(skyboxTexture));
  }
  renderer_->options().vsync = false;
  renderer_->options().bloom_enabled = true;
  renderer_->options().bloom_threshold = 0.5;
  renderer_->options().bloom_intensity = 0.8;
  renderer_->options().motion_blur_enabled = true;
  renderer_->options().motion_blur_strength = 1;
  renderer_->options().aa_mode = AntiAliasingMode::TAA;
  renderer_->options().msaa_mode = SamplingMode::DISABLED;
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
  dispatcher.Dispatch<WindowResizeEvent>(WIESEL_BIND_FN(OnResizeEvent));
}

bool DemoLayer::OnKeyPress(KeyPressedEvent& event) {
  if (event.GetKeyCode() == KeyF1) {
    app_.Close();
    return true;
  }
  if (event.GetKeyCode() == Key1) {
    renderer_->options().msaa_mode = SamplingMode::DISABLED;
  }
  if (event.GetKeyCode() == Key2) {
    renderer_->options().msaa_mode = SamplingMode::X2;
  }
  if (event.GetKeyCode() == Key3) {
    renderer_->options().msaa_mode = SamplingMode::X4;
  }
  if (event.GetKeyCode() == Key4) {
    renderer_->options().msaa_mode = SamplingMode::X8;
  }
  return false;
}

bool DemoLayer::OnKeyReleased(KeyReleasedEvent& event) {
  return false;
}

bool DemoLayer::OnMouseMoved(MouseMovedEvent& event) {
  return false;
}

bool DemoLayer::OnResizeEvent(WindowResizeEvent& event) {
  if (app_.IsEditorEnabled()) {
    return false;
  }
  for (entt::entity entity : scene_->GetAllEntitiesWith<CameraComponent>()) {
    CameraComponent& camera = scene_->GetComponent<CameraComponent>(entity);
    camera.viewport_size.x = event.window_size().width;
    camera.viewport_size.y = event.window_size().height;
    camera.aspect_ratio = event.aspect_ratio();
    camera.view_changed = true;
  }
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
