#include "ai/w_agent_controller.hpp"

namespace Wiesel {

void AgentController::Evaluate(float dt) {
  switch_timer -= dt;

  // If current goal finished, allow immediate re-evaluation
  if (active_goal && active_goal->IsFinished()) {
    active_goal->OnDeactivate();
    active_goal = nullptr;
    switch_timer = 0.0f;
  }

  // Cooldown prevents flicker between goals
  if (switch_timer > 0.0f) {
    return;
  }

  // Find highest-priority activatable goal
  AgentGoal* best = nullptr;
  int best_priority = -1;
  for (auto& goal : goals) {
    if (goal->CanActivate() && goal->GetPriority() > best_priority) {
      best = goal.get();
      best_priority = goal->GetPriority();
    }
  }

  if (best != active_goal) {
    if (active_goal) {
      active_goal->OnDeactivate();
    }
    active_goal = best;
    if (active_goal) {
      active_goal->OnActivate();
    }
    switch_timer = switch_cooldown;
  }
}

void AgentController::Update(float dt) {
  if (active_goal) {
    active_goal->OnUpdate(dt);
  }
}

}  // namespace Wiesel