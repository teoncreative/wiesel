//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "script/mono/w_monobehavior.hpp"

#include "mono_util.h"
#include "script/w_scriptmanager.hpp"
#include "input/w_input.hpp"

namespace Wiesel {

MonoBehavior::MonoBehavior(Entity entity, const std::string& script_name) :
      IBehavior(script_name, entity) {
  unset_ = true;
  internal_behavior_ = false;
  unset_ = false;
  enabled_ = true;
  script_instance_ = nullptr;
  InstantiateScript();
}

MonoBehavior::~MonoBehavior() {
}

void MonoBehavior::OnUpdate(float_t delta_time) {
  if (unset_ || !enabled_) {
    return;
  }
  script_instance_->OnUpdate(delta_time);
}

void MonoBehavior::OnEvent(Event& event) {
  EventDispatcher dispatcher{event};

  dispatcher.Dispatch<ScriptsReloadedEvent>(WIESEL_BIND_FN(OnReloadScripts));
  dispatcher.Dispatch<KeyPressedEvent>(WIESEL_BIND_FN(OnKeyPressed));
  dispatcher.Dispatch<KeyReleasedEvent>(WIESEL_BIND_FN(OnKeyReleased));
  dispatcher.Dispatch<MouseMovedEvent>(WIESEL_BIND_FN(OnMouseMoved));
}

void MonoBehavior::InstantiateScript() {
  if (name_.empty()) {
    return;
  }
  script_instance_ = ScriptManager::CreateScriptInstance(this);
}

bool MonoBehavior::OnReloadScripts(ScriptsReloadedEvent& event) {
  std::map<std::string, std::function<MonoObject*()>> copy = script_instance_->attached_variables_;
  script_instance_ = nullptr;
  InstantiateScript();
  script_instance_->attached_variables_ = copy;
  return false;
}

bool MonoBehavior::OnKeyPressed(KeyPressedEvent& event) {
  if (unset_ || !enabled_) {
    return false;
  }
  return script_instance_->OnKeyPressed(event);
}

bool MonoBehavior::OnKeyReleased(KeyReleasedEvent& event) {
  if (unset_ || !enabled_) {
    return false;
  }
  return script_instance_->OnKeyReleased(event);
}

bool MonoBehavior::OnMouseMoved(MouseMovedEvent& event) {
  if (unset_ || !enabled_) {
    return false;
  }
  return script_instance_->OnMouseMoved(event);
}
}