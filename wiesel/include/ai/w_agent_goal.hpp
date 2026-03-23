#pragma once

#include "scene/w_entity.hpp"
#include "w_pch.hpp"

namespace Wiesel {

class AgentGoal {
 public:
  explicit AgentGoal(const std::string& name) : name_(name) {}

  virtual ~AgentGoal() = default;

  virtual int GetPriority() const = 0;
  virtual bool CanActivate() const = 0;

  virtual void OnActivate() {}

  virtual void OnDeactivate() {}

  virtual void OnUpdate(float dt) = 0;

  virtual bool IsFinished() const { return false; }

  const std::string& GetName() const { return name_; }

  void SetEntity(Entity entity) { entity_ = entity; }

  Entity GetEntity() const { return entity_; }

 protected:
  template <typename T>
  T& GetComponent() const {
    return entity_.GetComponent<T>();
  }

  template <typename T>
  bool HasComponent() const {
    return entity_.HasComponent<T>();
  }

  Scene* GetScene() const { return entity_.GetScene(); }

  std::string name_;
  mutable Entity entity_{entt::null, nullptr};
};

}  // namespace Wiesel