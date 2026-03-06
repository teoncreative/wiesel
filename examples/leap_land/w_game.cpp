
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
#include "rendering/features/w_canvas_feature.hpp"
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
  player_entity_ = player_entity;
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
    coin_entities_.push_back(coin);
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

  // Hazards (wood spikes and bombs on platforms)
  {
    // Spikes on Platform 3 (static colliders so player bounces off)
    Entity spikes1 = CreateModel(scene_, "Spikes 1", "wood_spikes.fbx",
                                  {15.0f, 1.0f, 1.0f}, palette,
                                  {0.6f, 0.6f, 0.6f});
    {
      auto& col = spikes1.AddComponent<BoxColliderComponent>();
      col.half_extents = {0.3f, 0.4f, 0.3f};
      col.offset = {0.0f, 0.4f, 0.0f};
    }
    hazard_entities_.push_back(spikes1);

    Entity spikes2 = CreateModel(scene_, "Spikes 2", "wood_spikes.fbx",
                                  {17.0f, 1.0f, -0.5f}, palette,
                                  {0.6f, 0.6f, 0.6f},
                                  {0.0f, 45.0f, 0.0f});
    {
      auto& col = spikes2.AddComponent<BoxColliderComponent>();
      col.half_extents = {0.3f, 0.4f, 0.3f};
      col.offset = {0.0f, 0.4f, 0.0f};
    }
    hazard_entities_.push_back(spikes2);

    // Bomb near Platform 2
    Entity bomb = CreateModel(scene_, "Bomb", "bomb.fbx",
                               {10.0f, 2.0f, -0.5f}, palette,
                               {0.5f, 0.5f, 0.5f});
    hazard_entities_.push_back(bomb);
  }

  // More decorations
  {
    // Grass blades around start platform
    CreateModel(scene_, "Grass 1", "grass_blades_1.fbx",
                {-1.5f, 0.0f, 2.5f}, palette, {0.5f, 0.5f, 0.5f});
    CreateModel(scene_, "Grass 2", "grass_blades_2.fbx",
                {3.0f, 0.0f, 1.5f}, palette, {0.5f, 0.5f, 0.5f});
    CreateModel(scene_, "Grass 3", "grass_blades_3.fbx",
                {-2.5f, 0.0f, -2.0f}, palette, {0.5f, 0.5f, 0.5f});
    CreateModel(scene_, "Grass 4", "grass_blades_4.fbx",
                {0.5f, 0.0f, 3.0f}, palette, {0.5f, 0.5f, 0.5f});

    // Plants on start platform
    CreateModel(scene_, "Plant 1", "plant.fbx",
                {-3.5f, 0.0f, -1.0f}, palette, {0.5f, 0.5f, 0.5f});
    CreateModel(scene_, "Plant 2", "plant.fbx",
                {3.5f, 0.0f, 2.5f}, palette, {0.5f, 0.5f, 0.5f});

    // Flower on Platform 3
    CreateModel(scene_, "Flower 2", "flower.fbx",
                {14.5f, 1.0f, -1.5f}, palette, {0.5f, 0.5f, 0.5f});
    CreateModel(scene_, "Flower 3", "flower.fbx",
                {17.5f, 1.0f, 1.0f}, palette, {0.4f, 0.4f, 0.4f});

    // Crate on Platform 1 (platform top is Y=1.0)
    CreateModel(scene_, "Crate", "crate.fbx",
                {5.5f, 1.0f, 0.0f}, palette, {0.4f, 0.4f, 0.4f});

    // Fence along start platform edge
    CreateModel(scene_, "Fence 1", "fence_2.fbx",
                {0.0f, 0.0f, 3.8f}, palette, {0.8f, 0.8f, 0.8f});
    CreateModel(scene_, "Fence 2", "fence_1.fbx",
                {-3.0f, 0.0f, 3.8f}, palette, {0.8f, 0.8f, 0.8f});

    // Rocks on platforms
    CreateModel(scene_, "Rock 2", "rock_2.fbx",
                {16.5f, 1.0f, 1.5f}, palette, {0.6f, 0.6f, 0.6f});
    CreateModel(scene_, "Rock 3", "rock_3.fbx",
                {-3.0f, 0.0f, 2.5f}, palette, {0.5f, 0.5f, 0.5f});

    // Tree stump on Platform 3
    CreateModel(scene_, "Stump", "tree_stump.fbx",
                {15.5f, 1.0f, -1.8f}, palette, {0.7f, 0.7f, 0.7f});

    // Treasure chest near the flag (goal reward)
    CreateModel(scene_, "Treasure", "treasure_chest.fbx",
                {22.5f, 3.0f, 0.5f}, palette, {0.6f, 0.6f, 0.6f});

    // Balloons near goal
    CreateModel(scene_, "Balloon Red", "balloon_red.fbx",
                {21.0f, 5.0f, 0.5f}, palette, {0.5f, 0.5f, 0.5f});
    CreateModel(scene_, "Balloon Blue", "balloon_blue.fbx",
                {23.0f, 5.5f, -0.5f}, palette, {0.5f, 0.5f, 0.5f});

    // Gem near Platform 2 (decorative, platform top is Y=2.0)
    CreateModel(scene_, "Gem", "gem.fbx",
                {10.0f, 2.0f, 0.5f}, palette, {0.4f, 0.4f, 0.4f});
  }

  // HUD Canvas
  {
    // Root canvas entity (top-left column layout with padding)
    Entity hud = scene_->CreateEntity("HUD Canvas");
    auto& canvas = hud.AddComponent<CanvasComponent>();
    canvas.direction = LayoutDirection::Column;
    canvas.alignment = ChildAlignment::Start;
    canvas.spacing = 8.0f;
    auto& hud_rt = hud.AddComponent<RectangleTransformComponent>();
    hud_rt.anchor = AnchorPreset::TopLeft;
    hud_rt.position = {0, 0};
    hud_rt.size = {250, 200};
    hud_rt.padding = {16, 16, 16, 16};

    // HP bar background (dark red)
    Entity hp_bg = scene_->CreateEntity("HP Bar BG");
    auto& hp_bg_rect = hp_bg.AddComponent<CanvasRectComponent>();
    hp_bg_rect.color = {0.3f, 0.05f, 0.05f, 0.8f};
    auto& hp_bg_rt = hp_bg.AddComponent<RectangleTransformComponent>();
    hp_bg_rt.size = {200, 20};
    scene_->LinkEntities(hud, hp_bg);
    hp_bg_entity_ = hp_bg;

    // HP bar fill (green) - nested inside HP BG
    Entity hp_fill = scene_->CreateEntity("HP Bar Fill");
    auto& hp_fill_rect = hp_fill.AddComponent<CanvasRectComponent>();
    hp_fill_rect.color = {0.2f, 0.8f, 0.2f, 1.0f};
    auto& hp_fill_rt = hp_fill.AddComponent<RectangleTransformComponent>();
    hp_fill_rt.size = {1.0f, 1.0f};
    hp_fill_rt.size_mode_x = SizeMode::Percent;
    hp_fill_rt.size_mode_y = SizeMode::Percent;
    hp_fill_rt.anchor = AnchorPreset::TopLeft;
    scene_->LinkEntities(hp_bg, hp_fill);
    hp_fill_entity_ = hp_fill;

    // HP label
    Entity hp_label = scene_->CreateEntity("HP Label");
    auto& hp_text = hp_label.AddComponent<TextComponent>();
    hp_text.text = "HP: 100 / 100";
    hp_text.font_size = 28.0f;
    hp_text.color = {1.0f, 1.0f, 1.0f, 1.0f};
    auto& hp_label_rt = hp_label.AddComponent<RectangleTransformComponent>();
    hp_label_rt.size = {250, 36};
    scene_->LinkEntities(hud, hp_label);
    hp_text_entity_ = hp_label;

    // Level indicator
    Entity level_label = scene_->CreateEntity("Level Label");
    auto& level_text = level_label.AddComponent<TextComponent>();
    level_text.text = "Level 1";
    level_text.font_size = 32.0f;
    level_text.color = {1.0f, 0.85f, 0.3f, 1.0f};
    auto& level_rt = level_label.AddComponent<RectangleTransformComponent>();
    level_rt.size = {250, 40};
    scene_->LinkEntities(hud, level_label);
    level_text_entity_ = level_label;

    // Coin counter
    Entity coin_label = scene_->CreateEntity("Coin Counter");
    auto& coin_text = coin_label.AddComponent<TextComponent>();
    coin_text.text = "Coins: 0 / 4";
    coin_text.font_size = 28.0f;
    coin_text.color = {1.0f, 0.9f, 0.1f, 1.0f};
    auto& coin_rt = coin_label.AddComponent<RectangleTransformComponent>();
    coin_rt.size = {250, 36};
    scene_->LinkEntities(hud, coin_label);
    coin_text_entity_ = coin_label;
  }

  // Death Screen (hidden initially)
  {
    Entity death_canvas = scene_->CreateEntity("Death Screen");
    auto& dc = death_canvas.AddComponent<CanvasComponent>();
    dc.direction = LayoutDirection::None;
    dc.sort_order = 100;  // draw on top of HUD
    auto& dc_rt = death_canvas.AddComponent<RectangleTransformComponent>();
    dc_rt.anchor = AnchorPreset::TopLeft;
    dc_rt.size = {1.0f, 1.0f};
    dc_rt.size_mode_x = SizeMode::Percent;
    dc_rt.size_mode_y = SizeMode::Percent;

    // Dark overlay (full screen)
    Entity overlay = scene_->CreateEntity("Death Overlay");
    auto& overlay_rect = overlay.AddComponent<CanvasRectComponent>();
    overlay_rect.color = {0.0f, 0.0f, 0.0f, 0.0f};  // invisible initially
    auto& overlay_rt = overlay.AddComponent<RectangleTransformComponent>();
    overlay_rt.anchor = AnchorPreset::TopLeft;
    overlay_rt.size = {1.0f, 1.0f};
    overlay_rt.size_mode_x = SizeMode::Percent;
    overlay_rt.size_mode_y = SizeMode::Percent;
    scene_->LinkEntities(death_canvas, overlay);
    death_overlay_entity_ = overlay;

    // "YOU DIED" text (centered above the button)
    Entity death_text = scene_->CreateEntity("Death Text");
    auto& dt = death_text.AddComponent<TextComponent>();
    dt.text = "YOU DIED";
    dt.font_size = 64.0f;
    dt.color = {0.9f, 0.15f, 0.1f, 0.0f};  // invisible initially
    auto& dt_rt = death_text.AddComponent<RectangleTransformComponent>();
    dt_rt.anchor = AnchorPreset::MiddleCenter;
    dt_rt.pivot = AnchorPreset::BottomCenter;
    dt_rt.position = {0, -15};  // 15px above center
    dt_rt.size = {300, 80};
    scene_->LinkEntities(death_canvas, death_text);
    death_text_entity_ = death_text;

    // Respawn button (rect background + text, centered below "YOU DIED")
    Entity btn_bg = scene_->CreateEntity("Respawn Button");
    auto& btn_rect = btn_bg.AddComponent<CanvasRectComponent>();
    btn_rect.color = {0.0f, 0.0f, 0.0f, 0.0f};  // invisible initially
    auto& btn_rt = btn_bg.AddComponent<RectangleTransformComponent>();
    btn_rt.anchor = AnchorPreset::MiddleCenter;
    btn_rt.pivot = AnchorPreset::TopCenter;
    btn_rt.position = {0, 15};  // 15px below center
    btn_rt.size = {300, 50};
    scene_->LinkEntities(death_canvas, btn_bg);
    death_btn_bg_entity_ = btn_bg;

    Entity btn_text = scene_->CreateEntity("Respawn Text");
    auto& bt = btn_text.AddComponent<TextComponent>();
    bt.text = "RESPAWN [SPACE]";
    bt.font_size = 28.0f;
    bt.color = {1.0f, 1.0f, 1.0f, 0.0f};  // invisible initially
    auto& bt_rt = btn_text.AddComponent<RectangleTransformComponent>();
    bt_rt.anchor = AnchorPreset::MiddleCenter;
    bt_rt.pivot = AnchorPreset::MiddleCenter;
    bt_rt.size = {260, 36};
    scene_->LinkEntities(btn_bg, btn_text);
    death_btn_text_entity_ = btn_text;
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
  pipeline->AddFeature<CanvasFeature>(renderer_);
  scene_->SetRenderPipeline(pipeline);
}

