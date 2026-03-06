#pragma once

#include "scene/w_components.hpp"
#include "scene/w_entity.hpp"
#include "w_agent_goal.hpp"
#include "w_pch.hpp"

namespace Wiesel {

struct AgentController : public IComponent {
  AgentController() = default;
  AgentController(const AgentController&) = delete;
  AgentController& operator=(const AgentController&) = delete;
  AgentController(AgentController&&) noexcept = default;
  AgentController& operator=(AgentController&&) noexcept = default;

  std::vector<std::unique_ptr<AgentGoal>> goals;
  AgentGoal* active_goal = nullptr;
  float switch_cooldown = 0.3f;
  float switch_timer = 0.0f;

  template <typename T, typename... Args>
  T& AddGoal(Entity entity, Args&&... args) {
    auto goal = std::make_unique<T>(std::forward<Args>(args)...);
    goal->SetEntity(entity);
    T& ref = *goal;
    goals.push_back(std::move(goal));
    return ref;
  }

  void Evaluate(float dt);
  void Update(float dt);
};

}  // namespace Wiesel