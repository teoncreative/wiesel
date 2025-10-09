//
// Created by Metehan Gezer on 18/04/2025.
//

#ifndef WIESEL_PARENT_W_EDITOR_H
#define WIESEL_PARENT_W_EDITOR_H

#include "behavior/w_behavior.hpp"
#include "scene/w_scene.hpp"
#include "w_application.hpp"

namespace Wiesel::Editor {

class EditorLayer : public Layer {
 public:
  explicit EditorLayer(Application& app, Ref<Scene> scene);
  ~EditorLayer() override;

  void OnAttach() override;
  void OnDetach() override;
  void OnUpdate(float_t delta_time) override;
  void OnEvent(Event& event) override;

  void RenderEntity(Entity& entity, entt::entity entity_id, int depth, bool& ignore_menu);
  void UpdateHierarchyOrder();

  void OnBeginPresent() override;
  void OnPostPresent() override;
  void OnPrePresent() override;

 private:
  Application& app_;
  Ref<Scene> scene_;
};
}

#endif  //WIESEL_PARENT_W_EDITOR_H
