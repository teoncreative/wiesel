
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
  MonoBehavior(Entity entity, const std::string& scriptName);
  ~MonoBehavior() override;

  void OnUpdate(float_t deltaTime) override;
  void OnEvent(Event& event) override;

  ScriptInstance* GetScriptInstance() const { return m_ScriptInstance; }
 private:
  void InstantiateScript();
  bool OnReloadScripts(ScriptsReloadedEvent& event);
  bool OnKeyPressed(KeyPressedEvent& event);
  bool OnKeyReleased(KeyReleasedEvent& event);
  bool OnMouseMoved(MouseMovedEvent& event);

  ScriptInstance* m_ScriptInstance;
};

}  // namespace Wiesel