
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include <cxxopts.hpp>
#include "asset/w_asset_manager.hpp"
#include "imgui_internal.h"
#include "input/w_input.hpp"
#include "layer/w_layerimgui.hpp"
#include "scene/w_componentutil.hpp"
#include "script/mono/w_monobehavior.hpp"
#include "systems/w_canvas_system.hpp"
#include "util/w_keycodes.hpp"
#include "util/w_math.hpp"
#include "w_cargame.hpp"
#include "w_editor.hpp"
#include "w_engine.hpp"
#include "w_entrypoint.hpp"

#include <random>

#include "layer/w_layerscene.hpp"
#include "rendering/w_render_feature.hpp"
#include "rendering/features/w_shadow_feature.hpp"
#include "rendering/features/w_geometry_feature.hpp"
#include "rendering/features/w_ssao_feature.hpp"
#include "rendering/features/w_lighting_feature.hpp"
#include "rendering/features/w_sprite_feature.hpp"
#include "rendering/features/w_composite_feature.hpp"
#include "rendering/features/w_toon_feature.hpp"

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

  Entity camera_entity = scene_->CreateEntity("Camera");
  {
    auto& transform = camera_entity.GetComponent<TransformComponent>();
    auto& camera = camera_entity.AddComponent<CameraComponent>();
    camera.viewport_size = {2560, 1440};
    camera.far_plane = 1000.0f;
    transform.position = glm::vec3(0.0f, 1.0f, 0.0f);
    Engine::renderer()->SetupCameraComponent(camera);
  }
  {
    Entity entity = scene_->CreateEntity("City");
    auto& transform = entity.GetComponent<TransformComponent>();
    transform.scale = {1, 1, 1};
    transform.position = {0.0f, 0.0f, 0.0f};
    auto& model = entity.AddComponent<ModelComponent>();
    model.model_handle = assets.Register("City", AssetType::Model, "/app/models/city/gmae.obj");
  }
  {
    Entity car_entity = scene_->CreateEntity("Car");
    auto& transform = car_entity.GetComponent<TransformComponent>();
    transform.scale = {0.06, 0.06, 0.06};
    transform.position = {0.0f, 0.0f, 0.0f};
    transform.rotation = {0.0f, 180.0f, 0.0f};
    auto& model = car_entity.AddComponent<ModelComponent>();
    model.model_handle = assets.Register("Mercedes AMG GT3", AssetType::Model, "/app/models/car/Mercedes_AMG_GT3.obj");
    auto& behaviors = car_entity.AddComponent<BehaviorsComponent>();
    MonoBehavior& car_script = behaviors.AddBehavior<MonoBehavior>(car_entity, "CarScript");
    car_script.AttachExternComponent<TransformComponent>("CameraTransform", camera_entity.handle());
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
    SpriteBuilder builder{"/app/textures/speedometer_320.png", {320, 298}};
    builder.SetSampler(Engine::renderer()->GetDefaultLinearSampler());
    builder.AddFrame(0, {0,0}, {320, 298});
    sprite.asset_handle_ = builder.Build();
    assets.Register("Speedometer Sprite", AssetType::Sprite, "/app/textures/speedometer_320.png");
  }
  {
    auto skybox_texture = Engine::renderer()->CreateCubemapTexture({
        "/app/textures/skymap/right.jpg",
        "/app/textures/skymap/left.jpg",
        "/app/textures/skymap/top.jpg",
        "/app/textures/skymap/bottom.jpg",
        "/app/textures/skymap/front.jpg",
        "/app/textures/skymap/back.jpg"
    }, {}, {});
    assets.RegisterAndStore<Texture>("Skybox Cubemap", AssetType::Skybox,
                                     "/app/textures/skymap/", skybox_texture);
    scene_->SetSkybox(std::make_shared<Skybox>(skybox_texture));
  }

  renderer_->options().vsync = false;
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
}

bool GameLayer::OnKeyPress(KeyPressedEvent& event) {
  if (event.GetKeyCode() == KeyF1) {
    app_.Close();
    return true;
  }
  return false;
}

bool GameLayer::OnKeyReleased(KeyReleasedEvent& event) {
  return false;
}

bool GameLayer::OnMouseMoved(MouseMovedEvent& event) {
  return false;
}

void GameApplication::Init() {
  LOG_DEBUG("Init");
  std::shared_ptr<Scene> scene = std::make_shared<Scene>();
  if (enable_editor_) {
    PushLayer(std::make_shared<ImGuiLayer>());
    PushLayer(std::make_shared<GameLayer>(*this, scene));
    PushLayer(std::make_shared<EditorLayer>(*this, scene));
  } else {
    PushLayer(std::make_shared<GameLayer>(*this, scene));
    PushLayer(std::make_shared<SceneLayer>(scene));
  }
}

GameApplication::GameApplication(bool enable_editor) : Application({"Wiesel Demo"}, {}), enable_editor_(enable_editor) {
}

GameApplication::~GameApplication() {
}
}  // namespace WieselDemo

// Called from entrypoint
Application* Wiesel::CreateApp() {
  bool enable_editor = Engine::properties().editor_enabled;
  return new LeapLand::GameApplication(enable_editor);
}
