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
#include <mono/metadata/mono-config.h>
#include <mono/metadata/mono-debug.h>
#include "input/w_input.hpp"
#include "mono_util.h"
#include "scene/w_entity.hpp"
#include "scene/w_scene.hpp"
#include "script/mono/w_monobehavior.hpp"
#include "util/w_logger.hpp"
#include "w_engine.hpp"

namespace Wiesel {

// todo move these bindings to script glue
void Internal_LogInfo(MonoString* str) {
  const char* cstr = mono_string_to_utf8(str);
  LOG_INFO("{}", cstr);
  mono_free((void*) cstr);
}

float Internal_GetAxis(MonoString* str) {
  const char* cstr = mono_string_to_utf8(str);
  float value = InputManager::GetAxis(cstr);
  mono_free((void*) cstr);
  return value;
}

MonoObject* Internal_GetComponent(MonoBehavior* behavior, MonoString* str) {
  const char* cstr = mono_string_to_utf8(str);
  MonoObject* component = ScriptManager::GetComponentByName(behavior, cstr);
  mono_free((void*) cstr);
  return component;
}

#define GENERATE_INTERNALS_FOR_TRACKED_COMPONENT(component, getname, setname, type, field) \
float getname(MonoBehavior* behavior) { \
    return behavior->GetComponent<component>().field; \
} \
void setname(MonoBehavior* behavior, type value) {                            \
  auto& c = behavior->GetComponent<component>();\
  c.field = value;\
  c.IsChanged = true;\
}

// I know these macros can be pretty triggering for some people but
// trust me this is way cleaner this way, it becomes a mess otherwise
GENERATE_INTERNALS_FOR_TRACKED_COMPONENT(TransformComponent,
                                         Internal_TransformComponent_GetPositionX,
                                         Internal_TransformComponent_SetPositionX,
                                         float, Position.x);
GENERATE_INTERNALS_FOR_TRACKED_COMPONENT(TransformComponent,
                                         Internal_TransformComponent_GetPositionY,
                                         Internal_TransformComponent_SetPositionY,
                                         float, Position.y);
GENERATE_INTERNALS_FOR_TRACKED_COMPONENT(TransformComponent,
                                         Internal_TransformComponent_GetPositionZ,
                                         Internal_TransformComponent_SetPositionZ,
                                         float, Position.z);

GENERATE_INTERNALS_FOR_TRACKED_COMPONENT(TransformComponent,
                                         Internal_TransformComponent_GetRotationX,
                                         Internal_TransformComponent_SetRotationX,
                                         float, Rotation.x);
GENERATE_INTERNALS_FOR_TRACKED_COMPONENT(TransformComponent,
                                         Internal_TransformComponent_GetRotationY,
                                         Internal_TransformComponent_SetRotationY,
                                         float, Rotation.y);
GENERATE_INTERNALS_FOR_TRACKED_COMPONENT(TransformComponent,
                                         Internal_TransformComponent_GetRotationZ,
                                         Internal_TransformComponent_SetRotationZ,
                                         float, Rotation.z);

GENERATE_INTERNALS_FOR_TRACKED_COMPONENT(TransformComponent,
                                         Internal_TransformComponent_GetScaleX,
                                         Internal_TransformComponent_SetScaleX,
                                         float, Scale.x);
GENERATE_INTERNALS_FOR_TRACKED_COMPONENT(TransformComponent,
                                         Internal_TransformComponent_GetScaleY,
                                         Internal_TransformComponent_SetScaleY,
                                         float, Scale.y);
GENERATE_INTERNALS_FOR_TRACKED_COMPONENT(TransformComponent,
                                         Internal_TransformComponent_GetScaleZ,
                                         Internal_TransformComponent_SetScaleZ,
                                         float, Scale.z);

ScriptInstance::ScriptInstance(ScriptData* data, MonoBehavior* behavior) {
  m_Behavior = behavior;
  m_ScriptData = data;
  m_Instance = mono_object_new(ScriptManager::GetAppDomain(), data->GetKlass());
  mono_runtime_object_init(m_Instance);

  void* args[1];
  uint64_t behaviorPtr = (uint64_t) behavior;
  args[0] = &behaviorPtr;

  mono_runtime_invoke(data->GetSetHandleMethod(), m_Instance, args, nullptr);
}

ScriptInstance::~ScriptInstance() {
  mono_gchandle_free(mono_gchandle_new(m_Instance, false));
}

void ScriptInstance::OnStart() {
  mono_runtime_invoke(m_ScriptData->GetOnStartMethod(), m_Instance, nullptr, nullptr);
}

void ScriptInstance::OnUpdate(float_t deltaTime) {
  void* args[1];
  args[0] = &deltaTime;
  mono_runtime_invoke(m_ScriptData->GetOnUpdateMethod(), m_Instance, args, nullptr);
}

MonoDomain* ScriptManager::m_RootDomain = nullptr;
MonoAssembly* ScriptManager::m_CoreAssembly = nullptr;
MonoImage* ScriptManager::m_CoreAssemblyImage = nullptr;
MonoDomain* ScriptManager::m_AppDomain = nullptr;
MonoAssembly* ScriptManager::m_AppAssembly = nullptr;
MonoImage* ScriptManager::m_AppAssemblyImage = nullptr;
MonoClass* ScriptManager::m_MonoBehaviorClass = nullptr;
MonoClass* ScriptManager::m_MonoTransformComponentClass = nullptr;
MonoMethod* ScriptManager::m_SetHandleMethod = nullptr;

std::map<std::string, ScriptManager::ComponentGetter> ScriptManager::m_ComponentGetters;
std::map<std::string, ScriptData*> ScriptManager::m_ScriptData;

MonoObject* ScriptManager::GetComponentByName(MonoBehavior* behavior, const std::string& name) {
  return m_ComponentGetters[name](behavior);
}

void ScriptManager::Init() {
  LOG_INFO("Initializing mono...");
  mono_set_dirs("mono/lib", "mono/etc");
  mono_config_parse("mono/etc/mono/config");

//#define MONO_DEBUG
#ifdef MONO_DEBUG
  const char* opt[2] =
      {
          "--debugger-agent=address=0.0.0.0:50000,transport=dt_socket,server=y",
          "--soft-breakpoints"
      };

  mono_jit_parse_options(2, reinterpret_cast<char**>(&opt));
  mono_debug_init(MONO_DEBUG_FORMAT_MONO);
#endif

  m_RootDomain = mono_jit_init("WieselJITRuntime");

  RegisterComponents();
  RegisterInternals();
  LoadCore();
  LoadApp();
}

void ScriptManager::Destroy() {
  LOG_INFO("Cleaning up script manager...");
 // mono_domain_set(m_RootDomain, true);
  //mono_domain_unload(m_EngineDomain);
  //mono_domain_free(m_EngineDomain, true);
  //mono_jit_cleanup(m_RootDomain);
}

void ScriptManager::Reload() {
  LOG_INFO("Reloading scripts...");

  mono_domain_set(m_RootDomain, true);
  mono_domain_unload(m_AppDomain);

  for (const auto& [first, second] : m_ScriptData) {
    delete second;
  }
  m_ScriptData.clear();

  RegisterComponents();
  RegisterInternals();
  LoadCore();
  LoadApp();

  ScriptsReloadedEvent event{};
  Application::Get()->OnEvent(event);
}

void ScriptManager::LoadCore() {
  LOG_INFO("Compiling core scripts...");
  std::vector<std::string> files = {
      "assets/internal_scripts/Internal.cs",
      "assets/internal_scripts/MonoBehavior.cs",
      "assets/internal_scripts/Input.cs",
      "assets/internal_scripts/math/Vector3.cs",
      "assets/internal_scripts/components/TransformComponent.cs"
  };
  CompileToDLL("obj/Core.dll", files);

  m_CoreAssembly = mono_domain_assembly_open(m_RootDomain, "obj/Core.dll");
  assert(m_CoreAssembly);

  m_CoreAssemblyImage = mono_assembly_get_image(m_CoreAssembly);
  m_MonoBehaviorClass = mono_class_from_name(m_CoreAssemblyImage, "WieselEngine", "MonoBehavior");
  m_SetHandleMethod = mono_class_get_method_from_name(m_MonoBehaviorClass, "SetHandle", 1);

  // Component classes
  m_MonoTransformComponentClass = mono_class_from_name(m_CoreAssemblyImage, "WieselEngine", "TransformComponent");
}

void ScriptManager::LoadApp() {
  std::vector<std::string> sourceFiles = {
      "assets/scripts/TestBehavior.cs"
  };
  CompileToDLL("obj/App.dll", sourceFiles, "obj", {"Core.dll"});

  m_AppDomain = mono_domain_create_appdomain("WieselApp", nullptr);
  mono_domain_set(m_AppDomain, true);
  //mono_domain_assembly_open(m_AppDomain, "obj/Core.dll");
  m_AppAssembly = mono_domain_assembly_open(m_AppDomain, "obj/App.dll");
  assert(m_AppAssembly);

  m_AppAssemblyImage = mono_assembly_get_image(m_AppAssembly);

  const MonoTableInfo* tableInfo = mono_image_get_table_info(m_AppAssemblyImage, MONO_TABLE_TYPEDEF);
  int rows = mono_table_info_get_rows(tableInfo);

  for (int i = 0; i < rows; i++) {
    uint32_t cols[MONO_TYPEDEF_SIZE];
    mono_metadata_decode_row(tableInfo, i, cols, MONO_TYPEDEF_SIZE);
    std::string className = mono_metadata_string_heap(m_AppAssemblyImage, cols[MONO_TYPEDEF_NAME]);
    if (className == "<Module>") {
      continue;
    }
    std::string classNamespace = mono_metadata_string_heap(m_AppAssemblyImage, cols[MONO_TYPEDEF_NAMESPACE]);
    // this is needed to load the class, facepalm Microsoft
    mono_class_from_name(m_AppAssemblyImage, classNamespace.c_str(), className.c_str());

    LOG_INFO("Found class {} in namespace {}", className, classNamespace);
    MonoClass* klass = mono_class_from_name(m_AppAssemblyImage, classNamespace.c_str(), className.c_str());
    if (!klass) {
      LOG_ERROR("Class {} in namespace {} not found!", className, classNamespace);
      continue;
    }
    MonoMethod* onStartMethod = mono_class_get_method_from_name(klass, "OnStart", 0);
    MonoMethod* onUpdateMethod = mono_class_get_method_from_name(klass, "OnUpdate", 1);
    m_ScriptData.insert(std::pair(className, new ScriptData(klass, onStartMethod, onUpdateMethod, m_SetHandleMethod)));
  }

}

void ScriptManager::RegisterInternals() {
  mono_add_internal_call("WieselEngine.EngineInternal::LogInfo", reinterpret_cast<void*>(Internal_LogInfo));
  mono_add_internal_call("WieselEngine.EngineInternal::GetAxis", reinterpret_cast<void*>(Internal_GetAxis));
  mono_add_internal_call("WieselEngine.EngineInternal::GetComponent", reinterpret_cast<void*>(Internal_GetComponent));
  mono_add_internal_call("WieselEngine.TransformComponent::GetPositionX", reinterpret_cast<void*>(Internal_TransformComponent_GetPositionX));
  mono_add_internal_call("WieselEngine.TransformComponent::GetPositionY", reinterpret_cast<void*>(Internal_TransformComponent_GetPositionY));
  mono_add_internal_call("WieselEngine.TransformComponent::GetPositionZ", reinterpret_cast<void*>(Internal_TransformComponent_GetPositionZ));
  mono_add_internal_call("WieselEngine.TransformComponent::SetPositionX", reinterpret_cast<void*>(Internal_TransformComponent_SetPositionX));
  mono_add_internal_call("WieselEngine.TransformComponent::SetPositionY", reinterpret_cast<void*>(Internal_TransformComponent_SetPositionY));
  mono_add_internal_call("WieselEngine.TransformComponent::SetPositionZ", reinterpret_cast<void*>(Internal_TransformComponent_SetPositionZ));
  mono_add_internal_call("WieselEngine.TransformComponent::GetRotationX", reinterpret_cast<void*>(Internal_TransformComponent_GetRotationX));
  mono_add_internal_call("WieselEngine.TransformComponent::GetRotationY", reinterpret_cast<void*>(Internal_TransformComponent_GetRotationY));
  mono_add_internal_call("WieselEngine.TransformComponent::GetRotationZ", reinterpret_cast<void*>(Internal_TransformComponent_GetRotationZ));
  mono_add_internal_call("WieselEngine.TransformComponent::SetRotationX", reinterpret_cast<void*>(Internal_TransformComponent_SetRotationX));
  mono_add_internal_call("WieselEngine.TransformComponent::SetRotationY", reinterpret_cast<void*>(Internal_TransformComponent_SetRotationY));
  mono_add_internal_call("WieselEngine.TransformComponent::SetRotationZ", reinterpret_cast<void*>(Internal_TransformComponent_SetRotationZ));
  mono_add_internal_call("WieselEngine.TransformComponent::GetScaleX", reinterpret_cast<void*>(Internal_TransformComponent_GetScaleX));
  mono_add_internal_call("WieselEngine.TransformComponent::GetScaleY", reinterpret_cast<void*>(Internal_TransformComponent_GetScaleY));
  mono_add_internal_call("WieselEngine.TransformComponent::GetScaleZ", reinterpret_cast<void*>(Internal_TransformComponent_GetScaleZ));
  mono_add_internal_call("WieselEngine.TransformComponent::SetScaleX", reinterpret_cast<void*>(Internal_TransformComponent_SetScaleX));
  mono_add_internal_call("WieselEngine.TransformComponent::SetScaleY", reinterpret_cast<void*>(Internal_TransformComponent_SetScaleY));
  mono_add_internal_call("WieselEngine.TransformComponent::SetScaleZ", reinterpret_cast<void*>(Internal_TransformComponent_SetScaleZ));
}

void ScriptManager::RegisterComponents() {
  m_ComponentGetters.clear();

  m_ComponentGetters.insert(
      std::pair("TransformComponent",
                [](MonoBehavior* behavior) -> MonoObject* {
                  // todo add macro for this
                  MonoObject* obj = mono_object_new(m_AppDomain, m_MonoTransformComponentClass);
                  void* args[1];
                  uint64_t behaviorPtr = (uint64_t) behavior;
                  args[0] = &behaviorPtr;

                  MonoMethod* method = mono_class_get_method_from_name(m_MonoTransformComponentClass, ".ctor", 1);
                  mono_runtime_invoke(method, obj, args, nullptr);
                  return obj;
                }));
}

ScriptInstance* ScriptManager::CreateScriptInstance(MonoBehavior* behavior) {
  if (!m_ScriptData.contains(behavior->GetName())) {
    return nullptr;
  }
  ScriptData* data = m_ScriptData[behavior->GetName()];
  return new ScriptInstance(data, behavior);
}
}