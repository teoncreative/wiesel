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
#include <mono/metadata/assembly.h>
#include <mono/metadata/debug-helpers.h>
#include <mono/metadata/mono-config.h>
#include <mono/metadata/mono-debug.h>
#include <mono/metadata/object.h>
#include "asset/w_asset_manager.h"
#include "audio/w_audio.h"
#include "input/w_input.h"
#include "mono_util.h"
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
#include "ui/w_canvas.h"
#include "util/w_logger.h"
#include "util/w_platform.h"
#include "w_engine.h"

namespace Wiesel {

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

// todo move these bindings to script glue

#define WIESEL_ADD_INTERNAL_CALL(name)                     \
  mono_add_internal_call("WieselEngine.Internals::" #name, \
                         reinterpret_cast<void*>(Internals_##name))

// Wraps mono_runtime_invoke with exception handling.
// Logs to both terminal and developer console.
// Returns the result. Sets *had_exception to true if an exception occurred.
static MonoObject* InvokeSafe(MonoMethod* method, MonoObject* obj, void** args,
                              bool* had_exception = nullptr) {
  MonoObject* exception = nullptr;
  MonoObject* result = mono_runtime_invoke(method, obj, args, &exception);
  if (exception) {
    MonoString* exc_str = mono_object_to_string(exception, nullptr);
    if (exc_str) {
      const char* cstr = mono_string_to_utf8(exc_str);
      LOG_ERROR("C# Exception: {}", cstr);
      Engine::console().LogError(cstr);
      mono_free((void*)cstr);
    }
    if (had_exception) {
      *had_exception = true;
    }
    return nullptr;
  }
  return result;
}

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
  bool value = InputManager::GetAction(cstr);
  mono_free((void*)cstr);
  return value;
}

void Internals_Input_SetCursorMode(uint16_t mode) {
  Engine::window()->SetCursorMode((CursorMode)mode);
}

uint16_t Internals_Input_GetCursorMode() {
  uint16_t cursor_mode = Engine::window()->GetCursorMode();
  return cursor_mode;
}

bool Internals_Input_GetKeyDown(MonoString* str) {
  const char* cstr = mono_string_to_utf8(str);
  bool value = InputManager::GetActionDown(cstr);
  mono_free((void*)cstr);
  return value;
}

bool Internals_Input_GetKeyUp(MonoString* str) {
  const char* cstr = mono_string_to_utf8(str);
  bool value = InputManager::GetActionUp(cstr);
  mono_free((void*)cstr);
  return value;
}

// Common guard: validates scene pointer before dereferencing.
// Prevents crashes when C# code calls internal methods before entity is set up.
#define VALIDATE_SCENE_OR_RETURN(scene_ptr, entity_id, retval) \
  if (scene_ptr == 0)                                          \
    return retval;                                             \
  Scene* scene = reinterpret_cast<Scene*>(scene_ptr);          \
  entt::entity handle = static_cast<entt::entity>(entity_id);  \
  if (!scene->GetRegistry().valid(handle))                     \
  return retval

// --- AudioSourceComponent bindings ---

#define GET_AUDIO_SRC_OR_RETURN(scene_ptr, entity_id, retval) \
  VALIDATE_SCENE_OR_RETURN(scene_ptr, entity_id, retval);     \
  if (!scene->HasComponent<AudioSourceComponent>(handle))     \
    return retval;                                            \
  auto& src = scene->GetComponent<AudioSourceComponent>(handle)

void Internals_AudioSource_Play(uint64_t scene_ptr, uint64_t entity_id) {
  GET_AUDIO_SRC_OR_RETURN(scene_ptr, entity_id, );
  if (src.playing_handle_.IsValid()) {
    Engine::audio().Stop(src.playing_handle_);
  }
  auto& transform = scene->GetComponent<TransformComponent>(handle);
  src.playing_handle_ =
      Engine::audio().Play(src.clip, src.MakeParams(transform.GetPosition()));
}

void Internals_AudioSource_PlayClip(uint64_t scene_ptr, uint64_t entity_id,
                                    MonoString* clip_handle) {
  GET_AUDIO_SRC_OR_RETURN(scene_ptr, entity_id, );
  const char* cstr = mono_string_to_utf8(clip_handle);
  AssetHandle clip = AssetHandle::FromString(cstr);
  mono_free((void*)cstr);
  if (src.playing_handle_.IsValid()) {
    Engine::audio().Stop(src.playing_handle_);
  }
  auto& transform = scene->GetComponent<TransformComponent>(handle);
  src.playing_handle_ =
      Engine::audio().Play(clip, src.MakeParams(transform.GetPosition()));
}

void Internals_AudioSource_Stop(uint64_t scene_ptr, uint64_t entity_id) {
  GET_AUDIO_SRC_OR_RETURN(scene_ptr, entity_id, );
  if (src.playing_handle_.IsValid()) {
    Engine::audio().Stop(src.playing_handle_);
    src.playing_handle_ = {};
  }
}

bool Internals_AudioSource_GetIsPlaying(uint64_t scene_ptr,
                                        uint64_t entity_id) {
  GET_AUDIO_SRC_OR_RETURN(scene_ptr, entity_id, false);
  return src.playing_handle_.IsValid();
}

float Internals_AudioSource_GetVolume(uint64_t scene_ptr, uint64_t entity_id) {
  GET_AUDIO_SRC_OR_RETURN(scene_ptr, entity_id, 0.0f);
  return src.volume;
}

void Internals_AudioSource_SetVolume(uint64_t scene_ptr, uint64_t entity_id,
                                     float v) {
  GET_AUDIO_SRC_OR_RETURN(scene_ptr, entity_id, );
  src.volume = v;
  if (src.playing_handle_.IsValid()) {
    Engine::audio().SetSoundVolume(src.playing_handle_, v);
  }
}

float Internals_AudioSource_GetPitch(uint64_t scene_ptr, uint64_t entity_id) {
  GET_AUDIO_SRC_OR_RETURN(scene_ptr, entity_id, 1.0f);
  return src.pitch;
}

void Internals_AudioSource_SetPitch(uint64_t scene_ptr, uint64_t entity_id,
                                    float v) {
  GET_AUDIO_SRC_OR_RETURN(scene_ptr, entity_id, );
  src.pitch = v;
}

bool Internals_AudioSource_GetLoop(uint64_t scene_ptr, uint64_t entity_id) {
  GET_AUDIO_SRC_OR_RETURN(scene_ptr, entity_id, false);
  return src.loop;
}

void Internals_AudioSource_SetLoop(uint64_t scene_ptr, uint64_t entity_id,
                                   bool v) {
  GET_AUDIO_SRC_OR_RETURN(scene_ptr, entity_id, );
  src.loop = v;
}

bool Internals_AudioSource_GetMute(uint64_t scene_ptr, uint64_t entity_id) {
  GET_AUDIO_SRC_OR_RETURN(scene_ptr, entity_id, false);
  return src.mute;
}

void Internals_AudioSource_SetMute(uint64_t scene_ptr, uint64_t entity_id,
                                   bool v) {
  GET_AUDIO_SRC_OR_RETURN(scene_ptr, entity_id, );
  src.mute = v;
}

float Internals_AudioSource_GetSpatialBlend(uint64_t scene_ptr,
                                            uint64_t entity_id) {
  GET_AUDIO_SRC_OR_RETURN(scene_ptr, entity_id, 0.0f);
  return src.spatial_blend;
}

void Internals_AudioSource_SetSpatialBlend(uint64_t scene_ptr,
                                           uint64_t entity_id, float v) {
  GET_AUDIO_SRC_OR_RETURN(scene_ptr, entity_id, );
  src.spatial_blend = v;
}

#undef GET_AUDIO_SRC

// --- LightDirect bindings ---

#define GET_LDIR_OR_RETURN(sp, eid, retval)               \
  VALIDATE_SCENE_OR_RETURN(sp, eid, retval);              \
  if (!scene->HasComponent<LightDirectComponent>(handle)) \
    return retval;                                        \
  auto& light = scene->GetComponent<LightDirectComponent>(handle)

float Internals_LightDirect_GetColorR(uint64_t sp, uint64_t eid) {
  GET_LDIR_OR_RETURN(sp, eid, 1.0f);
  return light.light_data.base.color.r;
}

float Internals_LightDirect_GetColorG(uint64_t sp, uint64_t eid) {
  GET_LDIR_OR_RETURN(sp, eid, 1.0f);
  return light.light_data.base.color.g;
}

float Internals_LightDirect_GetColorB(uint64_t sp, uint64_t eid) {
  GET_LDIR_OR_RETURN(sp, eid, 1.0f);
  return light.light_data.base.color.b;
}

void Internals_LightDirect_SetColorR(uint64_t sp, uint64_t eid, float v) {
  GET_LDIR_OR_RETURN(sp, eid, );
  light.light_data.base.color.r = v;
}

void Internals_LightDirect_SetColorG(uint64_t sp, uint64_t eid, float v) {
  GET_LDIR_OR_RETURN(sp, eid, );
  light.light_data.base.color.g = v;
}

void Internals_LightDirect_SetColorB(uint64_t sp, uint64_t eid, float v) {
  GET_LDIR_OR_RETURN(sp, eid, );
  light.light_data.base.color.b = v;
}

float Internals_LightDirect_GetAmbient(uint64_t sp, uint64_t eid) {
  GET_LDIR_OR_RETURN(sp, eid, 0.2f);
  return light.light_data.base.ambient;
}

void Internals_LightDirect_SetAmbient(uint64_t sp, uint64_t eid, float v) {
  GET_LDIR_OR_RETURN(sp, eid, );
  light.light_data.base.ambient = v;
}

float Internals_LightDirect_GetDiffuse(uint64_t sp, uint64_t eid) {
  GET_LDIR_OR_RETURN(sp, eid, 1.0f);
  return light.light_data.base.diffuse;
}

void Internals_LightDirect_SetDiffuse(uint64_t sp, uint64_t eid, float v) {
  GET_LDIR_OR_RETURN(sp, eid, );
  light.light_data.base.diffuse = v;
}

float Internals_LightDirect_GetSpecular(uint64_t sp, uint64_t eid) {
  GET_LDIR_OR_RETURN(sp, eid, 0.85f);
  return light.light_data.base.specular;
}

void Internals_LightDirect_SetSpecular(uint64_t sp, uint64_t eid, float v) {
  GET_LDIR_OR_RETURN(sp, eid, );
  light.light_data.base.specular = v;
}

float Internals_LightDirect_GetDensity(uint64_t sp, uint64_t eid) {
  GET_LDIR_OR_RETURN(sp, eid, 1.0f);
  return light.light_data.base.density;
}

void Internals_LightDirect_SetDensity(uint64_t sp, uint64_t eid, float v) {
  GET_LDIR_OR_RETURN(sp, eid, );
  light.light_data.base.density = v;
}

#undef GET_LDIR_OR_RETURN

// --- LightPoint bindings ---

#define GET_LPOINT_OR_RETURN(sp, eid, retval)            \
  VALIDATE_SCENE_OR_RETURN(sp, eid, retval);             \
  if (!scene->HasComponent<LightPointComponent>(handle)) \
    return retval;                                       \
  auto& light = scene->GetComponent<LightPointComponent>(handle)

float Internals_LightPoint_GetColorR(uint64_t sp, uint64_t eid) {
  GET_LPOINT_OR_RETURN(sp, eid, 1.0f);
  return light.light_data.base.color.r;
}

float Internals_LightPoint_GetColorG(uint64_t sp, uint64_t eid) {
  GET_LPOINT_OR_RETURN(sp, eid, 1.0f);
  return light.light_data.base.color.g;
}

float Internals_LightPoint_GetColorB(uint64_t sp, uint64_t eid) {
  GET_LPOINT_OR_RETURN(sp, eid, 1.0f);
  return light.light_data.base.color.b;
}

void Internals_LightPoint_SetColorR(uint64_t sp, uint64_t eid, float v) {
  GET_LPOINT_OR_RETURN(sp, eid, );
  light.light_data.base.color.r = v;
}

void Internals_LightPoint_SetColorG(uint64_t sp, uint64_t eid, float v) {
  GET_LPOINT_OR_RETURN(sp, eid, );
  light.light_data.base.color.g = v;
}

void Internals_LightPoint_SetColorB(uint64_t sp, uint64_t eid, float v) {
  GET_LPOINT_OR_RETURN(sp, eid, );
  light.light_data.base.color.b = v;
}

float Internals_LightPoint_GetAmbient(uint64_t sp, uint64_t eid) {
  GET_LPOINT_OR_RETURN(sp, eid, 0.2f);
  return light.light_data.base.ambient;
}

void Internals_LightPoint_SetAmbient(uint64_t sp, uint64_t eid, float v) {
  GET_LPOINT_OR_RETURN(sp, eid, );
  light.light_data.base.ambient = v;
}

float Internals_LightPoint_GetDiffuse(uint64_t sp, uint64_t eid) {
  GET_LPOINT_OR_RETURN(sp, eid, 1.0f);
  return light.light_data.base.diffuse;
}

void Internals_LightPoint_SetDiffuse(uint64_t sp, uint64_t eid, float v) {
  GET_LPOINT_OR_RETURN(sp, eid, );
  light.light_data.base.diffuse = v;
}

float Internals_LightPoint_GetSpecular(uint64_t sp, uint64_t eid) {
  GET_LPOINT_OR_RETURN(sp, eid, 0.85f);
  return light.light_data.base.specular;
}

void Internals_LightPoint_SetSpecular(uint64_t sp, uint64_t eid, float v) {
  GET_LPOINT_OR_RETURN(sp, eid, );
  light.light_data.base.specular = v;
}

float Internals_LightPoint_GetDensity(uint64_t sp, uint64_t eid) {
  GET_LPOINT_OR_RETURN(sp, eid, 1.0f);
  return light.light_data.base.density;
}

void Internals_LightPoint_SetDensity(uint64_t sp, uint64_t eid, float v) {
  GET_LPOINT_OR_RETURN(sp, eid, );
  light.light_data.base.density = v;
}

float Internals_LightPoint_GetConstant(uint64_t sp, uint64_t eid) {
  GET_LPOINT_OR_RETURN(sp, eid, 1.0f);
  return light.light_data.constant;
}

void Internals_LightPoint_SetConstant(uint64_t sp, uint64_t eid, float v) {
  GET_LPOINT_OR_RETURN(sp, eid, );
  light.light_data.constant = v;
}

float Internals_LightPoint_GetLinear(uint64_t sp, uint64_t eid) {
  GET_LPOINT_OR_RETURN(sp, eid, 0.09f);
  return light.light_data.linear;
}

void Internals_LightPoint_SetLinear(uint64_t sp, uint64_t eid, float v) {
  GET_LPOINT_OR_RETURN(sp, eid, );
  light.light_data.linear = v;
}

float Internals_LightPoint_GetExp(uint64_t sp, uint64_t eid) {
  GET_LPOINT_OR_RETURN(sp, eid, 0.032f);
  return light.light_data.exp;
}

void Internals_LightPoint_SetExp(uint64_t sp, uint64_t eid, float v) {
  GET_LPOINT_OR_RETURN(sp, eid, );
  light.light_data.exp = v;
}

#undef GET_LPOINT_OR_RETURN

// --- Entity AddComponent/RemoveComponent ---

void Internals_Entity_AddComponent(uint64_t scene_ptr, uint64_t entity_id,
                                   MonoString* name) {
  if (scene_ptr == 0) {
    return;
  }
  Scene* scene = reinterpret_cast<Scene*>(scene_ptr);
  entt::entity handle = static_cast<entt::entity>(entity_id);
  if (!scene->HasEntity(handle)) {
    return;
  }
  const char* cstr = mono_string_to_utf8(name);
  Engine::script_manager().AddComponentByName(scene, handle, cstr);
  mono_free((void*)cstr);
}

void Internals_Entity_RemoveComponent(uint64_t scene_ptr, uint64_t entity_id,
                                      MonoString* name) {
  if (scene_ptr == 0) {
    return;
  }
  Scene* scene = reinterpret_cast<Scene*>(scene_ptr);
  entt::entity handle = static_cast<entt::entity>(entity_id);
  if (!scene->HasEntity(handle)) {
    return;
  }
  const char* cstr = mono_string_to_utf8(name);
  Engine::script_manager().RemoveComponentByName(scene, handle, cstr);
  mono_free((void*)cstr);
}

// --- Mouse bindings ---

int Internals_Input_GetMouseX() {
  return InputManager::GetMouseX();
}

int Internals_Input_GetMouseY() {
  return InputManager::GetMouseY();
}

bool Internals_Input_GetMouseButton(int button) {
  return ImGui::IsMouseDown(static_cast<ImGuiMouseButton>(button));
}

bool Internals_Input_GetMouseButtonDown(int button) {
  return ImGui::IsMouseClicked(static_cast<ImGuiMouseButton>(button));
}

bool Internals_Input_GetMouseButtonUp(int button) {
  return ImGui::IsMouseReleased(static_cast<ImGuiMouseButton>(button));
}

// --- Gamepad bindings ---

bool Internals_Input_GetGamepadButton(int gamepad_index, int button) {
  return InputManager::IsGamepadButtonPressed(
      gamepad_index, static_cast<GamepadButton>(button));
}

float Internals_Input_GetGamepadAxis(int gamepad_index, int axis) {
  return InputManager::GetGamepadAxis(gamepad_index,
                                      static_cast<GamepadAxis>(axis));
}

int Internals_Input_GetConnectedGamepadCount() {
  return InputManager::GetConnectedGamepadCount();
}

// --- Entity tag bindings ---

bool Internals_Entity_HasTag(uint64_t scene_ptr, uint64_t entity_id,
                             MonoString* tag) {
  if (scene_ptr == 0) {
    return false;
  }
  Scene* scene = reinterpret_cast<Scene*>(scene_ptr);
  entt::entity handle = static_cast<entt::entity>(entity_id);
  if (!scene->HasComponent<TagComponent>(handle)) {
    return false;
  }
  const char* cstr = mono_string_to_utf8(tag);
  bool result = scene->GetComponent<TagComponent>(handle).HasTag(cstr);
  mono_free((void*)cstr);
  return result;
}

void Internals_Entity_AddTag(uint64_t scene_ptr, uint64_t entity_id,
                             MonoString* tag) {
  if (scene_ptr == 0) {
    return;
  }
  Scene* scene = reinterpret_cast<Scene*>(scene_ptr);
  entt::entity handle = static_cast<entt::entity>(entity_id);
  if (!scene->HasComponent<TagComponent>(handle)) {
    return;
  }
  const char* cstr = mono_string_to_utf8(tag);
  scene->GetComponent<TagComponent>(handle).AddTag(cstr);
  mono_free((void*)cstr);
}

void Internals_Entity_RemoveTag(uint64_t scene_ptr, uint64_t entity_id,
                                MonoString* tag) {
  if (scene_ptr == 0) {
    return;
  }
  Scene* scene = reinterpret_cast<Scene*>(scene_ptr);
  entt::entity handle = static_cast<entt::entity>(entity_id);
  if (!scene->HasComponent<TagComponent>(handle)) {
    return;
  }
  const char* cstr = mono_string_to_utf8(tag);
  scene->GetComponent<TagComponent>(handle).RemoveTag(cstr);
  mono_free((void*)cstr);
}

MonoArray* Internals_Scene_FindEntitiesByTag(uint64_t scene_ptr,
                                             MonoString* tag) {
  if (scene_ptr == 0) {
    return nullptr;
  }
  Scene* scene = reinterpret_cast<Scene*>(scene_ptr);
  const char* cstr = mono_string_to_utf8(tag);
  auto entities = scene->FindEntitiesByTag(cstr);
  mono_free((void*)cstr);

  MonoArray* arr = mono_array_new(Engine::script_manager().app_domain(),
                                  mono_get_uint64_class(), entities.size());
  for (size_t i = 0; i < entities.size(); i++) {
    mono_array_set(arr, uint64_t, i, static_cast<uint64_t>(entities[i]));
  }
  return arr;
}

// --- CameraComponent bindings ---

#define GET_CAM_OR_RETURN(scene_ptr, entity_id, retval)   \
  VALIDATE_SCENE_OR_RETURN(scene_ptr, entity_id, retval); \
  if (!scene->HasComponent<CameraComponent>(handle))      \
    return retval;                                        \
  auto& cam = scene->GetComponent<CameraComponent>(handle)

int Internals_Camera_GetProjectionMode(uint64_t sp, uint64_t eid) {
  GET_CAM_OR_RETURN(sp, eid, 0);
  return static_cast<int>(cam.projection_mode);
}

void Internals_Camera_SetProjectionMode(uint64_t sp, uint64_t eid, int mode) {
  GET_CAM_OR_RETURN(sp, eid, );
  cam.projection_mode = static_cast<ProjectionMode>(mode);
  cam.view_changed = true;
}

float Internals_Camera_GetFOV(uint64_t sp, uint64_t eid) {
  GET_CAM_OR_RETURN(sp, eid, 60.0f);
  return cam.field_of_view;
}

void Internals_Camera_SetFOV(uint64_t sp, uint64_t eid, float v) {
  GET_CAM_OR_RETURN(sp, eid, );
  cam.field_of_view = v;
  cam.view_changed = true;
}

float Internals_Camera_GetOrthoSize(uint64_t sp, uint64_t eid) {
  GET_CAM_OR_RETURN(sp, eid, 5.0f);
  return cam.ortho_size;
}

void Internals_Camera_SetOrthoSize(uint64_t sp, uint64_t eid, float v) {
  GET_CAM_OR_RETURN(sp, eid, );
  cam.ortho_size = v;
  cam.view_changed = true;
}

float Internals_Camera_GetNearPlane(uint64_t sp, uint64_t eid) {
  GET_CAM_OR_RETURN(sp, eid, 0.01f);
  return cam.near_plane;
}

void Internals_Camera_SetNearPlane(uint64_t sp, uint64_t eid, float v) {
  GET_CAM_OR_RETURN(sp, eid, );
  cam.near_plane = v;
  cam.view_changed = true;
}

float Internals_Camera_GetFarPlane(uint64_t sp, uint64_t eid) {
  GET_CAM_OR_RETURN(sp, eid, 1000.0f);
  return cam.far_plane;
}

void Internals_Camera_SetFarPlane(uint64_t sp, uint64_t eid, float v) {
  GET_CAM_OR_RETURN(sp, eid, );
  cam.far_plane = v;
  cam.view_changed = true;
}

bool Internals_Camera_GetEnabled(uint64_t sp, uint64_t eid) {
  GET_CAM_OR_RETURN(sp, eid, false);
  return cam.enabled;
}

void Internals_Camera_SetEnabled(uint64_t sp, uint64_t eid, bool v) {
  GET_CAM_OR_RETURN(sp, eid, );
  cam.enabled = v;
}

float Internals_Camera_GetBgColorR(uint64_t sp, uint64_t eid) {
  GET_CAM_OR_RETURN(sp, eid, 0.0f);
  return cam.background_color.r;
}

float Internals_Camera_GetBgColorG(uint64_t sp, uint64_t eid) {
  GET_CAM_OR_RETURN(sp, eid, 0.0f);
  return cam.background_color.g;
}

float Internals_Camera_GetBgColorB(uint64_t sp, uint64_t eid) {
  GET_CAM_OR_RETURN(sp, eid, 0.0f);
  return cam.background_color.b;
}

float Internals_Camera_GetBgColorA(uint64_t sp, uint64_t eid) {
  GET_CAM_OR_RETURN(sp, eid, 1.0f);
  return cam.background_color.a;
}

void Internals_Camera_SetBgColorR(uint64_t sp, uint64_t eid, float v) {
  GET_CAM_OR_RETURN(sp, eid, );
  cam.background_color.r = v;
}

void Internals_Camera_SetBgColorG(uint64_t sp, uint64_t eid, float v) {
  GET_CAM_OR_RETURN(sp, eid, );
  cam.background_color.g = v;
}

void Internals_Camera_SetBgColorB(uint64_t sp, uint64_t eid, float v) {
  GET_CAM_OR_RETURN(sp, eid, );
  cam.background_color.b = v;
}

void Internals_Camera_SetBgColorA(uint64_t sp, uint64_t eid, float v) {
  GET_CAM_OR_RETURN(sp, eid, );
  cam.background_color.a = v;
}

#undef GET_CAM_OR_RETURN

// --- SpriteRendererComponent bindings ---

#define GET_SPRITE_RENDERER_OR_RETURN(scene_ptr, entity_id, retval) \
  VALIDATE_SCENE_OR_RETURN(scene_ptr, entity_id, retval);           \
  if (!scene->HasComponent<SpriteRendererComponent>(handle))        \
    return retval;                                                  \
  auto& spr_r = scene->GetComponent<SpriteRendererComponent>(handle)

bool Internals_SpriteRenderer_GetFlipX(uint64_t sp, uint64_t eid) {
  GET_SPRITE_RENDERER_OR_RETURN(sp, eid, false);
  return spr_r.flip_x_;
}

void Internals_SpriteRenderer_SetFlipX(uint64_t sp, uint64_t eid, bool v) {
  GET_SPRITE_RENDERER_OR_RETURN(sp, eid, );
  spr_r.flip_x_ = v;
}

bool Internals_SpriteRenderer_GetFlipY(uint64_t sp, uint64_t eid) {
  GET_SPRITE_RENDERER_OR_RETURN(sp, eid, false);
  return spr_r.flip_y_;
}

void Internals_SpriteRenderer_SetFlipY(uint64_t sp, uint64_t eid, bool v) {
  GET_SPRITE_RENDERER_OR_RETURN(sp, eid, );
  spr_r.flip_y_ = v;
}

float Internals_SpriteRenderer_GetTintR(uint64_t sp, uint64_t eid) {
  GET_SPRITE_RENDERER_OR_RETURN(sp, eid, 1.0f);
  return spr_r.tint_.r;
}

float Internals_SpriteRenderer_GetTintG(uint64_t sp, uint64_t eid) {
  GET_SPRITE_RENDERER_OR_RETURN(sp, eid, 1.0f);
  return spr_r.tint_.g;
}

float Internals_SpriteRenderer_GetTintB(uint64_t sp, uint64_t eid) {
  GET_SPRITE_RENDERER_OR_RETURN(sp, eid, 1.0f);
  return spr_r.tint_.b;
}

float Internals_SpriteRenderer_GetTintA(uint64_t sp, uint64_t eid) {
  GET_SPRITE_RENDERER_OR_RETURN(sp, eid, 1.0f);
  return spr_r.tint_.a;
}

void Internals_SpriteRenderer_SetTintR(uint64_t sp, uint64_t eid, float v) {
  GET_SPRITE_RENDERER_OR_RETURN(sp, eid, );
  spr_r.tint_.r = v;
}

void Internals_SpriteRenderer_SetTintG(uint64_t sp, uint64_t eid, float v) {
  GET_SPRITE_RENDERER_OR_RETURN(sp, eid, );
  spr_r.tint_.g = v;
}

void Internals_SpriteRenderer_SetTintB(uint64_t sp, uint64_t eid, float v) {
  GET_SPRITE_RENDERER_OR_RETURN(sp, eid, );
  spr_r.tint_.b = v;
}

void Internals_SpriteRenderer_SetTintA(uint64_t sp, uint64_t eid, float v) {
  GET_SPRITE_RENDERER_OR_RETURN(sp, eid, );
  spr_r.tint_.a = v;
}

int Internals_SpriteRenderer_GetSortLayer(uint64_t sp, uint64_t eid) {
  GET_SPRITE_RENDERER_OR_RETURN(sp, eid, 0);
  return spr_r.sort_layer_;
}

void Internals_SpriteRenderer_SetSortLayer(uint64_t sp, uint64_t eid, int v) {
  GET_SPRITE_RENDERER_OR_RETURN(sp, eid, );
  spr_r.sort_layer_ = static_cast<uint8_t>(std::clamp(v, 0, 255));
}

#undef GET_SPRITE_RENDERER_OR_RETURN

// --- SpriteAnimatorComponent bindings ---

#define GET_SPRITE_ANIMATOR_OR_RETURN(scene_ptr, entity_id, retval) \
  VALIDATE_SCENE_OR_RETURN(scene_ptr, entity_id, retval);           \
  if (!scene->HasComponent<SpriteAnimatorComponent>(handle))        \
    return retval;                                                  \
  auto& spr_a = scene->GetComponent<SpriteAnimatorComponent>(handle)

void Internals_SpriteAnimator_Play(uint64_t sp, uint64_t eid, MonoString* state,
                                   bool restart) {
  GET_SPRITE_ANIMATOR_OR_RETURN(sp, eid, );
  const char* cstr = mono_string_to_utf8(state);
  spr_a.Play(cstr, restart);
  mono_free((void*)cstr);
}

void Internals_SpriteAnimator_Stop(uint64_t sp, uint64_t eid) {
  GET_SPRITE_ANIMATOR_OR_RETURN(sp, eid, );
  spr_a.Stop();
}

bool Internals_SpriteAnimator_GetIsPlaying(uint64_t sp, uint64_t eid) {
  GET_SPRITE_ANIMATOR_OR_RETURN(sp, eid, false);
  return spr_a.playing_;
}

MonoString* Internals_SpriteAnimator_GetCurrentState(uint64_t sp,
                                                     uint64_t eid) {
  GET_SPRITE_ANIMATOR_OR_RETURN(
      sp, eid, mono_string_new(Engine::script_manager().app_domain(), ""));
  return mono_string_new(Engine::script_manager().app_domain(),
                         spr_a.current_state_name_.c_str());
}

int Internals_SpriteAnimator_GetCurrentFrame(uint64_t sp, uint64_t eid) {
  GET_SPRITE_ANIMATOR_OR_RETURN(sp, eid, 0);
  return static_cast<int>(spr_a.current_frame_index_);
}

void Internals_SpriteAnimator_SetBool(uint64_t sp, uint64_t eid,
                                      MonoString* name, bool val) {
  GET_SPRITE_ANIMATOR_OR_RETURN(sp, eid, );
  const char* cstr = mono_string_to_utf8(name);
  spr_a.state_machine_.SetBool(cstr, val);
  mono_free((void*)cstr);
}

void Internals_SpriteAnimator_SetInt(uint64_t sp, uint64_t eid,
                                     MonoString* name, int val) {
  GET_SPRITE_ANIMATOR_OR_RETURN(sp, eid, );
  const char* cstr = mono_string_to_utf8(name);
  spr_a.state_machine_.SetInt(cstr, val);
  mono_free((void*)cstr);
}

void Internals_SpriteAnimator_SetFloat(uint64_t sp, uint64_t eid,
                                       MonoString* name, float val) {
  GET_SPRITE_ANIMATOR_OR_RETURN(sp, eid, );
  const char* cstr = mono_string_to_utf8(name);
  spr_a.state_machine_.SetFloat(cstr, val);
  mono_free((void*)cstr);
}

void Internals_SpriteAnimator_SetTrigger(uint64_t sp, uint64_t eid,
                                         MonoString* name) {
  GET_SPRITE_ANIMATOR_OR_RETURN(sp, eid, );
  const char* cstr = mono_string_to_utf8(name);
  spr_a.state_machine_.SetTrigger(cstr);
  mono_free((void*)cstr);
}

bool Internals_SpriteAnimator_GetBool(uint64_t sp, uint64_t eid,
                                      MonoString* name) {
  GET_SPRITE_ANIMATOR_OR_RETURN(sp, eid, false);
  const char* cstr = mono_string_to_utf8(name);
  bool val = spr_a.state_machine_.GetBool(cstr);
  mono_free((void*)cstr);
  return val;
}

int Internals_SpriteAnimator_GetInt(uint64_t sp, uint64_t eid,
                                    MonoString* name) {
  GET_SPRITE_ANIMATOR_OR_RETURN(sp, eid, 0);
  const char* cstr = mono_string_to_utf8(name);
  int val = spr_a.state_machine_.GetInt(cstr);
  mono_free((void*)cstr);
  return val;
}

float Internals_SpriteAnimator_GetFloat(uint64_t sp, uint64_t eid,
                                        MonoString* name) {
  GET_SPRITE_ANIMATOR_OR_RETURN(sp, eid, 0.0f);
  const char* cstr = mono_string_to_utf8(name);
  float val = spr_a.state_machine_.GetFloat(cstr);
  mono_free((void*)cstr);
  return val;
}

#undef GET_SPRITE_ANIMATOR_OR_RETURN

// --- Audio bindings ---

void Internals_Audio_PlayPath(MonoString* path, int bus, float volume,
                              float pitch, bool loop) {
  const char* cstr = mono_string_to_utf8(path);
  SoundParams params;
  params.bus = static_cast<AudioBus>(bus);
  params.volume = volume;
  params.pitch = pitch;
  params.loop = loop;
  Engine::audio().Play(cstr, params);
  mono_free((void*)cstr);
}

void Internals_Audio_PlayAtPath(MonoString* path, float x, float y, float z,
                                int bus, float volume, float minDist,
                                float maxDist) {
  const char* cstr = mono_string_to_utf8(path);
  SoundParams params;
  params.bus = static_cast<AudioBus>(bus);
  params.volume = volume;
  params.spatial_blend = 1.0f;
  params.position = {x, y, z};
  params.min_distance = minDist;
  params.max_distance = maxDist;
  Engine::audio().Play(cstr, params);
  mono_free((void*)cstr);
}

void Internals_Audio_PlayClip(MonoString* handle_str, int bus, float volume,
                              float pitch, bool loop) {
  const char* cstr = mono_string_to_utf8(handle_str);
  AssetHandle clip = AssetHandle::FromString(cstr);
  mono_free((void*)cstr);
  SoundParams params;
  params.bus = static_cast<AudioBus>(bus);
  params.volume = volume;
  params.pitch = pitch;
  params.loop = loop;
  Engine::audio().Play(clip, params);
}

void Internals_Audio_PlayAtClip(MonoString* handle_str, float x, float y,
                                float z, int bus, float volume, float minDist,
                                float maxDist) {
  const char* cstr = mono_string_to_utf8(handle_str);
  AssetHandle clip = AssetHandle::FromString(cstr);
  mono_free((void*)cstr);
  Engine::audio().PlaySoundAt(clip, {x, y, z}, static_cast<AudioBus>(bus),
                              volume, minDist, maxDist);
}

void Internals_Audio_PlayMusic(MonoString* path, float volume) {
  const char* cstr = mono_string_to_utf8(path);
  Engine::audio().PlayMusic(cstr, volume);
  mono_free((void*)cstr);
}

void Internals_Audio_PlayMusicClip(MonoString* handle_str, float volume) {
  const char* cstr = mono_string_to_utf8(handle_str);
  AssetHandle clip = AssetHandle::FromString(cstr);
  mono_free((void*)cstr);
  Engine::audio().PlayMusic(clip, volume);
}

void Internals_Audio_StopMusic() {
  Engine::audio().StopMusic();
}

void Internals_Audio_SetMasterVolume(float volume) {
  Engine::audio().SetMasterVolume(volume);
}

float Internals_Audio_GetMasterVolume() {
  return Engine::audio().GetMasterVolume();
}

void Internals_Audio_SetSFXVolume(float volume) {
  Engine::audio().SetSFXVolume(volume);
}

float Internals_Audio_GetSFXVolume() {
  return Engine::audio().GetSFXVolume();
}

void Internals_Audio_SetMusicVolume(float volume) {
  Engine::audio().SetMusicVolume(volume);
}

float Internals_Audio_GetMusicVolume() {
  return Engine::audio().GetMusicVolume();
}

// Helper: ensure MaterialInstance exists on a ModelComponent (mesh index 0)
static std::shared_ptr<MaterialInstance>& EnsureMaterialInstance(
    ModelComponent& model) {
  if (model.material_instances.empty()) {
    model.material_instances.resize(1);
  }
  if (!model.material_instances[0]) {
    model.material_instances[0] = std::make_shared<MaterialInstance>();
    // Create a transient material if no handle is set
    std::shared_ptr<Material> fallback = std::make_shared<Material>();
    AssetHandle h = Engine::asset_manager().RegisterAndStore<Material>(
        "script_material", AssetType::Material, "", fallback);
    model.material_instances[0]->base_material_handle = h;
  }
  return model.material_instances[0];
}

// ModelComponent material properties
float Internals_ModelComponent_GetColorTintR(uint64_t scene_ptr,
                                             uint64_t entity_id) {
  if (scene_ptr == 0) {
    return {};
  }
  Scene* scene = reinterpret_cast<Scene*>(scene_ptr);
  auto& model =
      scene->GetComponent<ModelComponent>(static_cast<entt::entity>(entity_id));
  return EnsureMaterialInstance(model)->GetColorTint().r;
}

float Internals_ModelComponent_GetColorTintG(uint64_t scene_ptr,
                                             uint64_t entity_id) {
  if (scene_ptr == 0) {
    return {};
  }
  Scene* scene = reinterpret_cast<Scene*>(scene_ptr);
  auto& model =
      scene->GetComponent<ModelComponent>(static_cast<entt::entity>(entity_id));
  return EnsureMaterialInstance(model)->GetColorTint().g;
}

float Internals_ModelComponent_GetColorTintB(uint64_t scene_ptr,
                                             uint64_t entity_id) {
  if (scene_ptr == 0) {
    return {};
  }
  Scene* scene = reinterpret_cast<Scene*>(scene_ptr);
  auto& model =
      scene->GetComponent<ModelComponent>(static_cast<entt::entity>(entity_id));
  return EnsureMaterialInstance(model)->GetColorTint().b;
}

float Internals_ModelComponent_GetColorTintA(uint64_t scene_ptr,
                                             uint64_t entity_id) {
  if (scene_ptr == 0) {
    return {};
  }
  Scene* scene = reinterpret_cast<Scene*>(scene_ptr);
  auto& model =
      scene->GetComponent<ModelComponent>(static_cast<entt::entity>(entity_id));
  return EnsureMaterialInstance(model)->GetColorTint().a;
}

void Internals_ModelComponent_SetColorTintR(uint64_t scene_ptr,
                                            uint64_t entity_id, float v) {
  if (scene_ptr == 0) {
    return;
  }
  Scene* scene = reinterpret_cast<Scene*>(scene_ptr);
  auto& model =
      scene->GetComponent<ModelComponent>(static_cast<entt::entity>(entity_id));
  auto& inst = EnsureMaterialInstance(model);
  auto tint = inst->GetColorTint();
  tint.r = v;
  inst->SetColorTint(tint);
}

void Internals_ModelComponent_SetColorTintG(uint64_t scene_ptr,
                                            uint64_t entity_id, float v) {
  if (scene_ptr == 0) {
    return;
  }
  Scene* scene = reinterpret_cast<Scene*>(scene_ptr);
  auto& model =
      scene->GetComponent<ModelComponent>(static_cast<entt::entity>(entity_id));
  auto& inst = EnsureMaterialInstance(model);
  auto tint = inst->GetColorTint();
  tint.g = v;
  inst->SetColorTint(tint);
}

void Internals_ModelComponent_SetColorTintB(uint64_t scene_ptr,
                                            uint64_t entity_id, float v) {
  if (scene_ptr == 0) {
    return;
  }
  Scene* scene = reinterpret_cast<Scene*>(scene_ptr);
  auto& model =
      scene->GetComponent<ModelComponent>(static_cast<entt::entity>(entity_id));
  auto& inst = EnsureMaterialInstance(model);
  auto tint = inst->GetColorTint();
  tint.b = v;
  inst->SetColorTint(tint);
}

void Internals_ModelComponent_SetColorTintA(uint64_t scene_ptr,
                                            uint64_t entity_id, float v) {
  if (scene_ptr == 0) {
    return;
  }
  Scene* scene = reinterpret_cast<Scene*>(scene_ptr);
  auto& model =
      scene->GetComponent<ModelComponent>(static_cast<entt::entity>(entity_id));
  std::shared_ptr<MaterialInstance>& inst = EnsureMaterialInstance(model);
  glm::vec4 tint = inst->GetColorTint();
  tint.a = v;
  inst->SetColorTint(tint);
}

float Internals_ModelComponent_GetRoughness(uint64_t scene_ptr,
                                            uint64_t entity_id) {
  if (scene_ptr == 0) {
    return {};
  }
  Scene* scene = reinterpret_cast<Scene*>(scene_ptr);
  auto& model =
      scene->GetComponent<ModelComponent>(static_cast<entt::entity>(entity_id));
  return EnsureMaterialInstance(model)->GetRoughness();
}

void Internals_ModelComponent_SetRoughness(uint64_t scene_ptr,
                                           uint64_t entity_id, float v) {
  if (scene_ptr == 0) {
    return;
  }
  Scene* scene = reinterpret_cast<Scene*>(scene_ptr);
  auto& model =
      scene->GetComponent<ModelComponent>(static_cast<entt::entity>(entity_id));
  EnsureMaterialInstance(model)->SetRoughness(v);
}

float Internals_ModelComponent_GetMetallic(uint64_t scene_ptr,
                                           uint64_t entity_id) {
  if (scene_ptr == 0) {
    return {};
  }
  Scene* scene = reinterpret_cast<Scene*>(scene_ptr);
  auto& model =
      scene->GetComponent<ModelComponent>(static_cast<entt::entity>(entity_id));
  return EnsureMaterialInstance(model)->GetMetallic();
}

void Internals_ModelComponent_SetMetallic(uint64_t scene_ptr,
                                          uint64_t entity_id, float v) {
  if (scene_ptr == 0) {
    return;
  }
  Scene* scene = reinterpret_cast<Scene*>(scene_ptr);
  auto& model =
      scene->GetComponent<ModelComponent>(static_cast<entt::entity>(entity_id));
  EnsureMaterialInstance(model)->SetMetallic(v);
}

float Internals_ModelComponent_GetSpecular(uint64_t scene_ptr,
                                           uint64_t entity_id) {
  if (scene_ptr == 0) {
    return {};
  }
  Scene* scene = reinterpret_cast<Scene*>(scene_ptr);
  auto& model =
      scene->GetComponent<ModelComponent>(static_cast<entt::entity>(entity_id));
  return EnsureMaterialInstance(model)->GetSpecular();
}

void Internals_ModelComponent_SetSpecular(uint64_t scene_ptr,
                                          uint64_t entity_id, float v) {
  if (scene_ptr == 0) {
    return;
  }
  Scene* scene = reinterpret_cast<Scene*>(scene_ptr);
  auto& model =
      scene->GetComponent<ModelComponent>(static_cast<entt::entity>(entity_id));
  EnsureMaterialInstance(model)->SetSpecular(v);
}

// --- Time ---

float Internals_Time_GetDeltaTime() {
  return Engine::app().GetDeltaTime();
}

float Internals_Time_GetTimeScale() {
  return Engine::app().GetTimeScale();
}

void Internals_Time_SetTimeScale(float value) {
  Engine::app().SetTimeScale(value);
}

float Internals_Time_GetElapsedTime() {
  return Time::GetTime();
}

// --- Scene ---

uint64_t Internals_Scene_FindEntity(uint64_t scene_ptr, MonoString* name) {
  if (scene_ptr == 0) {
    return UINT64_MAX;
  }
  Scene* scene = reinterpret_cast<Scene*>(scene_ptr);
  const char* cstr = mono_string_to_utf8(name);
  entt::entity entity = scene->FindEntityByName(cstr);
  mono_free((void*)cstr);
  if (entity == entt::null) {
    return UINT64_MAX;
  }
  return static_cast<uint64_t>(entity);
}

uint64_t Internals_Scene_CreateEntity(uint64_t scene_ptr, MonoString* name) {
  if (scene_ptr == 0) {
    return 0;
  }
  Scene* scene = reinterpret_cast<Scene*>(scene_ptr);
  if (!scene) {
    LOG_ERROR("Scene_CreateEntity: no active scene");
    return 0;
  }
  const char* cstr = mono_string_to_utf8(name);
  Entity entity = scene->CreateEntity(cstr);
  mono_free((void*)cstr);
  return static_cast<uint32_t>(entity.handle());
}

void Internals_Scene_DestroyEntity(uint64_t scene_ptr, uint64_t entity_id) {
  if (scene_ptr == 0) {
    return;
  }
  Scene* scene = reinterpret_cast<Scene*>(scene_ptr);
  entt::entity handle = static_cast<entt::entity>(entity_id);
  if (!scene->HasEntity(handle)) {
    return;
  }
  Entity entity{handle, scene};
  scene->RemoveEntity(entity);
}

MonoObject* Internals_Behavior_GetComponent(Scene* scene, entt::entity entity,
                                            MonoString* str) {
  const char* cstr = mono_string_to_utf8(str);
  MonoObject* component =
      Engine::script_manager().GetComponentByName(scene, entity, cstr);
  mono_free((void*)cstr);
  return component;
}

bool Internals_Behavior_HasComponent(Scene* scene, entt::entity entity,
                                     MonoString* str) {
  const char* cstr = mono_string_to_utf8(str);
  bool hasComponent =
      Engine::script_manager().HasComponentByName(scene, entity, cstr);
  mono_free((void*)cstr);
  return hasComponent;
}

float Internals_TransformComponent_GetPositionX(Scene* scene,
                                                entt::entity entity) {
  return scene->GetComponent<TransformComponent>(entity).GetPosition().x;
}

void Internals_TransformComponent_SetPositionX(Scene* scene,
                                               entt::entity entity,
                                               float value) {
  auto& c = scene->GetComponent<TransformComponent>(entity);
  if (c.GetPosition().x == value) {
    return;
  }
  c.PositionMut().x = value;
  c.MarkChanged();
}

float Internals_TransformComponent_GetPositionY(Scene* scene,
                                                entt::entity entity) {
  return scene->GetComponent<TransformComponent>(entity).GetPosition().y;
}

void Internals_TransformComponent_SetPositionY(Scene* scene,
                                               entt::entity entity,
                                               float value) {
  auto& c = scene->GetComponent<TransformComponent>(entity);
  if (c.GetPosition().y == value) {
    return;
  }
  c.PositionMut().y = value;
  c.MarkChanged();
}

float Internals_TransformComponent_GetPositionZ(Scene* scene,
                                                entt::entity entity) {
  return scene->GetComponent<TransformComponent>(entity).GetPosition().z;
}

void Internals_TransformComponent_SetPositionZ(Scene* scene,
                                               entt::entity entity,
                                               float value) {
  auto& c = scene->GetComponent<TransformComponent>(entity);
  if (c.GetPosition().z == value) {
    return;
  }
  c.PositionMut().z = value;
  c.MarkChanged();
}

float Internals_TransformComponent_GetRotationX(Scene* scene,
                                                entt::entity entity) {
  return scene->GetComponent<TransformComponent>(entity).GetRotation().x;
}

void Internals_TransformComponent_SetRotationX(Scene* scene,
                                               entt::entity entity,
                                               float value) {
  auto& c = scene->GetComponent<TransformComponent>(entity);
  if (c.GetRotation().x == value) {
    return;
  }
  c.RotationMut().x = value;
  c.MarkChanged();
}

float Internals_TransformComponent_GetRotationY(Scene* scene,
                                                entt::entity entity) {
  return scene->GetComponent<TransformComponent>(entity).GetRotation().y;
}

void Internals_TransformComponent_SetRotationY(Scene* scene,
                                               entt::entity entity,
                                               float value) {
  auto& c = scene->GetComponent<TransformComponent>(entity);
  if (c.GetRotation().y == value) {
    return;
  }
  c.RotationMut().y = value;
  c.MarkChanged();
}

float Internals_TransformComponent_GetRotationZ(Scene* scene,
                                                entt::entity entity) {
  return scene->GetComponent<TransformComponent>(entity).GetRotation().z;
}

void Internals_TransformComponent_SetRotationZ(Scene* scene,
                                               entt::entity entity,
                                               float value) {
  auto& c = scene->GetComponent<TransformComponent>(entity);
  if (c.GetRotation().z == value) {
    return;
  }
  c.RotationMut().z = value;
  c.MarkChanged();
}

float Internals_TransformComponent_GetScaleX(Scene* scene,
                                             entt::entity entity) {
  return scene->GetComponent<TransformComponent>(entity).GetScale().x;
}

void Internals_TransformComponent_SetScaleX(Scene* scene, entt::entity entity,
                                            float value) {
  auto& c = scene->GetComponent<TransformComponent>(entity);
  if (c.GetScale().x == value) {
    return;
  }
  c.ScaleMut().x = value;
  c.MarkChanged();
}

float Internals_TransformComponent_GetScaleY(Scene* scene,
                                             entt::entity entity) {
  return scene->GetComponent<TransformComponent>(entity).GetScale().y;
}

void Internals_TransformComponent_SetScaleY(Scene* scene, entt::entity entity,
                                            float value) {
  auto& c = scene->GetComponent<TransformComponent>(entity);
  if (c.GetScale().y == value) {
    return;
  }
  c.ScaleMut().y = value;
  c.MarkChanged();
}

float Internals_TransformComponent_GetScaleZ(Scene* scene,
                                             entt::entity entity) {
  return scene->GetComponent<TransformComponent>(entity).GetScale().z;
}

void Internals_TransformComponent_SetScaleZ(Scene* scene, entt::entity entity,
                                            float value) {
  auto& c = scene->GetComponent<TransformComponent>(entity);
  if (c.GetScale().z == value) {
    return;
  }
  c.ScaleMut().z = value;
  c.MarkChanged();
}

bool Internals_ModelComponent_GetEnableRendering(Scene* scene,
                                                 entt::entity entity) {
  return scene->GetComponent<ModelComponent>(entity).enable_rendering;
}

void Internals_ModelComponent_SetEnableRendering(Scene* scene,
                                                 entt::entity entity,
                                                 bool value) {
  scene->GetComponent<ModelComponent>(entity).enable_rendering = value;
}

// --- BoxColliderComponent ---
float Internals_BoxCollider_GetOffsetX(Scene* scene, entt::entity entity) {
  return scene->GetComponent<BoxColliderComponent>(entity).offset.x;
}

float Internals_BoxCollider_GetOffsetY(Scene* scene, entt::entity entity) {
  return scene->GetComponent<BoxColliderComponent>(entity).offset.y;
}

float Internals_BoxCollider_GetOffsetZ(Scene* scene, entt::entity entity) {
  return scene->GetComponent<BoxColliderComponent>(entity).offset.z;
}

void Internals_BoxCollider_SetOffsetX(Scene* scene, entt::entity entity,
                                      float v) {
  scene->GetComponent<BoxColliderComponent>(entity).offset.x = v;
}

void Internals_BoxCollider_SetOffsetY(Scene* scene, entt::entity entity,
                                      float v) {
  scene->GetComponent<BoxColliderComponent>(entity).offset.y = v;
}

void Internals_BoxCollider_SetOffsetZ(Scene* scene, entt::entity entity,
                                      float v) {
  scene->GetComponent<BoxColliderComponent>(entity).offset.z = v;
}

float Internals_BoxCollider_GetHalfExtentsX(Scene* scene, entt::entity entity) {
  return scene->GetComponent<BoxColliderComponent>(entity).half_extents.x;
}

float Internals_BoxCollider_GetHalfExtentsY(Scene* scene, entt::entity entity) {
  return scene->GetComponent<BoxColliderComponent>(entity).half_extents.y;
}

float Internals_BoxCollider_GetHalfExtentsZ(Scene* scene, entt::entity entity) {
  return scene->GetComponent<BoxColliderComponent>(entity).half_extents.z;
}

void Internals_BoxCollider_SetHalfExtentsX(Scene* scene, entt::entity entity,
                                           float v) {
  scene->GetComponent<BoxColliderComponent>(entity).half_extents.x = v;
}

void Internals_BoxCollider_SetHalfExtentsY(Scene* scene, entt::entity entity,
                                           float v) {
  scene->GetComponent<BoxColliderComponent>(entity).half_extents.y = v;
}

void Internals_BoxCollider_SetHalfExtentsZ(Scene* scene, entt::entity entity,
                                           float v) {
  scene->GetComponent<BoxColliderComponent>(entity).half_extents.z = v;
}

bool Internals_BoxCollider_GetIsTrigger(Scene* scene, entt::entity entity) {
  return scene->GetComponent<BoxColliderComponent>(entity).is_trigger;
}

void Internals_BoxCollider_SetIsTrigger(Scene* scene, entt::entity entity,
                                        bool v) {
  scene->GetComponent<BoxColliderComponent>(entity).is_trigger = v;
}

// --- SphereColliderComponent ---
float Internals_SphereCollider_GetOffsetX(Scene* scene, entt::entity entity) {
  return scene->GetComponent<SphereColliderComponent>(entity).offset.x;
}

float Internals_SphereCollider_GetOffsetY(Scene* scene, entt::entity entity) {
  return scene->GetComponent<SphereColliderComponent>(entity).offset.y;
}

float Internals_SphereCollider_GetOffsetZ(Scene* scene, entt::entity entity) {
  return scene->GetComponent<SphereColliderComponent>(entity).offset.z;
}

void Internals_SphereCollider_SetOffsetX(Scene* scene, entt::entity entity,
                                         float v) {
  scene->GetComponent<SphereColliderComponent>(entity).offset.x = v;
}

void Internals_SphereCollider_SetOffsetY(Scene* scene, entt::entity entity,
                                         float v) {
  scene->GetComponent<SphereColliderComponent>(entity).offset.y = v;
}

void Internals_SphereCollider_SetOffsetZ(Scene* scene, entt::entity entity,
                                         float v) {
  scene->GetComponent<SphereColliderComponent>(entity).offset.z = v;
}

float Internals_SphereCollider_GetRadius(Scene* scene, entt::entity entity) {
  return scene->GetComponent<SphereColliderComponent>(entity).radius;
}

void Internals_SphereCollider_SetRadius(Scene* scene, entt::entity entity,
                                        float v) {
  scene->GetComponent<SphereColliderComponent>(entity).radius = v;
}

bool Internals_SphereCollider_GetIsTrigger(Scene* scene, entt::entity entity) {
  return scene->GetComponent<SphereColliderComponent>(entity).is_trigger;
}

void Internals_SphereCollider_SetIsTrigger(Scene* scene, entt::entity entity,
                                           bool v) {
  scene->GetComponent<SphereColliderComponent>(entity).is_trigger = v;
}

// --- RigidBodyComponent ---
// All runtime physics access goes through RigidBodyComponent C++ API,
// which is the single source of truth for both C++ and C# callers.
int32_t Internals_RigidBody_GetType(Scene* scene, entt::entity entity) {
  return (int32_t)scene->GetComponent<RigidBodyComponent>(entity).type;
}

void Internals_RigidBody_SetType(Scene* scene, entt::entity entity, int32_t v) {
  auto& rb = scene->GetComponent<RigidBodyComponent>(entity);
  rb.type = (RigidBodyType)v;
  rb.is_dirty = true;
}

float Internals_RigidBody_GetMass(Scene* scene, entt::entity entity) {
  return scene->GetComponent<RigidBodyComponent>(entity).mass;
}

void Internals_RigidBody_SetMass(Scene* scene, entt::entity entity, float v) {
  scene->GetComponent<RigidBodyComponent>(entity).SetMassRuntime(v);
}

float Internals_RigidBody_GetLinearVelocityX(Scene* scene,
                                             entt::entity entity) {
  return scene->GetComponent<RigidBodyComponent>(entity).GetLinearVelocity().x;
}

float Internals_RigidBody_GetLinearVelocityY(Scene* scene,
                                             entt::entity entity) {
  return scene->GetComponent<RigidBodyComponent>(entity).GetLinearVelocity().y;
}

float Internals_RigidBody_GetLinearVelocityZ(Scene* scene,
                                             entt::entity entity) {
  return scene->GetComponent<RigidBodyComponent>(entity).GetLinearVelocity().z;
}

void Internals_RigidBody_SetLinearVelocity(Scene* scene, entt::entity entity,
                                           float x, float y, float z) {
  scene->GetComponent<RigidBodyComponent>(entity).SetLinearVelocity({x, y, z});
}

float Internals_RigidBody_GetAngularVelocityX(Scene* scene,
                                              entt::entity entity) {
  return scene->GetComponent<RigidBodyComponent>(entity).GetAngularVelocity().x;
}

float Internals_RigidBody_GetAngularVelocityY(Scene* scene,
                                              entt::entity entity) {
  return scene->GetComponent<RigidBodyComponent>(entity).GetAngularVelocity().y;
}

float Internals_RigidBody_GetAngularVelocityZ(Scene* scene,
                                              entt::entity entity) {
  return scene->GetComponent<RigidBodyComponent>(entity).GetAngularVelocity().z;
}

void Internals_RigidBody_SetAngularVelocity(Scene* scene, entt::entity entity,
                                            float x, float y, float z) {
  scene->GetComponent<RigidBodyComponent>(entity).SetAngularVelocity({x, y, z});
}

float Internals_RigidBody_GetFriction(Scene* scene, entt::entity entity) {
  return scene->GetComponent<RigidBodyComponent>(entity).friction;
}

void Internals_RigidBody_SetFriction(Scene* scene, entt::entity entity,
                                     float v) {
  scene->GetComponent<RigidBodyComponent>(entity).SetFrictionRuntime(v);
}

float Internals_RigidBody_GetRestitution(Scene* scene, entt::entity entity) {
  return scene->GetComponent<RigidBodyComponent>(entity).restitution;
}

void Internals_RigidBody_SetRestitution(Scene* scene, entt::entity entity,
                                        float v) {
  scene->GetComponent<RigidBodyComponent>(entity).SetRestitutionRuntime(v);
}

float Internals_RigidBody_GetLinearDamping(Scene* scene, entt::entity entity) {
  return scene->GetComponent<RigidBodyComponent>(entity).linear_damping;
}

void Internals_RigidBody_SetLinearDamping(Scene* scene, entt::entity entity,
                                          float v) {
  scene->GetComponent<RigidBodyComponent>(entity).SetLinearDampingRuntime(v);
}

float Internals_RigidBody_GetAngularDamping(Scene* scene, entt::entity entity) {
  return scene->GetComponent<RigidBodyComponent>(entity).angular_damping;
}

void Internals_RigidBody_SetAngularDamping(Scene* scene, entt::entity entity,
                                           float v) {
  scene->GetComponent<RigidBodyComponent>(entity).SetAngularDampingRuntime(v);
}

void Internals_RigidBody_AddForce(Scene* scene, entt::entity entity, float x,
                                  float y, float z) {
  scene->GetComponent<RigidBodyComponent>(entity).AddForce({x, y, z});
}

void Internals_RigidBody_AddImpulse(Scene* scene, entt::entity entity, float x,
                                    float y, float z) {
  scene->GetComponent<RigidBodyComponent>(entity).AddImpulse({x, y, z});
}

// --- Physics queries (backed by PhysicsWorld) ---
bool Internals_Physics_Raycast(Scene* scene, float ox, float oy, float oz,
                               float dx, float dy, float dz, float maxDist,
                               uint64_t ignoreEntity, uint64_t* outEntity,
                               float* outPx, float* outPy, float* outPz,
                               float* outNx, float* outNy, float* outNz,
                               float* outDist) {
  glm::vec3 origin(ox, oy, oz);
  glm::vec3 dir = glm::normalize(glm::vec3(dx, dy, dz));
  glm::vec3 to = origin + dir * maxDist;

  entt::entity ignore =
      (ignoreEntity != 0)
          ? static_cast<entt::entity>(static_cast<uint32_t>(ignoreEntity))
          : entt::null;

  RaycastHit hit;
  if (scene->GetPhysicsWorld().Raycast(origin, to, hit, ignore)) {
    *outEntity = (uint64_t)hit.entity;
    *outPx = hit.point.x;
    *outPy = hit.point.y;
    *outPz = hit.point.z;
    *outNx = hit.normal.x;
    *outNy = hit.normal.y;
    *outNz = hit.normal.z;
    *outDist = hit.distance;
    return true;
  }
  return false;
}

MonoArray* Internals_Physics_OverlapBox(Scene* scene, float cx, float cy,
                                        float cz, float hx, float hy,
                                        float hz) {
  auto entities = scene->GetPhysicsWorld().OverlapBox(glm::vec3(cx, cy, cz),
                                                      glm::vec3(hx, hy, hz));

  MonoArray* arr = mono_array_new(Engine::script_manager().app_domain(),
                                  mono_get_uint64_class(), entities.size());
  for (size_t i = 0; i < entities.size(); i++) {
    mono_array_set(arr, uint64_t, i, (uint64_t)entities[i]);
  }
  return arr;
}

MonoArray* Internals_Physics_OverlapSphere(Scene* scene, float cx, float cy,
                                           float cz, float radius) {
  auto entities =
      scene->GetPhysicsWorld().OverlapSphere(glm::vec3(cx, cy, cz), radius);

  MonoArray* arr = mono_array_new(Engine::script_manager().app_domain(),
                                  mono_get_uint64_class(), entities.size());
  for (size_t i = 0; i < entities.size(); i++) {
    mono_array_set(arr, uint64_t, i, (uint64_t)entities[i]);
  }
  return arr;
}

MonoObject* CreateVector3fWithValues(float x, float y, float z) {
  MonoObject* obj = mono_object_new(Engine::script_manager().app_domain(),
                                    Engine::script_manager().vector3f_class());
  void* args[3];
  args[0] = &x;
  args[1] = &y;
  args[2] = &z;
  MonoMethod* method = mono_class_get_method_from_name(
      Engine::script_manager().vector3f_class(), ".ctor", 3);
  InvokeSafe(method, obj, args);
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

MonoObject* Internals_TransformComponent_GetWorldPosition(Scene* scene,
                                                          entt::entity entity) {
  auto& c = scene->GetComponent<TransformComponent>(entity);
  glm::vec3 val = c.GetWorldPosition();
  return CreateVector3fWithValues(val.x, val.y, val.z);
}

MonoObject* Internals_TransformComponent_GetWorldScale(Scene* scene,
                                                       entt::entity entity) {
  auto& c = scene->GetComponent<TransformComponent>(entity);
  glm::vec3 val = c.GetWorldScale();
  return CreateVector3fWithValues(val.x, val.y, val.z);
}

MonoObject* Internals_TransformComponent_LocalToWorldDirection(
    Scene* scene, entt::entity entity, float x, float y, float z) {
  auto& c = scene->GetComponent<TransformComponent>(entity);
  glm::vec3 val = c.LocalToWorldDirection({x, y, z});
  return CreateVector3fWithValues(val.x, val.y, val.z);
}

MonoObject* Internals_TransformComponent_WorldToLocalDirection(
    Scene* scene, entt::entity entity, float x, float y, float z) {
  auto& c = scene->GetComponent<TransformComponent>(entity);
  glm::vec3 val = c.WorldToLocalDirection({x, y, z});
  return CreateVector3fWithValues(val.x, val.y, val.z);
}

MonoObject* Internals_TransformComponent_LocalToWorldPoint(Scene* scene,
                                                           entt::entity entity,
                                                           float x, float y,
                                                           float z) {
  auto& c = scene->GetComponent<TransformComponent>(entity);
  glm::vec3 val = c.LocalToWorldPoint({x, y, z});
  return CreateVector3fWithValues(val.x, val.y, val.z);
}

MonoObject* Internals_TransformComponent_WorldToLocalPoint(Scene* scene,
                                                           entt::entity entity,
                                                           float x, float y,
                                                           float z) {
  auto& c = scene->GetComponent<TransformComponent>(entity);
  glm::vec3 val = c.WorldToLocalPoint({x, y, z});
  return CreateVector3fWithValues(val.x, val.y, val.z);
}

void Internals_TransformComponent_Translate(Scene* scene, entt::entity entity,
                                            float x, float y, float z,
                                            int space) {
  auto& c = scene->GetComponent<TransformComponent>(entity);
  c.Translate({x, y, z}, static_cast<TransformComponent::Space>(space));
}

// --- RectangleTransformComponent ---
float Internals_RectTransform_GetPositionX(Scene* s, entt::entity e) {
  return s->GetComponent<RectangleTransformComponent>(e).position.x;
}

float Internals_RectTransform_GetPositionY(Scene* s, entt::entity e) {
  return s->GetComponent<RectangleTransformComponent>(e).position.y;
}

void Internals_RectTransform_SetPositionX(Scene* s, entt::entity e, float v) {
  auto& c = s->GetComponent<RectangleTransformComponent>(e);
  c.position.x = v;
  c.MarkChanged();
}

void Internals_RectTransform_SetPositionY(Scene* s, entt::entity e, float v) {
  auto& c = s->GetComponent<RectangleTransformComponent>(e);
  c.position.y = v;
  c.MarkChanged();
}

float Internals_RectTransform_GetRotation(Scene* s, entt::entity e) {
  return s->GetComponent<RectangleTransformComponent>(e).rotation;
}

void Internals_RectTransform_SetRotation(Scene* s, entt::entity e, float v) {
  auto& c = s->GetComponent<RectangleTransformComponent>(e);
  c.rotation = v;
  c.MarkChanged();
}

float Internals_RectTransform_GetSizeX(Scene* s, entt::entity e) {
  return s->GetComponent<RectangleTransformComponent>(e).size.x;
}

float Internals_RectTransform_GetSizeY(Scene* s, entt::entity e) {
  return s->GetComponent<RectangleTransformComponent>(e).size.y;
}

void Internals_RectTransform_SetSizeX(Scene* s, entt::entity e, float v) {
  auto& c = s->GetComponent<RectangleTransformComponent>(e);
  c.size.x = v;
  c.MarkChanged();
}

void Internals_RectTransform_SetSizeY(Scene* s, entt::entity e, float v) {
  auto& c = s->GetComponent<RectangleTransformComponent>(e);
  c.size.y = v;
  c.MarkChanged();
}

float Internals_RectTransform_GetScaleX(Scene* s, entt::entity e) {
  return s->GetComponent<RectangleTransformComponent>(e).scale.x;
}

float Internals_RectTransform_GetScaleY(Scene* s, entt::entity e) {
  return s->GetComponent<RectangleTransformComponent>(e).scale.y;
}

void Internals_RectTransform_SetScaleX(Scene* s, entt::entity e, float v) {
  auto& c = s->GetComponent<RectangleTransformComponent>(e);
  c.scale.x = v;
  c.MarkChanged();
}

void Internals_RectTransform_SetScaleY(Scene* s, entt::entity e, float v) {
  auto& c = s->GetComponent<RectangleTransformComponent>(e);
  c.scale.y = v;
  c.MarkChanged();
}

int32_t Internals_RectTransform_GetAnchor(Scene* s, entt::entity e) {
  return static_cast<int32_t>(
      s->GetComponent<RectangleTransformComponent>(e).anchor);
}

void Internals_RectTransform_SetAnchor(Scene* s, entt::entity e, int32_t v) {
  auto& c = s->GetComponent<RectangleTransformComponent>(e);
  c.anchor = static_cast<AnchorPreset>(v);
  c.MarkChanged();
}

int32_t Internals_RectTransform_GetPivot(Scene* s, entt::entity e) {
  return static_cast<int32_t>(
      s->GetComponent<RectangleTransformComponent>(e).pivot);
}

void Internals_RectTransform_SetPivot(Scene* s, entt::entity e, int32_t v) {
  auto& c = s->GetComponent<RectangleTransformComponent>(e);
  c.pivot = static_cast<AnchorPreset>(v);
  c.MarkChanged();
}

int32_t Internals_RectTransform_GetSizeModeX(Scene* s, entt::entity e) {
  return static_cast<int32_t>(
      s->GetComponent<RectangleTransformComponent>(e).size_mode_x);
}

void Internals_RectTransform_SetSizeModeX(Scene* s, entt::entity e, int32_t v) {
  auto& c = s->GetComponent<RectangleTransformComponent>(e);
  c.size_mode_x = static_cast<SizeMode>(v);
  c.MarkChanged();
}

int32_t Internals_RectTransform_GetSizeModeY(Scene* s, entt::entity e) {
  return static_cast<int32_t>(
      s->GetComponent<RectangleTransformComponent>(e).size_mode_y);
}

void Internals_RectTransform_SetSizeModeY(Scene* s, entt::entity e, int32_t v) {
  auto& c = s->GetComponent<RectangleTransformComponent>(e);
  c.size_mode_y = static_cast<SizeMode>(v);
  c.MarkChanged();
}

float Internals_RectTransform_GetPaddingLeft(Scene* s, entt::entity e) {
  return s->GetComponent<RectangleTransformComponent>(e).padding.x;
}

float Internals_RectTransform_GetPaddingTop(Scene* s, entt::entity e) {
  return s->GetComponent<RectangleTransformComponent>(e).padding.y;
}

float Internals_RectTransform_GetPaddingRight(Scene* s, entt::entity e) {
  return s->GetComponent<RectangleTransformComponent>(e).padding.z;
}

float Internals_RectTransform_GetPaddingBottom(Scene* s, entt::entity e) {
  return s->GetComponent<RectangleTransformComponent>(e).padding.w;
}

void Internals_RectTransform_SetPaddingLeft(Scene* s, entt::entity e, float v) {
  auto& c = s->GetComponent<RectangleTransformComponent>(e);
  c.padding.x = v;
  c.MarkChanged();
}

void Internals_RectTransform_SetPaddingTop(Scene* s, entt::entity e, float v) {
  auto& c = s->GetComponent<RectangleTransformComponent>(e);
  c.padding.y = v;
  c.MarkChanged();
}

void Internals_RectTransform_SetPaddingRight(Scene* s, entt::entity e,
                                             float v) {
  auto& c = s->GetComponent<RectangleTransformComponent>(e);
  c.padding.z = v;
  c.MarkChanged();
}

void Internals_RectTransform_SetPaddingBottom(Scene* s, entt::entity e,
                                              float v) {
  auto& c = s->GetComponent<RectangleTransformComponent>(e);
  c.padding.w = v;
  c.MarkChanged();
}

float Internals_RectTransform_GetComputedPositionX(Scene* s, entt::entity e) {
  return s->GetComponent<RectangleTransformComponent>(e).computed_position.x;
}

float Internals_RectTransform_GetComputedPositionY(Scene* s, entt::entity e) {
  return s->GetComponent<RectangleTransformComponent>(e).computed_position.y;
}

float Internals_RectTransform_GetComputedSizeX(Scene* s, entt::entity e) {
  return s->GetComponent<RectangleTransformComponent>(e).computed_size.x;
}

float Internals_RectTransform_GetComputedSizeY(Scene* s, entt::entity e) {
  return s->GetComponent<RectangleTransformComponent>(e).computed_size.y;
}

// --- CanvasComponent ---
int32_t Internals_Canvas_GetDirection(Scene* s, entt::entity e) {
  return static_cast<int32_t>(s->GetComponent<CanvasComponent>(e).direction);
}

void Internals_Canvas_SetDirection(Scene* s, entt::entity e, int32_t v) {
  s->GetComponent<CanvasComponent>(e).direction =
      static_cast<LayoutDirection>(v);
}

int32_t Internals_Canvas_GetAlignment(Scene* s, entt::entity e) {
  return static_cast<int32_t>(s->GetComponent<CanvasComponent>(e).alignment);
}

void Internals_Canvas_SetAlignment(Scene* s, entt::entity e, int32_t v) {
  s->GetComponent<CanvasComponent>(e).alignment =
      static_cast<ChildAlignment>(v);
}

float Internals_Canvas_GetSpacing(Scene* s, entt::entity e) {
  return s->GetComponent<CanvasComponent>(e).spacing;
}

void Internals_Canvas_SetSpacing(Scene* s, entt::entity e, float v) {
  s->GetComponent<CanvasComponent>(e).spacing = v;
}

int32_t Internals_Canvas_GetSortOrder(Scene* s, entt::entity e) {
  return s->GetComponent<CanvasComponent>(e).sort_order;
}

void Internals_Canvas_SetSortOrder(Scene* s, entt::entity e, int32_t v) {
  s->GetComponent<CanvasComponent>(e).sort_order = v;
}

// --- CanvasRectComponent ---
float Internals_CanvasRect_GetColorR(Scene* s, entt::entity e) {
  return s->GetComponent<CanvasRectComponent>(e).color.r;
}

float Internals_CanvasRect_GetColorG(Scene* s, entt::entity e) {
  return s->GetComponent<CanvasRectComponent>(e).color.g;
}

float Internals_CanvasRect_GetColorB(Scene* s, entt::entity e) {
  return s->GetComponent<CanvasRectComponent>(e).color.b;
}

float Internals_CanvasRect_GetColorA(Scene* s, entt::entity e) {
  return s->GetComponent<CanvasRectComponent>(e).color.a;
}

void Internals_CanvasRect_SetColorR(Scene* s, entt::entity e, float v) {
  s->GetComponent<CanvasRectComponent>(e).color.r = v;
}

void Internals_CanvasRect_SetColorG(Scene* s, entt::entity e, float v) {
  s->GetComponent<CanvasRectComponent>(e).color.g = v;
}

void Internals_CanvasRect_SetColorB(Scene* s, entt::entity e, float v) {
  s->GetComponent<CanvasRectComponent>(e).color.b = v;
}

void Internals_CanvasRect_SetColorA(Scene* s, entt::entity e, float v) {
  s->GetComponent<CanvasRectComponent>(e).color.a = v;
}

// --- CanvasImageComponent ---
float Internals_CanvasImage_GetTintR(Scene* s, entt::entity e) {
  return s->GetComponent<CanvasImageComponent>(e).tint.r;
}

float Internals_CanvasImage_GetTintG(Scene* s, entt::entity e) {
  return s->GetComponent<CanvasImageComponent>(e).tint.g;
}

float Internals_CanvasImage_GetTintB(Scene* s, entt::entity e) {
  return s->GetComponent<CanvasImageComponent>(e).tint.b;
}

float Internals_CanvasImage_GetTintA(Scene* s, entt::entity e) {
  return s->GetComponent<CanvasImageComponent>(e).tint.a;
}

void Internals_CanvasImage_SetTintR(Scene* s, entt::entity e, float v) {
  s->GetComponent<CanvasImageComponent>(e).tint.r = v;
}

void Internals_CanvasImage_SetTintG(Scene* s, entt::entity e, float v) {
  s->GetComponent<CanvasImageComponent>(e).tint.g = v;
}

void Internals_CanvasImage_SetTintB(Scene* s, entt::entity e, float v) {
  s->GetComponent<CanvasImageComponent>(e).tint.b = v;
}

void Internals_CanvasImage_SetTintA(Scene* s, entt::entity e, float v) {
  s->GetComponent<CanvasImageComponent>(e).tint.a = v;
}

float Internals_CanvasImage_GetUVRectX(Scene* s, entt::entity e) {
  return s->GetComponent<CanvasImageComponent>(e).uv_rect.x;
}

float Internals_CanvasImage_GetUVRectY(Scene* s, entt::entity e) {
  return s->GetComponent<CanvasImageComponent>(e).uv_rect.y;
}

float Internals_CanvasImage_GetUVRectZ(Scene* s, entt::entity e) {
  return s->GetComponent<CanvasImageComponent>(e).uv_rect.z;
}

float Internals_CanvasImage_GetUVRectW(Scene* s, entt::entity e) {
  return s->GetComponent<CanvasImageComponent>(e).uv_rect.w;
}

void Internals_CanvasImage_SetUVRectX(Scene* s, entt::entity e, float v) {
  s->GetComponent<CanvasImageComponent>(e).uv_rect.x = v;
}

void Internals_CanvasImage_SetUVRectY(Scene* s, entt::entity e, float v) {
  s->GetComponent<CanvasImageComponent>(e).uv_rect.y = v;
}

void Internals_CanvasImage_SetUVRectZ(Scene* s, entt::entity e, float v) {
  s->GetComponent<CanvasImageComponent>(e).uv_rect.z = v;
}

void Internals_CanvasImage_SetUVRectW(Scene* s, entt::entity e, float v) {
  s->GetComponent<CanvasImageComponent>(e).uv_rect.w = v;
}

// --- TextComponent ---
MonoString* Internals_Text_GetText(Scene* s, entt::entity e) {
  return mono_string_new(Engine::script_manager().app_domain(),
                         s->GetComponent<TextComponent>(e).text.c_str());
}

void Internals_Text_SetText(Scene* s, entt::entity e, MonoString* v) {
  const char* str = mono_string_to_utf8(v);
  auto& c = s->GetComponent<TextComponent>(e);
  c.text = str;
  c.gpu_dirty_ = true;
  mono_free((void*)str);
}

float Internals_Text_GetFontSize(Scene* s, entt::entity e) {
  return s->GetComponent<TextComponent>(e).font_size;
}

void Internals_Text_SetFontSize(Scene* s, entt::entity e, float v) {
  auto& c = s->GetComponent<TextComponent>(e);
  c.font_size = v;
  c.gpu_dirty_ = true;
}

float Internals_Text_GetColorR(Scene* s, entt::entity e) {
  return s->GetComponent<TextComponent>(e).color.r;
}

float Internals_Text_GetColorG(Scene* s, entt::entity e) {
  return s->GetComponent<TextComponent>(e).color.g;
}

float Internals_Text_GetColorB(Scene* s, entt::entity e) {
  return s->GetComponent<TextComponent>(e).color.b;
}

float Internals_Text_GetColorA(Scene* s, entt::entity e) {
  return s->GetComponent<TextComponent>(e).color.a;
}

void Internals_Text_SetColorR(Scene* s, entt::entity e, float v) {
  auto& c = s->GetComponent<TextComponent>(e);
  c.color.r = v;
  c.gpu_dirty_ = true;
}

void Internals_Text_SetColorG(Scene* s, entt::entity e, float v) {
  auto& c = s->GetComponent<TextComponent>(e);
  c.color.g = v;
  c.gpu_dirty_ = true;
}

void Internals_Text_SetColorB(Scene* s, entt::entity e, float v) {
  auto& c = s->GetComponent<TextComponent>(e);
  c.color.b = v;
  c.gpu_dirty_ = true;
}

void Internals_Text_SetColorA(Scene* s, entt::entity e, float v) {
  auto& c = s->GetComponent<TextComponent>(e);
  c.color.a = v;
  c.gpu_dirty_ = true;
}

// --- AnimatorComponent ---
void Internals_Animator_SetBool(Scene* s, entt::entity e, MonoString* name,
                                bool value) {
  const char* cstr = mono_string_to_utf8(name);
  s->GetComponent<AnimatorComponent>(e).SetBool(cstr, value);
  mono_free((void*)cstr);
}

void Internals_Animator_SetInt(Scene* s, entt::entity e, MonoString* name,
                               int value) {
  const char* cstr = mono_string_to_utf8(name);
  s->GetComponent<AnimatorComponent>(e).SetInt(cstr, value);
  mono_free((void*)cstr);
}

void Internals_Animator_SetFloat(Scene* s, entt::entity e, MonoString* name,
                                 float value) {
  const char* cstr = mono_string_to_utf8(name);
  s->GetComponent<AnimatorComponent>(e).SetFloat(cstr, value);
  mono_free((void*)cstr);
}

void Internals_Animator_SetTrigger(Scene* s, entt::entity e, MonoString* name) {
  const char* cstr = mono_string_to_utf8(name);
  s->GetComponent<AnimatorComponent>(e).SetTrigger(cstr);
  mono_free((void*)cstr);
}

bool Internals_Animator_GetBool(Scene* s, entt::entity e, MonoString* name) {
  const char* cstr = mono_string_to_utf8(name);
  bool val = s->GetComponent<AnimatorComponent>(e).GetBool(cstr);
  mono_free((void*)cstr);
  return val;
}

int Internals_Animator_GetInt(Scene* s, entt::entity e, MonoString* name) {
  const char* cstr = mono_string_to_utf8(name);
  int val = s->GetComponent<AnimatorComponent>(e).GetInt(cstr);
  mono_free((void*)cstr);
  return val;
}

float Internals_Animator_GetFloat(Scene* s, entt::entity e, MonoString* name) {
  const char* cstr = mono_string_to_utf8(name);
  float val = s->GetComponent<AnimatorComponent>(e).GetFloat(cstr);
  mono_free((void*)cstr);
  return val;
}

void Internals_Animator_Play(Scene* s, entt::entity e, MonoString* stateName,
                             float blendTime) {
  const char* cstr = mono_string_to_utf8(stateName);
  s->GetComponent<AnimatorComponent>(e).Play(cstr, blendTime);
  mono_free((void*)cstr);
}

MonoString* Internals_Animator_GetCurrentState(Scene* s, entt::entity e) {
  auto& anim = s->GetComponent<AnimatorComponent>(e);
  return mono_string_new(Engine::script_manager().app_domain(),
                         anim.current_state_name.c_str());
}

bool Internals_Animator_GetIsPlaying(Scene* s, entt::entity e) {
  return s->GetComponent<AnimatorComponent>(e).playing;
}

void Internals_Animator_SetIsPlaying(Scene* s, entt::entity e, bool value) {
  s->GetComponent<AnimatorComponent>(e).playing = value;
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
  if (!errored_) {
    OnDestroy();
  }
  mono_gchandle_free(gc_handle_);
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

  // Unregister old script assets before re-registering
  AssetManager& mgr = Engine::asset_manager();
  for (AssetHandle handle : mgr.GetAllOfType(AssetType::Script)) {
    mgr.Unregister(handle);
  }

  mono_domain_set(root_domain_, true);
  if (app_domain_) {
    mono_domain_unload(app_domain_);
    app_domain_ = nullptr;
  }

  script_data_.clear();
  script_names_.clear();

  RegisterComponents();
  RegisterInternals();
  LoadCore();
  LoadApp();

  ScriptsReloadedEvent event{};
  Engine::BroadcastEvent(event);
}

void ScriptManager::ReloadAsync() {
  if (compiling_) {
    return;
  }

  std::optional<std::filesystem::path> physical =
      Engine::vfs()->GetPhysicalPath("/app/scripts");
  if (!physical.has_value() || !std::filesystem::exists(*physical)) {
    Reload();
    return;
  }

  // Collect source files on the main thread (fast, no Mono calls)
  std::vector<std::string> source_files;
  for (const auto& entry :
       std::filesystem::recursive_directory_iterator(*physical)) {
    if (entry.is_regular_file() && entry.path().extension() == ".cs") {
      source_files.push_back(entry.path().string());
    }
  }

  if (source_files.empty()) {
    Reload();
    return;
  }

  std::vector<std::string> link_libs;
  for (const auto& entry : std::filesystem::directory_iterator(".")) {
    if (entry.is_regular_file() && entry.path().extension() == ".dll" &&
        entry.path().filename() != "App.dll") {
      link_libs.push_back(entry.path().string());
    }
  }

  // Only compile on the background thread - do NOT touch Mono or unload anything.
  // Old scripts keep running. Domain swap happens in FinishReloadIfReady.
  LOG_INFO("Compiling scripts (async)...");
  pending_dll_path_ = "./App.dll";
  bool debug = enable_debugger_;
  std::string dll_path = pending_dll_path_;

  compiling_ = true;
  compile_future_ = std::async(
      std::launch::async, [dll_path, source_files, link_libs, debug]() {
        return CompileToDLL(dll_path, source_files, "", link_libs, debug);
      });
}

bool ScriptManager::FinishReloadIfReady() {
  if (!compiling_) {
    return false;
  }
  if (compile_future_.wait_for(std::chrono::milliseconds(0)) !=
      std::future_status::ready) {
    return false;  // still compiling
  }

  last_compile_result_ = compile_future_.get();
  compiling_ = false;

  if (!last_compile_result_.success) {
    LOG_ERROR("Script compilation failed (exit code {}):\n{}",
              last_compile_result_.exit_code, last_compile_result_.output);
    return true;
  }

  // Compilation succeeded - now do the atomic domain swap on the main thread.
  // Unregister old scripts, unload old domain, reload everything with new DLL.
  AssetManager& mgr = Engine::asset_manager();
  for (AssetHandle handle : mgr.GetAllOfType(AssetType::Script)) {
    mgr.Unregister(handle);
  }

  mono_domain_set(root_domain_, true);
  if (app_domain_) {
    mono_domain_unload(app_domain_);
    app_domain_ = nullptr;
  }

  script_data_.clear();
  script_names_.clear();

  RegisterComponents();
  RegisterInternals();
  LoadCore();

  // Register script assets
  std::optional<std::filesystem::path> physical =
      Engine::vfs()->GetPhysicalPath("/app/scripts");
  if (physical.has_value() && std::filesystem::exists(*physical)) {
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(*physical)) {
      if (entry.is_regular_file() && entry.path().extension() == ".cs") {
        std::filesystem::path rel =
            std::filesystem::relative(entry.path(), *physical);
        std::string vfs_path = "/app/scripts/" + rel.generic_string();
        mgr.Register(entry.path().stem().string(), AssetType::Script, vfs_path);
      }
    }
  }

  // Load the compiled DLL (shared with LoadApp)
  if (!LoadAppDll(pending_dll_path_)) {
    return true;
  }

  LOG_INFO("Scripts compiled and loaded ({} scripts)", script_names_.size());
  ScriptsReloadedEvent event{};
  Engine::BroadcastEvent(event);
  return true;
}

void ScriptManager::LoadCore() {
  std::string dll_path = "./Core.dll";

  // If source files are available, always compile (ensures latest code)
  std::optional<std::filesystem::path> physical =
      Engine::vfs()->GetPhysicalPath("/engine/scripts");
  if (physical.has_value() && std::filesystem::exists(*physical)) {
    std::vector<std::string> source_files;
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(*physical)) {
      if (entry.is_regular_file() && entry.path().extension() == ".cs") {
        source_files.push_back(entry.path().string());

        std::filesystem::path rel =
            std::filesystem::relative(entry.path(), *physical);
        std::string vfs_path = "/engine/scripts/" + rel.generic_string();
        std::string script_name = entry.path().stem().string();
        Engine::asset_manager().Register(script_name, AssetType::Script,
                                         vfs_path);
      }
    }
    LOG_INFO("Compiling core scripts ({} files)...", source_files.size());
    last_compile_result_ =
        CompileToDLL(dll_path, source_files, "", {}, enable_debugger_);
    if (!last_compile_result_.success) {
      LOG_ERROR("Core script compilation failed (exit code {}):\n{}",
                last_compile_result_.exit_code, last_compile_result_.output);
    }
  } else if (!std::filesystem::exists(dll_path)) {
    LOG_ERROR("No core scripts or pre-compiled Core.dll found");
  }

  LOG_INFO("Loading Core.dll from {}", dll_path);
  core_assembly_ = mono_domain_assembly_open(root_domain_, dll_path.c_str());
  assert(core_assembly_);

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

void ScriptManager::LoadApp() {
  std::string dll_path = "./App.dll";

  // If source files are available, always compile (ensures latest code)
  std::optional<std::filesystem::path> physical =
      Engine::vfs()->GetPhysicalPath("/app/scripts");
  if (physical.has_value() && std::filesystem::exists(*physical)) {
    std::vector<std::string> source_files;
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(*physical)) {
      if (entry.is_regular_file() && entry.path().extension() == ".cs") {
        source_files.push_back(entry.path().string());

        std::filesystem::path rel =
            std::filesystem::relative(entry.path(), *physical);
        std::string vfs_path = "/app/scripts/" + rel.generic_string();
        std::string script_name = entry.path().stem().string();
        Engine::asset_manager().Register(script_name, AssetType::Script,
                                         vfs_path);
      }
    }

    if (!source_files.empty()) {
      // Find compiled DLLs to reference (e.g. Core.dll)
      std::vector<std::string> link_libs;
      for (const auto& entry : std::filesystem::directory_iterator(".")) {
        if (entry.is_regular_file() && entry.path().extension() == ".dll" &&
            entry.path().filename() != "App.dll") {
          link_libs.push_back(entry.path().string());
        }
      }

      LOG_INFO("Compiling app scripts ({} files, {} references)...",
               source_files.size(), link_libs.size());
      last_compile_result_ =
          CompileToDLL(dll_path, source_files, "", link_libs, enable_debugger_);
      if (!last_compile_result_.success) {
        LOG_ERROR("App script compilation failed (exit code {}):\n{}",
                  last_compile_result_.exit_code, last_compile_result_.output);
        return;
      }

      LoadAppDll(dll_path);
      return;
    }
  }

  // No source files - fall back to pre-compiled DLL
  if (std::filesystem::exists(dll_path)) {
    LoadAppDll(dll_path);
  } else {
    LOG_WARN("No app scripts or pre-compiled App.dll found");
  }
}

bool ScriptManager::LoadAppDll(const std::string& dll_path) {
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

    std::unordered_map<std::string, FieldData> fields;
    MonoClassField* field;
    void* iter = nullptr;
    while ((field = mono_class_get_fields(klass, &iter))) {
      std::string field_name = mono_field_get_name(field);
      uint32_t field_flags = mono_field_get_flags(field);
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

// SceneManager internal calls
void Internals_SceneManager_LoadScene(MonoString* name) {
  const char* cstr = mono_string_to_utf8(name);
  Engine::scene_manager().LoadScene(cstr);
  mono_free((void*)cstr);
}

void Internals_SceneManager_LoadScenePath(MonoString* path) {
  const char* cstr = mono_string_to_utf8(path);
  Engine::scene_manager().LoadSceneFromPath(cstr);
  mono_free((void*)cstr);
}

void Internals_SceneManager_LoadSceneWithLoading(MonoString* target,
                                                 MonoString* loading) {
  const char* t = mono_string_to_utf8(target);
  const char* l = mono_string_to_utf8(loading);
  Engine::scene_manager().LoadSceneWithLoading(t, l);
  mono_free((void*)t);
  mono_free((void*)l);
}

float Internals_SceneManager_GetLoadProgress() {
  return Engine::scene_manager().GetLoadProgress();
}

bool Internals_SceneManager_IsSceneReady() {
  return Engine::scene_manager().IsSceneReady();
}

void Internals_SceneManager_ActivateLoadedScene() {
  Engine::scene_manager().ActivateLoadedScene();
}

uint64_t Internals_Prefab_Instantiate(uint64_t scene_ptr, MonoString* path) {
  if (scene_ptr == 0) {
    return 0;
  }
  Scene* scene = reinterpret_cast<Scene*>(scene_ptr);
  const char* cstr = mono_string_to_utf8(path);

  // Try VFS path first, then filesystem
  std::filesystem::path fs_path;
  auto physical = Engine::vfs()->GetPhysicalPath(cstr);
  if (physical.has_value()) {
    fs_path = *physical;
  } else {
    fs_path = cstr;
  }
  mono_free((void*)cstr);

  auto shared_scene = Engine::scene_manager().GetActiveScene();
  if (!shared_scene) {
    LOG_ERROR("Prefab_Instantiate: no active scene");
    return 0;
  }

  Entity entity = Prefab::InstantiateFromFile(shared_scene, fs_path);
  return static_cast<uint32_t>(entity.handle());
}

// Console
void Internals_Console_RegisterCommand(MonoString* name,
                                       MonoString* description,
                                       MonoObject* callback) {
  const char* name_cstr = mono_string_to_utf8(name);
  const char* desc_cstr = mono_string_to_utf8(description);
  std::string name_str = name_cstr;
  std::string desc_str = desc_cstr;
  mono_free((void*)name_cstr);
  mono_free((void*)desc_cstr);

  uint32_t gc_handle = mono_gchandle_new(callback, true);

  Engine::console().Register(
      name_str, desc_str, [gc_handle](const std::vector<std::string>& args) {
        MonoObject* delegate = mono_gchandle_get_target(gc_handle);
        if (!delegate) {
          return;
        }

        MonoDomain* domain = mono_domain_get();
        MonoArray* mono_args =
            mono_array_new(domain, mono_get_string_class(), args.size());
        for (size_t i = 0; i < args.size(); i++) {
          mono_array_set(mono_args, MonoString*, i,
                         mono_string_new(domain, args[i].c_str()));
        }

        void* invoke_args[1] = {mono_args};
        MonoObject* exception = nullptr;
        MonoMethod* invoke_method =
            mono_get_delegate_invoke(mono_object_get_class(delegate));
        mono_runtime_invoke(invoke_method, delegate, invoke_args, &exception);

        if (exception) {
          MonoString* exc_str = mono_object_to_string(exception, nullptr);
          if (exc_str) {
            const char* exc_cstr = mono_string_to_utf8(exc_str);
            Engine::console().LogError(exc_cstr);
            mono_free((void*)exc_cstr);
          }
        }
      });
}

void Internals_Console_UnregisterCommand(MonoString* name) {
  const char* cstr = mono_string_to_utf8(name);
  Engine::console().Unregister(cstr);
  mono_free((void*)cstr);
}

void Internals_Console_Execute(MonoString* command_line) {
  const char* cstr = mono_string_to_utf8(command_line);
  Engine::console().Execute(cstr);
  mono_free((void*)cstr);
}

void Internals_Console_LogInfo(MonoString* message) {
  const char* cstr = mono_string_to_utf8(message);
  Engine::console().LogInfo(cstr);
  mono_free((void*)cstr);
}

void Internals_Console_LogWarning(MonoString* message) {
  const char* cstr = mono_string_to_utf8(message);
  Engine::console().LogWarning(cstr);
  mono_free((void*)cstr);
}

void Internals_Console_LogError(MonoString* message) {
  const char* cstr = mono_string_to_utf8(message);
  Engine::console().LogError(cstr);
  mono_free((void*)cstr);
}

void ScriptManager::RegisterInternals() {
  WIESEL_ADD_INTERNAL_CALL(Log_Info);
  WIESEL_ADD_INTERNAL_CALL(Input_GetAxis);
  WIESEL_ADD_INTERNAL_CALL(Input_GetKey);
  WIESEL_ADD_INTERNAL_CALL(Input_GetKeyDown);
  WIESEL_ADD_INTERNAL_CALL(Input_GetKeyUp);
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
  WIESEL_ADD_INTERNAL_CALL(TransformComponent_GetWorldPosition);
  WIESEL_ADD_INTERNAL_CALL(TransformComponent_GetWorldScale);
  WIESEL_ADD_INTERNAL_CALL(TransformComponent_LocalToWorldDirection);
  WIESEL_ADD_INTERNAL_CALL(TransformComponent_WorldToLocalDirection);
  WIESEL_ADD_INTERNAL_CALL(TransformComponent_LocalToWorldPoint);
  WIESEL_ADD_INTERNAL_CALL(TransformComponent_WorldToLocalPoint);
  WIESEL_ADD_INTERNAL_CALL(TransformComponent_Translate);
  WIESEL_ADD_INTERNAL_CALL(ModelComponent_GetEnableRendering);
  WIESEL_ADD_INTERNAL_CALL(ModelComponent_SetEnableRendering);
  WIESEL_ADD_INTERNAL_CALL(ModelComponent_GetColorTintR);
  WIESEL_ADD_INTERNAL_CALL(ModelComponent_GetColorTintG);
  WIESEL_ADD_INTERNAL_CALL(ModelComponent_GetColorTintB);
  WIESEL_ADD_INTERNAL_CALL(ModelComponent_GetColorTintA);
  WIESEL_ADD_INTERNAL_CALL(ModelComponent_SetColorTintR);
  WIESEL_ADD_INTERNAL_CALL(ModelComponent_SetColorTintG);
  WIESEL_ADD_INTERNAL_CALL(ModelComponent_SetColorTintB);
  WIESEL_ADD_INTERNAL_CALL(ModelComponent_SetColorTintA);
  WIESEL_ADD_INTERNAL_CALL(ModelComponent_GetRoughness);
  WIESEL_ADD_INTERNAL_CALL(ModelComponent_SetRoughness);
  WIESEL_ADD_INTERNAL_CALL(ModelComponent_GetMetallic);
  WIESEL_ADD_INTERNAL_CALL(ModelComponent_SetMetallic);
  WIESEL_ADD_INTERNAL_CALL(ModelComponent_GetSpecular);
  WIESEL_ADD_INTERNAL_CALL(ModelComponent_SetSpecular);
  // BoxColliderComponent
  WIESEL_ADD_INTERNAL_CALL(BoxCollider_GetOffsetX);
  WIESEL_ADD_INTERNAL_CALL(BoxCollider_GetOffsetY);
  WIESEL_ADD_INTERNAL_CALL(BoxCollider_GetOffsetZ);
  WIESEL_ADD_INTERNAL_CALL(BoxCollider_SetOffsetX);
  WIESEL_ADD_INTERNAL_CALL(BoxCollider_SetOffsetY);
  WIESEL_ADD_INTERNAL_CALL(BoxCollider_SetOffsetZ);
  WIESEL_ADD_INTERNAL_CALL(BoxCollider_GetHalfExtentsX);
  WIESEL_ADD_INTERNAL_CALL(BoxCollider_GetHalfExtentsY);
  WIESEL_ADD_INTERNAL_CALL(BoxCollider_GetHalfExtentsZ);
  WIESEL_ADD_INTERNAL_CALL(BoxCollider_SetHalfExtentsX);
  WIESEL_ADD_INTERNAL_CALL(BoxCollider_SetHalfExtentsY);
  WIESEL_ADD_INTERNAL_CALL(BoxCollider_SetHalfExtentsZ);
  WIESEL_ADD_INTERNAL_CALL(BoxCollider_GetIsTrigger);
  WIESEL_ADD_INTERNAL_CALL(BoxCollider_SetIsTrigger);
  // SphereColliderComponent
  WIESEL_ADD_INTERNAL_CALL(SphereCollider_GetOffsetX);
  WIESEL_ADD_INTERNAL_CALL(SphereCollider_GetOffsetY);
  WIESEL_ADD_INTERNAL_CALL(SphereCollider_GetOffsetZ);
  WIESEL_ADD_INTERNAL_CALL(SphereCollider_SetOffsetX);
  WIESEL_ADD_INTERNAL_CALL(SphereCollider_SetOffsetY);
  WIESEL_ADD_INTERNAL_CALL(SphereCollider_SetOffsetZ);
  WIESEL_ADD_INTERNAL_CALL(SphereCollider_GetRadius);
  WIESEL_ADD_INTERNAL_CALL(SphereCollider_SetRadius);
  WIESEL_ADD_INTERNAL_CALL(SphereCollider_GetIsTrigger);
  WIESEL_ADD_INTERNAL_CALL(SphereCollider_SetIsTrigger);
  // RigidBodyComponent
  WIESEL_ADD_INTERNAL_CALL(RigidBody_GetType);
  WIESEL_ADD_INTERNAL_CALL(RigidBody_SetType);
  WIESEL_ADD_INTERNAL_CALL(RigidBody_GetMass);
  WIESEL_ADD_INTERNAL_CALL(RigidBody_SetMass);
  WIESEL_ADD_INTERNAL_CALL(RigidBody_GetLinearVelocityX);
  WIESEL_ADD_INTERNAL_CALL(RigidBody_GetLinearVelocityY);
  WIESEL_ADD_INTERNAL_CALL(RigidBody_GetLinearVelocityZ);
  WIESEL_ADD_INTERNAL_CALL(RigidBody_SetLinearVelocity);
  WIESEL_ADD_INTERNAL_CALL(RigidBody_GetAngularVelocityX);
  WIESEL_ADD_INTERNAL_CALL(RigidBody_GetAngularVelocityY);
  WIESEL_ADD_INTERNAL_CALL(RigidBody_GetAngularVelocityZ);
  WIESEL_ADD_INTERNAL_CALL(RigidBody_SetAngularVelocity);
  WIESEL_ADD_INTERNAL_CALL(RigidBody_GetFriction);
  WIESEL_ADD_INTERNAL_CALL(RigidBody_SetFriction);
  WIESEL_ADD_INTERNAL_CALL(RigidBody_GetRestitution);
  WIESEL_ADD_INTERNAL_CALL(RigidBody_SetRestitution);
  WIESEL_ADD_INTERNAL_CALL(RigidBody_GetLinearDamping);
  WIESEL_ADD_INTERNAL_CALL(RigidBody_SetLinearDamping);
  WIESEL_ADD_INTERNAL_CALL(RigidBody_GetAngularDamping);
  WIESEL_ADD_INTERNAL_CALL(RigidBody_SetAngularDamping);
  WIESEL_ADD_INTERNAL_CALL(RigidBody_AddForce);
  WIESEL_ADD_INTERNAL_CALL(RigidBody_AddImpulse);
  // Physics queries
  WIESEL_ADD_INTERNAL_CALL(Physics_Raycast);
  WIESEL_ADD_INTERNAL_CALL(Physics_OverlapBox);
  WIESEL_ADD_INTERNAL_CALL(Physics_OverlapSphere);
  // RectangleTransformComponent
  WIESEL_ADD_INTERNAL_CALL(RectTransform_GetPositionX);
  WIESEL_ADD_INTERNAL_CALL(RectTransform_GetPositionY);
  WIESEL_ADD_INTERNAL_CALL(RectTransform_SetPositionX);
  WIESEL_ADD_INTERNAL_CALL(RectTransform_SetPositionY);
  WIESEL_ADD_INTERNAL_CALL(RectTransform_GetRotation);
  WIESEL_ADD_INTERNAL_CALL(RectTransform_SetRotation);
  WIESEL_ADD_INTERNAL_CALL(RectTransform_GetSizeX);
  WIESEL_ADD_INTERNAL_CALL(RectTransform_GetSizeY);
  WIESEL_ADD_INTERNAL_CALL(RectTransform_SetSizeX);
  WIESEL_ADD_INTERNAL_CALL(RectTransform_SetSizeY);
  WIESEL_ADD_INTERNAL_CALL(RectTransform_GetScaleX);
  WIESEL_ADD_INTERNAL_CALL(RectTransform_GetScaleY);
  WIESEL_ADD_INTERNAL_CALL(RectTransform_SetScaleX);
  WIESEL_ADD_INTERNAL_CALL(RectTransform_SetScaleY);
  WIESEL_ADD_INTERNAL_CALL(RectTransform_GetAnchor);
  WIESEL_ADD_INTERNAL_CALL(RectTransform_SetAnchor);
  WIESEL_ADD_INTERNAL_CALL(RectTransform_GetPivot);
  WIESEL_ADD_INTERNAL_CALL(RectTransform_SetPivot);
  WIESEL_ADD_INTERNAL_CALL(RectTransform_GetSizeModeX);
  WIESEL_ADD_INTERNAL_CALL(RectTransform_SetSizeModeX);
  WIESEL_ADD_INTERNAL_CALL(RectTransform_GetSizeModeY);
  WIESEL_ADD_INTERNAL_CALL(RectTransform_SetSizeModeY);
  WIESEL_ADD_INTERNAL_CALL(RectTransform_GetPaddingLeft);
  WIESEL_ADD_INTERNAL_CALL(RectTransform_GetPaddingTop);
  WIESEL_ADD_INTERNAL_CALL(RectTransform_GetPaddingRight);
  WIESEL_ADD_INTERNAL_CALL(RectTransform_GetPaddingBottom);
  WIESEL_ADD_INTERNAL_CALL(RectTransform_SetPaddingLeft);
  WIESEL_ADD_INTERNAL_CALL(RectTransform_SetPaddingTop);
  WIESEL_ADD_INTERNAL_CALL(RectTransform_SetPaddingRight);
  WIESEL_ADD_INTERNAL_CALL(RectTransform_SetPaddingBottom);
  WIESEL_ADD_INTERNAL_CALL(RectTransform_GetComputedPositionX);
  WIESEL_ADD_INTERNAL_CALL(RectTransform_GetComputedPositionY);
  WIESEL_ADD_INTERNAL_CALL(RectTransform_GetComputedSizeX);
  WIESEL_ADD_INTERNAL_CALL(RectTransform_GetComputedSizeY);
  // CanvasComponent
  WIESEL_ADD_INTERNAL_CALL(Canvas_GetDirection);
  WIESEL_ADD_INTERNAL_CALL(Canvas_SetDirection);
  WIESEL_ADD_INTERNAL_CALL(Canvas_GetAlignment);
  WIESEL_ADD_INTERNAL_CALL(Canvas_SetAlignment);
  WIESEL_ADD_INTERNAL_CALL(Canvas_GetSpacing);
  WIESEL_ADD_INTERNAL_CALL(Canvas_SetSpacing);
  WIESEL_ADD_INTERNAL_CALL(Canvas_GetSortOrder);
  WIESEL_ADD_INTERNAL_CALL(Canvas_SetSortOrder);
  // CanvasRectComponent
  WIESEL_ADD_INTERNAL_CALL(CanvasRect_GetColorR);
  WIESEL_ADD_INTERNAL_CALL(CanvasRect_GetColorG);
  WIESEL_ADD_INTERNAL_CALL(CanvasRect_GetColorB);
  WIESEL_ADD_INTERNAL_CALL(CanvasRect_GetColorA);
  WIESEL_ADD_INTERNAL_CALL(CanvasRect_SetColorR);
  WIESEL_ADD_INTERNAL_CALL(CanvasRect_SetColorG);
  WIESEL_ADD_INTERNAL_CALL(CanvasRect_SetColorB);
  WIESEL_ADD_INTERNAL_CALL(CanvasRect_SetColorA);
  // CanvasImageComponent
  WIESEL_ADD_INTERNAL_CALL(CanvasImage_GetTintR);
  WIESEL_ADD_INTERNAL_CALL(CanvasImage_GetTintG);
  WIESEL_ADD_INTERNAL_CALL(CanvasImage_GetTintB);
  WIESEL_ADD_INTERNAL_CALL(CanvasImage_GetTintA);
  WIESEL_ADD_INTERNAL_CALL(CanvasImage_SetTintR);
  WIESEL_ADD_INTERNAL_CALL(CanvasImage_SetTintG);
  WIESEL_ADD_INTERNAL_CALL(CanvasImage_SetTintB);
  WIESEL_ADD_INTERNAL_CALL(CanvasImage_SetTintA);
  WIESEL_ADD_INTERNAL_CALL(CanvasImage_GetUVRectX);
  WIESEL_ADD_INTERNAL_CALL(CanvasImage_GetUVRectY);
  WIESEL_ADD_INTERNAL_CALL(CanvasImage_GetUVRectZ);
  WIESEL_ADD_INTERNAL_CALL(CanvasImage_GetUVRectW);
  WIESEL_ADD_INTERNAL_CALL(CanvasImage_SetUVRectX);
  WIESEL_ADD_INTERNAL_CALL(CanvasImage_SetUVRectY);
  WIESEL_ADD_INTERNAL_CALL(CanvasImage_SetUVRectZ);
  WIESEL_ADD_INTERNAL_CALL(CanvasImage_SetUVRectW);
  // TextComponent
  WIESEL_ADD_INTERNAL_CALL(Text_GetText);
  WIESEL_ADD_INTERNAL_CALL(Text_SetText);
  WIESEL_ADD_INTERNAL_CALL(Text_GetFontSize);
  WIESEL_ADD_INTERNAL_CALL(Text_SetFontSize);
  WIESEL_ADD_INTERNAL_CALL(Text_GetColorR);
  WIESEL_ADD_INTERNAL_CALL(Text_GetColorG);
  WIESEL_ADD_INTERNAL_CALL(Text_GetColorB);
  WIESEL_ADD_INTERNAL_CALL(Text_GetColorA);
  WIESEL_ADD_INTERNAL_CALL(Text_SetColorR);
  WIESEL_ADD_INTERNAL_CALL(Text_SetColorG);
  WIESEL_ADD_INTERNAL_CALL(Text_SetColorB);
  WIESEL_ADD_INTERNAL_CALL(Text_SetColorA);

  WIESEL_ADD_INTERNAL_CALL(Animator_SetBool);
  WIESEL_ADD_INTERNAL_CALL(Animator_SetInt);
  WIESEL_ADD_INTERNAL_CALL(Animator_SetFloat);
  WIESEL_ADD_INTERNAL_CALL(Animator_SetTrigger);
  WIESEL_ADD_INTERNAL_CALL(Animator_GetBool);
  WIESEL_ADD_INTERNAL_CALL(Animator_GetInt);
  WIESEL_ADD_INTERNAL_CALL(Animator_GetFloat);
  WIESEL_ADD_INTERNAL_CALL(Animator_Play);
  WIESEL_ADD_INTERNAL_CALL(Animator_GetCurrentState);
  WIESEL_ADD_INTERNAL_CALL(Animator_GetIsPlaying);
  WIESEL_ADD_INTERNAL_CALL(Animator_SetIsPlaying);
  // SceneManager
  WIESEL_ADD_INTERNAL_CALL(SceneManager_LoadScene);
  WIESEL_ADD_INTERNAL_CALL(SceneManager_LoadScenePath);
  WIESEL_ADD_INTERNAL_CALL(SceneManager_LoadSceneWithLoading);
  WIESEL_ADD_INTERNAL_CALL(SceneManager_GetLoadProgress);
  WIESEL_ADD_INTERNAL_CALL(SceneManager_IsSceneReady);
  WIESEL_ADD_INTERNAL_CALL(SceneManager_ActivateLoadedScene);
  WIESEL_ADD_INTERNAL_CALL(Prefab_Instantiate);
  // Scene
  WIESEL_ADD_INTERNAL_CALL(Time_GetDeltaTime);
  WIESEL_ADD_INTERNAL_CALL(Time_GetTimeScale);
  WIESEL_ADD_INTERNAL_CALL(Time_SetTimeScale);
  WIESEL_ADD_INTERNAL_CALL(Time_GetElapsedTime);
  WIESEL_ADD_INTERNAL_CALL(Scene_CreateEntity);
  WIESEL_ADD_INTERNAL_CALL(Scene_FindEntity);
  WIESEL_ADD_INTERNAL_CALL(Scene_DestroyEntity);
  // Console
  WIESEL_ADD_INTERNAL_CALL(LightDirect_GetColorR);
  WIESEL_ADD_INTERNAL_CALL(LightDirect_GetColorG);
  WIESEL_ADD_INTERNAL_CALL(LightDirect_GetColorB);
  WIESEL_ADD_INTERNAL_CALL(LightDirect_SetColorR);
  WIESEL_ADD_INTERNAL_CALL(LightDirect_SetColorG);
  WIESEL_ADD_INTERNAL_CALL(LightDirect_SetColorB);
  WIESEL_ADD_INTERNAL_CALL(LightDirect_GetAmbient);
  WIESEL_ADD_INTERNAL_CALL(LightDirect_SetAmbient);
  WIESEL_ADD_INTERNAL_CALL(LightDirect_GetDiffuse);
  WIESEL_ADD_INTERNAL_CALL(LightDirect_SetDiffuse);
  WIESEL_ADD_INTERNAL_CALL(LightDirect_GetSpecular);
  WIESEL_ADD_INTERNAL_CALL(LightDirect_SetSpecular);
  WIESEL_ADD_INTERNAL_CALL(LightDirect_GetDensity);
  WIESEL_ADD_INTERNAL_CALL(LightDirect_SetDensity);

  WIESEL_ADD_INTERNAL_CALL(LightPoint_GetColorR);
  WIESEL_ADD_INTERNAL_CALL(LightPoint_GetColorG);
  WIESEL_ADD_INTERNAL_CALL(LightPoint_GetColorB);
  WIESEL_ADD_INTERNAL_CALL(LightPoint_SetColorR);
  WIESEL_ADD_INTERNAL_CALL(LightPoint_SetColorG);
  WIESEL_ADD_INTERNAL_CALL(LightPoint_SetColorB);
  WIESEL_ADD_INTERNAL_CALL(LightPoint_GetAmbient);
  WIESEL_ADD_INTERNAL_CALL(LightPoint_SetAmbient);
  WIESEL_ADD_INTERNAL_CALL(LightPoint_GetDiffuse);
  WIESEL_ADD_INTERNAL_CALL(LightPoint_SetDiffuse);
  WIESEL_ADD_INTERNAL_CALL(LightPoint_GetSpecular);
  WIESEL_ADD_INTERNAL_CALL(LightPoint_SetSpecular);
  WIESEL_ADD_INTERNAL_CALL(LightPoint_GetDensity);
  WIESEL_ADD_INTERNAL_CALL(LightPoint_SetDensity);
  WIESEL_ADD_INTERNAL_CALL(LightPoint_GetConstant);
  WIESEL_ADD_INTERNAL_CALL(LightPoint_SetConstant);
  WIESEL_ADD_INTERNAL_CALL(LightPoint_GetLinear);
  WIESEL_ADD_INTERNAL_CALL(LightPoint_SetLinear);
  WIESEL_ADD_INTERNAL_CALL(LightPoint_GetExp);
  WIESEL_ADD_INTERNAL_CALL(LightPoint_SetExp);

  WIESEL_ADD_INTERNAL_CALL(Entity_AddComponent);
  WIESEL_ADD_INTERNAL_CALL(Entity_RemoveComponent);

  WIESEL_ADD_INTERNAL_CALL(Input_GetMouseX);
  WIESEL_ADD_INTERNAL_CALL(Input_GetMouseY);
  WIESEL_ADD_INTERNAL_CALL(Input_GetMouseButton);
  WIESEL_ADD_INTERNAL_CALL(Input_GetMouseButtonDown);
  WIESEL_ADD_INTERNAL_CALL(Input_GetMouseButtonUp);
  WIESEL_ADD_INTERNAL_CALL(Input_GetGamepadButton);
  WIESEL_ADD_INTERNAL_CALL(Input_GetGamepadAxis);
  WIESEL_ADD_INTERNAL_CALL(Input_GetConnectedGamepadCount);

  WIESEL_ADD_INTERNAL_CALL(Entity_HasTag);
  WIESEL_ADD_INTERNAL_CALL(Entity_AddTag);
  WIESEL_ADD_INTERNAL_CALL(Entity_RemoveTag);
  WIESEL_ADD_INTERNAL_CALL(Scene_FindEntitiesByTag);

  WIESEL_ADD_INTERNAL_CALL(Camera_GetProjectionMode);
  WIESEL_ADD_INTERNAL_CALL(Camera_SetProjectionMode);
  WIESEL_ADD_INTERNAL_CALL(Camera_GetFOV);
  WIESEL_ADD_INTERNAL_CALL(Camera_SetFOV);
  WIESEL_ADD_INTERNAL_CALL(Camera_GetOrthoSize);
  WIESEL_ADD_INTERNAL_CALL(Camera_SetOrthoSize);
  WIESEL_ADD_INTERNAL_CALL(Camera_GetNearPlane);
  WIESEL_ADD_INTERNAL_CALL(Camera_SetNearPlane);
  WIESEL_ADD_INTERNAL_CALL(Camera_GetFarPlane);
  WIESEL_ADD_INTERNAL_CALL(Camera_SetFarPlane);
  WIESEL_ADD_INTERNAL_CALL(Camera_GetEnabled);
  WIESEL_ADD_INTERNAL_CALL(Camera_SetEnabled);
  WIESEL_ADD_INTERNAL_CALL(Camera_GetBgColorR);
  WIESEL_ADD_INTERNAL_CALL(Camera_GetBgColorG);
  WIESEL_ADD_INTERNAL_CALL(Camera_GetBgColorB);
  WIESEL_ADD_INTERNAL_CALL(Camera_GetBgColorA);
  WIESEL_ADD_INTERNAL_CALL(Camera_SetBgColorR);
  WIESEL_ADD_INTERNAL_CALL(Camera_SetBgColorG);
  WIESEL_ADD_INTERNAL_CALL(Camera_SetBgColorB);
  WIESEL_ADD_INTERNAL_CALL(Camera_SetBgColorA);

  // SpriteRenderer
  WIESEL_ADD_INTERNAL_CALL(SpriteRenderer_GetFlipX);
  WIESEL_ADD_INTERNAL_CALL(SpriteRenderer_SetFlipX);
  WIESEL_ADD_INTERNAL_CALL(SpriteRenderer_GetFlipY);
  WIESEL_ADD_INTERNAL_CALL(SpriteRenderer_SetFlipY);
  WIESEL_ADD_INTERNAL_CALL(SpriteRenderer_GetTintR);
  WIESEL_ADD_INTERNAL_CALL(SpriteRenderer_GetTintG);
  WIESEL_ADD_INTERNAL_CALL(SpriteRenderer_GetTintB);
  WIESEL_ADD_INTERNAL_CALL(SpriteRenderer_GetTintA);
  WIESEL_ADD_INTERNAL_CALL(SpriteRenderer_SetTintR);
  WIESEL_ADD_INTERNAL_CALL(SpriteRenderer_SetTintG);
  WIESEL_ADD_INTERNAL_CALL(SpriteRenderer_SetTintB);
  WIESEL_ADD_INTERNAL_CALL(SpriteRenderer_SetTintA);
  WIESEL_ADD_INTERNAL_CALL(SpriteRenderer_GetSortLayer);
  WIESEL_ADD_INTERNAL_CALL(SpriteRenderer_SetSortLayer);

  // SpriteAnimator
  WIESEL_ADD_INTERNAL_CALL(SpriteAnimator_Play);
  WIESEL_ADD_INTERNAL_CALL(SpriteAnimator_Stop);
  WIESEL_ADD_INTERNAL_CALL(SpriteAnimator_GetIsPlaying);
  WIESEL_ADD_INTERNAL_CALL(SpriteAnimator_GetCurrentState);
  WIESEL_ADD_INTERNAL_CALL(SpriteAnimator_GetCurrentFrame);
  WIESEL_ADD_INTERNAL_CALL(SpriteAnimator_SetBool);
  WIESEL_ADD_INTERNAL_CALL(SpriteAnimator_SetInt);
  WIESEL_ADD_INTERNAL_CALL(SpriteAnimator_SetFloat);
  WIESEL_ADD_INTERNAL_CALL(SpriteAnimator_SetTrigger);
  WIESEL_ADD_INTERNAL_CALL(SpriteAnimator_GetBool);
  WIESEL_ADD_INTERNAL_CALL(SpriteAnimator_GetInt);
  WIESEL_ADD_INTERNAL_CALL(SpriteAnimator_GetFloat);

  WIESEL_ADD_INTERNAL_CALL(AudioSource_Play);
  WIESEL_ADD_INTERNAL_CALL(AudioSource_PlayClip);
  WIESEL_ADD_INTERNAL_CALL(AudioSource_Stop);
  WIESEL_ADD_INTERNAL_CALL(AudioSource_GetIsPlaying);
  WIESEL_ADD_INTERNAL_CALL(AudioSource_GetVolume);
  WIESEL_ADD_INTERNAL_CALL(AudioSource_SetVolume);
  WIESEL_ADD_INTERNAL_CALL(AudioSource_GetPitch);
  WIESEL_ADD_INTERNAL_CALL(AudioSource_SetPitch);
  WIESEL_ADD_INTERNAL_CALL(AudioSource_GetLoop);
  WIESEL_ADD_INTERNAL_CALL(AudioSource_SetLoop);
  WIESEL_ADD_INTERNAL_CALL(AudioSource_GetMute);
  WIESEL_ADD_INTERNAL_CALL(AudioSource_SetMute);
  WIESEL_ADD_INTERNAL_CALL(AudioSource_GetSpatialBlend);
  WIESEL_ADD_INTERNAL_CALL(AudioSource_SetSpatialBlend);

  WIESEL_ADD_INTERNAL_CALL(Audio_PlayPath);
  WIESEL_ADD_INTERNAL_CALL(Audio_PlayAtPath);
  WIESEL_ADD_INTERNAL_CALL(Audio_PlayClip);
  WIESEL_ADD_INTERNAL_CALL(Audio_PlayAtClip);
  WIESEL_ADD_INTERNAL_CALL(Audio_PlayMusic);
  WIESEL_ADD_INTERNAL_CALL(Audio_PlayMusicClip);
  WIESEL_ADD_INTERNAL_CALL(Audio_StopMusic);
  WIESEL_ADD_INTERNAL_CALL(Audio_SetMasterVolume);
  WIESEL_ADD_INTERNAL_CALL(Audio_GetMasterVolume);
  WIESEL_ADD_INTERNAL_CALL(Audio_SetSFXVolume);
  WIESEL_ADD_INTERNAL_CALL(Audio_GetSFXVolume);
  WIESEL_ADD_INTERNAL_CALL(Audio_SetMusicVolume);
  WIESEL_ADD_INTERNAL_CALL(Audio_GetMusicVolume);

  WIESEL_ADD_INTERNAL_CALL(Console_RegisterCommand);
  WIESEL_ADD_INTERNAL_CALL(Console_UnregisterCommand);
  WIESEL_ADD_INTERNAL_CALL(Console_Execute);
  WIESEL_ADD_INTERNAL_CALL(Console_LogInfo);
  WIESEL_ADD_INTERNAL_CALL(Console_LogWarning);
  WIESEL_ADD_INTERNAL_CALL(Console_LogError);
}

void ScriptManager::RegisterComponents() {
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