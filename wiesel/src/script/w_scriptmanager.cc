//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "script/w_scriptmanager.h"

#include <direct.h>
#include <imgui.h>
#include "asset/w_asset_manager.h"
#include "audio/w_audio.h"
#include "input/w_input.h"
#include "mono_wrappers.h"
#include "physics/w_collider.h"
#include "physics/w_collision_system.h"
#include "physics/w_physics_world.h"
#include "physics/w_rigidbody.h"
#include "rendering/w_mesh.h"
#include "rendering/w_sprite.h"
#include "scene/w_entity.h"
#include "scene/w_prefab.h"
#include "scene/w_scene.h"
#include "scene/w_scene_manager.h"
#include "script/mono/w_monobehavior.h"
#include "script/w_scriptglue.h"
#include "ui/w_canvas.h"
#include "util/w_logger.h"
#include "util/w_platform.h"
#include "w_engine.h"

namespace Wiesel {

static constexpr const char* kCoreDllPath = "./Core.dll";
static constexpr const char* kAppDllPath = "./App.dll";

MonoObject* InvokeSafe(MonoMethod* method, MonoObject* obj, void** args,
                       bool* had_exception) {
  MonoObject* exception = nullptr;
  MonoObject* result = mono_runtime_invoke(method, obj, args, &exception);
  if (exception) {
    MonoString* exc_str = mono_object_to_string(exception, nullptr);
    if (exc_str) {
      char* cstr = mono_string_to_utf8(exc_str);
      LOG_ERROR("C# Exception: {}", cstr);
      Engine::console().LogError(cstr);
      mono_free(cstr);
    }
    if (had_exception) {
      *had_exception = true;
    }
    return nullptr;
  }
  return result;
}

static std::vector<std::filesystem::path> s_assembly_search_paths;

static MonoAssembly* AssemblyPreloadHook(MonoAssemblyName* aname,
                                         char** /*assemblies_path*/,
                                         void* /*user_data*/) {
  const char* name = mono_assembly_name_get_name(aname);
  if (!name) {
    return nullptr;
  }

  std::string dll_name = std::string(name) + ".dll";

  for (const auto& dir : s_assembly_search_paths) {
    std::filesystem::path candidate = dir / dll_name;
    if (std::filesystem::exists(candidate)) {
      MonoImageOpenStatus status;
      MonoAssembly* assembly =
          mono_assembly_open(candidate.string().c_str(), &status);
      if (assembly && status == MONO_IMAGE_OK) {
        LOG_INFO("Assembly resolved: {} -> {}", name, candidate.string());
        return assembly;
      }
    }
  }

  return nullptr;
}

ScriptInstance::ScriptInstance(std::shared_ptr<ScriptData> data,
                               MonoBehavior* behavior) {
  behavior_ = behavior;
  script_data_ = data;
  handle_ = mono_object_new(Engine::script_manager().app_domain(),
                            data->mono_class());

  // Set entity/scene fields BEFORE calling the constructor, so that
  // GetComponent etc. work if called from the C# constructor.
  uint64_t behaviorPtr = (uint64_t)behavior;
  uint64_t scenePtr = (uint64_t)behavior->scene();
  uint64_t entityId = (uint64_t)behavior->handle();
  MonoClass* baseClass = Engine::script_manager().behavior_class();
  MonoClassField* field =
      mono_class_get_field_from_name(baseClass, "behaviorPtr");
  mono_field_set_value(handle_, field, &behaviorPtr);
  field = mono_class_get_field_from_name(baseClass, "scenePtr");
  mono_field_set_value(handle_, field, &scenePtr);
  field = mono_class_get_field_from_name(baseClass, "entityId");
  mono_field_set_value(handle_, field, &entityId);

  mono_runtime_object_init(handle_);
  gc_handle_ = mono_gchandle_new(handle_, true);
}

ScriptInstance::~ScriptInstance() {
  if (detached_) {
    return;  // Domain was unloaded, nothing to clean up
  }
  if (!errored_) {
    OnDestroy();
  }
  mono_gchandle_free(gc_handle_);
}

void ScriptInstance::Detach() {
  detached_ = true;
}

void ScriptInstance::OnStart() {
  UpdateAttachments();
  if (!script_data_->on_start_method()) {
    return;
  }
  InvokeSafe(script_data_->on_start_method(), handle_, nullptr, &errored_);
  if (errored_) {
    OnDisable();
  }
}

void ScriptInstance::OnUpdate(float_t delta_time) {
  PROFILE_ZONE_SCOPED_N("ScriptInstance::OnUpdate");
  if (errored_) {
    return;
  }

  if (!has_started_) {
    OnStart();
    has_started_ = true;
    if (errored_) {
      return;
    }
  }

  if (!script_data_->on_update_method()) {
    return;
  }
  mono_domain_set(Engine::script_manager().app_domain(), true);
  void* args[1];
  args[0] = &delta_time;
  InvokeSafe(script_data_->on_update_method(), handle_, args, &errored_);
  if (errored_) {
    OnDisable();
  }
}

bool ScriptInstance::OnKeyPressed(KeyPressedEvent& event) {
  if (errored_ || !script_data_->on_key_pressed_method()) {
    return false;
  }
  mono_domain_set(Engine::script_manager().app_domain(), true);
  void* args[2];
  int32_t keyCode = event.GetKeyCode();
  bool repeat = event.IsRepeat();
  args[0] = &keyCode;
  args[1] = &repeat;
  MonoObject* data = InvokeSafe(script_data_->on_key_pressed_method(), handle_,
                                args, &errored_);
  if (!data) {
    return false;
  }
  return *(bool*)mono_object_unbox(data);
}

bool ScriptInstance::OnKeyReleased(KeyReleasedEvent& event) {
  if (errored_ || !script_data_->on_key_released_method()) {
    return false;
  }
  mono_domain_set(Engine::script_manager().app_domain(), true);
  void* args[1];
  int32_t keyCode = event.GetKeyCode();
  args[0] = &keyCode;
  MonoObject* data = InvokeSafe(script_data_->on_key_released_method(), handle_,
                                args, &errored_);
  if (!data) {
    return false;
  }
  return *(bool*)mono_object_unbox(data);
}

bool ScriptInstance::OnMouseMoved(MouseMovedEvent& event) {
  if (errored_ || !script_data_->on_mouse_moved_method()) {
    return false;
  }
  mono_domain_set(Engine::script_manager().app_domain(), true);
  void* args[3];
  float x = event.GetX();
  float y = event.GetY();
  int32_t cursorMode = event.GetCursorMode();
  args[0] = &x;
  args[1] = &y;
  args[2] = &cursorMode;
  MonoObject* data = InvokeSafe(script_data_->on_mouse_moved_method(), handle_,
                                args, &errored_);
  if (!data) {
    return false;
  }
  return *(bool*)mono_object_unbox(data);
}

void ScriptInstance::OnTriggerEnter(entt::entity other) {
  if (errored_ || !script_data_->on_trigger_enter_method()) {
    return;
  }
  mono_domain_set(Engine::script_manager().app_domain(), true);
  void* args[1];
  uint64_t otherId = (uint64_t)other;
  args[0] = &otherId;
  InvokeSafe(script_data_->on_trigger_enter_method(), handle_, args, &errored_);
}

void ScriptInstance::OnTriggerStay(entt::entity other) {
  if (errored_ || !script_data_->on_trigger_stay_method()) {
    return;
  }
  mono_domain_set(Engine::script_manager().app_domain(), true);
  void* args[1];
  uint64_t otherId = (uint64_t)other;
  args[0] = &otherId;
  InvokeSafe(script_data_->on_trigger_stay_method(), handle_, args, &errored_);
}

void ScriptInstance::OnTriggerExit(entt::entity other) {
  if (errored_ || !script_data_->on_trigger_exit_method()) {
    return;
  }
  mono_domain_set(Engine::script_manager().app_domain(), true);
  void* args[1];
  uint64_t otherId = (uint64_t)other;
  args[0] = &otherId;
  InvokeSafe(script_data_->on_trigger_exit_method(), handle_, args, &errored_);
}

void ScriptInstance::OnCollisionEnter(entt::entity other) {
  if (errored_ || !script_data_->on_collision_enter_method()) {
    return;
  }
  mono_domain_set(Engine::script_manager().app_domain(), true);
  void* args[1];
  uint64_t otherId = (uint64_t)other;
  args[0] = &otherId;
  InvokeSafe(script_data_->on_collision_enter_method(), handle_, args,
             &errored_);
}

void ScriptInstance::OnCollisionStay(entt::entity other) {
  if (errored_ || !script_data_->on_collision_stay_method()) {
    return;
  }
  mono_domain_set(Engine::script_manager().app_domain(), true);
  void* args[1];
  uint64_t otherId = (uint64_t)other;
  args[0] = &otherId;
  InvokeSafe(script_data_->on_collision_stay_method(), handle_, args,
             &errored_);
}

void ScriptInstance::OnCollisionExit(entt::entity other) {
  if (errored_ || !script_data_->on_collision_exit_method()) {
    return;
  }
  mono_domain_set(Engine::script_manager().app_domain(), true);
  void* args[1];
  uint64_t otherId = (uint64_t)other;
  args[0] = &otherId;
  InvokeSafe(script_data_->on_collision_exit_method(), handle_, args,
             &errored_);
}

void ScriptInstance::OnDisable() {
  if (!script_data_->on_disable_method()) {
    return;
  }
  // Don't pass errored_ - OnDisable is a best-effort cleanup,
  // we don't want a failure here to block OnDestroy.
  InvokeSafe(script_data_->on_disable_method(), handle_, nullptr);
}

void ScriptInstance::OnDestroy() {
  if (!script_data_->on_destroy_method()) {
    return;
  }
  InvokeSafe(script_data_->on_destroy_method(), handle_, nullptr);
}

bool ScriptInstance::OnPointerClick(float x, float y) {
  if (errored_ || !script_data_->on_pointer_click_method()) {
    return false;
  }
  void* args[2] = {&x, &y};
  MonoObject* result = InvokeSafe(script_data_->on_pointer_click_method(),
                                  handle_, args, &errored_);
  if (!result) {
    return false;
  }
  return *(bool*)mono_object_unbox(result);
}

bool ScriptInstance::OnPointerDown(float x, float y) {
  if (errored_ || !script_data_->on_pointer_down_method()) {
    return false;
  }
  void* args[2] = {&x, &y};
  MonoObject* result = InvokeSafe(script_data_->on_pointer_down_method(),
                                  handle_, args, &errored_);
  if (!result) {
    return false;
  }
  return *(bool*)mono_object_unbox(result);
}

bool ScriptInstance::OnPointerUp(float x, float y) {
  if (errored_ || !script_data_->on_pointer_up_method()) {
    return false;
  }
  void* args[2] = {&x, &y};
  MonoObject* result = InvokeSafe(script_data_->on_pointer_up_method(), handle_,
                                  args, &errored_);
  if (!result) {
    return false;
  }
  return *(bool*)mono_object_unbox(result);
}

void ScriptInstance::OnPointerEnter() {
  if (errored_ || !script_data_->on_pointer_enter_method()) {
    return;
  }
  InvokeSafe(script_data_->on_pointer_enter_method(), handle_, nullptr,
             &errored_);
}

void ScriptInstance::OnPointerExit() {
  if (errored_ || !script_data_->on_pointer_exit_method()) {
    return;
  }
  InvokeSafe(script_data_->on_pointer_exit_method(), handle_, nullptr,
             &errored_);
}

void ScriptInstance::OnSelect() {
  if (errored_ || !script_data_->on_select_method()) {
    return;
  }
  InvokeSafe(script_data_->on_select_method(), handle_, nullptr, &errored_);
}

void ScriptInstance::OnDeselect() {
  if (errored_ || !script_data_->on_deselect_method()) {
    return;
  }
  InvokeSafe(script_data_->on_deselect_method(), handle_, nullptr, &errored_);
}

bool ScriptInstance::OnSubmit() {
  if (errored_ || !script_data_->on_submit_method()) {
    return false;
  }
  MonoObject* result =
      InvokeSafe(script_data_->on_submit_method(), handle_, nullptr, &errored_);
  if (!result) {
    return false;
  }
  return *(bool*)mono_object_unbox(result);
}

bool ScriptInstance::OnCancel() {
  if (errored_ || !script_data_->on_cancel_method()) {
    return false;
  }
  MonoObject* result =
      InvokeSafe(script_data_->on_cancel_method(), handle_, nullptr, &errored_);
  if (!result) {
    return false;
  }
  return *(bool*)mono_object_unbox(result);
}

// explicitly instantiate needed types, this is required:
template void ScriptInstance::AttachExternComponent<TransformComponent>(
    std::string, entt::entity);

template <class T>
void ScriptInstance::AttachExternComponent(std::string variable,
                                           entt::entity entity) {
  Scene* scene = behavior_->scene();
  attached_variables_.insert(std::pair(variable, [scene, entity]() {
    return Engine::script_manager().GetComponent<T>(scene, entity);
  }));

  if (has_started_) {
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

ScriptManager::~ScriptManager() {
  Destroy();
}

MonoObject* ScriptManager::GetComponentByName(Scene* scene, entt::entity entity,
                                              const std::string& name) {
  if (!scene || entity == entt::null) {
    return nullptr;
  }
  // Check if the component exists first
  auto checker_it = component_checkers_.find(name);
  if (checker_it != component_checkers_.end() && checker_it->second) {
    if (!checker_it->second(scene, entity)) {
      return nullptr;  // returns null to C#
    }
  }

  auto& fn = component_getters_[name];
  if (fn == nullptr) {
    return nullptr;
  }
  return fn(scene, entity);
}

template <class T>
MonoObject* ScriptManager::GetComponent(Wiesel::Scene* scene,
                                        entt::entity entity) {
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

  // Set up assembly search paths for automatic DLL resolution
  std::filesystem::path exe_dir = GetExecutableDirectory();
  s_assembly_search_paths.clear();
  s_assembly_search_paths.push_back(exe_dir);
  s_assembly_search_paths.push_back(std::filesystem::current_path());

  LOG_INFO("Assembly search paths:");
  for (const auto& path : s_assembly_search_paths) {
    LOG_INFO("  - {}", path.string());
  }

  mono_install_assembly_preload_hook(AssemblyPreloadHook, nullptr);

  if (enable_debugger_) {
    const char* opt[] = {
        "--debugger-agent=transport=dt_socket,address=0.0.0.0:50000,server=y,"
        "suspend=n"};
    mono_jit_parse_options(1, reinterpret_cast<char**>(&opt));
    mono_debug_init(MONO_DEBUG_FORMAT_MONO);
  }

  root_domain_ = mono_jit_init("WieselJITRuntime");
}

void ScriptManager::Destroy() {
  LOG_INFO("Cleaning up script manager...");
  // mono_domain_set(m_RootDomain, true);
  //mono_domain_unload(m_EngineDomain);
  //mono_domain_free(m_EngineDomain, true);
  //mono_jit_cleanup(m_RootDomain);
}

// --- Helper: collect .cs source files (physical paths for compilation) ---
static std::vector<std::string> CollectCsFiles(const std::string& vfs_prefix) {
  std::vector<std::string> files;
  std::vector<std::string> vfs_files =
      Engine::vfs()->ListFiles(vfs_prefix, true);
  for (const std::string& vfs_path : vfs_files) {
    std::filesystem::path rel(vfs_path);
    if (rel.extension() != ".cs") {
      continue;
    }
    // Compilation needs physical paths
    std::optional<std::filesystem::path> physical =
        Engine::vfs()->GetPhysicalPath(vfs_path);
    if (physical.has_value()) {
      files.push_back(physical->string());
    }
  }
  return files;
}

// --- Helper: collect DLLs in working directory (excluding a specific one) ---
static std::vector<std::string> CollectLinkLibs(
    const std::string& exclude_name) {
  std::vector<std::string> libs;
  for (const auto& entry : std::filesystem::directory_iterator(".")) {
    if (entry.is_regular_file() && entry.path().extension() == ".dll" &&
        entry.path().filename() != exclude_name) {
      libs.push_back(entry.path().string());
    }
  }
  return libs;
}

// --- Helper: register .cs files as script assets ---
static void RegisterScriptAssets(const std::string& vfs_prefix) {
  std::vector<std::string> files = Engine::vfs()->ListFiles(vfs_prefix, true);
  for (const std::string& vfs_path : files) {
    std::filesystem::path rel_path(vfs_path);
    if (rel_path.extension() == ".cs") {
      std::string name = rel_path.stem().string();
      Engine::asset_manager().Register(name, AssetType::Script, vfs_path);
    }
  }
}

void ScriptManager::Reload() {
  PROFILE_ZONE_SCOPED_N("ScriptManager::Reload");
  auto core_sources = CollectCsFiles("engine://scripts");
  auto app_sources = CollectCsFiles("app://");
  auto link_libs = CollectLinkLibs("App.dll");
  bool debug = enable_debugger_;

  // Compile Core if needed
  if (!core_sources.empty() && !std::filesystem::exists(kCoreDllPath)) {
    LOG_INFO("Compiling core ({} files)...", core_sources.size());
    DotNetProject core("Core");
    core.SetOutputPath(kCoreDllPath);
    core.SetSources(core_sources);
    core.SetGenerateDocs(true);
    last_compile_result_ = core.Build(debug);
    if (!last_compile_result_.success) {
      LOG_ERROR("Core compilation failed:\n{}", last_compile_result_.output);
      return;
    }
  }

  // Compile App
  if (!app_sources.empty()) {
    LOG_INFO("Compiling app ({} files)...", app_sources.size());
    DotNetProject app("App");
    app.SetOutputPath(kAppDllPath);
    app.SetSources(app_sources);
    app.SetReferences(link_libs);
    last_compile_result_ = app.Build(debug);
    if (!last_compile_result_.success) {
      LOG_ERROR("App compilation failed:\n{}", last_compile_result_.output);
      return;
    }
  }

  SwapDomain();
}

void ScriptManager::ReloadAsync(bool force_recompile_core) {
  if (compiling_) {
    return;
  }

  std::vector<std::string> core_sources = CollectCsFiles("engine://scripts");
  std::vector<std::string> app_sources = CollectCsFiles("app://");
  bool need_core =
      force_recompile_core || !std::filesystem::exists(kCoreDllPath);

  if (app_sources.empty() && !need_core) {
    return;
  }

  LOG_INFO("Compiling scripts (async)...");
  bool debug = enable_debugger_;
  compiling_ = true;

  std::thread([this, core_sources, app_sources, need_core, debug]() {
    CompileResult result{true, 0, "", ""};

    if (need_core && !core_sources.empty()) {
      LOG_INFO("Compiling core ({} files)...", core_sources.size());
      DotNetProject core("Core");
      core.SetOutputPath(kCoreDllPath);
      core.SetSources(core_sources);
      core.SetGenerateDocs(true);
      result = core.Build(debug);
    }

    if (result.success && !app_sources.empty()) {
      std::vector<std::string> link_libs = CollectLinkLibs("App.dll");
      LOG_INFO("Compiling app ({} files)...", app_sources.size());
      DotNetProject app("App");
      app.SetOutputPath(kAppDllPath);
      app.SetSources(app_sources);
      app.SetReferences(link_libs);
      result = app.Build(debug);
    }

    Engine::app().SubmitToMainThread([this, result]() {
      last_compile_result_ = result;
      compiling_ = false;

      if (!result.success) {
        LOG_ERROR("Compilation failed (exit code {}):\n{}", result.exit_code,
                  result.output);
        return;
      }

      SwapDomain();
    });
  }).detach();
}

void ScriptManager::SwapDomain() {
  PROFILE_ZONE_SCOPED_N("ScriptManager::SwapDomain");

  // Unregister old assets
  AssetManager& mgr = Engine::asset_manager();
  for (AssetHandle handle : mgr.GetAllOfType(AssetType::Script)) {
    mgr.Unregister(handle);
  }

  // Unload old domain
  mono_domain_set(root_domain_, true);
  if (app_domain_) {
    PROFILE_ZONE_SCOPED_N("mono_domain_unload");
    mono_domain_unload(app_domain_);
    app_domain_ = nullptr;
  }

  script_data_.clear();
  script_names_.clear();

  RegisterComponents();
  RegisterScriptGlue();

  // Load Core.dll (already compiled)
  LoadCoreDll();

  // Register script assets
  RegisterScriptAssets("engine://scripts/");
  RegisterScriptAssets("app://");

  // Load App.dll (already compiled)
  if (std::filesystem::exists(kAppDllPath)) {
    LoadAppDll(kAppDllPath);
  }

  LOG_INFO("Scripts loaded ({} scripts)", script_names_.size());
  ScriptsReloadedEvent event{};
  Engine::BroadcastEvent(event);
}

void ScriptManager::LoadCoreDll() {
  PROFILE_ZONE_SCOPED_N("ScriptManager::LoadCoreDll");
  std::string dll_path = kCoreDllPath;

  if (!std::filesystem::exists(dll_path)) {
    LOG_ERROR("Core.dll not found");
    return;
  }

  core_assembly_ = mono_domain_assembly_open(root_domain_, dll_path.c_str());
  if (!core_assembly_) {
    LOG_ERROR("Failed to load Core.dll assembly from '{}'", dll_path);
    return;
  }

  core_assembly_image_ = mono_assembly_get_image(core_assembly_);
  behavior_class_ = mono_class_from_name(core_assembly_image_, "WieselEngine",
                                         "MonoBehavior");
  set_handle_method_ =
      mono_class_get_method_from_name(behavior_class_, "SetHandle", 1);

  // Component classes
  transform_component_class_ = mono_class_from_name(
      core_assembly_image_, "WieselEngine", "TransformComponent");
  model_component_class_ = mono_class_from_name(
      core_assembly_image_, "WieselEngine", "ModelComponent");
  box_collider_class_ = mono_class_from_name(
      core_assembly_image_, "WieselEngine", "BoxColliderComponent");
  sphere_collider_class_ = mono_class_from_name(
      core_assembly_image_, "WieselEngine", "SphereColliderComponent");
  rigidbody_class_ = mono_class_from_name(core_assembly_image_, "WieselEngine",
                                          "RigidBodyComponent");
  rect_transform_class_ = mono_class_from_name(
      core_assembly_image_, "WieselEngine", "RectTransformComponent");
  canvas_component_class_ = mono_class_from_name(
      core_assembly_image_, "WieselEngine", "CanvasComponent");
  canvas_rect_class_ = mono_class_from_name(
      core_assembly_image_, "WieselEngine", "CanvasRectComponent");
  canvas_image_class_ = mono_class_from_name(
      core_assembly_image_, "WieselEngine", "CanvasImageComponent");
  text_component_class_ = mono_class_from_name(core_assembly_image_,
                                               "WieselEngine", "TextComponent");
  animator_component_class_ = mono_class_from_name(
      core_assembly_image_, "WieselEngine", "AnimatorComponent");
  audio_source_class_ = mono_class_from_name(
      core_assembly_image_, "WieselEngine", "AudioSourceComponent");
  sprite_renderer_class_ = mono_class_from_name(
      core_assembly_image_, "WieselEngine", "SpriteRendererComponent");
  sprite_animator_class_ = mono_class_from_name(
      core_assembly_image_, "WieselEngine", "SpriteAnimatorComponent");
  camera_class_ = mono_class_from_name(core_assembly_image_, "WieselEngine",
                                       "CameraComponent");
  light_direct_class_ = mono_class_from_name(
      core_assembly_image_, "WieselEngine", "LightDirectComponent");
  light_point_class_ = mono_class_from_name(
      core_assembly_image_, "WieselEngine", "LightPointComponent");
  vector3f_class_ =
      mono_class_from_name(core_assembly_image_, "WieselEngine", "Vector3f");
  entity_class_ =
      mono_class_from_name(core_assembly_image_, "WieselEngine", "Entity");
  prefab_class_ =
      mono_class_from_name(core_assembly_image_, "WieselEngine", "Prefab");
  audio_clip_class_ =
      mono_class_from_name(core_assembly_image_, "WieselEngine", "AudioClip");
}

bool ScriptManager::LoadAppDll(const std::string& dll_path) {
  PROFILE_ZONE_SCOPED_N("ScriptManager::LoadAppDll");
  app_domain_ =
      mono_domain_create_appdomain(const_cast<char*>("WieselApp"), nullptr);
  mono_domain_set(app_domain_, true);
  LOG_INFO("Loading App.dll from {}", dll_path);
  app_assembly_ = mono_domain_assembly_open(app_domain_, dll_path.c_str());
  if (!app_assembly_) {
    LOG_ERROR("Failed to load App.dll: {}", dll_path);
    return false;
  }

  app_assembly_image_ = mono_assembly_get_image(app_assembly_);

  const MonoTableInfo* table_info =
      mono_image_get_table_info(app_assembly_image_, MONO_TABLE_TYPEDEF);
  int rows = mono_table_info_get_rows(table_info);

  for (int i = 0; i < rows; i++) {
    uint32_t cols[MONO_TYPEDEF_SIZE];
    mono_metadata_decode_row(table_info, i, cols, MONO_TYPEDEF_SIZE);
    std::string class_name =
        mono_metadata_string_heap(app_assembly_image_, cols[MONO_TYPEDEF_NAME]);
    if (class_name == "<Module>") {
      continue;
    }

    std::string class_namespace = mono_metadata_string_heap(
        app_assembly_image_, cols[MONO_TYPEDEF_NAMESPACE]);
    MonoClass* klass = mono_class_from_name(
        app_assembly_image_, class_namespace.c_str(), class_name.c_str());
    if (!klass) {
      continue;
    }

    // Only register classes that inherit from MonoBehavior
    if (!mono_class_is_subclass_of(klass, behavior_class_, false)) {
      continue;
    }

    std::unordered_map<std::string, FieldData> fields;
    MonoClassField* field;
    void* iter = nullptr;
    while ((field = mono_class_get_fields(klass, &iter))) {
      std::string field_name = mono_field_get_name(field);
      uint32_t field_flags = mono_field_get_flags(field);
      // Mono field access mask: 0x0007 isolates the access level bits.
      // 0x0006 = public. Only expose public fields to the inspector.
      if ((field_flags & 0x0007) != 0x0006) {
        continue;
      }
      fields.insert(
          std::pair(field_name, FieldData(field, field_name, field_flags)));
    }

    MonoMethod* on_start = mono_class_get_method_from_name(klass, "OnStart", 0);
    MonoMethod* on_update =
        mono_class_get_method_from_name(klass, "OnUpdate", 1);
    MonoMethod* on_key_pressed =
        mono_class_get_method_from_name(klass, "OnKeyPressed", 2);
    MonoMethod* on_key_released =
        mono_class_get_method_from_name(klass, "OnKeyReleased", 1);
    MonoMethod* on_mouse_moved =
        mono_class_get_method_from_name(klass, "OnMouseMoved", 3);
    MonoMethod* on_trigger_enter =
        mono_class_get_method_from_name(klass, "OnTriggerEnter", 1);
    MonoMethod* on_trigger_stay =
        mono_class_get_method_from_name(klass, "OnTriggerStay", 1);
    MonoMethod* on_trigger_exit =
        mono_class_get_method_from_name(klass, "OnTriggerExit", 1);
    MonoMethod* on_collision_enter =
        mono_class_get_method_from_name(klass, "OnCollisionEnter", 1);
    MonoMethod* on_collision_stay =
        mono_class_get_method_from_name(klass, "OnCollisionStay", 1);
    MonoMethod* on_collision_exit =
        mono_class_get_method_from_name(klass, "OnCollisionExit", 1);
    MonoMethod* on_disable =
        mono_class_get_method_from_name(klass, "OnDisable", 0);
    MonoMethod* on_destroy =
        mono_class_get_method_from_name(klass, "OnDestroy", 0);
    MonoMethod* on_ptr_click =
        mono_class_get_method_from_name(klass, "OnPointerClick", 2);
    MonoMethod* on_ptr_down =
        mono_class_get_method_from_name(klass, "OnPointerDown", 2);
    MonoMethod* on_ptr_up =
        mono_class_get_method_from_name(klass, "OnPointerUp", 2);
    MonoMethod* on_ptr_enter =
        mono_class_get_method_from_name(klass, "OnPointerEnter", 0);
    MonoMethod* on_ptr_exit =
        mono_class_get_method_from_name(klass, "OnPointerExit", 0);
    MonoMethod* on_select =
        mono_class_get_method_from_name(klass, "OnSelect", 0);
    MonoMethod* on_deselect =
        mono_class_get_method_from_name(klass, "OnDeselect", 0);
    MonoMethod* on_submit =
        mono_class_get_method_from_name(klass, "OnSubmit", 0);
    MonoMethod* on_cancel =
        mono_class_get_method_from_name(klass, "OnCancel", 0);

    script_data_.insert(std::pair(
        class_name,
        std::make_shared<ScriptData>(
            klass, on_start, on_update, set_handle_method_, on_key_pressed,
            on_key_released, on_mouse_moved, on_trigger_enter, on_trigger_stay,
            on_trigger_exit, on_collision_enter, on_collision_stay,
            on_collision_exit, on_disable, on_destroy, on_ptr_click,
            on_ptr_down, on_ptr_up, on_ptr_enter, on_ptr_exit, on_select,
            on_deselect, on_submit, on_cancel, fields)));
    script_names_.push_back(class_name);
    LOG_INFO("Registered script: {}", class_name);
  }
  return true;
}

void ScriptManager::RegisterComponents() {
  PROFILE_ZONE_SCOPED_N("ScriptManager::RegisterComponents");
  component_getters_.clear();
  component_checkers_.clear();
  component_adders_.clear();
  component_removers_.clear();

  RegisterComponent<TransformComponent>(
      "TransformComponent",
      [this](Scene* scene, entt::entity entity) -> MonoObject* {
        MonoObject* obj =
            mono_object_new(app_domain_, transform_component_class_);
        void* args[2];
        uint64_t scenePtr = (uint64_t)scene;
        uint64_t entityId = (uint64_t)entity;
        args[0] = &scenePtr;
        args[1] = &entityId;

        MonoMethod* method = mono_class_get_method_from_name(
            transform_component_class_, ".ctor", 2);
        InvokeSafe(method, obj, args);
        return obj;
      },
      [](Scene* scene, entt::entity entity) -> bool {
        return scene->HasComponent<TransformComponent>(entity);
      });

  RegisterComponent<ModelComponent>(
      "ModelComponent",
      [this](Scene* scene, entt::entity entity) -> MonoObject* {
        MonoObject* obj = mono_object_new(app_domain_, model_component_class_);
        void* args[2];
        uint64_t scenePtr = (uint64_t)scene;
        uint64_t entityId = (uint64_t)entity;
        args[0] = &scenePtr;
        args[1] = &entityId;

        MonoMethod* method =
            mono_class_get_method_from_name(model_component_class_, ".ctor", 2);
        InvokeSafe(method, obj, args);
        return obj;
      },
      [](Scene* scene, entt::entity entity) -> bool {
        return scene->HasComponent<ModelComponent>(entity);
      });

  RegisterComponent<BoxColliderComponent>(
      "BoxColliderComponent",
      [this](Scene* scene, entt::entity entity) -> MonoObject* {
        MonoObject* obj = mono_object_new(app_domain_, box_collider_class_);
        void* args[2];
        uint64_t scenePtr = (uint64_t)scene;
        uint64_t entityId = (uint64_t)entity;
        args[0] = &scenePtr;
        args[1] = &entityId;

        MonoMethod* method =
            mono_class_get_method_from_name(box_collider_class_, ".ctor", 2);
        InvokeSafe(method, obj, args);
        return obj;
      },
      [](Scene* scene, entt::entity entity) -> bool {
        return scene->HasComponent<BoxColliderComponent>(entity);
      });

  RegisterComponent<SphereColliderComponent>(
      "SphereColliderComponent",
      [this](Scene* scene, entt::entity entity) -> MonoObject* {
        MonoObject* obj = mono_object_new(app_domain_, sphere_collider_class_);
        void* args[2];
        uint64_t scenePtr = (uint64_t)scene;
        uint64_t entityId = (uint64_t)entity;
        args[0] = &scenePtr;
        args[1] = &entityId;

        MonoMethod* method =
            mono_class_get_method_from_name(sphere_collider_class_, ".ctor", 2);
        InvokeSafe(method, obj, args);
        return obj;
      },
      [](Scene* scene, entt::entity entity) -> bool {
        return scene->HasComponent<SphereColliderComponent>(entity);
      });

  RegisterComponent<RigidBodyComponent>(
      "RigidBodyComponent",
      [this](Scene* scene, entt::entity entity) -> MonoObject* {
        MonoObject* obj = mono_object_new(app_domain_, rigidbody_class_);
        void* args[2];
        uint64_t scenePtr = (uint64_t)scene;
        uint64_t entityId = (uint64_t)entity;
        args[0] = &scenePtr;
        args[1] = &entityId;

        MonoMethod* method =
            mono_class_get_method_from_name(rigidbody_class_, ".ctor", 2);
        InvokeSafe(method, obj, args);
        return obj;
      },
      [](Scene* scene, entt::entity entity) -> bool {
        return scene->HasComponent<RigidBodyComponent>(entity);
      });

  RegisterComponent<RectangleTransformComponent>(
      "RectTransformComponent",
      [this](Scene* scene, entt::entity entity) -> MonoObject* {
        MonoObject* obj = mono_object_new(app_domain_, rect_transform_class_);
        void* args[2];
        uint64_t scenePtr = (uint64_t)scene;
        uint64_t entityId = (uint64_t)entity;
        args[0] = &scenePtr;
        args[1] = &entityId;
        MonoMethod* method =
            mono_class_get_method_from_name(rect_transform_class_, ".ctor", 2);
        InvokeSafe(method, obj, args);
        return obj;
      },
      [](Scene* scene, entt::entity entity) -> bool {
        return scene->HasComponent<RectangleTransformComponent>(entity);
      });

  RegisterComponent<CanvasComponent>(
      "CanvasComponent",
      [this](Scene* scene, entt::entity entity) -> MonoObject* {
        MonoObject* obj = mono_object_new(app_domain_, canvas_component_class_);
        void* args[2];
        uint64_t scenePtr = (uint64_t)scene;
        uint64_t entityId = (uint64_t)entity;
        args[0] = &scenePtr;
        args[1] = &entityId;
        MonoMethod* method = mono_class_get_method_from_name(
            canvas_component_class_, ".ctor", 2);
        InvokeSafe(method, obj, args);
        return obj;
      },
      [](Scene* scene, entt::entity entity) -> bool {
        return scene->HasComponent<CanvasComponent>(entity);
      });

  RegisterComponent<CanvasRectComponent>(
      "CanvasRectComponent",
      [this](Scene* scene, entt::entity entity) -> MonoObject* {
        MonoObject* obj = mono_object_new(app_domain_, canvas_rect_class_);
        void* args[2];
        uint64_t scenePtr = (uint64_t)scene;
        uint64_t entityId = (uint64_t)entity;
        args[0] = &scenePtr;
        args[1] = &entityId;
        MonoMethod* method =
            mono_class_get_method_from_name(canvas_rect_class_, ".ctor", 2);
        InvokeSafe(method, obj, args);
        return obj;
      },
      [](Scene* scene, entt::entity entity) -> bool {
        return scene->HasComponent<CanvasRectComponent>(entity);
      });

  RegisterComponent<CanvasImageComponent>(
      "CanvasImageComponent",
      [this](Scene* scene, entt::entity entity) -> MonoObject* {
        MonoObject* obj = mono_object_new(app_domain_, canvas_image_class_);
        void* args[2];
        uint64_t scenePtr = (uint64_t)scene;
        uint64_t entityId = (uint64_t)entity;
        args[0] = &scenePtr;
        args[1] = &entityId;
        MonoMethod* method =
            mono_class_get_method_from_name(canvas_image_class_, ".ctor", 2);
        InvokeSafe(method, obj, args);
        return obj;
      },
      [](Scene* scene, entt::entity entity) -> bool {
        return scene->HasComponent<CanvasImageComponent>(entity);
      });

  RegisterComponent<TextComponent>(
      "TextComponent",
      [this](Scene* scene, entt::entity entity) -> MonoObject* {
        MonoObject* obj = mono_object_new(app_domain_, text_component_class_);
        void* args[2];
        uint64_t scenePtr = (uint64_t)scene;
        uint64_t entityId = (uint64_t)entity;
        args[0] = &scenePtr;
        args[1] = &entityId;
        MonoMethod* method =
            mono_class_get_method_from_name(text_component_class_, ".ctor", 2);
        InvokeSafe(method, obj, args);
        return obj;
      },
      [](Scene* scene, entt::entity entity) -> bool {
        return scene->HasComponent<TextComponent>(entity);
      });

  if (animator_component_class_) {
    RegisterComponent<AnimatorComponent>(
        "AnimatorComponent",
        [this](Scene* scene, entt::entity entity) -> MonoObject* {
          MonoObject* obj =
              mono_object_new(app_domain_, animator_component_class_);
          void* args[2];
          uint64_t scenePtr = (uint64_t)scene;
          uint64_t entityId = (uint64_t)entity;
          args[0] = &scenePtr;
          args[1] = &entityId;
          MonoMethod* method = mono_class_get_method_from_name(
              animator_component_class_, ".ctor", 2);
          InvokeSafe(method, obj, args);
          return obj;
        },
        [](Scene* scene, entt::entity entity) -> bool {
          return scene->HasComponent<AnimatorComponent>(entity);
        });
  }

  if (audio_source_class_) {
    RegisterComponent<AudioSourceComponent>(
        "AudioSourceComponent",
        [this](Scene* scene, entt::entity entity) -> MonoObject* {
          MonoObject* obj = mono_object_new(app_domain_, audio_source_class_);
          void* args[2];
          uint64_t scenePtr = (uint64_t)scene;
          uint64_t entityId = (uint64_t)entity;
          args[0] = &scenePtr;
          args[1] = &entityId;
          MonoMethod* method =
              mono_class_get_method_from_name(audio_source_class_, ".ctor", 2);
          InvokeSafe(method, obj, args);
          return obj;
        },
        [](Scene* scene, entt::entity entity) -> bool {
          return scene->HasComponent<AudioSourceComponent>(entity);
        });
  }

  if (camera_class_) {
    RegisterComponent<CameraComponent>(
        "CameraComponent",
        [this](Scene* scene, entt::entity entity) -> MonoObject* {
          MonoObject* obj = mono_object_new(app_domain_, camera_class_);
          void* args[2];
          uint64_t scenePtr = (uint64_t)scene;
          uint64_t entityId = (uint64_t)entity;
          args[0] = &scenePtr;
          args[1] = &entityId;
          MonoMethod* method =
              mono_class_get_method_from_name(camera_class_, ".ctor", 2);
          InvokeSafe(method, obj, args);
          return obj;
        },
        [](Scene* scene, entt::entity entity) -> bool {
          return scene->HasComponent<CameraComponent>(entity);
        });
  }

  if (light_direct_class_) {
    RegisterComponent<LightDirectComponent>(
        "LightDirectComponent",
        [this](Scene* scene, entt::entity entity) -> MonoObject* {
          MonoObject* obj = mono_object_new(app_domain_, light_direct_class_);
          void* args[2];
          uint64_t scenePtr = (uint64_t)scene;
          uint64_t entityId = (uint64_t)entity;
          args[0] = &scenePtr;
          args[1] = &entityId;
          MonoMethod* method =
              mono_class_get_method_from_name(light_direct_class_, ".ctor", 2);
          InvokeSafe(method, obj, args);
          return obj;
        },
        [](Scene* scene, entt::entity entity) -> bool {
          return scene->HasComponent<LightDirectComponent>(entity);
        });
  }

  if (light_point_class_) {
    RegisterComponent<LightPointComponent>(
        "LightPointComponent",
        [this](Scene* scene, entt::entity entity) -> MonoObject* {
          MonoObject* obj = mono_object_new(app_domain_, light_point_class_);
          void* args[2];
          uint64_t scenePtr = (uint64_t)scene;
          uint64_t entityId = (uint64_t)entity;
          args[0] = &scenePtr;
          args[1] = &entityId;
          MonoMethod* method =
              mono_class_get_method_from_name(light_point_class_, ".ctor", 2);
          InvokeSafe(method, obj, args);
          return obj;
        },
        [](Scene* scene, entt::entity entity) -> bool {
          return scene->HasComponent<LightPointComponent>(entity);
        });
  }

  if (sprite_renderer_class_) {
    RegisterComponent<SpriteRendererComponent>(
        "SpriteRendererComponent",
        [this](Scene* scene, entt::entity entity) -> MonoObject* {
          MonoObject* obj =
              mono_object_new(app_domain_, sprite_renderer_class_);
          void* args[2];
          uint64_t scenePtr = (uint64_t)scene;
          uint64_t entityId = (uint64_t)entity;
          args[0] = &scenePtr;
          args[1] = &entityId;
          MonoMethod* method = mono_class_get_method_from_name(
              sprite_renderer_class_, ".ctor", 2);
          InvokeSafe(method, obj, args);
          return obj;
        },
        [](Scene* scene, entt::entity entity) -> bool {
          return scene->HasComponent<SpriteRendererComponent>(entity);
        });
  }

  if (sprite_animator_class_) {
    RegisterComponent<SpriteAnimatorComponent>(
        "SpriteAnimatorComponent",
        [this](Scene* scene, entt::entity entity) -> MonoObject* {
          MonoObject* obj =
              mono_object_new(app_domain_, sprite_animator_class_);
          void* args[2];
          uint64_t scenePtr = (uint64_t)scene;
          uint64_t entityId = (uint64_t)entity;
          args[0] = &scenePtr;
          args[1] = &entityId;
          MonoMethod* method = mono_class_get_method_from_name(
              sprite_animator_class_, ".ctor", 2);
          InvokeSafe(method, obj, args);
          return obj;
        },
        [](Scene* scene, entt::entity entity) -> bool {
          return scene->HasComponent<SpriteAnimatorComponent>(entity);
        });
  }
}

std::unique_ptr<ScriptInstance> ScriptManager::CreateScriptInstance(
    MonoBehavior* behavior) {
  if (!script_data_.contains(behavior->GetName())) {
    return nullptr;
  }
  std::shared_ptr<ScriptData> data = script_data_[behavior->GetName()];
  return std::make_unique<ScriptInstance>(data, behavior);
}

template <class T>
void ScriptManager::RegisterComponent(std::string name, ComponentGetter getter,
                                      ComponentChecker checker,
                                      ComponentAdder adder,
                                      ComponentRemover remover) {
  component_getters_.insert(std::pair(name, getter));
  component_getters_by_type_.insert(
      std::pair(std::type_index(typeid(T)), getter));
  component_checkers_.insert(std::pair(name, checker));

  // Auto-generate adder/remover from template type if not provided
  if (!adder) {
    adder = [](Scene* scene, entt::entity entity) {
      if (!scene->HasComponent<T>(entity)) {
        scene->GetRegistry().emplace<T>(entity);
      }
    };
  } else {
    component_adders_.insert(std::pair(name, adder));
  }
  if (!remover) {
    remover = [](Scene* scene, entt::entity entity) {
      if (scene->HasComponent<T>(entity)) {
        scene->GetRegistry().remove<T>(entity);
      }
    };
  } else {
    component_removers_.insert(std::pair(name, remover));
  }
}

void ScriptManager::AddComponentByName(Scene* scene, entt::entity entity,
                                       const std::string& name) {
  auto it = component_adders_.find(name);
  if (it != component_adders_.end() && it->second) {
    it->second(scene, entity);
  }
}

void ScriptManager::RemoveComponentByName(Scene* scene, entt::entity entity,
                                          const std::string& name) {
  auto it = component_removers_.find(name);
  if (it != component_removers_.end() && it->second) {
    it->second(scene, entity);
  }
}
}  // namespace Wiesel