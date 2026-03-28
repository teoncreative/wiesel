
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "events/w_keyevents.h"
#include "events/w_mouseevents.h"
#include "scene/w_scene.h"
#include "script/mono/w_mono_util.h"

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
  Entity,
  Prefab,
  AudioClip,
  Object
};

class FieldData {
 public:
  FieldData(MonoClassField* field, const std::string& fieldName,
            uint32_t fieldFlags)
      : field_(field), field_name_(fieldName), field_flags_(fieldFlags) {
    std::string typeName = mono_type_get_name(mono_field_get_type(field_));
    if (typeName == "System.Boolean") {
      field_type_ = FieldType::Boolean;
    } else if (typeName == "System.Single") {
      field_type_ = FieldType::Float;
    } else if (typeName == "System.Double") {
      field_type_ = FieldType::Double;
    } else if (typeName == "System.Int32") {
      field_type_ = FieldType::Integer;
    } else if (typeName == "System.Int64") {
      field_type_ = FieldType::Long;
    } else if (typeName == "System.UInt32") {
      field_type_ = FieldType::UnsignedInteger;
    } else if (typeName == "System.UInt64") {
      field_type_ = FieldType::UnsignedLong;
    } else if (typeName == "System.String") {
      field_type_ = FieldType::String;
    } else if (typeName == "WieselEngine.Entity") {
      field_type_ = FieldType::Entity;
    } else if (typeName == "WieselEngine.Prefab") {
      field_type_ = FieldType::Prefab;
    } else if (typeName == "WieselEngine.AudioClip") {
      field_type_ = FieldType::AudioClip;
    } else {
      field_type_ = FieldType::Object;
      LOG_WARN("Unknown script field type '{}' for field '{}'", typeName,
               fieldName);
    }
    formatted_name_ = FormatVariableName(field_name_);
  }

  template <typename T>
  void Set(MonoObject* instance, T value) {
    mono_field_set_value(instance, field_, value);
  }

  template <typename T>
  T Get(MonoObject* instance) const {
    T value;
    mono_field_get_value(instance, field_, &value);
    return value;
  }

  MonoClassField* field() const { return field_; }

  const std::string& field_name() const { return field_name_; }

  uint32_t field_flags() const { return field_flags_; }

  FieldType field_type() const { return field_type_; }

  const std::string& formatted_name() const { return formatted_name_; }

 private:
  MonoClassField* field_;
  std::string field_name_;
  std::string formatted_name_;
  uint32_t field_flags_;
  FieldType field_type_;
};

class ScriptData {
 public:
  ScriptData(MonoClass* klass, MonoMethod* on_start_method,
             MonoMethod* on_update_method, MonoMethod* set_handle_method,
             MonoMethod* key_pressed_method, MonoMethod* key_released_method,
             MonoMethod* mouse_moved_method, MonoMethod* trigger_enter_method,
             MonoMethod* trigger_stay_method, MonoMethod* trigger_exit_method,
             MonoMethod* collision_enter_method,
             MonoMethod* collision_stay_method,
             MonoMethod* collision_exit_method, MonoMethod* on_disable_method,
             MonoMethod* on_destroy_method, MonoMethod* on_pointer_click_method,
             MonoMethod* on_pointer_down_method,
             MonoMethod* on_pointer_up_method,
             MonoMethod* on_pointer_enter_method,
             MonoMethod* on_pointer_exit_method, MonoMethod* on_select_method,
             MonoMethod* on_deselect_method, MonoMethod* on_submit_method,
             MonoMethod* on_cancel_method,
             std::unordered_map<std::string, FieldData> fields)
      : mono_class_(klass),
        on_update_method_(on_update_method),
        on_start_method_(on_start_method),
        set_handle_method_(set_handle_method),
        on_key_pressed_method_(key_pressed_method),
        on_key_released_method_(key_released_method),
        on_mouse_moved_method_(mouse_moved_method),
        on_trigger_enter_method_(trigger_enter_method),
        on_trigger_stay_method_(trigger_stay_method),
        on_trigger_exit_method_(trigger_exit_method),
        on_collision_enter_method_(collision_enter_method),
        on_collision_stay_method_(collision_stay_method),
        on_collision_exit_method_(collision_exit_method),
        on_disable_method_(on_disable_method),
        on_destroy_method_(on_destroy_method),
        on_pointer_click_method_(on_pointer_click_method),
        on_pointer_down_method_(on_pointer_down_method),
        on_pointer_up_method_(on_pointer_up_method),
        on_pointer_enter_method_(on_pointer_enter_method),
        on_pointer_exit_method_(on_pointer_exit_method),
        on_select_method_(on_select_method),
        on_deselect_method_(on_deselect_method),
        on_submit_method_(on_submit_method),
        on_cancel_method_(on_cancel_method),
        fields_(fields) {}

