
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
    return m_Scene->AddComponent<T>(m_EntityHandle, std::forward<Args>(args)...);
  }

  template <typename T>
  T& GetComponent() {  // This function is intentionally not marked as const!
    return m_Scene->GetComponent<T>(m_EntityHandle);
  }

  template <typename T>
  bool HasComponent() const {
    return m_Scene->HasComponent<T>(m_EntityHandle);
  }

  template <typename T>
  void RemoveComponent() {
    m_Scene->RemoveComponent<T>(m_EntityHandle);
  }

  operator bool() const { return m_EntityHandle != entt::null; }

  operator entt::entity() const { return m_EntityHandle; }

  operator uint32_t() const { return (uint32_t)m_EntityHandle; }

  UUID GetUUID() { return GetComponent<IdComponent>().Id; }

  const std::string& GetName() { return GetComponent<TagComponent>().Tag; }

  Entity GetParent() const { return {m_Parent, m_Scene}; }
  entt::entity GetParentHandle() const { return m_Parent; }
  const std::vector<entt::entity>* GetChildHandles() const { return m_Childs; }

  bool operator==(const Entity& other) const {
    return m_EntityHandle == other.m_EntityHandle && m_Scene == other.m_Scene;
  }

  bool operator!=(const Entity& other) const { return !(*this == other); }

  entt::entity GetHandle() const { return m_EntityHandle; }

  Scene* GetScene() const { return m_Scene; }


 private:
  entt::entity m_EntityHandle;
  Scene* m_Scene;
  entt::entity m_Parent;
  std::vector<entt::entity>* m_Childs;
};

}  // namespace Wiesel