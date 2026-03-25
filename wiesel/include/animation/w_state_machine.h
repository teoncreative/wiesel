
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "animation/w_animation_controller.h"
#include "w_pch.h"

namespace Wiesel {

// Generic state machine runtime. Evaluates transitions based on parameters.
// Doesn't know about sprites or skeletons - just states and transitions.
// Designed to be used by both SpriteAnimator and skeletal Animator.
struct StateMachineRuntime {
  AnimationController controller;
  std::map<std::string, AnimParam> parameters;
  std::string current_state;
  float state_time = 0.0f;

  // Set parameters
  void SetBool(const std::string& name, bool val);
  void SetInt(const std::string& name, int val);
  void SetFloat(const std::string& name, float val);
  void SetTrigger(const std::string& name);

  bool GetBool(const std::string& name) const;
  int GetInt(const std::string& name) const;
  float GetFloat(const std::string& name) const;

  // Evaluate transitions. Returns the fired transition's to_state,
  // or empty string if no transition fires.
  // Consumes triggers that were used.
  std::string EvaluateTransitions();

  // Initialize to default state if not already set
  void EnsureDefaultState();

  // Get current state data
  const AnimationState* GetCurrentState() const;
};

// Evaluate a single condition against parameter map
bool EvaluateCondition(const TransitionCondition& cond,
                       const std::map<std::string, AnimParam>& params);

}  // namespace Wiesel