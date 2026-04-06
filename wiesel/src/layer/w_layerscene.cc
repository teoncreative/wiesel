//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

//
// Created by Metehan Gezer on 09/10/2025.
//

#include "layer/w_layerscene.h"

#include "scene/w_scene_manager.h"
#include "w_engine.h"

namespace Wiesel {

SceneLayer::SceneLayer() : Layer("Scene Layer") {}

SceneLayer::~SceneLayer() {}

void SceneLayer::OnAttach() {}

void SceneLayer::OnDetach() {}

void SceneLayer::OnUpdate(float_t delta_time) {
  auto& sm = Engine::scene_manager();
  sm.BeginFrame();
  for (auto& scene : sm.GetLoadedScenes()) {
    scene->OnUpdate(delta_time);
  }
}

void SceneLayer::OnEvent(Event& event) {
  for (auto& scene : Engine::scene_manager().GetLoadedScenes()) {
    scene->OnEvent(event);
  }
}

void SceneLayer::OnBeginPresent() {}

void SceneLayer::OnPresent() {
  std::shared_ptr<Renderer> renderer = Engine::renderer();
  renderer->DrawFullscreen(renderer->GetPresentPipeline(),
                           {renderer->GetFinalOutputDescriptor()});
}

void SceneLayer::OnPostPresent() {
  Engine::scene_manager().EndFrame();
}

void SceneLayer::OnPrePresent() {
  Engine::scene_manager().RenderGameView();
}

}  // namespace Wiesel