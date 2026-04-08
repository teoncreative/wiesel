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

#include "w_pch.h"

namespace Wiesel {

enum class AnimParamType { Bool, Int, Float, Trigger };

struct AnimParam {
  AnimParamType type = AnimParamType::Bool;

  union {
    bool b;
    int i;
    float f;
  };

  AnimParam() : type(AnimParamType::Bool), b(false) {}

  AnimParam(AnimParamType t, bool val) : type(t), b(val) {}

  AnimParam(AnimParamType t, int val) : type(t), i(val) {}

  AnimParam(AnimParamType t, float val) : type(t), f(val) {}

  static AnimParam MakeBool(bool val) { return {AnimParamType::Bool, val}; }

  static AnimParam MakeInt(int val) { return {AnimParamType::Int, val}; }

  static AnimParam MakeFloat(float val) { return {AnimParamType::Float, val}; }

  static AnimParam MakeTrigger() {
    AnimParam p;
    p.type = AnimParamType::Trigger;
    p.b = false;
    return p;
  }
};

enum class ConditionOp { Equals, NotEquals, Greater, Less };

struct TransitionCondition {
  std::string param_name;
  ConditionOp op = ConditionOp::Equals;
  AnimParamType param_type = AnimParamType::Bool;

  union {
    bool b;
    int i;
    float f;
  } value = {.b = true};

  static TransitionCondition BoolEquals(const std::string& name, bool val) {
    TransitionCondition c;
    c.param_name = name;
    c.op = ConditionOp::Equals;
    c.param_type = AnimParamType::Bool;
    c.value.b = val;
    return c;
  }

  static TransitionCondition Trigger(const std::string& name) {
    TransitionCondition c;
    c.param_name = name;
    c.op = ConditionOp::Equals;
    c.param_type = AnimParamType::Trigger;
    c.value.b = true;
    return c;
  }

  static TransitionCondition FloatGreater(const std::string& name, float val) {
    TransitionCondition c;
    c.param_name = name;
    c.op = ConditionOp::Greater;
    c.param_type = AnimParamType::Float;
    c.value.f = val;
    return c;
  }

  static TransitionCondition FloatLess(const std::string& name, float val) {
    TransitionCondition c;
    c.param_name = name;
    c.op = ConditionOp::Less;
    c.param_type = AnimParamType::Float;
    c.value.f = val;
    return c;
  }

  static TransitionCondition IntEquals(const std::string& name, int val) {
    TransitionCondition c;
    c.param_name = name;
    c.op = ConditionOp::Equals;
    c.param_type = AnimParamType::Int;
    c.value.i = val;
    return c;
  }
};

struct AnimationState {
  std::string name;
  std::string clip_name;
  float speed = 1.0f;
  bool looping = true;
};

struct AnimationTransition {
  std::string from_state;  // empty = "any state"
  std::string to_state;
  float blend_duration = 0.25f;  // seconds
  std::vector<TransitionCondition> conditions;
  // Editor metadata
  int32_t editor_id = -1;
};

struct AnimationController {
  std::vector<AnimationState> states;
  std::vector<AnimationTransition> transitions;
  std::string default_state;

  bool IsEmpty() const { return states.empty(); }

  const AnimationState* FindState(const std::string& name) const {
    for (const auto& s : states) {
      if (s.name == name) {
        return &s;
      }
    }
    return nullptr;
  }
};

}  // namespace Wiesel