  MonoClass* mono_class() const { return mono_class_; }

  MonoMethod* on_update_method() const { return on_update_method_; }

  MonoMethod* on_start_method() const { return on_start_method_; }

  MonoMethod* set_handle_method() const { return set_handle_method_; }

  MonoMethod* on_key_pressed_method() const { return on_key_pressed_method_; }

  MonoMethod* on_key_released_method() const { return on_key_released_method_; }

  MonoMethod* on_mouse_moved_method() const { return on_mouse_moved_method_; }

  MonoMethod* on_trigger_enter_method() const {
    return on_trigger_enter_method_;
  }

  MonoMethod* on_trigger_stay_method() const { return on_trigger_stay_method_; }

  MonoMethod* on_trigger_exit_method() const { return on_trigger_exit_method_; }

  MonoMethod* on_collision_enter_method() const {
    return on_collision_enter_method_;
  }

  MonoMethod* on_collision_stay_method() const {
    return on_collision_stay_method_;
  }

  MonoMethod* on_collision_exit_method() const {
    return on_collision_exit_method_;
  }

  MonoMethod* on_disable_method() const { return on_disable_method_; }

  MonoMethod* on_destroy_method() const { return on_destroy_method_; }

  MonoMethod* on_pointer_click_method() const {
    return on_pointer_click_method_;
  }

  MonoMethod* on_pointer_down_method() const { return on_pointer_down_method_; }

  MonoMethod* on_pointer_up_method() const { return on_pointer_up_method_; }

  MonoMethod* on_pointer_enter_method() const {
    return on_pointer_enter_method_;
  }

  MonoMethod* on_pointer_exit_method() const { return on_pointer_exit_method_; }

  MonoMethod* on_select_method() const { return on_select_method_; }

  MonoMethod* on_deselect_method() const { return on_deselect_method_; }

  MonoMethod* on_submit_method() const { return on_submit_method_; }

  MonoMethod* on_cancel_method() const { return on_cancel_method_; }

  std::unordered_map<std::string, FieldData>& fields() { return fields_; }

 private:
  MonoClass* mono_class_;
  MonoMethod* on_update_method_;
  MonoMethod* on_start_method_;
  MonoMethod* set_handle_method_;
  MonoMethod* on_key_pressed_method_;
  MonoMethod* on_key_released_method_;
  MonoMethod* on_mouse_moved_method_;
  MonoMethod* on_trigger_enter_method_;
  MonoMethod* on_trigger_stay_method_;
  MonoMethod* on_trigger_exit_method_;
  MonoMethod* on_collision_enter_method_;
  MonoMethod* on_collision_stay_method_;
  MonoMethod* on_collision_exit_method_;
  MonoMethod* on_disable_method_;
  MonoMethod* on_destroy_method_;
  MonoMethod* on_pointer_click_method_;
  MonoMethod* on_pointer_down_method_;
  MonoMethod* on_pointer_up_method_;
  MonoMethod* on_pointer_enter_method_;
  MonoMethod* on_pointer_exit_method_;
  MonoMethod* on_select_method_;
  MonoMethod* on_deselect_method_;
  MonoMethod* on_submit_method_;
  MonoMethod* on_cancel_method_;

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
  void OnUpdate(float_t delta_time);

  void ResetStartState() {
    has_started_ = false;
    errored_ = false;
  }

  bool HasErrored() const { return errored_; }

  bool OnKeyPressed(KeyPressedEvent& event);
  bool OnKeyReleased(KeyReleasedEvent& event);
  bool OnMouseMoved(MouseMovedEvent& event);

  void OnTriggerEnter(entt::entity other);
  void OnTriggerStay(entt::entity other);
  void OnTriggerExit(entt::entity other);

  void OnCollisionEnter(entt::entity other);
  void OnCollisionStay(entt::entity other);
  void OnCollisionExit(entt::entity other);

  void OnDisable();
  void OnDestroy();

  bool OnPointerClick(float x, float y);
  bool OnPointerDown(float x, float y);
  bool OnPointerUp(float x, float y);
  void OnPointerEnter();
  void OnPointerExit();
  void OnSelect();
  void OnDeselect();
  bool OnSubmit();
  bool OnCancel();

  template <class T>
  void AttachExternComponent(std::string variable, entt::entity entity);

  void UpdateAttachments();

