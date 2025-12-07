
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
  T Get(MonoObject* instance) const {
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
             std::unordered_map<std::string, FieldData> fields) : mono_class_(klass),
        on_update_method_(onUpdateMethod),
        on_start_method_(onStartMethod),
        set_handle_method_(setHandleMethod),
        on_key_pressed_method_(keyPressedMethod),
        on_key_released_method_(keyReleasedMethod),
        on_mouse_moved_method_(mouseMovedMethod),
        fields_(fields) {}

  MonoClass* mono_class() const { return mono_class_; }
  MonoMethod* on_update_method() const { return on_update_method_; }
  MonoMethod* on_start_method() const { return on_start_method_; }
  MonoMethod* set_handle_method() const { return set_handle_method_; }
  MonoMethod* on_key_pressed_method() const { return on_key_pressed_method_; }
  MonoMethod* on_key_released_method() const { return on_key_released_method_; }
  MonoMethod* on_mouse_moved_method() const { return on_mouse_moved_method_; }
  std::unordered_map<std::string, FieldData>& fields() { return fields_; }

 private:
  MonoClass* mono_class_;
  MonoMethod* on_update_method_;
  MonoMethod* on_start_method_;
  MonoMethod* set_handle_method_;
  MonoMethod* on_key_pressed_method_;
  MonoMethod* on_key_released_method_;
  MonoMethod* on_mouse_moved_method_;

  std::unordered_map<std::string, FieldData> fields_;
};


class ScriptInstance {
 public:
  ScriptInstance(std::shared_ptr<ScriptData> data, MonoBehavior* behavior);
  ~ScriptInstance();

  MonoObject* handle() const { return handle_; }
  MonoBehavior* behavior() const { return behavior_; }
  ScriptData& script_data() const { return *script_data_; }

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
  MonoObject* handle_;
  MonoBehavior* behavior_;
  std::shared_ptr<ScriptData> script_data_;
  uint32_t gc_handle_;
  std::map<std::string, std::function<MonoObject*()>> attached_variables_;
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

  static MonoDomain* root_domain() { return root_domain_; }
  static MonoDomain* app_domain() { return app_domain_; }
  static MonoClass* vector3f_class() { return vector3f_class_; }
  static MonoClass* behavior_class() { return behavior_class_; }
  static const std::vector<std::string>& script_names() { return script_names_; }

  static MonoObject* GetComponentByName(Scene* scene, entt::entity entity, const std::string& name);
  template<class T>
  static MonoObject* GetComponent(Scene* scene, entt::entity entity);
  static bool HasComponentByName(Scene* scene, entt::entity entity, const std::string& name);
  static std::unique_ptr<ScriptInstance> CreateScriptInstance(MonoBehavior* behavior);

  template<class T>
  static void RegisterComponent(std::string name, ComponentGetter getter, ComponentChecker checker);
 private:
  static MonoDomain* root_domain_;
  static MonoAssembly* core_assembly_;
  static MonoImage* core_assembly_image_;
  static MonoDomain* app_domain_;
  static MonoAssembly* app_assembly_;
  static MonoImage* app_assembly_image_;

  static MonoClass* behavior_class_;
  static MonoClass* transform_component_class_;
  static MonoClass* vector3f_class_;
  static MonoMethod* set_handle_method_;
  static std::map<std::string, ComponentGetter> component_getters_;
  static std::map<std::type_index, ComponentGetter> component_getters_by_type_;
  static std::map<std::string, ComponentChecker> component_checkers_;
  static std::map<std::string, std::shared_ptr<ScriptData>> script_data_;
  static std::vector<std::string> script_names_;
  static bool enable_debugger_;
};

}