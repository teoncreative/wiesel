
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

#include "behavior/w_behavior.hpp"
#include "script/w_scriptmanager.hpp"

#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/class.h>
#include <mono/metadata/debug-helpers.h>

namespace Wiesel {
class MonoBehavior : public IBehavior {
 public:
  MonoBehavior(Entity entity, const std::string& script_name);
  ~MonoBehavior() override;

  void OnUpdate(float_t delta_time) override;
  void OnEvent(Event& event) override;

  template<class T>
  void AttachExternComponent(std::string variable, entt::entity entity) {
    if (unset_ || !enabled_) {
      return;
    }
    script_instance_->AttachExternComponent<T>(variable, entity);
  }

  ScriptInstance* script_instance() const { return script_instance_.get(); }
 private:
  void InstantiateScript();
  bool OnReloadScripts(ScriptsReloadedEvent& event);
  bool OnKeyPressed(KeyPressedEvent& event);
  bool OnKeyReleased(KeyReleasedEvent& event);
  bool OnMouseMoved(MouseMovedEvent& event);

  std::unique_ptr<ScriptInstance> script_instance_;
};

}  // namespace Wiesel