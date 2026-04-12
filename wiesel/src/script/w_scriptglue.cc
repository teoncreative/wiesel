//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "script/w_scriptglue.h"
#include "util/w_logger.h"
#include "w_engine.h"

#include <direct.h>
#include <imgui.h>
#include "asset/w_asset_manager.h"
#include "audio/w_audio.h"
#include "cursor/w_cursor.h"
#include "input/w_input.h"
#include "physics/w_collider.h"
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
#include "ui/w_ui_document.h"
#include "util/w_logger.h"
#include "util/w_platform.h"
#include "w_engine.h"

namespace Wiesel {

#define WIESEL_ADD_INTERNAL_CALL(name)                     \
  mono_add_internal_call("WieselEngine.Internals::" #name, \
                         reinterpret_cast<void*>(Internals_##name))

void Internals_Log_Info(MonoString* str) {
  char* cstr = mono_string_to_utf8(str);
  LOG_INFO("{}", cstr);
  mono_free(cstr);
}

float Internals_Input_GetAxis(MonoString* str) {
  char* cstr = mono_string_to_utf8(str);
  float value = Engine::input().GetAxis(cstr);
  mono_free(cstr);
  return value;
}

bool Internals_Input_GetKey(MonoString* str) {
  char* cstr = mono_string_to_utf8(str);
  bool value = Engine::input().GetAction(cstr);
  mono_free(cstr);
  return value;
}

void Internals_Input_SetCursorMode(uint16_t mode) {
  auto cursor_mode = static_cast<CursorMode>(mode);
  Engine::window()->SetCursorMode(cursor_mode);
  if (cursor_mode == CursorModeRelative) {
    Engine::window()->SetCursorCaptureSource(CursorCaptureSource::Game);
  } else {
    Engine::window()->SetCursorCaptureSource(CursorCaptureSource::None);
  }
}

uint16_t Internals_Input_GetCursorMode() {
  uint16_t cursor_mode = Engine::window()->GetCursorMode();
  return cursor_mode;
}

bool Internals_Input_GetKeyDown(MonoString* str) {
  char* cstr = mono_string_to_utf8(str);
  bool value = Engine::input().GetActionDown(cstr);
  mono_free(cstr);
  return value;
}

bool Internals_Input_GetKeyUp(MonoString* str) {
  char* cstr = mono_string_to_utf8(str);
  bool value = Engine::input().GetActionUp(cstr);
  mono_free(cstr);
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
  char* cstr = mono_string_to_utf8(clip_handle);
  AssetHandle clip = AssetHandle::FromString(cstr);
  mono_free(cstr);
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
  char* cstr = mono_string_to_utf8(name);
  Engine::script_manager().AddComponentByName(scene, handle, cstr);
  mono_free(cstr);
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
  char* cstr = mono_string_to_utf8(name);
  Engine::script_manager().RemoveComponentByName(scene, handle, cstr);
  mono_free(cstr);
}

// --- Mouse bindings ---

int Internals_Input_GetMouseX() {
  return Engine::input().GetMouseX();
}

int Internals_Input_GetMouseY() {
  return Engine::input().GetMouseY();
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
  return Engine::input().IsGamepadButtonPressed(
      gamepad_index, static_cast<GamepadButton>(button));
}

float Internals_Input_GetGamepadAxis(int gamepad_index, int axis) {
  return Engine::input().GetGamepadAxis(gamepad_index,
                                        static_cast<GamepadAxis>(axis));
}

int Internals_Input_GetConnectedGamepadCount() {
  return Engine::input().GetConnectedGamepadCount();
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
  char* cstr = mono_string_to_utf8(tag);
  bool result = scene->GetComponent<TagComponent>(handle).HasTag(cstr);
  mono_free(cstr);
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
  char* cstr = mono_string_to_utf8(tag);
  scene->GetComponent<TagComponent>(handle).AddTag(cstr);
  mono_free(cstr);
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
  char* cstr = mono_string_to_utf8(tag);
  scene->GetComponent<TagComponent>(handle).RemoveTag(cstr);
  mono_free(cstr);
}

MonoArray* Internals_Scene_FindEntitiesByTag(uint64_t scene_ptr,
                                             MonoString* tag) {
  if (scene_ptr == 0) {
    return nullptr;
  }
  Scene* scene = reinterpret_cast<Scene*>(scene_ptr);
  char* cstr = mono_string_to_utf8(tag);
  auto entities = scene->FindEntitiesByTag(cstr);
  mono_free(cstr);

  MonoClass* entity_class = Engine::script_manager().entity_class();
  MonoArray* arr = mono_array_new(Engine::script_manager().app_domain(),
                                  entity_class, entities.size());
  for (size_t i = 0; i < entities.size(); i++) {
    MonoObject* obj =
        Engine::script_manager().CreateCSharpEntity(scene, entities[i]);
    mono_array_setref(arr, i, obj);
  }
  return arr;
}

// --- Child entity access ---

int Internals_Entity_GetChildCount(uint64_t scene_ptr, uint64_t entity_id) {
  if (scene_ptr == 0) {
    return 0;
  }
  Scene* scene = reinterpret_cast<Scene*>(scene_ptr);
  entt::entity handle = static_cast<entt::entity>(entity_id);
  if (!scene->HasComponent<TreeComponent>(handle)) {
    return 0;
  }
  TreeComponent& tree = scene->GetComponent<TreeComponent>(handle);
  return static_cast<int>(tree.childs.size());
}

MonoObject* Internals_Entity_GetChild(uint64_t scene_ptr, uint64_t entity_id,
                                      int index) {
  if (scene_ptr == 0) {
    return nullptr;
  }
  Scene* scene = reinterpret_cast<Scene*>(scene_ptr);
  entt::entity handle = static_cast<entt::entity>(entity_id);
  if (!scene->HasComponent<TreeComponent>(handle)) {
    return nullptr;
  }
  TreeComponent& tree = scene->GetComponent<TreeComponent>(handle);
  if (index < 0 || index >= static_cast<int>(tree.childs.size())) {
    return nullptr;
  }
  return Engine::script_manager().CreateCSharpEntity(scene, tree.childs[index]);
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

// --- AnimatorComponent bindings ---

#define GET_ANIMATOR_OR_RETURN(scene_ptr, entity_id, retval) \
  VALIDATE_SCENE_OR_RETURN(scene_ptr, entity_id, retval);    \
  if (!scene->HasComponent<AnimatorComponent>(handle))       \
    return retval;                                           \
  auto& anim = scene->GetComponent<AnimatorComponent>(handle)

void Internals_SpriteAnimator_Play(uint64_t sp, uint64_t eid, MonoString* state,
                                   bool /*restart*/) {
  GET_ANIMATOR_OR_RETURN(sp, eid, );
  char* cstr = mono_string_to_utf8(state);
  anim.Play(cstr);
  mono_free(cstr);
}

void Internals_SpriteAnimator_Stop(uint64_t sp, uint64_t eid) {
  GET_ANIMATOR_OR_RETURN(sp, eid, );
  anim.Stop();
}

bool Internals_SpriteAnimator_GetIsPlaying(uint64_t sp, uint64_t eid) {
  GET_ANIMATOR_OR_RETURN(sp, eid, false);
  return anim.playing;
}

MonoString* Internals_SpriteAnimator_GetCurrentState(uint64_t sp,
                                                     uint64_t eid) {
  GET_ANIMATOR_OR_RETURN(
      sp, eid, mono_string_new(Engine::script_manager().app_domain(), ""));
  return mono_string_new(Engine::script_manager().app_domain(),
                         anim.GetCurrentState().c_str());
}

int Internals_SpriteAnimator_GetCurrentFrame(uint64_t sp, uint64_t eid) {
  GET_ANIMATOR_OR_RETURN(sp, eid, 0);
  if (scene->HasComponent<SpriteAnimRuntime>(handle)) {
    return static_cast<int>(
        scene->GetComponent<SpriteAnimRuntime>(handle).current_frame_index);
  }
  return 0;
}

void Internals_SpriteAnimator_SetBool(uint64_t sp, uint64_t eid,
                                      MonoString* name, bool val) {
  GET_ANIMATOR_OR_RETURN(sp, eid, );
  char* cstr = mono_string_to_utf8(name);
  anim.SetBool(cstr, val);
  mono_free(cstr);
}

void Internals_SpriteAnimator_SetInt(uint64_t sp, uint64_t eid,
                                     MonoString* name, int val) {
  GET_ANIMATOR_OR_RETURN(sp, eid, );
  char* cstr = mono_string_to_utf8(name);
  anim.SetInt(cstr, val);
  mono_free(cstr);
}

void Internals_SpriteAnimator_SetFloat(uint64_t sp, uint64_t eid,
                                       MonoString* name, float val) {
  GET_ANIMATOR_OR_RETURN(sp, eid, );
  char* cstr = mono_string_to_utf8(name);
  anim.SetFloat(cstr, val);
  mono_free(cstr);
}

void Internals_SpriteAnimator_SetTrigger(uint64_t sp, uint64_t eid,
                                         MonoString* name) {
  GET_ANIMATOR_OR_RETURN(sp, eid, );
  char* cstr = mono_string_to_utf8(name);
  anim.SetTrigger(cstr);
  mono_free(cstr);
}

bool Internals_SpriteAnimator_GetBool(uint64_t sp, uint64_t eid,
                                      MonoString* name) {
  GET_ANIMATOR_OR_RETURN(sp, eid, false);
  char* cstr = mono_string_to_utf8(name);
  bool val = anim.GetBool(cstr);
  mono_free(cstr);
  return val;
}

int Internals_SpriteAnimator_GetInt(uint64_t sp, uint64_t eid,
                                    MonoString* name) {
  GET_ANIMATOR_OR_RETURN(sp, eid, 0);
  char* cstr = mono_string_to_utf8(name);
  int val = anim.GetInt(cstr);
  mono_free(cstr);
  return val;
}

float Internals_SpriteAnimator_GetFloat(uint64_t sp, uint64_t eid,
                                        MonoString* name) {
  GET_ANIMATOR_OR_RETURN(sp, eid, 0.0f);
  char* cstr = mono_string_to_utf8(name);
  float val = anim.GetFloat(cstr);
  mono_free(cstr);
  return val;
}

#undef GET_ANIMATOR_OR_RETURN

// --- Audio bindings ---

void Internals_Audio_PlayPath(MonoString* path, int bus, float volume,
                              float pitch, bool loop) {
  char* cstr = mono_string_to_utf8(path);
  SoundParams params;
  params.bus = static_cast<AudioBus>(bus);
  params.volume = volume;
  params.pitch = pitch;
  params.loop = loop;
  Engine::audio().Play(cstr, params);
  mono_free(cstr);
}

void Internals_Audio_PlayAtPath(MonoString* path, float x, float y, float z,
                                int bus, float volume, float minDist,
                                float maxDist) {
  char* cstr = mono_string_to_utf8(path);
  SoundParams params;
  params.bus = static_cast<AudioBus>(bus);
  params.volume = volume;
  params.spatial_blend = 1.0f;
  params.position = {x, y, z};
  params.min_distance = minDist;
  params.max_distance = maxDist;
  Engine::audio().Play(cstr, params);
  mono_free(cstr);
}

void Internals_Audio_PlayClip(MonoString* handle_str, int bus, float volume,
                              float pitch, bool loop) {
  char* cstr = mono_string_to_utf8(handle_str);
  AssetHandle clip = AssetHandle::FromString(cstr);
  mono_free(cstr);
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
  char* cstr = mono_string_to_utf8(handle_str);
  AssetHandle clip = AssetHandle::FromString(cstr);
  mono_free(cstr);
  Engine::audio().PlaySoundAt(clip, {x, y, z}, static_cast<AudioBus>(bus),
                              volume, minDist, maxDist);
}

void Internals_Audio_PlayMusic(MonoString* path, float volume) {
  char* cstr = mono_string_to_utf8(path);
  Engine::audio().PlayMusic(cstr, volume);
  mono_free(cstr);
}

void Internals_Audio_PlayMusicClip(MonoString* handle_str, float volume) {
  char* cstr = mono_string_to_utf8(handle_str);
  AssetHandle clip = AssetHandle::FromString(cstr);
  mono_free(cstr);
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

MonoObject* Internals_Scene_FindEntity(uint64_t scene_ptr, MonoString* name) {
  if (scene_ptr == 0) {
    return nullptr;
  }
  Scene* scene = reinterpret_cast<Scene*>(scene_ptr);
  char* cstr = mono_string_to_utf8(name);
  entt::entity entity = scene->FindEntityByName(cstr);
  mono_free(cstr);
  if (entity == entt::null) {
    return nullptr;
  }
  return Engine::script_manager().CreateCSharpEntity(scene, entity);
}

MonoObject* Internals_Scene_CreateEntity(uint64_t scene_ptr, MonoString* name) {
  if (scene_ptr == 0) {
    return nullptr;
  }
  Scene* scene = reinterpret_cast<Scene*>(scene_ptr);
  if (!scene) {
    LOG_ERROR("Scene_CreateEntity: no active scene");
    return nullptr;
  }
  char* cstr = mono_string_to_utf8(name);
  Entity entity = scene->CreateEntity(cstr);
  mono_free(cstr);
  return Engine::script_manager().CreateCSharpEntity(scene, entity.handle());
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
  char* cstr = mono_string_to_utf8(str);
  MonoObject* component =
      Engine::script_manager().GetComponentByName(scene, entity, cstr);
  mono_free(cstr);
  return component;
}

bool Internals_Behavior_HasComponent(Scene* scene, entt::entity entity,
                                     MonoString* str) {
  char* cstr = mono_string_to_utf8(str);
  bool hasComponent =
      Engine::script_manager().HasComponentByName(scene, entity, cstr);
  mono_free(cstr);
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
  rb.needs_recreate = true;
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
  char* str = mono_string_to_utf8(v);
  auto& c = s->GetComponent<TextComponent>(e);
  c.text = str;
  c.gpu_dirty_ = true;
  mono_free(str);
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
  char* cstr = mono_string_to_utf8(name);
  s->GetComponent<AnimatorComponent>(e).SetBool(cstr, value);
  mono_free(cstr);
}

void Internals_Animator_SetInt(Scene* s, entt::entity e, MonoString* name,
                               int value) {
  char* cstr = mono_string_to_utf8(name);
  s->GetComponent<AnimatorComponent>(e).SetInt(cstr, value);
  mono_free(cstr);
}

void Internals_Animator_SetFloat(Scene* s, entt::entity e, MonoString* name,
                                 float value) {
  char* cstr = mono_string_to_utf8(name);
  s->GetComponent<AnimatorComponent>(e).SetFloat(cstr, value);
  mono_free(cstr);
}

void Internals_Animator_SetTrigger(Scene* s, entt::entity e, MonoString* name) {
  char* cstr = mono_string_to_utf8(name);
  s->GetComponent<AnimatorComponent>(e).SetTrigger(cstr);
  mono_free(cstr);
}

bool Internals_Animator_GetBool(Scene* s, entt::entity e, MonoString* name) {
  char* cstr = mono_string_to_utf8(name);
  bool val = s->GetComponent<AnimatorComponent>(e).GetBool(cstr);
  mono_free(cstr);
  return val;
}

int Internals_Animator_GetInt(Scene* s, entt::entity e, MonoString* name) {
  char* cstr = mono_string_to_utf8(name);
  int val = s->GetComponent<AnimatorComponent>(e).GetInt(cstr);
  mono_free(cstr);
  return val;
}

float Internals_Animator_GetFloat(Scene* s, entt::entity e, MonoString* name) {
  char* cstr = mono_string_to_utf8(name);
  float val = s->GetComponent<AnimatorComponent>(e).GetFloat(cstr);
  mono_free(cstr);
  return val;
}

void Internals_Animator_Play(Scene* s, entt::entity e, MonoString* stateName,
                             float /*blendTime*/) {
  char* cstr = mono_string_to_utf8(stateName);
  s->GetComponent<AnimatorComponent>(e).Play(cstr);
  mono_free(cstr);
}

MonoString* Internals_Animator_GetCurrentState(Scene* s, entt::entity e) {
  auto& anim = s->GetComponent<AnimatorComponent>(e);
  return mono_string_new(Engine::script_manager().app_domain(),
                         anim.GetCurrentState().c_str());
}

bool Internals_Animator_GetIsPlaying(Scene* s, entt::entity e) {
  return s->GetComponent<AnimatorComponent>(e).playing;
}

void Internals_Animator_SetIsPlaying(Scene* s, entt::entity e, bool value) {
  s->GetComponent<AnimatorComponent>(e).playing = value;
}

void Internals_Animator_Stop(Scene* s, entt::entity e) {
  s->GetComponent<AnimatorComponent>(e).Stop();
}

// SceneManager internal calls
void Internals_SceneManager_LoadScene(MonoString* name, int mode) {
  char* cstr = mono_string_to_utf8(name);
  Engine::scene_manager().LoadScene(cstr, static_cast<LoadSceneMode>(mode));
  mono_free(cstr);
}

void Internals_SceneManager_LoadScenePath(MonoString* path, int mode) {
  char* cstr = mono_string_to_utf8(path);
  Engine::scene_manager().LoadSceneFromPath(cstr,
                                            static_cast<LoadSceneMode>(mode));
  mono_free(cstr);
}

void Internals_SceneManager_LoadSceneAsync(MonoString* name, int mode) {
  char* cstr = mono_string_to_utf8(name);
  Engine::scene_manager().LoadSceneAsync(cstr,
                                         static_cast<LoadSceneMode>(mode));
  mono_free(cstr);
}

void Internals_SceneManager_LoadSceneAsyncPath(MonoString* path, int mode) {
  char* cstr = mono_string_to_utf8(path);
  Engine::scene_manager().LoadSceneAsyncFromPath(
      cstr, static_cast<LoadSceneMode>(mode));
  mono_free(cstr);
}

void Internals_SceneManager_LoadSceneWithLoading(MonoString* target,
                                                 MonoString* loading) {
  char* t = mono_string_to_utf8(target);
  char* l = mono_string_to_utf8(loading);
  Engine::scene_manager().LoadSceneWithLoading(t, l);
  mono_free(t);
  mono_free(l);
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

MonoObject* Internals_Prefab_Instantiate(uint64_t scene_ptr,
                                         MonoString* handle_str) {
  if (scene_ptr == 0) {
    return nullptr;
  }
  Scene* scene = reinterpret_cast<Scene*>(scene_ptr);
  char* cstr = mono_string_to_utf8(handle_str);
  AssetHandle handle = AssetHandle::FromString(cstr);
  mono_free(cstr);

  std::shared_ptr<Scene> shared_scene =
      Engine::scene_manager().FindSceneByPtr(scene);
  if (!shared_scene) {
    LOG_ERROR("Prefab_Instantiate: scene not found in loaded scenes");
    return nullptr;
  }

  Entity entity = Prefab::Instantiate(shared_scene, handle);
  if (!entity) {
    return nullptr;
  }
  return Engine::script_manager().CreateCSharpEntity(scene, entity.handle());
}

void Internals_SceneManager_UnloadScene(MonoString* name) {
  char* cstr = mono_string_to_utf8(name);
  Engine::scene_manager().UnloadScene(std::string(cstr));
  mono_free(cstr);
}

int Internals_SceneManager_GetLoadedSceneCount() {
  return static_cast<int>(Engine::scene_manager().GetLoadedScenes().size());
}

uint64_t Internals_SceneManager_GetLoadedScene(int index) {
  auto& scenes = Engine::scene_manager().GetLoadedScenes();
  if (index < 0 || index >= static_cast<int>(scenes.size())) {
    return 0;
  }
  return reinterpret_cast<uint64_t>(scenes[index].get());
}

uint64_t Internals_SceneManager_FindScene(MonoString* name) {
  char* cstr = mono_string_to_utf8(name);
  auto scene = Engine::scene_manager().FindScene(cstr);
  mono_free(cstr);
  return scene ? reinterpret_cast<uint64_t>(scene.get()) : 0;
}

MonoString* Internals_Scene_GetName(uint64_t scene_ptr) {
  if (scene_ptr == 0) {
    return nullptr;
  }
  Scene* scene = reinterpret_cast<Scene*>(scene_ptr);
  return mono_string_new(mono_domain_get(), scene->GetName().c_str());
}

MonoObject* Internals_SceneManager_MoveEntityToScene(uint64_t scene_ptr,
                                                     uint64_t entity_id,
                                                     uint64_t target_scene_ptr,
                                                     bool move_children) {
  if (scene_ptr == 0 || target_scene_ptr == 0) {
    return nullptr;
  }
  Scene* source = reinterpret_cast<Scene*>(scene_ptr);
  auto target = Engine::scene_manager().FindSceneByPtr(
      reinterpret_cast<Scene*>(target_scene_ptr));
  if (!target) {
    LOG_ERROR("MoveEntityToScene: target scene not found");
    return nullptr;
  }
  entt::entity handle = static_cast<entt::entity>(entity_id);
  Entity entity{handle, source};
  Entity new_entity =
      Engine::scene_manager().MoveEntityToScene(entity, target, move_children);
  if (!new_entity) {
    return nullptr;
  }
  return Engine::script_manager().CreateCSharpEntity(target.get(),
                                                     new_entity.handle());
}

// Console
void Internals_Console_RegisterCommand(MonoString* name,
                                       MonoString* description,
                                       MonoObject* callback) {
  char* name_cstr = mono_string_to_utf8(name);
  char* desc_cstr = mono_string_to_utf8(description);
  std::string name_str = name_cstr;
  std::string desc_str = desc_cstr;
  mono_free(name_cstr);
  mono_free(desc_cstr);

  uint32_t gc_handle = mono_gchandle_new(callback, true);

  // Todo we need an handle so we can delete this command when the script dies
  DeveloperConsole::Get().Register(
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
        MonoMethod* invoke_method =
            mono_get_delegate_invoke(mono_object_get_class(delegate));
        InvokeSafe(invoke_method, delegate, invoke_args, nullptr);
      });
}

void Internals_Console_UnregisterCommand(MonoString* name) {
  char* cstr = mono_string_to_utf8(name);
  DeveloperConsole::Get().Unregister(cstr);
  mono_free(cstr);
}

void Internals_Console_Execute(MonoString* command_line) {
  char* cstr = mono_string_to_utf8(command_line);
  DeveloperConsole::Get().Execute(cstr);
  mono_free(cstr);
}

void Internals_Console_LogInfo(MonoString* message) {
  char* cstr = mono_string_to_utf8(message);
  DCON_LOG_INFO("{}", cstr);
  mono_free(cstr);
}

void Internals_Console_LogWarning(MonoString* message) {
  char* cstr = mono_string_to_utf8(message);
  DCON_LOG_WARN("{}", cstr);
  mono_free(cstr);
}

void Internals_Console_LogError(MonoString* message) {
  char* cstr = mono_string_to_utf8(message);
  DCON_LOG_ERROR("{}", cstr);
  mono_free(cstr);
}

// --- UIDocumentComponent bindings ---

#define GET_UI_DOC_OR_RETURN(scene_ptr, entity_id, retval) \
  VALIDATE_SCENE_OR_RETURN(scene_ptr, entity_id, retval);  \
  if (!scene->HasComponent<UIDocumentComponent>(handle))   \
    return retval;                                         \
  auto& ui_doc = scene->GetComponent<UIDocumentComponent>(handle)

void Internals_UIDocument_SetInt(uint64_t scene_ptr, uint64_t entity_id,
                                 MonoString* name, int value) {
  GET_UI_DOC_OR_RETURN(scene_ptr, entity_id, );
  char* cname = mono_string_to_utf8(name);
  ui_doc.data_model.SetInt(cname, value);
  mono_free(cname);
}

int Internals_UIDocument_GetInt(uint64_t scene_ptr, uint64_t entity_id,
                                MonoString* name) {
  GET_UI_DOC_OR_RETURN(scene_ptr, entity_id, 0);
  char* cname = mono_string_to_utf8(name);
  int result = ui_doc.data_model.GetInt(cname);
  mono_free(cname);
  return result;
}

void Internals_UIDocument_SetFloat(uint64_t scene_ptr, uint64_t entity_id,
                                   MonoString* name, float value) {
  GET_UI_DOC_OR_RETURN(scene_ptr, entity_id, );
  char* cname = mono_string_to_utf8(name);
  ui_doc.data_model.SetFloat(cname, value);
  mono_free(cname);
}

float Internals_UIDocument_GetFloat(uint64_t scene_ptr, uint64_t entity_id,
                                    MonoString* name) {
  GET_UI_DOC_OR_RETURN(scene_ptr, entity_id, 0.0f);
  char* cname = mono_string_to_utf8(name);
  float result = ui_doc.data_model.GetFloat(cname);
  mono_free(cname);
  return result;
}

void Internals_UIDocument_SetString(uint64_t scene_ptr, uint64_t entity_id,
                                    MonoString* name, MonoString* value) {
  GET_UI_DOC_OR_RETURN(scene_ptr, entity_id, );
  char* cname = mono_string_to_utf8(name);
  char* cvalue = mono_string_to_utf8(value);
  ui_doc.data_model.SetString(cname, cvalue);
  mono_free(cname);
  mono_free(cvalue);
}

MonoString* Internals_UIDocument_GetString(uint64_t scene_ptr,
                                           uint64_t entity_id,
                                           MonoString* name) {
  GET_UI_DOC_OR_RETURN(scene_ptr, entity_id,
                       mono_string_new(mono_domain_get(), ""));
  char* cname = mono_string_to_utf8(name);
  std::string result = ui_doc.data_model.GetString(cname);
  mono_free(cname);
  return mono_string_new(mono_domain_get(), result.c_str());
}

void Internals_UIDocument_SetBool(uint64_t scene_ptr, uint64_t entity_id,
                                  MonoString* name, bool value) {
  GET_UI_DOC_OR_RETURN(scene_ptr, entity_id, );
  char* cname = mono_string_to_utf8(name);
  ui_doc.data_model.SetBool(cname, value);
  mono_free(cname);
}

bool Internals_UIDocument_GetBool(uint64_t scene_ptr, uint64_t entity_id,
                                  MonoString* name) {
  GET_UI_DOC_OR_RETURN(scene_ptr, entity_id, false);
  char* cname = mono_string_to_utf8(name);
  bool result = ui_doc.data_model.GetBool(cname);
  mono_free(cname);
  return result;
}

void Internals_UIDocument_SetVisible(uint64_t scene_ptr, uint64_t entity_id,
                                     bool visible) {
  GET_UI_DOC_OR_RETURN(scene_ptr, entity_id, );
  ui_doc.visible = visible;
}

bool Internals_UIDocument_GetVisible(uint64_t scene_ptr, uint64_t entity_id) {
  GET_UI_DOC_OR_RETURN(scene_ptr, entity_id, false);
  return ui_doc.visible;
}

// --- Settings bindings ---

// Graphics
bool Internals_Settings_GetVSync() {
  return Engine::renderer()->options().vsync;
}

void Internals_Settings_SetVSync(bool value) {
  Engine::renderer()->options().vsync = value;
}

bool Internals_Settings_GetSSAO() {
  return Engine::renderer()->options().ssao_enabled;
}

void Internals_Settings_SetSSAO(bool value) {
  Engine::renderer()->options().ssao_enabled = value;
}

bool Internals_Settings_GetBloom() {
  return Engine::renderer()->options().bloom_enabled;
}

void Internals_Settings_SetBloom(bool value) {
  Engine::renderer()->options().bloom_enabled = value;
}

bool Internals_Settings_GetMotionBlur() {
  return Engine::renderer()->options().motion_blur_enabled;
}

void Internals_Settings_SetMotionBlur(bool value) {
  Engine::renderer()->options().motion_blur_enabled = value;
}

bool Internals_Settings_GetShadows() {
  return Engine::renderer()->options().shadows_enabled;
}

void Internals_Settings_SetShadows(bool value) {
  Engine::renderer()->options().shadows_enabled = value;
}

bool Internals_Settings_GetRTShadows() {
  return Engine::renderer()->options().rt_shadows_enabled;
}

void Internals_Settings_SetRTShadows(bool value) {
  Engine::renderer()->options().rt_shadows_enabled = value;
}

bool Internals_Settings_IsRTSupported() {
  return Engine::renderer()->IsRayTracingSupported();
}

int Internals_Settings_GetAAMode() {
  return static_cast<int>(Engine::renderer()->options().aa_mode.Get());
}

void Internals_Settings_SetAAMode(int mode) {
  Engine::renderer()->options().aa_mode = static_cast<AntiAliasingMode>(mode);
}

float Internals_Settings_GetBloomIntensity() {
  return Engine::renderer()->options().bloom_intensity;
}

void Internals_Settings_SetBloomIntensity(float value) {
  Engine::renderer()->options().bloom_intensity = value;
}

float Internals_Settings_GetMotionBlurStrength() {
  return Engine::renderer()->options().motion_blur_strength;
}

void Internals_Settings_SetMotionBlurStrength(float value) {
  Engine::renderer()->options().motion_blur_strength = value;
}

// Audio
float Internals_Settings_GetMasterVolume() {
  return Engine::audio().GetMasterVolume();
}

void Internals_Settings_SetMasterVolume(float value) {
  Engine::audio().SetMasterVolume(value);
}

float Internals_Settings_GetMusicVolume() {
  return Engine::audio().GetMusicVolume();
}

void Internals_Settings_SetMusicVolume(float value) {
  Engine::audio().SetMusicVolume(value);
}

float Internals_Settings_GetSFXVolume() {
  return Engine::audio().GetSFXVolume();
}

void Internals_Settings_SetSFXVolume(float value) {
  Engine::audio().SetSFXVolume(value);
}

int Internals_Settings_GetShadowQuality() {
  int res = Engine::renderer()->options().shadow_map_resolution;
  if (res >= 4096) {
    return 3;
  }
  if (res >= 2048) {
    return 2;
  }
  if (res >= 1024) {
    return 1;
  }
  return 0;
}

void Internals_Settings_SetShadowQuality(int quality) {
  int res = 512;
  switch (quality) {
    case 1:
      res = 1024;
      break;
    case 2:
      res = 2048;
      break;
    case 3:
      res = 4096;
      break;
  }
  Engine::renderer()->options().shadow_map_resolution = res;
}

int Internals_Settings_GetAnisotropicFiltering() {
  return Engine::renderer()->options().anisotropic_filtering;
}

void Internals_Settings_SetAnisotropicFiltering(int value) {
  Engine::renderer()->options().anisotropic_filtering = value;
}

int Internals_Settings_GetTextureQuality() {
  return Engine::renderer()->options().texture_quality;
}

void Internals_Settings_SetTextureQuality(int value) {
  int old_value = Engine::renderer()->options().texture_quality;
  Engine::renderer()->options().texture_quality = value;
  if (old_value != value) {
    // Reload all textures at the new quality level
    Engine::app().SubmitToMainThread(
        []() { Engine::asset_manager().ReloadAllOfType(AssetType::Texture); });
  }
}

// --- Cursor bindings ---

void Internals_Cursor_SetState(MonoString* state) {
  char* cstr = mono_string_to_utf8(state);
  Engine::cursor_manager().SetCursorState(cstr);
  mono_free(cstr);
}

MonoString* Internals_Cursor_GetState() {
  const auto& state = Engine::cursor_manager().GetCursorState();
  return mono_string_new(mono_domain_get(), state.c_str());
}

void RegisterScriptGlue() {
  PROFILE_ZONE_SCOPED_N("RegisterScriptGlue");
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
  WIESEL_ADD_INTERNAL_CALL(Animator_Stop);
  WIESEL_ADD_INTERNAL_CALL(Animator_GetCurrentState);
  WIESEL_ADD_INTERNAL_CALL(Animator_GetIsPlaying);
  WIESEL_ADD_INTERNAL_CALL(Animator_SetIsPlaying);
  // SceneManager
  WIESEL_ADD_INTERNAL_CALL(SceneManager_LoadScene);
  WIESEL_ADD_INTERNAL_CALL(SceneManager_LoadScenePath);
  WIESEL_ADD_INTERNAL_CALL(SceneManager_LoadSceneAsync);
  WIESEL_ADD_INTERNAL_CALL(SceneManager_LoadSceneAsyncPath);
  WIESEL_ADD_INTERNAL_CALL(SceneManager_LoadSceneWithLoading);
  WIESEL_ADD_INTERNAL_CALL(SceneManager_GetLoadProgress);
  WIESEL_ADD_INTERNAL_CALL(SceneManager_IsSceneReady);
  WIESEL_ADD_INTERNAL_CALL(SceneManager_ActivateLoadedScene);
  WIESEL_ADD_INTERNAL_CALL(SceneManager_UnloadScene);
  WIESEL_ADD_INTERNAL_CALL(SceneManager_GetLoadedSceneCount);
  WIESEL_ADD_INTERNAL_CALL(SceneManager_GetLoadedScene);
  WIESEL_ADD_INTERNAL_CALL(SceneManager_FindScene);
  WIESEL_ADD_INTERNAL_CALL(SceneManager_MoveEntityToScene);
  WIESEL_ADD_INTERNAL_CALL(Scene_GetName);
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
  WIESEL_ADD_INTERNAL_CALL(Entity_GetChildCount);
  WIESEL_ADD_INTERNAL_CALL(Entity_GetChild);

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

  // UIDocument
  WIESEL_ADD_INTERNAL_CALL(UIDocument_SetInt);
  WIESEL_ADD_INTERNAL_CALL(UIDocument_GetInt);
  WIESEL_ADD_INTERNAL_CALL(UIDocument_SetFloat);
  WIESEL_ADD_INTERNAL_CALL(UIDocument_GetFloat);
  WIESEL_ADD_INTERNAL_CALL(UIDocument_SetString);
  WIESEL_ADD_INTERNAL_CALL(UIDocument_GetString);
  WIESEL_ADD_INTERNAL_CALL(UIDocument_SetBool);
  WIESEL_ADD_INTERNAL_CALL(UIDocument_GetBool);
  WIESEL_ADD_INTERNAL_CALL(UIDocument_SetVisible);
  WIESEL_ADD_INTERNAL_CALL(UIDocument_GetVisible);

  // Cursor
  WIESEL_ADD_INTERNAL_CALL(Cursor_SetState);
  WIESEL_ADD_INTERNAL_CALL(Cursor_GetState);

  // Settings
  WIESEL_ADD_INTERNAL_CALL(Settings_GetVSync);
  WIESEL_ADD_INTERNAL_CALL(Settings_SetVSync);
  WIESEL_ADD_INTERNAL_CALL(Settings_GetSSAO);
  WIESEL_ADD_INTERNAL_CALL(Settings_SetSSAO);
  WIESEL_ADD_INTERNAL_CALL(Settings_GetBloom);
  WIESEL_ADD_INTERNAL_CALL(Settings_SetBloom);
  WIESEL_ADD_INTERNAL_CALL(Settings_GetMotionBlur);
  WIESEL_ADD_INTERNAL_CALL(Settings_SetMotionBlur);
  WIESEL_ADD_INTERNAL_CALL(Settings_GetShadows);
  WIESEL_ADD_INTERNAL_CALL(Settings_SetShadows);
  WIESEL_ADD_INTERNAL_CALL(Settings_GetRTShadows);
  WIESEL_ADD_INTERNAL_CALL(Settings_SetRTShadows);
  WIESEL_ADD_INTERNAL_CALL(Settings_IsRTSupported);
  WIESEL_ADD_INTERNAL_CALL(Settings_GetAAMode);
  WIESEL_ADD_INTERNAL_CALL(Settings_SetAAMode);
  WIESEL_ADD_INTERNAL_CALL(Settings_GetBloomIntensity);
  WIESEL_ADD_INTERNAL_CALL(Settings_SetBloomIntensity);
  WIESEL_ADD_INTERNAL_CALL(Settings_GetMotionBlurStrength);
  WIESEL_ADD_INTERNAL_CALL(Settings_SetMotionBlurStrength);
  WIESEL_ADD_INTERNAL_CALL(Settings_GetMasterVolume);
  WIESEL_ADD_INTERNAL_CALL(Settings_SetMasterVolume);
  WIESEL_ADD_INTERNAL_CALL(Settings_GetMusicVolume);
  WIESEL_ADD_INTERNAL_CALL(Settings_SetMusicVolume);
  WIESEL_ADD_INTERNAL_CALL(Settings_GetSFXVolume);
  WIESEL_ADD_INTERNAL_CALL(Settings_SetSFXVolume);
  WIESEL_ADD_INTERNAL_CALL(Settings_GetShadowQuality);
  WIESEL_ADD_INTERNAL_CALL(Settings_SetShadowQuality);
  WIESEL_ADD_INTERNAL_CALL(Settings_GetAnisotropicFiltering);
  WIESEL_ADD_INTERNAL_CALL(Settings_SetAnisotropicFiltering);
  WIESEL_ADD_INTERNAL_CALL(Settings_GetTextureQuality);
  WIESEL_ADD_INTERNAL_CALL(Settings_SetTextureQuality);
}

}  // namespace Wiesel
