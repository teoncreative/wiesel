
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
#include "events/w_keyevents.hpp"
#include "events/w_mouseevents.hpp"
#include "scene/w_entity.hpp"
#include <typeindex>

namespace Wiesel {

class MonoBehavior;

enum class FieldType {
  Boolean,
  Float,
  Double,
  Integer,
  Long,
  UnsignedInteger,
  UnsignedLong,
  String,
  Object
};

class FieldData {
 public:
  FieldData(MonoClassField* field, const std::string& fieldName,
            uint32_t fieldFlags) : m_Field(field),
        m_FieldName(fieldName),
        m_FieldFlags(fieldFlags) {
    std::string typeName = mono_type_get_name(mono_field_get_type(m_Field));
    if (typeName == "System.Boolean") {
      m_FieldType = FieldType::Boolean;
    } else if (typeName == "System.Single") {
      m_FieldType = FieldType::Float;
    } else if (typeName == "System.Double") {
      m_FieldType = FieldType::Double;
    } else if (typeName == "System.Int32") {
      m_FieldType = FieldType::Integer;
    } else if (typeName == "System.Int64") {
      m_FieldType = FieldType::Long;
    } else if (typeName == "System.UInt32") {
      m_FieldType = FieldType::UnsignedInteger;
    } else if (typeName == "System.UInt64") {
      m_FieldType = FieldType::UnsignedLong;
    } else if (typeName == "System.String") {
      m_FieldType = FieldType::String;
    } else {
      m_FieldType = FieldType::Object;
    }
    m_FormattedName = FormatVariableName(m_FieldName);
  }

  template <typename T>
  void Set(MonoObject* instance, T value) {
    mono_field_set_value(instance, m_Field, value);
  }

  template <typename T>
  T Get(MonoObject* instance) {
    T value;
    mono_field_get_value(instance, m_Field, &value);
    return value;
  }

  MonoClassField* GetField() const { return m_Field; }
  const std::string& GetFieldName() const { return m_FieldName; }
  uint32_t GetFieldFlags() const { return m_FieldFlags; }
  FieldType GetFieldType() const { return m_FieldType; }
  const std::string& GetFormattedName() const { return m_FormattedName; }

 private:
  MonoClassField* m_Field;
  std::string m_FieldName;
  std::string m_FormattedName;
  uint32_t m_FieldFlags;
  FieldType m_FieldType;
};

class ScriptData {
 public:
  ScriptData(MonoClass* klass, MonoMethod* onStartMethod,
             MonoMethod* onUpdateMethod,
             MonoMethod* setHandleMethod,
             MonoMethod* keyPressedMethod,
             MonoMethod* keyReleasedMethod,
             MonoMethod* mouseMovedMethod,
             std::unordered_map<std::string, FieldData> fields) : m_Class(klass),
        m_OnUpdateMethod(onUpdateMethod),
        m_OnStartMethod(onStartMethod),
        m_SetHandleMethod(setHandleMethod),
        m_OnKeyPressedMethod(keyPressedMethod),
        m_OnKeyReleasedMethod(keyReleasedMethod),
        m_OnMouseMovedMethod(mouseMovedMethod),
        m_Fields(fields) {}

  MonoClass* GetClass() const { return m_Class; }
  MonoMethod* GetOnUpdateMethod() const { return m_OnUpdateMethod; }
  MonoMethod* GetOnStartMethod() const { return m_OnStartMethod; }
  MonoMethod* GetSetHandleMethod() const { return m_SetHandleMethod; }
  MonoMethod* GetOnKeyPressedMethod() const { return m_OnKeyPressedMethod; }
  MonoMethod* GetOnKeyReleasedMethod() const { return m_OnKeyReleasedMethod; }
  MonoMethod* GetOnMouseMovedMethod() const { return m_OnMouseMovedMethod; }
  std::unordered_map<std::string, FieldData>& GetFields() { return m_Fields; }

 private:
  MonoClass* m_Class;
  MonoMethod* m_OnUpdateMethod;
  MonoMethod* m_OnStartMethod;
  MonoMethod* m_SetHandleMethod;
  MonoMethod* m_OnKeyPressedMethod;
  MonoMethod* m_OnKeyReleasedMethod;
  MonoMethod* m_OnMouseMovedMethod;

  std::unordered_map<std::string, FieldData> m_Fields;
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

  bool OnKeyPressed(KeyPressedEvent& event);
  bool OnKeyReleased(KeyReleasedEvent& event);
  bool OnMouseMoved(MouseMovedEvent& event);

  template<class T>
  void AttachExternComponent(std::string variable, entt::entity entity);

  void UpdateAttachments();

 private:
  friend class MonoBehavior;

  bool m_StartRan = false;
  MonoObject* m_Instance;
  MonoBehavior* m_Behavior;
  ScriptData* m_ScriptData;
  uint32_t m_GCHandle;
  std::map<std::string, std::function<MonoObject*()>> m_AttachedVariables;
};

struct ScriptManagerProperties {
  bool EnableDebugger;
};

class ScriptManager {
 public:
  using ComponentGetter = std::function<MonoObject*(Scene*, entt::entity)>;
  using ComponentChecker = std::function<bool(Scene*, entt::entity)>;

  static void Init(const ScriptManagerProperties&& properties);
  static void Destroy();
  static void Reload();

  static void LoadCore();
  static void LoadApp();
  static void RegisterInternals();
  static void RegisterComponents();

  static MonoDomain* GetRootDomain() { return m_RootDomain; }
  static MonoDomain* GetAppDomain() { return m_AppDomain; }
  static MonoClass* GetVector3fClass() { return m_MonoVector3fClass; }
  static MonoClass* GetMonoBehaviorClass() { return m_MonoBehaviorClass; }
  static const std::vector<std::string> GetScriptNames() { return m_ScriptNames; }

  static MonoObject* GetComponentByName(Scene* scene, entt::entity entity, const std::string& name);
  template<class T>
  static MonoObject* GetComponent(Scene* scene, entt::entity entity);
  static bool HasComponentByName(Scene* scene, entt::entity entity, const std::string& name);
  static ScriptInstance* CreateScriptInstance(MonoBehavior* behavior);

  template<class T>
  static void RegisterComponent(std::string name, ComponentGetter getter, ComponentChecker checker);
 private:
  static MonoDomain* m_RootDomain;
  static MonoAssembly* m_CoreAssembly;
  static MonoImage* m_CoreAssemblyImage;
  static MonoDomain* m_AppDomain;
  static MonoAssembly* m_AppAssembly;
  static MonoImage* m_AppAssemblyImage;

  static MonoClass* m_MonoBehaviorClass;
  static MonoClass* m_MonoTransformComponentClass;
  static MonoClass* m_MonoVector3fClass;
  static MonoMethod* m_SetHandleMethod;
  static std::map<std::string, ComponentGetter> m_ComponentGetters;
  static std::map<std::type_index, ComponentGetter> m_ComponentGettersByType;
  static std::map<std::string, ComponentChecker> m_ComponentCheckers;
  static std::map<std::string, ScriptData*> m_ScriptData;
  static std::vector<std::string> m_ScriptNames;
  static bool m_EnableDebugger;
};

}