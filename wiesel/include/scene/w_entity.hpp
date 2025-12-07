
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

#include "scene/w_scene.hpp"
#include "w_pch.hpp"

namespace Wiesel {
class Entity {
 public:
  Entity(entt::entity handle, Scene* scene);
  ~Entity();

  template <typename T, typename... Args>
  T& AddComponent(Args&&... args) {
    return scene_->AddComponent<T>(entity_handle_, std::forward<Args>(args)...);
  }

  template <typename T>
  T& GetComponent() {  // This function is intentionally not marked as const!
    return scene_->GetComponent<T>(entity_handle_);
  }

  template <typename T>
  bool HasComponent() const {
    return scene_->HasComponent<T>(entity_handle_);
  }

  template <typename T>
  void RemoveComponent() {
    scene_->RemoveComponent<T>(entity_handle_);
  }

  operator bool() const { return entity_handle_ != entt::null; }

  operator entt::entity() const { return entity_handle_; }

  operator uint32_t() const { return (uint32_t)entity_handle_; }

  UUID GetUUID() { return GetComponent<IdComponent>().Id; }

  const std::string& GetName() { return GetComponent<TagComponent>().tag; }

  Entity GetParent() const { return {parent_handle_, scene_}; }
  entt::entity parent_handle() const { return parent_handle_; }
  const std::vector<entt::entity>* child_handles() const { return child_handles_; }

  bool operator==(const Entity& other) const {
    return entity_handle_ == other.entity_handle_ && scene_ == other.scene_;
  }

  bool operator!=(const Entity& other) const { return !(*this == other); }

  entt::entity handle() const { return entity_handle_; }

  Scene* GetScene() const { return scene_; }


 private:
  entt::entity entity_handle_;
  Scene* scene_;
  entt::entity parent_handle_;
  std::vector<entt::entity>* child_handles_;
};

}  // namespace Wiesel