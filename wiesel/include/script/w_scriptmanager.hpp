
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
#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/class.h>
#include "scene/w_scene.hpp"

namespace Wiesel {

class MonoBehavior;

class ScriptData {
 public:
  ScriptData(MonoClass* klass, MonoMethod* onStartMethod,
             MonoMethod* onUpdateMethod,
             MonoMethod* setHandleMethod) : m_Klass(klass),
        m_OnUpdateMethod(onUpdateMethod),
        m_OnStartMethod(onStartMethod),
        m_SetHandleMethod(setHandleMethod) {}

  MonoClass* GetKlass() const { return m_Klass; }
  MonoMethod* GetOnUpdateMethod() const { return m_OnUpdateMethod; }
  MonoMethod* GetOnStartMethod() const { return m_OnStartMethod; }
  MonoMethod* GetSetHandleMethod() const { return m_SetHandleMethod; }

 private:
  MonoClass* m_Klass;
  MonoMethod* m_OnUpdateMethod;
  MonoMethod* m_OnStartMethod;
  MonoMethod* m_SetHandleMethod;
};


class ScriptInstance {
 public:
  ScriptInstance(ScriptData* data, MonoBehavior* behavior);
  ~ScriptInstance();

  MonoObject* GetInstance() const { return m_Instance; }
  MonoBehavior* GetBehavior() const { return m_Behavior; }
  ScriptData* GetScriptData() const { return m_ScriptData; }

  void OnStart();
  void OnUpdate(float_t deltaTime);
 private:
  MonoObject* m_Instance;
  MonoBehavior* m_Behavior;
  ScriptData* m_ScriptData;
};

class ScriptManager {
 public:
  using ComponentGetter = std::function<MonoObject*(MonoBehavior*)>;

  static void Init();
  static void Destroy();
  static void Reload();

  static void LoadCore();
  static void LoadApp();
  static void RegisterInternals();
  static void RegisterComponents();

  static MonoDomain* GetRootDomain() { return m_RootDomain; }
  static MonoDomain* GetAppDomain() { return m_AppDomain; }

  static MonoObject* GetComponentByName(MonoBehavior* entity, const std::string& name);
  static ScriptInstance* CreateScriptInstance(MonoBehavior* behavior);
 private:
  static void Compile(const std::string& outputFile,
               const std::vector<std::string>& inputFiles);

  static MonoDomain* m_RootDomain;
  static MonoAssembly* m_CoreAssembly;
  static MonoImage* m_CoreAssemblyImage;
  static MonoDomain* m_AppDomain;
  static MonoAssembly* m_AppAssembly;
  static MonoImage* m_AppAssemblyImage;

  static MonoClass* m_MonoBehaviorClass;
  static MonoClass* m_MonoTransformComponentClass;
  static MonoMethod* m_SetHandleMethod;
  static std::map<std::string, ComponentGetter> m_ComponentGetters;
  static std::map<std::string, ScriptData*> m_ScriptData;
};

}