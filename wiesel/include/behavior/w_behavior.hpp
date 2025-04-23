
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

#include <events/w_keyevents.hpp>
#include <events/w_mouseevents.hpp>
#include "events/w_events.hpp"
#include "scene/w_entity.hpp"
#include "w_pch.hpp"

namespace Wiesel {

std::string GetBehaviorNameFromPath(const std::string& path);
std::string GetFileNameFromPath(const std::string& path);

class IBehavior {
 public:
  IBehavior(const std::string& name, Entity entity)
      : m_Name(name),
        m_Entity(entity),
        m_Scene(entity.GetScene()),
        m_InternalBehavior(true),
        m_Enabled(true),
        m_Unset(false) {}

  virtual ~IBehavior() {}

  virtual void OnUpdate(float_t deltaTime);
  virtual void OnEvent(Event& event);

  template <typename T, typename... Args>
  T& AddComponent(Args&&... args) {
    return m_Entity.AddComponent<T>(args...);
  }

  template <typename T>
  WIESEL_GETTER_FN T& GetComponent() {
    return m_Entity.GetComponent<T>();
  }

  template <typename T>
  bool HasComponent() {
    return m_Entity.HasComponent<T>();
  }

  template <typename T>
  void RemoveComponent() {
    m_Entity.RemoveComponent<T>();
  }

  WIESEL_GETTER_FN const std::string& GetName() { return m_Name; }

  WIESEL_GETTER_FN bool IsInternalBehavior() const {
    return m_InternalBehavior;
  }

  WIESEL_GETTER_FN bool IsEnabled() const { return m_Enabled; }

  virtual void SetEnabled(bool enabled);

  WIESEL_GETTER_FN Entity GetEntity() { return m_Entity; }
  WIESEL_GETTER_FN Scene* GetScene() { return m_Scene; }
  WIESEL_GETTER_FN entt::entity GetEntityHandle() { return m_Entity.GetHandle(); }

 protected:
  std::string m_Name;
  Entity m_Entity;
  Scene* m_Scene;
  bool m_InternalBehavior;
  bool m_Enabled;
  bool m_Unset;
};

// todo maybe use custom entity component system with
// support for having multiple instances of the same component type?
class BehaviorsComponent : public IComponent {
 public:
  BehaviorsComponent() {}

  virtual ~BehaviorsComponent() {}

  void OnEvent(Event&);

  template <typename T, typename... Args>
  T& AddBehavior(Args&&... args) {
    T* behavior = new T(std::forward<Args>(args)...);
    m_Behaviors.insert(std::pair(behavior->GetName(), behavior));
    return *behavior;
  }

  std::unordered_map<std::string, IBehavior*> m_Behaviors;
};

}  // namespace Wiesel