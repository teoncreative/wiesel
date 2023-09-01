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

MonoBehavior::MonoBehavior(Entity entity, const std::string& sourceFile) :
      IBehavior(GetBehaviorNameFromPath(sourceFile), entity) {
  mono_domain_set(ScriptManager::Get()->GetRootDomain(), false);
  std::string name = GetBehaviorNameFromPath(sourceFile);
  std::string dllPath = "obj/" + name + ".dll";
  ScriptManager::Get()->Compile(dllPath, {sourceFile});
  m_Domain = mono_domain_create_appdomain(&name[0], nullptr);
  mono_domain_set(m_Domain, false);
  mono_jit_thread_attach(m_Domain);
  mono_domain_assembly_open(m_Domain, "obj/engine/WieselMono.dll");

  m_Assembly = mono_domain_assembly_open(m_Domain, &dllPath[0]);
  if (!m_Assembly) {
    std::cout << "Assembly not found!" << std::endl;
    m_IsEnabled = false;
    return;
  }

  MonoImage* image = mono_assembly_get_image(m_Assembly);


  const MonoTableInfo* table_info = mono_image_get_table_info(image, MONO_TABLE_TYPEDEF);
  int rows = mono_table_info_get_rows(table_info);

  std::string className = "";
  std::string classNamespace = "";
  for (int i = 0; i < rows; i++) {
    uint32_t cols[MONO_TYPEDEF_SIZE];
    mono_metadata_decode_row(table_info, i, cols, MONO_TYPEDEF_SIZE);
    className = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAME]);
    if (className == "<Module>") {
      className = "";
      continue;
    }
    classNamespace = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAMESPACE]);
    mono_class_from_name(image, classNamespace.c_str(), className.c_str());
    break;
  }

  m_BehaviorClass = mono_class_from_name(image, classNamespace.c_str(), className.c_str());
  if (!m_BehaviorClass) {
    std::cout << "Class not found!" << std::endl;
    return;
  }

  m_MethodStart = mono_class_get_method_from_name(m_BehaviorClass, "Start", 0);
  m_MethodUpdate = mono_class_get_method_from_name(m_BehaviorClass, "Update", 0);

  m_BehaviorObject = mono_object_new(m_Domain, m_BehaviorClass);
  mono_runtime_object_init(m_BehaviorObject);

  void* args[2];
  /* Note we put the address of the value type in the args array */
  uint32_t entityId = (uint32_t) entity;
  args[0] = &entityId;
  uint64_t ptr = (uint64_t) entity.GetScene();
  args[1] = &ptr;

  MonoMethod* method = mono_class_get_method_from_name(ScriptManager::Get()->GetMonoBehaviorClass(), "SetHandle", 2);
  mono_runtime_invoke(method, m_BehaviorObject, args, nullptr);

  mono_runtime_invoke(m_MethodStart, m_BehaviorObject, nullptr, nullptr);
  m_CanEnable = true;
}

MonoBehavior::~MonoBehavior() {
  // switch back to the root domain
  mono_domain_set(ScriptManager::Get()->GetRootDomain(), false);
  // unload our appdomain
  mono_domain_unload(m_Domain);
}

void MonoBehavior::OnUpdate(float_t deltaTime) {
  if (!m_CanEnable || !m_IsEnabled) {
    return;
  }
  mono_runtime_invoke(m_MethodUpdate, m_BehaviorObject, nullptr, nullptr);
}

void MonoBehavior::OnEvent(Event& event) {

}

void MonoBehavior::SetEnabled(bool enabled) {
  if (!m_CanEnable && enabled) {
    return;
  }
  m_IsEnabled = enabled;
}
}