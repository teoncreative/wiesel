
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
};

}  // namespace WieselDemo