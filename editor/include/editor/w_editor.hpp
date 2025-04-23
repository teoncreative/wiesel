//
// Created by Metehan Gezer on 18/04/2025.
//

#ifndef WIESEL_PARENT_W_EDITOR_H
#define WIESEL_PARENT_W_EDITOR_H

#include "behavior/w_behavior.hpp"
#include "events/w_keyevents.hpp"
#include "events/w_mouseevents.hpp"
#include "scene/w_entity.hpp"
#include "scene/w_scene.hpp"
#include "w_application.hpp"
#include "w_pch.hpp"

namespace Wiesel::Editor {

class EditorOverlay : public Layer {
 public:
  explicit EditorOverlay(Application& app, Ref<Scene> scene);
  ~EditorOverlay() override;

  void OnAttach() override;
  void OnDetach() override;
  void OnUpdate(float_t deltaTime) override;
  void OnEvent(Event& event) override;

  void RenderEntity(Entity& entity, entt::entity entityId, int depth, bool& ignoreMenu);
  void OnImGuiRender() override;
  void UpdateHierarchyOrder();

 private:
  Application& m_App;
  Ref<Scene> m_Scene;
};
}

#endif  //WIESEL_PARENT_W_EDITOR_H
