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
#include "input/w_input.hpp"
#include "mono_util.h"
#include "scene/w_entity.hpp"
#include "util/w_logger.hpp"
#include "scene/w_scene.hpp"

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

MonoObject* Internal_GetComponent(MonoObject* obj, MonoString* str) {
  auto* odomain = mono_domain_get();
  auto* domain = mono_object_get_domain(obj);
  mono_domain_set(domain, false);
  const char* cstr = mono_string_to_utf8(str);
  uint32_t entityId = GetObjectFieldValue<uint32_t>(obj, ScriptManager::Get()->GetMonoBehaviorClass(), "entityId");
  void* scenePtr = GetObjectFieldValue<void*>(obj, ScriptManager::Get()->GetMonoBehaviorClass(), "scenePtr");
  Entity entity{(entt::entity) entityId, (Scene*) scenePtr};
  MonoObject* component = ScriptManager::Get()->GetComponentByName(entity, cstr);
  mono_free((void*) cstr);
  mono_domain_set(odomain, false);
  return component;
}

float Internal_TransformComponent_GetPositionX(entt::entity entityId, Scene* scene) {
  Entity entity{(entt::entity) entityId, (Scene*) scene};
  return entity.GetComponent<TransformComponent>().Position.x;
}

float Internal_TransformComponent_GetPositionY(entt::entity entityId, Scene* scene) {
  Entity entity{(entt::entity) entityId, (Scene*) scene};
  return entity.GetComponent<TransformComponent>().Position.y;
}

float Internal_TransformComponent_GetPositionZ(entt::entity entityId, Scene* scene) {
  Entity entity{(entt::entity) entityId, (Scene*) scene};
  return entity.GetComponent<TransformComponent>().Position.z;
}

void Internal_TransformComponent_SetPositionX(entt::entity entityId, Scene* scene, float value) {
  Entity entity{(entt::entity) entityId, (Scene*) scene};
  auto& component = entity.GetComponent<TransformComponent>();
  component.Position.x = value;
  component.IsChanged = true;
}

void Internal_TransformComponent_SetPositionY(entt::entity entityId, Scene* scene, float value) {
  Entity entity{(entt::entity) entityId, (Scene*) scene};
  auto& component = entity.GetComponent<TransformComponent>();
  component.Position.y = value;
  component.IsChanged = true;
}

void Internal_TransformComponent_SetPositionZ(entt::entity entityId, Scene* scene, float value) {
  Entity entity{(entt::entity) entityId, (Scene*) scene};
  auto& component = entity.GetComponent<TransformComponent>();
  component.Position.z = value;
  component.IsChanged = true;
}

ScriptManager* ScriptManager::m_ScriptManager = nullptr;

ScriptManager::ScriptManager() {
  LOG_INFO("Compiling internal scripts...");
  std::vector<std::string> files = {
      "assets/scripts/Internal.cs",
      "assets/scripts/MonoBehavior.cs",
      "assets/scripts/Input.cs",
      "assets/scripts/math/Vector3.cs",
      "assets/scripts/components/TransformComponent.cs"
  };
  CompileToDLL("obj/engine/WieselMono.dll", files);

  LOG_INFO("Initializing mono...");
  mono_set_dirs("mono/lib", "mono/etc");
  mono_config_parse("mono/etc/mono/config");

  mono_jit_init("Wiesel");

  m_RootDomain = mono_domain_get();
  mono_domain_set(m_RootDomain, false);
  mono_jit_thread_attach(m_RootDomain);

  m_EngineDomain = mono_domain_create_appdomain("Engine", nullptr);
  mono_domain_set(m_EngineDomain, false);
  mono_jit_thread_attach(m_EngineDomain);
  MonoAssembly* assembly = mono_domain_assembly_open(m_EngineDomain, "obj/engine/WieselMono.dll");
  if (!assembly) {
    return;
  }

  m_EngineImage = mono_assembly_get_image(assembly);
  m_MonoBehaviorClass = mono_class_from_name(m_EngineImage, "WieselEngine", "MonoBehavior");
  m_MonoTransformComponentClass = mono_class_from_name(m_EngineImage, "WieselEngine", "TransformComponent");

  mono_domain_set(m_RootDomain, false);
  mono_add_internal_call("WieselEngine.EngineInternal::LogInfo", reinterpret_cast<void*>(Internal_LogInfo));
  mono_add_internal_call("WieselEngine.EngineInternal::GetAxis", reinterpret_cast<void*>(Internal_GetAxis));
  mono_add_internal_call("WieselEngine.EngineInternal::GetComponent", reinterpret_cast<void*>(Internal_GetComponent));
  mono_add_internal_call("WieselEngine.TransformComponent::GetPositionX", reinterpret_cast<void*>(Internal_TransformComponent_GetPositionX));
  mono_add_internal_call("WieselEngine.TransformComponent::GetPositionY", reinterpret_cast<void*>(Internal_TransformComponent_GetPositionY));
  mono_add_internal_call("WieselEngine.TransformComponent::GetPositionZ", reinterpret_cast<void*>(Internal_TransformComponent_GetPositionZ));
  mono_add_internal_call("WieselEngine.TransformComponent::SetPositionX", reinterpret_cast<void*>(Internal_TransformComponent_SetPositionX));
  mono_add_internal_call("WieselEngine.TransformComponent::SetPositionY", reinterpret_cast<void*>(Internal_TransformComponent_SetPositionY));
  mono_add_internal_call("WieselEngine.TransformComponent::SetPositionZ", reinterpret_cast<void*>(Internal_TransformComponent_SetPositionZ));

  m_ComponentGetters.insert(
      std::pair("TransformComponent",
                [this](Entity& entity) -> MonoObject* {
                  // todo add macro for this
                  MonoObject* obj = mono_object_new(mono_domain_get(), m_MonoTransformComponentClass);
                  void* args[2];
                  /* Note we put the address of the value type in the args array */
                  uint32_t entityId = (uint32_t) entity;
                  args[0] = &entityId;
                  uint64_t ptr = (uint64_t) entity.GetScene();
                  args[1] = &ptr;

                  MonoMethod* method = mono_class_get_method_from_name(m_MonoTransformComponentClass, ".ctor", 2);
                  mono_runtime_invoke(method, obj, args, nullptr);
                  return obj;
                }));

}

ScriptManager::~ScriptManager() {
  LOG_INFO("Cleaning up script manager...");
  mono_domain_set(m_RootDomain, false);
  mono_domain_unload(m_EngineDomain);
}

void ScriptManager::Compile(const std::string& outputFile, const std::vector<std::string>& inputFiles) {
  CompileToDLL(outputFile, inputFiles, "obj/engine", {"WieselMono.dll"});
}

MonoObject* ScriptManager::GetComponentByName(Entity& entity, const std::string& name) {
  return m_ComponentGetters[name](entity);
}

void ScriptManager::Init() {
  m_ScriptManager = new ScriptManager();
}

void ScriptManager::Destroy() {
  delete m_ScriptManager;
  m_ScriptManager = nullptr;
}

}