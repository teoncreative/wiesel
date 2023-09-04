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
#include <mono/metadata/object.h>
#include "input/w_input.hpp"
#include "mono_util.h"
#include "scene/w_entity.hpp"
#include "scene/w_scene.hpp"
#include "script/mono/w_monobehavior.hpp"
#include "util/w_logger.hpp"
#include "w_engine.hpp"

namespace Wiesel {

#define WIESEL_ADD_INTERNAL_CALL(name) \
    mono_add_internal_call("WieselEngine.Internals::"#name, reinterpret_cast<void*>(Internals_##name));

// todo move these bindings to script glue
void Internals_Log_Info(MonoString* str) {
  const char* cstr = mono_string_to_utf8(str);
  LOG_INFO("{}", cstr);
  mono_free((void*) cstr);
}

float Internals_Input_GetAxis(MonoString* str) {
  const char* cstr = mono_string_to_utf8(str);
  float value = InputManager::GetAxis(cstr);
  mono_free((void*) cstr);
  return value;
}

MonoObject* Internals_Behavior_GetComponent(MonoBehavior* behavior, MonoString* str) {
  const char* cstr = mono_string_to_utf8(str);
  MonoObject* component = ScriptManager::GetComponentByName(behavior, cstr);
  mono_free((void*) cstr);
  return component;
}

bool Internals_Behavior_HasComponent(MonoBehavior* behavior, MonoString* str) {
  const char* cstr = mono_string_to_utf8(str);
  bool hasComponent = ScriptManager::HasComponentByName(behavior, cstr);
  mono_free((void*) cstr);
  return hasComponent;
}

float Internals_TransformComponent_GetPositionX(MonoBehavior* behavior) {
  return behavior->GetComponent<TransformComponent>().Position.x;
}

void Internals_TransformComponent_SetPositionX(MonoBehavior* behavior, float value) {
  auto& c = behavior->GetComponent<TransformComponent>();
  if (c.Position.x == value) {
    return;
  }
  c.Position.x = value;
  c.IsChanged = true;
}

float Internals_TransformComponent_GetPositionY(MonoBehavior* behavior) {
  return behavior->GetComponent<TransformComponent>().Position.y;
}

void Internals_TransformComponent_SetPositionY(MonoBehavior* behavior, float value) {
  auto& c = behavior->GetComponent<TransformComponent>();
  if (c.Position.y == value) {
    return;
  }
  c.Position.y = value;
  c.IsChanged = true;
}

float Internals_TransformComponent_GetPositionZ(MonoBehavior* behavior) {
  return behavior->GetComponent<TransformComponent>().Position.z;
}

void Internals_TransformComponent_SetPositionZ(MonoBehavior* behavior, float value) {
  auto& c = behavior->GetComponent<TransformComponent>();
  if (c.Position.z == value) {
    return;
  }
  c.Position.z = value;
  c.IsChanged = true;
}

float Internals_TransformComponent_GetRotationX(MonoBehavior* behavior) {
  return behavior->GetComponent<TransformComponent>().Rotation.x;
}

void Internals_TransformComponent_SetRotationX(MonoBehavior* behavior, float value) {
  auto& c = behavior->GetComponent<TransformComponent>();
  if (c.Rotation.x == value) {
    return;
  }
  c.Rotation.x = value;
  c.IsChanged = true;
}

float Internals_TransformComponent_GetRotationY(MonoBehavior* behavior) {
  return behavior->GetComponent<TransformComponent>().Rotation.y;
}

void Internals_TransformComponent_SetRotationY(MonoBehavior* behavior, float value) {
  auto& c = behavior->GetComponent<TransformComponent>();
  if (c.Rotation.y == value) {
    return;
  }
  c.Rotation.y = value;
  c.IsChanged = true;
}

float Internals_TransformComponent_GetRotationZ(MonoBehavior* behavior) {
  return behavior->GetComponent<TransformComponent>().Rotation.z;
}

void Internals_TransformComponent_SetRotationZ(MonoBehavior* behavior, float value) {
  auto& c = behavior->GetComponent<TransformComponent>();
  if (c.Rotation.z == value) {
    return;
  }
  c.Rotation.z = value;
  c.IsChanged = true;
}

float Internals_TransformComponent_GetScaleX(MonoBehavior* behavior) {
  return behavior->GetComponent<TransformComponent>().Scale.x;
}

void Internals_TransformComponent_SetScaleX(MonoBehavior* behavior, float value) {
  auto& c = behavior->GetComponent<TransformComponent>();
  if (c.Scale.x == value) {
    return;
  }
  c.Scale.x = value;
  c.IsChanged = true;
}

float Internals_TransformComponent_GetScaleY(MonoBehavior* behavior) {
  return behavior->GetComponent<TransformComponent>().Scale.y;
}

void Internals_TransformComponent_SetScaleY(MonoBehavior* behavior, float value) {
  auto& c = behavior->GetComponent<TransformComponent>();
  if (c.Scale.y == value) {
    return;
  }
  c.Scale.y = value;
  c.IsChanged = true;
}

float Internals_TransformComponent_GetScaleZ(MonoBehavior* behavior) {
  return behavior->GetComponent<TransformComponent>().Scale.z;
}

void Internals_TransformComponent_SetScaleZ(MonoBehavior* behavior, float value) {
  auto& c = behavior->GetComponent<TransformComponent>();
  if (c.Scale.z == value) {
    return;
  }
  c.Scale.z = value;
  c.IsChanged = true;
}

MonoObject* CreateVector3fWithValues(float x, float y, float z) {
  MonoObject* obj = mono_object_new(ScriptManager::GetAppDomain(), ScriptManager::GetVector3fClass());
  void* args[3];
  args[0] = &x;
  args[1] = &y;
  args[2] = &z;
  MonoMethod* method = mono_class_get_method_from_name(ScriptManager::GetVector3fClass(), ".ctor", 3);
  mono_runtime_invoke(method, obj, args, nullptr);
  return obj;
}

MonoObject* Internals_TransformComponent_GetForward(MonoBehavior* behavior) {
  auto& c = behavior->GetComponent<TransformComponent>();
  glm::vec3 val = c.GetForward();
  return CreateVector3fWithValues(val.x, val.y, val.z);
}

MonoObject* Internals_TransformComponent_GetBackward(MonoBehavior* behavior) {
  auto& c = behavior->GetComponent<TransformComponent>();
  glm::vec3 val = c.GetBackward();
  return CreateVector3fWithValues(val.x, val.y, val.z);
}

MonoObject* Internals_TransformComponent_GetLeft(MonoBehavior* behavior) {
  auto& c = behavior->GetComponent<TransformComponent>();
  glm::vec3 val = c.GetLeft();
  return CreateVector3fWithValues(val.x, val.y, val.z);
}

MonoObject* Internals_TransformComponent_GetRight(MonoBehavior* behavior) {
  auto& c = behavior->GetComponent<TransformComponent>();
  glm::vec3 val = c.GetRight();
  return CreateVector3fWithValues(val.x, val.y, val.z);
}

MonoObject* Internals_TransformComponent_GetUp(MonoBehavior* behavior) {
  auto& c = behavior->GetComponent<TransformComponent>();
  glm::vec3 val = c.GetUp();
  return CreateVector3fWithValues(val.x, val.y, val.z);
}

MonoObject* Internals_TransformComponent_GetDown(MonoBehavior* behavior) {
  auto& c = behavior->GetComponent<TransformComponent>();
  glm::vec3 val = c.GetDown();
  return CreateVector3fWithValues(val.x, val.y, val.z);
}

ScriptInstance::ScriptInstance(ScriptData* data, MonoBehavior* behavior) {
  m_Behavior = behavior;
  m_ScriptData = data;
  m_Instance = mono_object_new(ScriptManager::GetAppDomain(), data->GetKlass());
  mono_runtime_object_init(m_Instance);

  void* args[1];
  uint64_t behaviorPtr = (uint64_t) behavior;
  args[0] = &behaviorPtr;

  mono_runtime_invoke(data->GetSetHandleMethod(), m_Instance, args, nullptr);
  m_GCHandle = mono_gchandle_new(m_Instance, true);
}

ScriptInstance::~ScriptInstance() {
  mono_gchandle_free(m_GCHandle);
}

void ScriptInstance::OnStart() {
  mono_runtime_invoke(m_ScriptData->GetOnStartMethod(), m_Instance, nullptr, nullptr);
}

void ScriptInstance::OnUpdate(float_t deltaTime) {
  mono_domain_set(ScriptManager::GetAppDomain(), true);
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
MonoClass* ScriptManager::m_MonoVector3fClass = nullptr;
MonoMethod* ScriptManager::m_SetHandleMethod = nullptr;

std::map<std::string, ScriptManager::ComponentGetter> ScriptManager::m_ComponentGetters;
std::map<std::string, ScriptManager::ComponentChecker> ScriptManager::m_ComponentCheckers;
std::map<std::string, ScriptData*> ScriptManager::m_ScriptData;

MonoObject* ScriptManager::GetComponentByName(MonoBehavior* behavior, const std::string& name) {
  auto& fn = m_ComponentGetters[name];
  if (fn == nullptr) {
    return nullptr;
  }
  return fn(behavior);
}


bool ScriptManager::HasComponentByName(MonoBehavior* behavior, const std::string& name) {
  auto& fn = m_ComponentCheckers[name];
  if (fn == nullptr) {
    return false;
  }
  return fn(behavior);
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
      "assets/internal_scripts/Internals.cs",
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
  m_MonoVector3fClass = mono_class_from_name(m_CoreAssemblyImage, "WieselEngine", "Vector3f");
}

void ScriptManager::LoadApp() {
  // todo search files
  // todo load files from project file when project system is added
  std::vector<std::string> sourceFiles = {
      "assets/scripts/TestBehavior.cs",
      "assets/scripts/CameraScript.cs"
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
    std::unordered_map<std::string, FieldData> fields;
    MonoClassField *field;
    void* iter = nullptr;
    while ((field = mono_class_get_fields(klass, &iter))) {
      std::string fieldName = mono_field_get_name(field);
      uint32_t fieldFlags = mono_field_get_flags(field);
      // fieldFlags & FIELD_ATTRIBUTE_FIELD_ACCESS_MASK == FIELD_ATTRIBUTE_PUBLIC
      if ((fieldFlags & 0x0007) != 0x0006) {
        continue;
      }
      //LOG_INFO("Public field: {}, flags {}", fieldName, fieldFlags);
      fields.insert(std::pair(fieldName, FieldData(field, fieldName, fieldFlags)));
    }
    MonoMethod* onStartMethod = mono_class_get_method_from_name(klass, "OnStart", 0);
    MonoMethod* onUpdateMethod = mono_class_get_method_from_name(klass, "OnUpdate", 1);
    m_ScriptData.insert(std::pair(className, new ScriptData(klass, onStartMethod, onUpdateMethod, m_SetHandleMethod, fields)));
  }

}

void ScriptManager::RegisterInternals() {
  WIESEL_ADD_INTERNAL_CALL(Log_Info);
  WIESEL_ADD_INTERNAL_CALL(Input_GetAxis);
  WIESEL_ADD_INTERNAL_CALL(Behavior_GetComponent);
  WIESEL_ADD_INTERNAL_CALL(Behavior_HasComponent);
  WIESEL_ADD_INTERNAL_CALL(TransformComponent_GetPositionX);
  WIESEL_ADD_INTERNAL_CALL(TransformComponent_GetPositionY);
  WIESEL_ADD_INTERNAL_CALL(TransformComponent_GetPositionZ);
  WIESEL_ADD_INTERNAL_CALL(TransformComponent_SetPositionX);
  WIESEL_ADD_INTERNAL_CALL(TransformComponent_SetPositionY);
  WIESEL_ADD_INTERNAL_CALL(TransformComponent_SetPositionZ);
  WIESEL_ADD_INTERNAL_CALL(TransformComponent_GetRotationX);
  WIESEL_ADD_INTERNAL_CALL(TransformComponent_GetRotationY);
  WIESEL_ADD_INTERNAL_CALL(TransformComponent_GetRotationZ);
  WIESEL_ADD_INTERNAL_CALL(TransformComponent_SetRotationX);
  WIESEL_ADD_INTERNAL_CALL(TransformComponent_SetRotationY);
  WIESEL_ADD_INTERNAL_CALL(TransformComponent_SetRotationZ);
  WIESEL_ADD_INTERNAL_CALL(TransformComponent_GetScaleX);
  WIESEL_ADD_INTERNAL_CALL(TransformComponent_GetScaleY);
  WIESEL_ADD_INTERNAL_CALL(TransformComponent_GetScaleZ);
  WIESEL_ADD_INTERNAL_CALL(TransformComponent_SetScaleX);
  WIESEL_ADD_INTERNAL_CALL(TransformComponent_SetScaleY);
  WIESEL_ADD_INTERNAL_CALL(TransformComponent_SetScaleZ);
  WIESEL_ADD_INTERNAL_CALL(TransformComponent_GetForward);
  WIESEL_ADD_INTERNAL_CALL(TransformComponent_GetBackward);
  WIESEL_ADD_INTERNAL_CALL(TransformComponent_GetLeft);
  WIESEL_ADD_INTERNAL_CALL(TransformComponent_GetRight);
  WIESEL_ADD_INTERNAL_CALL(TransformComponent_GetUp);
  WIESEL_ADD_INTERNAL_CALL(TransformComponent_GetDown);
}

void ScriptManager::RegisterComponents() {
  m_ComponentGetters.clear();
  m_ComponentCheckers.clear();

  // Getters
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

  // Checkers
  m_ComponentCheckers.insert(
      std::pair("TransformComponent",
                [](MonoBehavior* behavior) -> bool {
                  return behavior->HasComponent<TransformComponent>();
                }));

  // Removers
}

ScriptInstance* ScriptManager::CreateScriptInstance(MonoBehavior* behavior) {
  if (!m_ScriptData.contains(behavior->GetName())) {
    return nullptr;
  }
  ScriptData* data = m_ScriptData[behavior->GetName()];
  return new ScriptInstance(data, behavior);
}
}