 private:
  friend class MonoBehavior;

  bool has_started_ = false;
  bool errored_ = false;
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
  using ComponentAdder = std::function<void(Scene*, entt::entity)>;
  using ComponentRemover = std::function<void(Scene*, entt::entity)>;

  ScriptManager() = default;
  ~ScriptManager();

  void Init(const ScriptManagerProperties&& properties);
  void Destroy();

  // Synchronous compile + load (blocks until done). Used for initial load.
  void Reload();

  // Async compile on background thread, SwapDomain on main thread when done.
  void ReloadAsync(bool force_recompile_core = false);
  bool FinishReloadIfReady();

  bool IsCompiling() const { return compiling_; }
  const CompileResult& last_compile_result() const {
    return last_compile_result_;
  }

 private:
  // Domain swap: unload old domain, load DLLs, register classes.
  // Must only be called on the main thread after compilation finishes.
  void SwapDomain();
  void LoadCoreDll();
  bool LoadAppDll(const std::string& dll_path);
  void RegisterComponents();

 public:
  MonoDomain* root_domain() { return root_domain_; }

  MonoDomain* app_domain() { return app_domain_; }

  MonoClass* vector3f_class() { return vector3f_class_; }

  MonoClass* entity_class() { return entity_class_; }

  MonoClass* prefab_class() { return prefab_class_; }

  MonoClass* audio_clip_class() { return audio_clip_class_; }

  MonoClass* behavior_class() { return behavior_class_; }

  const std::vector<std::string>& script_names() { return script_names_; }

  MonoObject* GetComponentByName(Scene* scene, entt::entity entity,
                                 const std::string& name);
  template <class T>
  MonoObject* GetComponent(Scene* scene, entt::entity entity);
  bool HasComponentByName(Scene* scene, entt::entity entity,
                          const std::string& name);
  void AddComponentByName(Scene* scene, entt::entity entity,
                          const std::string& name);
  void RemoveComponentByName(Scene* scene, entt::entity entity,
                             const std::string& name);
  std::unique_ptr<ScriptInstance> CreateScriptInstance(MonoBehavior* behavior);

  template <class T>
  void RegisterComponent(std::string name, ComponentGetter getter,
                         ComponentChecker checker,
                         ComponentAdder adder = nullptr,
                         ComponentRemover remover = nullptr);

 private:
  MonoDomain* root_domain_ = nullptr;
  MonoAssembly* core_assembly_ = nullptr;
  MonoImage* core_assembly_image_ = nullptr;
  MonoDomain* app_domain_ = nullptr;
  MonoAssembly* app_assembly_ = nullptr;
  MonoImage* app_assembly_image_ = nullptr;

  MonoClass* behavior_class_ = nullptr;
  MonoClass* transform_component_class_ = nullptr;
  MonoClass* model_component_class_ = nullptr;
  MonoClass* box_collider_class_ = nullptr;
  MonoClass* sphere_collider_class_ = nullptr;
  MonoClass* rigidbody_class_ = nullptr;
  MonoClass* rect_transform_class_ = nullptr;
  MonoClass* canvas_component_class_ = nullptr;
  MonoClass* canvas_rect_class_ = nullptr;
  MonoClass* canvas_image_class_ = nullptr;
  MonoClass* text_component_class_ = nullptr;
  MonoClass* animator_component_class_ = nullptr;
  MonoClass* audio_source_class_ = nullptr;
  MonoClass* sprite_renderer_class_ = nullptr;
  MonoClass* sprite_animator_class_ = nullptr;
  MonoClass* camera_class_ = nullptr;
  MonoClass* light_direct_class_ = nullptr;
  MonoClass* light_point_class_ = nullptr;
  MonoClass* vector3f_class_ = nullptr;
  MonoClass* entity_class_ = nullptr;
  MonoClass* prefab_class_ = nullptr;
  MonoClass* audio_clip_class_ = nullptr;
  MonoMethod* set_handle_method_ = nullptr;
  std::map<std::string, ComponentGetter> component_getters_;
  std::map<std::type_index, ComponentGetter> component_getters_by_type_;
  std::map<std::string, ComponentChecker> component_checkers_;
  std::map<std::string, ComponentAdder> component_adders_;
  std::map<std::string, ComponentRemover> component_removers_;
  std::map<std::string, std::shared_ptr<ScriptData>> script_data_;
  std::vector<std::string> script_names_;
  bool enable_debugger_ = false;

  // Async compilation
  bool compiling_ = false;
  std::future<CompileResult> compile_future_;
  CompileResult last_compile_result_;
};

}  // namespace Wiesel