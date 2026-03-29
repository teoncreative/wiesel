//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "script/mono/w_monobehavior.h"

#include "input/w_input.h"
#include "mono_wrappers.h"
#include "script/w_scriptmanager.h"
#include "w_engine.h"

namespace Wiesel {

MonoBehavior::MonoBehavior(Entity entity, const std::string& script_name)
    : IBehavior(script_name, entity) {
  script_instance_ = nullptr;
  InstantiateScript();
}

MonoBehavior::~MonoBehavior() {}

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
  script_instance_ = Engine::script_manager().CreateScriptInstance(this);
  if (script_instance_) {
    unset_ = false;
    enabled_ = true;
  }
}

bool MonoBehavior::OnReloadScripts(ScriptsReloadedEvent& event) {
  std::map<std::string, std::function<MonoObject*()>> copy;
  if (script_instance_) {
    copy = script_instance_->attached_variables_;
    // Detach before destroying so the destructor doesn't touch the dead domain
    script_instance_->Detach();
  }
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

bool MonoBehavior::OnPointerClick(float x, float y) {
  if (script_instance_) {
    return script_instance_->OnPointerClick(x, y);
  }
  return false;
}

bool MonoBehavior::OnPointerDown(float x, float y) {
  if (script_instance_) {
    return script_instance_->OnPointerDown(x, y);
  }
  return false;
}

bool MonoBehavior::OnPointerUp(float x, float y) {
  if (script_instance_) {
    return script_instance_->OnPointerUp(x, y);
  }
  return false;
}

void MonoBehavior::OnPointerEnter() {
  if (script_instance_) {
    script_instance_->OnPointerEnter();
  }
}

void MonoBehavior::OnPointerExit() {
  if (script_instance_) {
    script_instance_->OnPointerExit();
  }
}

void MonoBehavior::OnSelect() {
  if (script_instance_) {
    script_instance_->OnSelect();
  }
}

void MonoBehavior::OnDeselect() {
  if (script_instance_) {
    script_instance_->OnDeselect();
  }
}

bool MonoBehavior::OnSubmit() {
  if (script_instance_) {
    return script_instance_->OnSubmit();
  }
  return false;
}

bool MonoBehavior::OnCancel() {
  if (script_instance_) {
    return script_instance_->OnCancel();
  }
  return false;
}

}  // namespace Wiesel