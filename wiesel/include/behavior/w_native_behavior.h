
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

#include "behavior/w_behavior.h"

namespace wiesel {

// C++ behavior base class - mirrors C# MonoBehavior lifecycle.
// Users inherit from this and override the virtual methods they need.
class NativeBehavior : public IBehavior {
 public:
  NativeBehavior(Entity entity, const std::string& name)
      : IBehavior(name, entity) {
    unset_ = false;
    enabled_ = true;
    internal_behavior_ = false;
  }

  ~NativeBehavior() override = default;

  void OnUpdate(float_t delta_time) override {
    if (!started_) {
      OnStart();
      started_ = true;
    }
    OnTick(delta_time);
  }

  // Override these in your behavior
  virtual void OnStart() {}

  virtual void OnTick(float_t delta_time) {}

  virtual void OnDestroy() {}

 private:
  bool started_ = false;
};

// Factory function type for creating native behaviors
using NativeBehaviorFactory = std::function<NativeBehavior*(Entity entity)>;

// Registry for native behaviors
class NativeBehaviorRegistry {
 public:
  NativeBehaviorRegistry() = default;

  template <typename T>
  void Register(const std::string& name) {
    factories_[name] = [name](Entity entity) -> NativeBehavior* {
      return new T(entity, name);
    };
    names_.push_back(name);
  }

  bool Has(const std::string& name) const { return factories_.contains(name); }

  NativeBehavior* Create(const std::string& name, Entity entity) const {
    auto it = factories_.find(name);
    if (it == factories_.end()) {
      return nullptr;
    }
    return it->second(entity);
  }

  const std::vector<std::string>& GetNames() const { return names_; }

 private:
  std::unordered_map<std::string, NativeBehaviorFactory> factories_;
  std::vector<std::string> names_;
};

}  // namespace wiesel
