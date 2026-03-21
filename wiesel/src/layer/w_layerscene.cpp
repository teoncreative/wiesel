//
// Created by Metehan Gezer on 09/10/2025.
//

#include "layer/w_layerscene.hpp"

#include "scene/w_scene_manager.hpp"
#include "w_engine.hpp"

namespace Wiesel {

SceneLayer::SceneLayer() : Layer("Scene Layer") {}

SceneLayer::~SceneLayer() {}

void SceneLayer::OnAttach() {}

void SceneLayer::OnDetach() {}

void SceneLayer::OnUpdate(float_t delta_time) {
  auto& sm = Engine::scene_manager();
  sm.BeginFrame();
  auto scene = sm.GetActiveScene();
  if (scene) {
    scene->OnUpdate(delta_time);
  }
}

void SceneLayer::OnEvent(Event& event) {
  auto scene = Engine::scene_manager().GetActiveScene();
  if (scene) {
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
  auto scene = Engine::scene_manager().GetActiveScene();
  if (scene) {
    scene->Render();
  }
}

}  // namespace Wiesel