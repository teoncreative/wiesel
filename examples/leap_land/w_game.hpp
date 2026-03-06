
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "behavior/w_behavior.hpp"
#include "events/w_keyevents.hpp"
#include "events/w_mouseevents.hpp"
#include "scene/w_entity.hpp"
#include "scene/w_scene.hpp"
#include "w_application.hpp"
#include "w_pch.hpp"

namespace LeapLand {
class GameApplication : public Wiesel::Application {
public:
  GameApplication(bool enable_editor);
  ~GameApplication() override;

  void Init() override;

  bool IsEditorEnabled() const {
    return enable_editor_;
  }
private:
  bool enable_editor_;
};

class GameLayer : public Wiesel::Layer {
public:
  explicit GameLayer(GameApplication& app, std::shared_ptr<Wiesel::Scene> scene);
  ~GameLayer() override;

  void OnAttach() override;
  void OnDetach() override;
  void OnUpdate(float_t deltaTime) override;
  void OnEvent(Wiesel::Event& event) override;

  bool OnKeyPress(Wiesel::KeyPressedEvent& event);
  bool OnKeyReleased(Wiesel::KeyReleasedEvent& event);
  bool OnMouseMoved(Wiesel::MouseMovedEvent& event);
  bool OnResizeEvent(Wiesel::WindowResizeEvent& event);

private:
  GameApplication& app_;
  Wiesel::Ref<Wiesel::Scene> scene_;
  Wiesel::Ref<Wiesel::Renderer> renderer_;

  // Game state
  entt::entity player_entity_ = entt::null;
  std::vector<entt::entity> coin_entities_;
  std::vector<entt::entity> hazard_entities_;
  entt::entity hp_fill_entity_ = entt::null;
  entt::entity hp_text_entity_ = entt::null;
  entt::entity level_text_entity_ = entt::null;
  entt::entity coin_text_entity_ = entt::null;
  entt::entity hp_bg_entity_ = entt::null;
  int max_health_ = 100;
  int current_health_ = 100;
  int prev_coins_displayed_ = -1;
  int prev_health_displayed_ = -1;
  float damage_cooldown_ = 0.0f;

  // Death/respawn
  bool is_dead_ = false;
  entt::entity death_overlay_entity_ = entt::null;
  entt::entity death_text_entity_ = entt::null;
  entt::entity death_btn_bg_entity_ = entt::null;
  entt::entity death_btn_text_entity_ = entt::null;

  void SetDeathScreenVisible(bool visible);
  void Respawn();
};

}  // namespace LeapLand