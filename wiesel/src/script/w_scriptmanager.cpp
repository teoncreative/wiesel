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

#include <direct.h>
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

#define WIESEL_ADD_INTERNAL_CALL(name)                     \
  mono_add_internal_call("WieselEngine.Internals::" #name, \
                         reinterpret_cast<void*>(Internals_##name))

// todo move these bindings to script glue
void Internals_Log_Info(MonoString* str) {
  const char* cstr = mono_string_to_utf8(str);
  LOG_INFO("{}", cstr);
  mono_free((void*)cstr);
}

float Internals_Input_GetAxis(MonoString* str) {
  const char* cstr = mono_string_to_utf8(str);
  float value = InputManager::GetAxis(cstr);
  mono_free((void*)cstr);
  return value;
}

bool Internals_Input_GetKey(MonoString* str) {
  const char* cstr = mono_string_to_utf8(str);
  bool value = InputManager::GetKey(cstr);
  mono_free((void*)cstr);
  return value;
}

void Internals_Input_SetCursorMode(uint16_t mode) {
  Engine::GetWindow()->SetCursorMode((CursorMode)mode);
}

uint16_t Internals_Input_GetCursorMode() {
  uint16_t cursorMode = Engine::GetWindow()->GetCursorMode();
  return cursorMode;
}

MonoObject* Internals_Behavior_GetComponent(Scene* scene, entt::entity entity,
                                            MonoString* str) {
  const char* cstr = mono_string_to_utf8(str);
  MonoObject* component =
      ScriptManager::GetComponentByName(scene, entity, cstr);
  mono_free((void*)cstr);
  return component;
}

bool Internals_Behavior_HasComponent(Scene* scene, entt::entity entity,
                                     MonoString* str) {
  const char* cstr = mono_string_to_utf8(str);
  bool hasComponent = ScriptManager::HasComponentByName(scene, entity, cstr);
  mono_free((void*)cstr);
  return hasComponent;
}

float Internals_TransformComponent_GetPositionX(Scene* scene,
                                                entt::entity entity) {
  return scene->GetComponent<TransformComponent>(entity).position.x;
}

void Internals_TransformComponent_SetPositionX(Scene* scene,
                                               entt::entity entity,
                                               float value) {
  auto& c = scene->GetComponent<TransformComponent>(entity);
  if (c.position.x == value) {
    return;
  }
  c.position.x = value;
  c.is_changed = true;
}

float Internals_TransformComponent_GetPositionY(Scene* scene,
                                                entt::entity entity) {
  return scene->GetComponent<TransformComponent>(entity).position.y;
}

void Internals_TransformComponent_SetPositionY(Scene* scene,
                                               entt::entity entity,
                                               float value) {
  auto& c = scene->GetComponent<TransformComponent>(entity);
  if (c.position.y == value) {
    return;
  }
  c.position.y = value;
  c.is_changed = true;
}

float Internals_TransformComponent_GetPositionZ(Scene* scene,
                                                entt::entity entity) {
  return scene->GetComponent<TransformComponent>(entity).position.z;
}

void Internals_TransformComponent_SetPositionZ(Scene* scene,
                                               entt::entity entity,
                                               float value) {
  auto& c = scene->GetComponent<TransformComponent>(entity);
  if (c.position.z == value) {
    return;
  }
  c.position.z = value;
  c.is_changed = true;
}

float Internals_TransformComponent_GetRotationX(Scene* scene,
                                                entt::entity entity) {
  return scene->GetComponent<TransformComponent>(entity).rotation.x;
}

void Internals_TransformComponent_SetRotationX(Scene* scene,
                                               entt::entity entity,
                                               float value) {
  auto& c = scene->GetComponent<TransformComponent>(entity);
  if (c.rotation.x == value) {
    return;
  }
  c.rotation.x = value;
  c.is_changed = true;
}

float Internals_TransformComponent_GetRotationY(Scene* scene,
                                                entt::entity entity) {
  return scene->GetComponent<TransformComponent>(entity).rotation.y;
}

void Internals_TransformComponent_SetRotationY(Scene* scene,
                                               entt::entity entity,
                                               float value) {
  auto& c = scene->GetComponent<TransformComponent>(entity);
  if (c.rotation.y == value) {
    return;
  }
  c.rotation.y = value;
  c.is_changed = true;
}

float Internals_TransformComponent_GetRotationZ(Scene* scene,
                                                entt::entity entity) {
  return scene->GetComponent<TransformComponent>(entity).rotation.z;
}

void Internals_TransformComponent_SetRotationZ(Scene* scene,
                                               entt::entity entity,
                                               float value) {
  auto& c = scene->GetComponent<TransformComponent>(entity);
  if (c.rotation.z == value) {
    return;
  }
  c.rotation.z = value;
  c.is_changed = true;
}

float Internals_TransformComponent_GetScaleX(Scene* scene,
                                             entt::entity entity) {
  return scene->GetComponent<TransformComponent>(entity).scale.x;
}

void Internals_TransformComponent_SetScaleX(Scene* scene, entt::entity entity,
                                            float value) {
  auto& c = scene->GetComponent<TransformComponent>(entity);
  if (c.scale.x == value) {
    return;
  }
  c.scale.x = value;
  c.is_changed = true;
}

float Internals_TransformComponent_GetScaleY(Scene* scene,
                                             entt::entity entity) {
  return scene->GetComponent<TransformComponent>(entity).scale.y;
}

void Internals_TransformComponent_SetScaleY(Scene* scene, entt::entity entity,
                                            float value) {
  auto& c = scene->GetComponent<TransformComponent>(entity);
  if (c.scale.y == value) {
    return;
  }
  c.scale.y = value;
  c.is_changed = true;
}

float Internals_TransformComponent_GetScaleZ(Scene* scene,
                                             entt::entity entity) {
  return scene->GetComponent<TransformComponent>(entity).scale.z;
}

void Internals_TransformComponent_SetScaleZ(Scene* scene, entt::entity entity,
                                            float value) {
  auto& c = scene->GetComponent<TransformComponent>(entity);
  if (c.scale.z == value) {
    return;
  }
  c.scale.z = value;
  c.is_changed = true;
}

MonoObject* CreateVector3fWithValues(float x, float y, float z) {
  MonoObject* obj = mono_object_new(ScriptManager::app_domain(),
                                    ScriptManager::vector3f_class());
  void* args[3];
  args[0] = &x;
  args[1] = &y;
  args[2] = &z;
  MonoMethod* method = mono_class_get_method_from_name(
      ScriptManager::vector3f_class(), ".ctor", 3);
  mono_runtime_invoke(method, obj, args, nullptr);
  return obj;
}

MonoObject* Internals_TransformComponent_GetForward(Scene* scene,
                                                    entt::entity entity) {
  auto& c = scene->GetComponent<TransformComponent>(entity);
  glm::vec3 val = c.GetForward();
  return CreateVector3fWithValues(val.x, val.y, val.z);
}

MonoObject* Internals_TransformComponent_GetBackward(Scene* scene,
                                                     entt::entity entity) {
  auto& c = scene->GetComponent<TransformComponent>(entity);
  glm::vec3 val = c.GetBackward();
  return CreateVector3fWithValues(val.x, val.y, val.z);
}

MonoObject* Internals_TransformComponent_GetLeft(Scene* scene,
                                                 entt::entity entity) {
  auto& c = scene->GetComponent<TransformComponent>(entity);
  glm::vec3 val = c.GetLeft();
  return CreateVector3fWithValues(val.x, val.y, val.z);
}

MonoObject* Internals_TransformComponent_GetRight(Scene* scene,
                                                  entt::entity entity) {
  auto& c = scene->GetComponent<TransformComponent>(entity);
  glm::vec3 val = c.GetRight();
  return CreateVector3fWithValues(val.x, val.y, val.z);
}

MonoObject* Internals_TransformComponent_GetUp(Scene* scene,
                                               entt::entity entity) {
  auto& c = scene->GetComponent<TransformComponent>(entity);
  glm::vec3 val = c.GetUp();
  return CreateVector3fWithValues(val.x, val.y, val.z);
}

MonoObject* Internals_TransformComponent_GetDown(Scene* scene,
                                                 entt::entity entity) {
  auto& c = scene->GetComponent<TransformComponent>(entity);
  glm::vec3 val = c.GetDown();
  return CreateVector3fWithValues(val.x, val.y, val.z);
}

ScriptInstance::ScriptInstance(std::shared_ptr<ScriptData> data, MonoBehavior* behavior) {
  behavior_ = behavior;
  script_data_ = data;
  handle_ = mono_object_new(ScriptManager::app_domain(), data->mono_class());
  mono_runtime_object_init(handle_);

  uint64_t behaviorPtr = (uint64_t)behavior;
  uint64_t scenePtr = (uint64_t)behavior->scene();
  uint64_t entityId = (uint64_t)behavior->handle();
  MonoClass* baseClass = ScriptManager::behavior_class();
  MonoClassField* field =
      mono_class_get_field_from_name(baseClass, "behaviorPtr");
  mono_field_set_value(handle_, field, &behaviorPtr);
  field = mono_class_get_field_from_name(baseClass, "scenePtr");
  mono_field_set_value(handle_, field, &scenePtr);
  field = mono_class_get_field_from_name(baseClass, "entityId");
  mono_field_set_value(handle_, field, &entityId);
  gc_handle_ = mono_gchandle_new(handle_, true);
}

ScriptInstance::~ScriptInstance() {
  mono_gchandle_free(gc_handle_);
}

void ScriptInstance::OnStart() {
  UpdateAttachments();
  mono_runtime_invoke(script_data_->on_start_method(), handle_, nullptr,
                      nullptr);
}

void ScriptInstance::OnUpdate(float_t deltaTime) {
  if (!m_StartRan) {
    OnStart();
    m_StartRan = true;
  }

  mono_domain_set(ScriptManager::app_domain(), true);
  void* args[1];
  args[0] = &deltaTime;
  mono_runtime_invoke(script_data_->on_update_method(), handle_, args,
                      nullptr);
}

bool ScriptInstance::OnKeyPressed(KeyPressedEvent& event) {
  if (!script_data_->on_key_pressed_method()) {
    return false;
  }
  mono_domain_set(ScriptManager::app_domain(), true);
  void* args[2];
  int32_t keyCode = event.GetKeyCode();
  bool repeat = event.IsRepeat();
  args[0] = &keyCode;
  args[1] = &repeat;
  MonoObject* data = mono_runtime_invoke(script_data_->on_key_pressed_method(),
                                         handle_, args, nullptr);
  bool value = *(bool*)mono_object_unbox(data);
  return value;
}

bool ScriptInstance::OnKeyReleased(KeyReleasedEvent& event) {
  if (!script_data_->on_key_released_method()) {
    return false;
  }
  mono_domain_set(ScriptManager::app_domain(), true);
  void* args[1];
  int32_t keyCode = event.GetKeyCode();
  args[0] = &keyCode;
  MonoObject* data = mono_runtime_invoke(script_data_->on_key_released_method(),
                                         handle_, args, nullptr);
  bool value = *(bool*)mono_object_unbox(data);
  return value;
}

bool ScriptInstance::OnMouseMoved(MouseMovedEvent& event) {
  if (!script_data_->on_mouse_moved_method()) {
    return false;
  }
  mono_domain_set(ScriptManager::app_domain(), true);
  void* args[3];
  float x = event.GetX();
  float y = event.GetY();
  int32_t cursorMode = event.GetCursorMode();
  args[0] = &x;
  args[1] = &y;
  args[2] = &cursorMode;
  MonoObject* data = mono_runtime_invoke(script_data_->on_mouse_moved_method(),
                                         handle_, args, nullptr);
  bool value = *(bool*)mono_object_unbox(data);
  return value;
}

// explicitly instantiate needed types, this is required:
template void ScriptInstance::AttachExternComponent<TransformComponent>(std::string, entt::entity);

template <class T>
void ScriptInstance::AttachExternComponent(std::string variable,
                                           entt::entity entity) {
  Scene* scene = behavior_->scene();
  attached_variables_.insert(std::pair(variable, [scene, entity]() {
    return ScriptManager::GetComponent<T>(scene, entity);
  }));

  if (m_StartRan) {
    UpdateAttachments();
  }
}

void ScriptInstance::UpdateAttachments() {
  for (const auto& item : attached_variables_) {
    MonoObject* object = item.second();
    if (!object) {
      continue;
    }
    MonoClassField* field = mono_class_get_field_from_name(
        script_data_->mono_class(), item.first.c_str());
    if (!field) {
      continue;
    }
    mono_field_set_value(handle_, field, object);
  }
}

MonoDomain* ScriptManager::root_domain_ = nullptr;
MonoAssembly* ScriptManager::core_assembly_ = nullptr;
MonoImage* ScriptManager::core_assembly_image_ = nullptr;
MonoDomain* ScriptManager::app_domain_ = nullptr;
MonoAssembly* ScriptManager::app_assembly_ = nullptr;
MonoImage* ScriptManager::app_assembly_image_ = nullptr;
MonoClass* ScriptManager::behavior_class_ = nullptr;
MonoClass* ScriptManager::transform_component_class_ = nullptr;
MonoClass* ScriptManager::vector3f_class_ = nullptr;
MonoMethod* ScriptManager::set_handle_method_ = nullptr;

std::map<std::string, ScriptManager::ComponentGetter>
    ScriptManager::component_getters_;
std::map<std::type_index, ScriptManager::ComponentGetter>
    ScriptManager::component_getters_by_type_;
std::map<std::string, ScriptManager::ComponentChecker>
    ScriptManager::component_checkers_;
std::map<std::string, std::shared_ptr<ScriptData>> ScriptManager::script_data_;
std::vector<std::string> ScriptManager::script_names_;
bool ScriptManager::enable_debugger_;

MonoObject* ScriptManager::GetComponentByName(Scene* scene, entt::entity entity,
                                              const std::string& name) {
  auto& fn = component_getters_[name];
  if (fn == nullptr) {
    return nullptr;
  }
  return fn(scene, entity);
}

template <class T>
MonoObject* ScriptManager::GetComponent(Wiesel::Scene* scene, entt::entity entity) {
  auto& fn = component_getters_by_type_[std::type_index(typeid(T))];
  if (fn == nullptr) {
    return nullptr;
  }
  return fn(scene, entity);
}

bool ScriptManager::HasComponentByName(Scene* scene, entt::entity entity,
                                       const std::string& name) {
  auto& fn = component_checkers_[name];
  if (fn == nullptr) {
    return false;
  }
  return fn(scene, entity);
}

void ScriptManager::Init(const ScriptManagerProperties&& props) {
  enable_debugger_ = props.EnableDebugger;
  LOG_INFO("Initializing mono...");

  mono_set_dirs("mono/lib", "mono/etc");
  mono_config_parse("mono/etc/mono/config");

  if (enable_debugger_) {
    const char* opt[] = {
        "--debugger-agent=transport=dt_socket,address=0.0.0.0:50000,server=y,suspend=n"
    };
    mono_jit_parse_options(1, reinterpret_cast<char**>(&opt));
    mono_debug_init(MONO_DEBUG_FORMAT_MONO);
  }

  root_domain_ = mono_jit_init("WieselJITRuntime");

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

  mono_domain_set(root_domain_, true);
  mono_domain_unload(app_domain_);

  script_data_.clear();
  script_names_.clear();

  RegisterComponents();
  RegisterInternals();
  LoadCore();
  LoadApp();

  ScriptsReloadedEvent event{};
  Application::Get()->OnEvent(event);
}

void ScriptManager::LoadCore() {
  LOG_INFO("Compiling core scripts...");
  std::vector<std::string> sourceFiles;
  for (const auto& entry : std::filesystem::recursive_directory_iterator(
           "assets/internal_scripts")) {
    if (entry.is_regular_file() && entry.path().extension() == ".cs") {
      std::string name = entry.path().string();
      LOG_INFO("Found internal script {}", name);
      sourceFiles.push_back(name);
    }
  }
  CompileToDLL("obj/Core.dll", sourceFiles, "", {}, enable_debugger_);

  core_assembly_ = mono_domain_assembly_open(root_domain_, "obj/Core.dll");
  assert(core_assembly_);

  core_assembly_image_ = mono_assembly_get_image(core_assembly_);
  behavior_class_ =
      mono_class_from_name(core_assembly_image_, "WieselEngine", "MonoBehavior");
  set_handle_method_ =
      mono_class_get_method_from_name(behavior_class_, "SetHandle", 1);

  // Component classes
  transform_component_class_ = mono_class_from_name(
      core_assembly_image_, "WieselEngine", "TransformComponent");
  vector3f_class_ =
      mono_class_from_name(core_assembly_image_, "WieselEngine", "Vector3f");
}

void ScriptManager::LoadApp() {
  // todo load files from project file when project system is added
  std::vector<std::string> sourceFiles;
  for (const auto& entry :
       std::filesystem::recursive_directory_iterator("assets/scripts")) {
    if (entry.is_regular_file() && entry.path().extension() == ".cs") {
      std::string name = entry.path().string();
      LOG_INFO("Found user script {}", name);
      sourceFiles.push_back(name);
    }
  }
  std::vector<std::string> linkLibs;
  for (const auto& entry :
       std::filesystem::recursive_directory_iterator("obj")) {
    if (entry.is_regular_file() && entry.path().extension() == ".dll") {
      std::string name = entry.path().string();
      linkLibs.push_back(name);
      LOG_INFO("Found DLL to link {}", name);
    }
  }
  if (!CompileToDLL("obj/App.dll", sourceFiles, "obj", linkLibs, enable_debugger_)) {
    return;
  }
  app_domain_ = mono_domain_create_appdomain(const_cast<char*>("WieselApp"), nullptr);
  mono_domain_set(app_domain_, true);
  //mono_domain_assembly_open(m_AppDomain, "obj/Core.dll");
  app_assembly_ = mono_domain_assembly_open(app_domain_, "obj/App.dll");
  assert(app_assembly_);

  app_assembly_image_ = mono_assembly_get_image(app_assembly_);

  const MonoTableInfo* tableInfo =
      mono_image_get_table_info(app_assembly_image_, MONO_TABLE_TYPEDEF);
  int rows = mono_table_info_get_rows(tableInfo);

  for (int i = 0; i < rows; i++) {
    uint32_t cols[MONO_TYPEDEF_SIZE];
    mono_metadata_decode_row(tableInfo, i, cols, MONO_TYPEDEF_SIZE);
    std::string className =
        mono_metadata_string_heap(app_assembly_image_, cols[MONO_TYPEDEF_NAME]);
    if (className == "<Module>") {
      continue;
    }
    std::string classNamespace = mono_metadata_string_heap(
        app_assembly_image_, cols[MONO_TYPEDEF_NAMESPACE]);
    // this is needed to load the class, facepalm Microsoft
    mono_class_from_name(app_assembly_image_, classNamespace.c_str(),
                         className.c_str());

    LOG_INFO("Found class {} in namespace {}", className, classNamespace);
    MonoClass* klass = mono_class_from_name(
        app_assembly_image_, classNamespace.c_str(), className.c_str());
    if (!klass) {
      LOG_ERROR("Class {} in namespace {} not found!", className,
                classNamespace);
      continue;
    }
    std::unordered_map<std::string, FieldData> fields;
    MonoClassField* field;
    void* iter = nullptr;
    while ((field = mono_class_get_fields(klass, &iter))) {
      std::string fieldName = mono_field_get_name(field);
      uint32_t fieldFlags = mono_field_get_flags(field);
      // fieldFlags & FIELD_ATTRIBUTE_FIELD_ACCESS_MASK == FIELD_ATTRIBUTE_PUBLIC
      if ((fieldFlags & 0x0007) != 0x0006) {
        continue;
      }
      //LOG_INFO("Public field: {}, flags {}", fieldName, fieldFlags);
      fields.insert(
          std::pair(fieldName, FieldData(field, fieldName, fieldFlags)));
    }
    MonoMethod* onStartMethod =
        mono_class_get_method_from_name(klass, "OnStart", 0);
    MonoMethod* onUpdateMethod =
        mono_class_get_method_from_name(klass, "OnUpdate", 1);
    MonoMethod* onKeyPressedMethod = mono_class_get_method_from_name(
        klass, "OnKeyPressed", 2);  // KeyCode, bool isRepeat
    MonoMethod* onKeyReleasedMethod =
        mono_class_get_method_from_name(klass, "OnKeyReleased", 1);  // KeyCode
    MonoMethod* onMouseMovedMethod = mono_class_get_method_from_name(
        klass, "OnMouseMoved", 3);  // x, y, cursorMode
    script_data_.insert(std::pair(
        className,
        std::make_shared<ScriptData>(klass, onStartMethod, onUpdateMethod, set_handle_method_,
                       onKeyPressedMethod, onKeyReleasedMethod,
                       onMouseMovedMethod, fields)));
    script_names_.push_back(className);
  }
}

void ScriptManager::RegisterInternals() {
  WIESEL_ADD_INTERNAL_CALL(Log_Info);
  WIESEL_ADD_INTERNAL_CALL(Input_GetAxis);
  WIESEL_ADD_INTERNAL_CALL(Input_GetKey);
  WIESEL_ADD_INTERNAL_CALL(Input_SetCursorMode);
  WIESEL_ADD_INTERNAL_CALL(Input_GetCursorMode);
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
  component_getters_.clear();
  component_checkers_.clear();

  RegisterComponent<TransformComponent>(
      "TransformComponent",
      [](Scene* scene, entt::entity entity) -> MonoObject* {
        // todo add macro for this
        MonoObject* obj =
            mono_object_new(app_domain_, transform_component_class_);
        void* args[2];
        uint64_t scenePtr = (uint64_t)scene;
        uint64_t entityId = (uint64_t)entity;
        args[0] = &scenePtr;
        args[1] = &entityId;

        MonoMethod* method = mono_class_get_method_from_name(
            transform_component_class_, ".ctor", 2);
        mono_runtime_invoke(method, obj, args, nullptr);
        return obj;
      },
      [](Scene* scene, entt::entity entity) -> bool {
        return scene->HasComponent<TransformComponent>(entity);
      });
}

std::unique_ptr<ScriptInstance> ScriptManager::CreateScriptInstance(MonoBehavior* behavior) {
  if (!script_data_.contains(behavior->GetName())) {
    return nullptr;
  }
  std::shared_ptr<ScriptData> data = script_data_[behavior->GetName()];
  return std::make_unique<ScriptInstance>(data, behavior);
}

template <class T>
void ScriptManager::RegisterComponent(std::string name, ComponentGetter getter,
                                      ComponentChecker checker) {
  component_getters_.insert(std::pair(name, getter));
  component_getters_by_type_.insert(std::pair(std::type_index(typeid(T)), getter));
  component_checkers_.insert(std::pair(name, checker));
}
}  // namespace Wiesel