void GameLayer::OnDetach() {
  LOG_DEBUG("OnDetach");
}

void GameLayer::OnUpdate(float_t deltaTime) {
  // Skip all game logic when dead
  if (is_dead_) {
    return;
  }

  // Count collected coins
  int coins = 0;
  for (auto e : coin_entities_) {
    if (scene_->HasComponent<ModelComponent>(e)) {
      auto& model = scene_->GetComponent<ModelComponent>(e);
      if (!model.enable_rendering) {
        coins++;
      }
    }
  }

  if (coins != prev_coins_displayed_) {
    prev_coins_displayed_ = coins;
    auto& text = scene_->GetComponent<TextComponent>(coin_text_entity_);
    text.text = "Coins: " + std::to_string(coins) + " / "
                + std::to_string(static_cast<int>(coin_entities_.size()));
    text.gpu_dirty_ = true;
  }

  // Hazard damage (proximity check) - skip when dead
  damage_cooldown_ -= deltaTime;
  if (!is_dead_ && damage_cooldown_ <= 0.0f
      && scene_->HasComponent<TransformComponent>(player_entity_)) {
    auto& player_pos =
        scene_->GetComponent<TransformComponent>(player_entity_).position;

    for (size_t i = 0; i < hazard_entities_.size(); i++) {
      auto e = hazard_entities_[i];
      if (!scene_->HasComponent<TransformComponent>(e)) continue;
      // Skip already-destroyed hazards (bombs)
      if (scene_->HasComponent<ModelComponent>(e)
          && !scene_->GetComponent<ModelComponent>(e).enable_rendering) {
        continue;
      }
      auto& hazard_pos =
          scene_->GetComponent<TransformComponent>(e).position;
      float dist = glm::distance(
          glm::vec2(player_pos.x, player_pos.z),
          glm::vec2(hazard_pos.x, hazard_pos.z));
      float y_diff = player_pos.y - hazard_pos.y;
      // Close horizontally and not too far above
      if (dist < 1.0f && y_diff < 1.5f && y_diff > -0.5f) {
        current_health_ = std::max(0, current_health_ - 15);
        damage_cooldown_ = 1.0f;  // 1 second invincibility
        // Hide consumable hazards (bombs disappear on hit)
        if (scene_->HasComponent<ModelComponent>(e)) {
          auto& tag = scene_->GetComponent<TagComponent>(e).tag;
          if (tag.find("Bomb") != std::string::npos) {
            scene_->GetComponent<ModelComponent>(e).enable_rendering = false;
          }
        }
        break;
      }
    }

    // Fall damage (respawn resets health partially)
    if (player_pos.y < -10.0f) {
      current_health_ = std::max(0, current_health_ - 25);
      damage_cooldown_ = 1.5f;
    }
  }

  // Update health HUD
  if (current_health_ != prev_health_displayed_) {
    prev_health_displayed_ = current_health_;

    auto& hp_text = scene_->GetComponent<TextComponent>(hp_text_entity_);
    hp_text.text = "HP: " + std::to_string(current_health_) + " / "
                   + std::to_string(max_health_);
    hp_text.gpu_dirty_ = true;

    auto& hp_fill_rt =
        scene_->GetComponent<RectangleTransformComponent>(hp_fill_entity_);
    hp_fill_rt.size.x =
        static_cast<float>(current_health_) / static_cast<float>(max_health_);

    // Change bar color based on health
    auto& hp_fill_rect =
        scene_->GetComponent<CanvasRectComponent>(hp_fill_entity_);
    float ratio = static_cast<float>(current_health_) / max_health_;
    if (ratio > 0.5f) {
      hp_fill_rect.color = {0.2f, 0.8f, 0.2f, 1.0f};  // green
    } else if (ratio > 0.25f) {
      hp_fill_rect.color = {0.9f, 0.7f, 0.1f, 1.0f};  // yellow
    } else {
      hp_fill_rect.color = {0.9f, 0.15f, 0.1f, 1.0f};  // red
    }

    // Trigger death
    if (current_health_ <= 0 && !is_dead_) {
      is_dead_ = true;
      SetDeathScreenVisible(true);
      // Disable player controller and freeze physics
      if (scene_->HasComponent<BehaviorsComponent>(player_entity_)) {
        auto& behaviors =
            scene_->GetComponent<BehaviorsComponent>(player_entity_);
        for (auto& [name, behavior] : behaviors.behaviors_) {
          behavior->SetEnabled(false);
        }
      }
      if (scene_->HasComponent<RigidBodyComponent>(player_entity_)) {
        auto& rb = scene_->GetComponent<RigidBodyComponent>(player_entity_);
        rb.SetLinearVelocity({0.0f, 0.0f, 0.0f});
        rb.SetAngularVelocity({0.0f, 0.0f, 0.0f});
      }
    }
  }
}

