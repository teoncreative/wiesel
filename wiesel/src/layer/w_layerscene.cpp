//
// Created by Metehan Gezer on 09/10/2025.
//

#include "layer/w_layerscene.hpp"

namespace Wiesel {

SceneLayer::SceneLayer(std::shared_ptr<Scene> scene) : Layer("Scene Layer"), scene_(scene) {
}

SceneLayer::~SceneLayer() {}

void SceneLayer::OnAttach() {

}

void SceneLayer::OnDetach() {
}

void SceneLayer::OnUpdate(float_t delta_time) {
  scene_->OnUpdate(delta_time);
}

void SceneLayer::OnEvent(Event& event) {
  scene_->OnEvent(event);
}

void SceneLayer::OnBeginPresent() {
}

void SceneLayer::OnPostPresent() {
  scene_->ProcessDestroyQueue();
}

void SceneLayer::OnPrePresent() {
  scene_->Render();
}


}  // namespace Wiesel