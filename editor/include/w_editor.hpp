//
// Created by Metehan Gezer on 18/04/2025.
//

#ifndef WIESEL_PARENT_W_EDITOR_H
#define WIESEL_PARENT_W_EDITOR_H

#include "behavior/w_behavior.hpp"
#include "rendering/w_camera.hpp"
#include "scene/w_scene.hpp"
#include "w_application.hpp"

namespace Wiesel::Editor {

enum class EditorState { Edit, Playing };

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
  void TakeSnapshot();
  void RestoreSnapshot();

  Application& app_;
  Ref<Scene> scene_;

  // Play/Stop state
  EditorState editor_state_ = EditorState::Edit;

  // Editor free camera (not in the scene's ECS)
  CameraComponent editor_camera_;
  TransformComponent editor_camera_transform_;
  float editor_yaw_ = 0.0f;
  float editor_pitch_ = 0.0f;
  float camera_speed_ = 10.0f;
  float mouse_sensitivity_ = 0.1f;
  bool cursor_captured_ = false;
  bool game_panel_focused_ = false;
  bool scene_panel_visible_ = true;
  bool game_panel_visible_ = true;
  int resolution_preset_index_ = 0;  // index into kResolutionPresets

  // Scene snapshot for Play/Stop restore
  struct EntitySnapshot {
    glm::vec3 position, rotation, scale;
  };
  std::unordered_map<entt::entity, EntitySnapshot> scene_snapshot_;
};
}

#endif  //WIESEL_PARENT_W_EDITOR_H