void GameLayer::SetDeathScreenVisible(bool visible) {
  float alpha = visible ? 1.0f : 0.0f;

  // Dark overlay (high opacity to hide HUD)
  auto& overlay =
      scene_->GetComponent<CanvasRectComponent>(death_overlay_entity_);
  overlay.color = {0.0f, 0.0f, 0.0f, visible ? 0.85f : 0.0f};

  // "YOU DIED" text
  auto& death_text =
      scene_->GetComponent<TextComponent>(death_text_entity_);
  death_text.color = {0.9f, 0.15f, 0.1f, alpha};
  death_text.gpu_dirty_ = true;

  // Button background
  auto& btn_bg =
      scene_->GetComponent<CanvasRectComponent>(death_btn_bg_entity_);
  btn_bg.color = {0.2f, 0.5f, 0.9f, visible ? 0.9f : 0.0f};

  // Button text
  auto& btn_text =
      scene_->GetComponent<TextComponent>(death_btn_text_entity_);
  btn_text.color = {1.0f, 1.0f, 1.0f, alpha};
  btn_text.gpu_dirty_ = true;

  // Hide all HUD elements when dead
  float hud_alpha = visible ? 0.0f : 1.0f;

  auto& hp_text = scene_->GetComponent<TextComponent>(hp_text_entity_);
  hp_text.color.a = hud_alpha;
  hp_text.gpu_dirty_ = true;

  auto& level_text =
      scene_->GetComponent<TextComponent>(level_text_entity_);
  level_text.color.a = visible ? 0.0f : 1.0f;
  level_text.gpu_dirty_ = true;

  auto& coin_text = scene_->GetComponent<TextComponent>(coin_text_entity_);
  coin_text.color.a = hud_alpha;
  coin_text.gpu_dirty_ = true;

  // Hide HP bar rects
  auto& hp_bg_rect =
      scene_->GetComponent<CanvasRectComponent>(hp_bg_entity_);
  hp_bg_rect.color.a = visible ? 0.0f : 0.8f;

  auto& hp_fill_rect =
      scene_->GetComponent<CanvasRectComponent>(hp_fill_entity_);
  hp_fill_rect.color.a = hud_alpha;
}

