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

MonoBehavior::MonoBehavior(Entity entity, const std::string& scriptName) :
      IBehavior(scriptName, entity) {
  m_Unset = true;
  m_Enabled = false;
  m_InternalBehavior = false;
  m_Unset = false;
  m_Enabled = true;
  InstantiateScript();
}

MonoBehavior::~MonoBehavior() {
  delete m_ScriptInstance;
}

void MonoBehavior::OnUpdate(float_t deltaTime) {
  if (m_Unset || !m_Enabled) {
    return;
  }
  m_ScriptInstance->OnUpdate(deltaTime);
}

void MonoBehavior::OnEvent(Event& event) {
  EventDispatcher dispatcher{event};

  dispatcher.Dispatch<ScriptsReloadedEvent>(WIESEL_BIND_FN(OnReloadScripts));
}

void MonoBehavior::InstantiateScript() {
  m_ScriptInstance = ScriptManager::CreateScriptInstance(this);
  m_ScriptInstance->OnStart();
}

bool MonoBehavior::OnReloadScripts(ScriptsReloadedEvent& event) {
  delete m_ScriptInstance;
  m_ScriptInstance = nullptr;
  InstantiateScript();
  return false;
}

}