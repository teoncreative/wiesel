
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
    if (HasComponent<T>()) {
      // make this a macro
      throw std::runtime_error("Entity already has component!");
    }
    auto& component = m_Scene->m_Registry.emplace<T>(
        m_EntityHandle, std::forward<Args>(args)...);
    m_Scene->OnAddComponent(m_EntityHandle, component);
    return component;
  }

  template <typename T>
  T& GetComponent() {  // This function is intentionally not marked as const!
    return m_Scene->m_Registry.get<T>(m_EntityHandle);
  }

  template <typename T>
  bool HasComponent() const {
    return m_Scene->m_Registry.any_of<T>(m_EntityHandle);
  }

  template <typename T>
  void RemoveComponent() {
    if (!HasComponent<T>()) {
      return;
    }
    auto& component = GetComponent<T>();
    m_Scene->OnRemoveComponent<T>(m_EntityHandle, component);
    m_Scene->m_Registry.remove<T>(m_EntityHandle);
  }

  operator bool() const { return m_EntityHandle != entt::null; }

  operator entt::entity() const { return m_EntityHandle; }

  operator uint32_t() const { return (uint32_t)m_EntityHandle; }

  UUID GetUUID() { return GetComponent<IdComponent>().Id; }

  const std::string& GetName() { return GetComponent<TagComponent>().Tag; }

  Entity GetParent() const { return {m_Parent, m_Scene}; }

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
};

}  // namespace Wiesel