void GameLayer::Respawn() {
  is_dead_ = false;
  current_health_ = max_health_;
  prev_health_displayed_ = -1;  // force HUD refresh
  prev_coins_displayed_ = -1;   // force coin HUD refresh
  damage_cooldown_ = 2.0f;      // brief invincibility after respawn

  SetDeathScreenVisible(false);

  // Reset player position and velocity
  if (scene_->HasComponent<TransformComponent>(player_entity_)) {
    auto& t = scene_->GetComponent<TransformComponent>(player_entity_);
    t.position = {0.0f, 1.0f, 0.0f};
    t.is_changed = true;
  }
  if (scene_->HasComponent<RigidBodyComponent>(player_entity_)) {
    auto& rb = scene_->GetComponent<RigidBodyComponent>(player_entity_);
    rb.SetLinearVelocity({0.0f, 0.0f, 0.0f});
    rb.SetAngularVelocity({0.0f, 0.0f, 0.0f});
  }

  // Re-enable player controller
  if (scene_->HasComponent<BehaviorsComponent>(player_entity_)) {
    auto& behaviors =
        scene_->GetComponent<BehaviorsComponent>(player_entity_);
    for (auto& [name, behavior] : behaviors.behaviors_) {
      behavior->SetEnabled(true);
    }
  }

  // Restore all coins (re-enable rendering)
  for (auto e : coin_entities_) {
    if (scene_->HasComponent<ModelComponent>(e)) {
      scene_->GetComponent<ModelComponent>(e).enable_rendering = true;
    }
  }

  // Restore all hazards (re-enable bombs etc.)
  for (auto e : hazard_entities_) {
    if (scene_->HasComponent<ModelComponent>(e)) {
      scene_->GetComponent<ModelComponent>(e).enable_rendering = true;
    }
  }
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
  if (is_dead_ && event.GetKeyCode() == KeySpace) {
    Respawn();
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
  // Scene::OnWindowResizeEvent handles camera viewport updates
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
