
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

class ScriptManager {
 public:
  ScriptManager();
  ~ScriptManager();

  void Compile(const std::string& outputFile,
               const std::vector<std::string>& inputFiles);

  MonoClass* GetMonoBehaviorClass() const { return m_MonoBehaviorClass; }
  MonoDomain* GetRootDomain() const { return m_RootDomain; }
  MonoDomain* GetEngineDomain() const { return m_EngineDomain; }

  MonoObject* GetComponentByName(Entity& entity, const std::string& name);

  static void Init();
  static void Destroy();

  static ScriptManager* Get() { return m_ScriptManager; }


 private:
  using ComponentGetter = std::function<MonoObject*(Entity& entity)>;
  static ScriptManager* m_ScriptManager;

  MonoDomain* m_RootDomain;
  MonoDomain* m_EngineDomain;
  MonoImage* m_EngineImage;
  MonoClass* m_MonoBehaviorClass;
  MonoClass* m_MonoTransformComponentClass;
  std::map<std::string, ComponentGetter> m_ComponentGetters;
};

}