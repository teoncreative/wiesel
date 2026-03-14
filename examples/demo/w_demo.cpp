
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
#include "layer/w_layerconsole.hpp"
#include "layer/w_layerscene.hpp"

using namespace Wiesel;
using namespace Wiesel::Editor;

namespace LeapLand {

GameLayer::GameLayer(GameApplication& app, std::shared_ptr<Scene> scene) : app_(app), scene_(scene), Layer("Demo Layer") {
  renderer_ = Engine::renderer();
}

GameLayer::~GameLayer() = default;

void GameLayer::OnAttach() {
  LOG_DEBUG("OnAttach");
  auto& assets = Engine::asset_manager();

  {
    Entity entity = scene_->CreateEntity("Sponza");
    auto& transform = entity.GetComponent<TransformComponent>();
    transform.scale = {1.0f, 1.0f, 1.0f};
    transform.position = {5.0f, 2.0f, 0.0f};
    auto& model = entity.AddComponent<ModelComponent>();
    model.model_handle = assets.Register("Sponza", AssetType::Model, "/app/models/sponza/sponza.gltf");
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
    auto entity = scene_->CreateEntity("Light Red");
    auto& transform = entity.GetComponent<TransformComponent>();
    transform.position = {2.0f, 2.5f, 0.4f};
    auto& light = entity.AddComponent<LightPointComponent>();
    light.light_data.base.color = glm::vec3(1.0f, 0.0f, 0.0f);
    light.light_data.base.ambient = 0.05f;
    light.light_data.base.diffuse = 1.2f;
    light.light_data.base.specular = 0.8f;
    light.light_data.base.density = 1.5f;
    light.light_data.constant = 1.0f;
    light.light_data.linear = 0.045f;
    light.light_data.exp = 0.016f;
    auto& red_behaviors = entity.AddComponent<BehaviorsComponent>();
    red_behaviors.AddBehavior<MonoBehavior>(entity, "LightBob");
  }
  {
    auto entity = scene_->CreateEntity("Light Green");
    auto& transform = entity.GetComponent<TransformComponent>();
    transform.position = {5.0f, 2.5f, 0.4f};
    auto& light = entity.AddComponent<LightPointComponent>();
    light.light_data.base.color = glm::vec3(0.0f, 1.0f, 0.0f);
    light.light_data.base.ambient = 0.05f;
    light.light_data.base.diffuse = 1.2f;
    light.light_data.base.specular = 0.8f;
    light.light_data.base.density = 1.5f;
    light.light_data.constant = 1.0f;
    light.light_data.linear = 0.045f;
    light.light_data.exp = 0.016f;
    auto& green_behaviors = entity.AddComponent<BehaviorsComponent>();
    green_behaviors.AddBehavior<MonoBehavior>(entity, "LightBob");
  }
  {
    auto entity = scene_->CreateEntity("Light Blue");
    auto& transform = entity.GetComponent<TransformComponent>();
    transform.position = {8.0f, 2.5f, 0.4f};
    auto& light = entity.AddComponent<LightPointComponent>();
    light.light_data.base.color = glm::vec3(0.0f, 0.0f, 1.0f);
    light.light_data.base.ambient = 0.05f;
    light.light_data.base.diffuse = 1.2f;
    light.light_data.base.specular = 0.8f;
    light.light_data.base.density = 1.5f;
    light.light_data.constant = 1.0f;
    light.light_data.linear = 0.045f;
    light.light_data.exp = 0.016f;
    auto& blue_behaviors = entity.AddComponent<BehaviorsComponent>();
    blue_behaviors.AddBehavior<MonoBehavior>(entity, "LightBob");
  }
  {
    auto entity = scene_->CreateEntity("Camera");
    auto& camera = entity.AddComponent<CameraComponent>();
    camera.viewport_size = {2560, 1440};
    auto& transform = entity.GetComponent<TransformComponent>();
    transform.position = glm::vec3(0.0f, 1.0f, 0.0f);
    Engine::renderer()->SetupCameraComponent(camera);
    auto& behaviors = entity.AddComponent<BehaviorsComponent>();
    behaviors.AddBehavior<MonoBehavior>(entity, "CameraScript");
  }
  {
    auto skyboxTexture = Engine::renderer()->CreateCubemapTextureFromSingle("/app/textures/cubemap/Cubemap_Sky_03-512x512.png", {}, {});
    assets.RegisterAndStore<Texture>("Skybox Cubemap", AssetType::Skybox,
                                     "/app/textures/skymap/", skyboxTexture);
    scene_->SetSkybox(CreateReference<Skybox>(skyboxTexture));
  }
  renderer_->options().vsync = false;
  renderer_->options().bloom_enabled = true;
  renderer_->options().bloom_threshold = 0.5;
  renderer_->options().bloom_intensity = 0.8;
  renderer_->options().motion_blur_enabled = false;
  renderer_->options().motion_blur_strength = 1;
  renderer_->options().aa_mode = AntiAliasingMode::FXAA;
  renderer_->options().msaa_mode = SamplingMode::DISABLED;
}

void GameLayer::OnDetach() {
  LOG_DEBUG("OnDetach");
}

void GameLayer::OnUpdate(float_t deltaTime) {
  //LOG_INFO("OnUpdate {}", deltaTime);
}

void GameLayer::OnEvent(Event& event) {
  EventDispatcher dispatcher(event);

  dispatcher.Dispatch<KeyPressedEvent>(WIESEL_BIND_FN(OnKeyPress));
  dispatcher.Dispatch<KeyReleasedEvent>(WIESEL_BIND_FN(OnKeyReleased));
  dispatcher.Dispatch<MouseMovedEvent>(WIESEL_BIND_FN(OnMouseMoved));
  dispatcher.Dispatch<WindowResizeEvent>(WIESEL_BIND_FN(OnResizeEvent));
}

bool GameLayer::OnKeyPress(KeyPressedEvent& event) {
  if (event.GetKeyCode() == KeyF1) {
    app_.Close();
    return true;
  }
  if (event.GetKeyCode() == KeyGraveAccent && !event.IsRepeat()) {
    auto& console = Engine::console();
    console.Toggle();
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

bool GameLayer::OnKeyReleased(KeyReleasedEvent& event) {
  return false;
}

bool GameLayer::OnMouseMoved(MouseMovedEvent& event) {
  return false;
}

bool GameLayer::OnResizeEvent(WindowResizeEvent& event) {
  // Scene::OnWindowResizeEvent handles camera viewport updates
  return false;
}

void GameApplication::Init() {
  LOG_DEBUG("Init");
  std::shared_ptr<Scene> scene = std::make_shared<Scene>();
  if (enable_editor_) {
    PushLayer(std::make_shared<ImGuiLayer>());
    PushLayer(std::make_shared<ConsoleLayer>());
    PushLayer(std::make_shared<GameLayer>(*this, scene));
    PushLayer(std::make_shared<EditorLayer>(*this, scene));
  } else {
    PushLayer(std::make_shared<GameLayer>(*this, scene));
    PushLayer(std::make_shared<SceneLayer>(scene));
    PushLayer(std::make_shared<ImGuiLayer>());
    PushLayer(std::make_shared<ConsoleLayer>());
  }
}

GameApplication::GameApplication(bool enable_editor) : Application({"Wiesel Demo", {1600, 900}, true}, {}), enable_editor_(enable_editor) {
}

GameApplication::~GameApplication() {
}
}  // namespace WieselDemo

// Called from entrypoint
Application* Wiesel::CreateApp() {
  bool enable_editor = Engine::properties().editor_enabled;
  return new LeapLand::GameApplication(enable_editor);
}
