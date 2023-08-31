//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "script/w_scriptmanager.hpp"
#include <mono/metadata/debug-helpers.h>
#include "mono_util.h"
#include "util/w_logger.hpp"

namespace Wiesel {
ScriptManager* ScriptManager::m_ScriptManager = nullptr;

ScriptManager::ScriptManager() {
  mono_jit_init("Wiesel");

  m_RootDomain = mono_domain_get();
  mono_domain_set(m_RootDomain, false);
  mono_jit_thread_attach(m_RootDomain);

  CompileToDLL("obj/engine/WieselMono.dll", {"assets/scripts/MonoBehavior.cs", "assets/scripts/Log.cs"});
  m_EngineDomain = mono_domain_create_appdomain("Engine", nullptr);
  mono_domain_set(m_EngineDomain, false);
  mono_jit_thread_attach(m_EngineDomain);
  MonoAssembly* assembly = mono_domain_assembly_open(m_EngineDomain, "obj/engine/WieselMono.dll");
  if (!assembly) {
    return;
  }

  m_EngineImage = mono_assembly_get_image(assembly);
  m_MonoBehaviorClass = mono_class_from_name(m_EngineImage, "WieselEngine", "MonoBehavior");

  mono_domain_set(m_RootDomain, false);
}

ScriptManager::~ScriptManager() {
  mono_domain_set(m_RootDomain, false);
  mono_domain_unload(m_EngineDomain);
}

void ScriptManager::Compile(const std::string& outputFile, const std::vector<std::string>& inputFiles) {
  CompileToDLL(outputFile, inputFiles, "obj/engine", {"WieselMono.dll"});
}

void ScriptManager::Init() {
  m_ScriptManager = new ScriptManager();
}

void ScriptManager::Destroy() {
  delete m_ScriptManager;
  m_ScriptManager = nullptr;
}

}