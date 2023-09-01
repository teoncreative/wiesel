
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

#include "events/w_events.hpp"
#include "scene/w_entity.hpp"
#include "w_pch.hpp"

namespace Wiesel {

std::string GetBehaviorNameFromPath(const std::string& path);
std::string GetFileNameFromPath(const std::string& path);

template <typename T>
struct ExposedVariable {
  ExposedVariable(T value, const std::string& name)
      : Value(value), Name(name) {}

  ~ExposedVariable() = default;

  T Value;
  std::string Name;

  void Set(T v) { Value = v; }

  WIESEL_GETTER_FN const T& Get() const { return Value; }

  T* GetPtr() { return &Value; }

  virtual void RenderImGui() {}
};

template <typename T>
using ExposedVariablesList = std::vector<Ref<ExposedVariable<T>>>;

class IBehavior {
 public:
  IBehavior(const std::string& name, Entity entity, const std::string& file)
      : m_Name(name),
        m_Entity(entity),
        m_File(file),
        m_InternalBehavior(false),
        m_Enabled(true),
        m_Unset(false) {}

  IBehavior(const std::string& name, Entity entity)
      : m_Name(name),
        m_Entity(entity),
        m_File("Internal"),
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

  WIESEL_GETTER_FN const ExposedVariablesList<double>& GetExposedDoubles()
      const {
    return m_ExposedDoubles;
  }

  WIESEL_GETTER_FN bool IsInternalBehavior() const {
    return m_InternalBehavior;
  }

  WIESEL_GETTER_FN const std::string& GetFile() const { return m_File; }

  WIESEL_GETTER_FN bool IsEnabled() const { return m_Enabled; }

  virtual void SetEnabled(bool enabled);

  WIESEL_GETTER_FN Entity GetEntity() { return m_Entity; }

  WIESEL_GETTER_FN std::string* GetFilePtr() { return &m_File; }

  WIESEL_GETTER_FN virtual void* GetStatePtr() const { return nullptr; }

 protected:
  std::string m_Name;
  Entity m_Entity;
  std::string m_File;
  bool m_InternalBehavior;
  bool m_Enabled;
  bool m_Unset;
  // todo other types like vec3!
  ExposedVariablesList<double> m_ExposedDoubles;
};

// todo maybe use custom entity component system with
// support for having multiple instances of the same component type?
class BehaviorsComponent {
 public:
  BehaviorsComponent() {}

  virtual ~BehaviorsComponent() {}

  void OnEvent(Event&);

  template <typename T, typename... Args>
  Ref<T> AddBehavior(Args&&... args) {
    auto reference = CreateReference<T>(std::forward<Args>(args)...);
    m_Behaviors[reference->GetName()] = reference;
    return reference;
  }

  std::map<std::string, Ref<IBehavior>> m_Behaviors;
};

}  // namespace Wiesel