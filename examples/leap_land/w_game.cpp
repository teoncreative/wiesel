
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "asset/w_asset_manager.hpp"
#include "imgui_internal.h"
#include "input/w_input.hpp"
#include "physics/w_collider.hpp"
#include "physics/w_rigidbody.hpp"
#include "layer/w_layerimgui.hpp"
#include "scene/w_componentutil.hpp"
#include "script/mono/w_monobehavior.hpp"
#include "systems/w_canvas_system.hpp"
#include "util/w_keycodes.hpp"
#include "util/w_math.hpp"
#include "w_editor.hpp"
#include "w_engine.hpp"
#include "w_entrypoint.hpp"
#include "w_game.hpp"

#include "layer/w_layerscene.hpp"
#include "rendering/features/w_composite_feature.hpp"
#include "rendering/features/w_geometry_feature.hpp"
#include "rendering/features/w_lighting_feature.hpp"
#include "rendering/features/w_shadow_feature.hpp"
#include "rendering/features/w_sprite_feature.hpp"
#include "rendering/features/w_ssao_feature.hpp"
#include "rendering/features/w_toon_feature.hpp"

using namespace Wiesel;
using namespace Wiesel::Editor;

namespace LeapLand {

// Helper to create a model entity from a leap land FBX
static Entity CreateModel(Ref<Scene>& scene, const std::string& name,
                           const std::string& fbx_name,
                           glm::vec3 pos, Ref<Texture> palette = nullptr,
                           glm::vec3 scale = {1, 1, 1},
                           glm::vec3 rot = {0, 0, 0}) {
  auto& assets = AssetManager::Get();
  Entity entity = scene->CreateEntity(name);
  auto& transform = entity.GetComponent<TransformComponent>();
  transform.position = pos;
  transform.scale = scale / 100.0f;
  transform.rotation = rot;
  auto& model = entity.AddComponent<ModelComponent>();
  model.model_handle = assets.Register(
      name, AssetType::Model,
      "/app/models/" + fbx_name);
  if (palette) {
    model.default_texture = palette;
  }
  return entity;
}

GameLayer::GameLayer(GameApplication& app, std::shared_ptr<Scene> scene)
    : app_(app), scene_(scene), Layer("Game Layer") {
  renderer_ = Engine::GetRenderer();
}

GameLayer::~GameLayer() = default;

void GameLayer::OnAttach() {
  LOG_DEBUG("OnAttach");
  auto& assets = AssetManager::Get();

  // Load palette texture shared by all leap land models
  // Use nearest-neighbor sampling since this is a small color palette
  SamplerProps palette_sampler;
  palette_sampler.MagFilter = VK_FILTER_NEAREST;
  palette_sampler.MinFilter = VK_FILTER_NEAREST;
  auto palette = Engine::GetRenderer()->CreateTexture(
      "/app/textures/palette.png", {TextureTypeDiffuse}, palette_sampler);

  // Platforms (with box colliders for ground detection)
  {
    Entity e = CreateModel(scene_, "Start Platform", "ground_grass_8.fbx",
                {0.0f, 0.0f, 0.0f}, palette);
    auto& col = e.AddComponent<BoxColliderComponent>();
    col.half_extents = {4.0f, 0.5f, 4.0f};
    col.offset = {0.0f, -0.5f, 0.0f};
  }
  {
    Entity e = CreateModel(scene_, "Platform 1", "ground_dirt_2.fbx",
                {6.0f, 1.0f, 0.0f}, palette);
    auto& col = e.AddComponent<BoxColliderComponent>();
    col.half_extents = {1.0f, 0.5f, 1.0f};
    col.offset = {0.0f, -0.5f, 0.0f};
  }
  {
    Entity e = CreateModel(scene_, "Platform 2", "ground_dirt_2.fbx",
                {10.0f, 2.0f, 0.0f}, palette);
    auto& col = e.AddComponent<BoxColliderComponent>();
    col.half_extents = {1.0f, 0.5f, 1.0f};
    col.offset = {0.0f, -0.5f, 0.0f};
  }
  {
    Entity e = CreateModel(scene_, "Platform 3", "ground_grass_4.fbx",
                {16.0f, 1.0f, 0.0f}, palette);
    auto& col = e.AddComponent<BoxColliderComponent>();
    col.half_extents = {2.0f, 0.5f, 2.0f};
    col.offset = {0.0f, -0.5f, 0.0f};
  }
  {
    Entity e = CreateModel(scene_, "Platform 4", "ground_dirt_2.fbx",
                {22.0f, 3.0f, 0.0f}, palette);
    auto& col = e.AddComponent<BoxColliderComponent>();
    col.half_extents = {1.0f, 0.5f, 1.0f};
    col.offset = {0.0f, -0.5f, 0.0f};
  }

  // Player (slime) - created first so coins/camera can reference it
  Entity player_entity = CreateModel(scene_, "Player", "slime.fbx",
                                      {0.0f, 1.0f, 0.0f},
                                      palette, {1.0f, 1.0f, 1.0f});
  {
    auto& behaviors = player_entity.AddComponent<BehaviorsComponent>();
    behaviors.AddBehavior<MonoBehavior>(player_entity, "PlayerController");
    auto& collider = player_entity.AddComponent<SphereColliderComponent>();
    collider.offset.y = 0.3f;
    collider.radius = 0.3f;
    auto& rb = player_entity.AddComponent<RigidBodyComponent>();
    rb.type = RigidBodyType::Dynamic;
    rb.mass = 1.0f;
    rb.friction = 0.5f;
    rb.restitution = 0.0f;
    rb.lock_rotation_x = true;
    rb.lock_rotation_y = true;
    rb.lock_rotation_z = true;
  }

  // Coins (with spin + collection script)
  const glm::vec3 coinScale = {0.5f, 0.5f, 0.5f};
  for (auto& [name, pos] : std::vector<std::pair<std::string, glm::vec3>>{
      {"Coin 1", {6.0f, 2.5f, 0.0f}},
      {"Coin 2", {10.0f, 3.5f, 0.0f}},
      {"Coin 3", {16.0f, 2.5f, 0.0f}},
      {"Coin 4", {22.0f, 4.5f, 0.0f}},
  }) {
    Entity coin = CreateModel(scene_, name, "coin.fbx", pos, palette, coinScale);
    auto& behaviors = coin.AddComponent<BehaviorsComponent>();
    behaviors.AddBehavior<MonoBehavior>(coin, "CoinSpin");
    auto& collider = coin.AddComponent<SphereColliderComponent>();
    collider.radius = 1.0f;
    collider.is_trigger = true;
  }

  // Flag at goal
  CreateModel(scene_, "Flag", "flag.fbx", {22.0f, 3.0f, 0.0f}, palette);

  // Decorations on start platform
  CreateModel(scene_, "Tree 1", "tree_1.fbx", {-2.0f, 0.0f, 2.0f}, palette);
  CreateModel(scene_, "Tree 2", "tree_2.fbx", {2.0f, 0.0f, -2.0f}, palette);
  CreateModel(scene_, "Rock 1", "rock_1.fbx", {-1.0f, 0.0f, -1.5f},
              palette, {0.8f, 0.8f, 0.8f});
  CreateModel(scene_, "Mushroom", "mushroom.fbx", {1.5f, 0.0f, 1.0f},
              palette, {0.6f, 0.6f, 0.6f});
  CreateModel(scene_, "Flower", "flower.fbx", {-3.0f, 0.0f, 0.5f},
              palette, {0.5f, 0.5f, 0.5f});

  // Decoration clouds
  CreateModel(scene_, "Cloud 1", "cloud_1.fbx", {8.0f, 8.0f, -5.0f},
              palette, {2.0f, 2.0f, 2.0f});
  CreateModel(scene_, "Cloud 2", "cloud_2.fbx", {18.0f, 10.0f, 4.0f},
              palette, {1.5f, 1.5f, 1.5f});
  CreateModel(scene_, "Cloud 3", "cloud_3.fbx", {-2.0f, 9.0f, 6.0f},
              palette, {1.8f, 1.8f, 1.8f});

  // Sign at start
  CreateModel(scene_, "Sign", "sign_arrow.fbx", {3.0f, 0.0f, 0.0f},
              palette, {0.8f, 0.8f, 0.8f}, {0.0f, -90.0f, 0.0f});

  // Camera
  Entity camera_entity = scene_->CreateEntity("Camera");
  {
    auto& transform = camera_entity.GetComponent<TransformComponent>();
    transform.position = glm::vec3(0.0f, 5.0f, -10.0f);
    auto& camera = camera_entity.AddComponent<CameraComponent>();
    camera.viewport_size = {2560, 1440};
    camera.far_plane = 500.0f;
    Engine::GetRenderer()->SetupCameraComponent(camera);
    auto& behaviors = camera_entity.AddComponent<BehaviorsComponent>();
    MonoBehavior& mono_behavior = behaviors.AddBehavior<MonoBehavior>(camera_entity, "CameraFollow");
    mono_behavior.AttachExternComponent<TransformComponent>("PlayerTransform", player_entity);
  }

  {
    auto entity = scene_->CreateEntity("Sun");
    auto& transform = entity.GetComponent<TransformComponent>();
    transform.rotation = glm::vec3{50.0f, 30.0f, 0.0f};
    auto& light = entity.AddComponent<LightDirectComponent>();
    light.light_data.base.color = glm::vec3(1.0f, 0.98f, 0.92f);
    light.light_data.base.ambient = 0.25f;
    light.light_data.base.diffuse = 1.0f;
    light.light_data.base.specular = 0.3f;
    light.light_data.base.density = 1.0f;
  }

  {
    auto skybox_texture = Engine::GetRenderer()->CreateCubemapTexture({
        "/app/textures/skybox/px.png",
        "/app/textures/skybox/nx.png",
        "/app/textures/skybox/py.png",
        "/app/textures/skybox/ny.png",
        "/app/textures/skybox/pz.png",
        "/app/textures/skybox/nz.png"
    }, {}, {});
    assets.RegisterAndStore<Texture>("Skybox Cubemap", AssetType::Skybox,
                                     "/app/textures/skybox/", skybox_texture);
    scene_->SetSkybox(std::make_shared<Skybox>(skybox_texture));
  }

  renderer_->options().vsync = false;
  renderer_->options().bloom_enabled = false;
  renderer_->options().motion_blur_enabled = false;
  renderer_->options().aa_mode = AntiAliasingMode::FXAA;
  renderer_->options().msaa_mode = SamplingMode::DISABLED;

  auto pipeline = CreateReference<RenderPipeline>(renderer_);
  pipeline->AddFeature<ShadowFeature>(renderer_);
  pipeline->AddFeature<GeometryFeature>(renderer_);
  pipeline->AddFeature<SSAOFeature>(renderer_);
  pipeline->AddFeature<LightingFeature>(renderer_);
  pipeline->AddFeature<SpriteFeature>(renderer_);
  pipeline->AddFeature<CompositeFeature>(renderer_);
  pipeline->AddFeature<ToonFeature>(renderer_);
  scene_->SetRenderPipeline(pipeline);
}

void GameLayer::OnDetach() {
  LOG_DEBUG("OnDetach");
}

void GameLayer::OnUpdate(float_t deltaTime) {
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
  return false;
}

bool GameLayer::OnKeyReleased(KeyReleasedEvent& event) {
  return false;
}

bool GameLayer::OnMouseMoved(MouseMovedEvent& event) {
  return false;
}

bool GameLayer::OnResizeEvent(WindowResizeEvent& event) {
  if (app_.IsEditorEnabled()) {
    return false;
  }
  for (entt::entity entity : scene_->GetAllEntitiesWith<CameraComponent>()) {
    CameraComponent& camera = scene_->GetComponent<CameraComponent>(entity);
    camera.viewport_size.x = event.window_size().width;
    camera.viewport_size.y = event.window_size().height;
    camera.aspect_ratio = event.aspect_ratio();
    camera.view_changed = true;
    camera.resources_dirty = true;
  }
  return false;
}

GameApplication::GameApplication(bool enable_editor)
    : Application({"Leap Land"}, {}), enable_editor_(enable_editor) {
}

GameApplication::~GameApplication() {
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

}  // namespace LeapLand

// Called from entrypoint
Application* Wiesel::CreateApp() {
  bool enable_editor = Engine::GetEngineProperties().editor_enabled;
  return new LeapLand::GameApplication(enable_editor);
}
