
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
      : name_(name),
        entity_(entity),
        scene_(entity.GetScene()),
        internal_behavior_(true),
        enabled_(true),
        unset_(false) {}

  virtual ~IBehavior() {}

  virtual void OnUpdate(float_t delta_time);
  virtual void OnEvent(Event& event);

  template <typename T, typename... Args>
  T& AddComponent(Args&&... args) {
    return entity_.AddComponent<T>(args...);
  }

  template <typename T>
  WIESEL_GETTER_FN T& GetComponent() {
    return entity_.GetComponent<T>();
  }

  template <typename T>
  bool HasComponent() {
    return entity_.HasComponent<T>();
  }

  template <typename T>
  void RemoveComponent() {
    entity_.RemoveComponent<T>();
  }

  WIESEL_GETTER_FN const std::string& GetName() { return name_; }

  WIESEL_GETTER_FN bool IsInternalBehavior() const {
    return internal_behavior_;
  }

  WIESEL_GETTER_FN bool IsEnabled() const { return enabled_; }

  virtual void SetEnabled(bool enabled);

  WIESEL_GETTER_FN Entity entity() { return entity_; }
  WIESEL_GETTER_FN Scene* scene() { return scene_; }
  WIESEL_GETTER_FN entt::entity handle() { return entity_.handle(); }

 protected:
  std::string name_;
  Entity entity_;
  Scene* scene_;
  bool internal_behavior_;
  bool enabled_;
  bool unset_;
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
    behaviors_.insert(std::pair(behavior->GetName(), behavior));
    return *behavior;
  }

  std::unordered_map<std::string, IBehavior*> behaviors_;
};

}  // namespace Wiesel