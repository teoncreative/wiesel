
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "animation/w_state_machine.h"

namespace wiesel {

bool EvaluateCondition(const TransitionCondition& cond,
                       const std::map<std::string, AnimParam>& params) {
  auto it = params.find(cond.param_name);
  if (it == params.end()) {
    return false;
  }

  const auto& param = it->second;

  switch (cond.param_type) {
    case AnimParamType::Trigger:
      return param.b;

    case AnimParamType::Bool:
      switch (cond.op) {
        case ConditionOp::Equals:
          return param.b == cond.value.b;
        case ConditionOp::NotEquals:
          return param.b != cond.value.b;
        default:
          return false;
      }

    case AnimParamType::Int:
      switch (cond.op) {
        case ConditionOp::Equals:
          return param.i == cond.value.i;
        case ConditionOp::NotEquals:
          return param.i != cond.value.i;
        case ConditionOp::Greater:
          return param.i > cond.value.i;
        case ConditionOp::Less:
          return param.i < cond.value.i;
      }
      break;

    case AnimParamType::Float:
      switch (cond.op) {
        case ConditionOp::Equals:
          return std::abs(param.f - cond.value.f) <= 0.001f;
        case ConditionOp::NotEquals:
          return std::abs(param.f - cond.value.f) > 0.001f;
        case ConditionOp::Greater:
          return param.f > cond.value.f;
        case ConditionOp::Less:
          return param.f < cond.value.f;
      }
      break;
  }
  return false;
}

void StateMachineRuntime::SetBool(const std::string& name, bool val) {
  parameters[name] = AnimParam::MakeBool(val);
}

void StateMachineRuntime::SetInt(const std::string& name, int val) {
  parameters[name] = AnimParam::MakeInt(val);
}

void StateMachineRuntime::SetFloat(const std::string& name, float val) {
  parameters[name] = AnimParam::MakeFloat(val);
}

void StateMachineRuntime::SetTrigger(const std::string& name) {
  auto& p = parameters[name];
  p.type = AnimParamType::Trigger;
  p.b = true;
}

bool StateMachineRuntime::GetBool(const std::string& name) const {
  auto it = parameters.find(name);
  if (it == parameters.end()) {
    return false;
  }
  return it->second.b;
}

int StateMachineRuntime::GetInt(const std::string& name) const {
  auto it = parameters.find(name);
  if (it == parameters.end()) {
    return 0;
  }
  return it->second.i;
}

float StateMachineRuntime::GetFloat(const std::string& name) const {
  auto it = parameters.find(name);
  if (it == parameters.end()) {
    return 0.0f;
  }
  return it->second.f;
}

void StateMachineRuntime::EnsureDefaultState() {
  if (current_state.empty() && !controller.default_state.empty()) {
    current_state = controller.default_state;
    state_time = 0.0f;
  }
}

const AnimationState* StateMachineRuntime::GetCurrentState() const {
  return controller.FindState(current_state);
}

std::string StateMachineRuntime::EvaluateTransitions() {
  for (const auto& trans : controller.transitions) {
    // Skip if from_state doesn't match (empty = "any state")
    if (!trans.from_state.empty() && trans.from_state != current_state) {
      continue;
    }

    // Don't transition to the same state
    if (trans.to_state == current_state) {
      continue;
    }

    // Evaluate all conditions
    bool all_met = true;
    for (const auto& cond : trans.conditions) {
      if (!EvaluateCondition(cond, parameters)) {
        all_met = false;
        break;
      }
    }

    if (all_met) {
      // Consume triggers used in this transition
      for (const auto& cond : trans.conditions) {
        if (cond.param_type == AnimParamType::Trigger) {
          auto it = parameters.find(cond.param_name);
          if (it != parameters.end()) {
            it->second.b = false;
          }
        }
      }

      current_state = trans.to_state;
      state_time = 0.0f;
      return trans.to_state;
    }
  }
  return "";
}

}  // namespace wiesel