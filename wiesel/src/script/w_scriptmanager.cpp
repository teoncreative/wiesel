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
#include "physics/w_collider.hpp"
#include "physics/w_collision_system.hpp"
#include "physics/w_rigidbody.hpp"
#include "physics/w_physics_world.hpp"
#include "rendering/w_mesh.hpp"
#include <mono/metadata/debug-helpers.h>
#include <mono/metadata/mono-config.h>
#include <mono/metadata/mono-debug.h>
#include <mono/metadata/object.h>
#include "asset/w_asset_manager.hpp"
#include "input/w_input.hpp"
#include "mono_util.h"
#include "scene/w_entity.hpp"
#include "scene/w_scene.hpp"
#include "script/mono/w_monobehavior.hpp"
#include "ui/w_canvas.hpp"
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
void Internals_BoxCollider_SetOffsetX(Scene* scene, entt::entity entity, float v) {
  scene->GetComponent<BoxColliderComponent>(entity).offset.x = v;
}
void Internals_BoxCollider_SetOffsetY(Scene* scene, entt::entity entity, float v) {
  scene->GetComponent<BoxColliderComponent>(entity).offset.y = v;
}
void Internals_BoxCollider_SetOffsetZ(Scene* scene, entt::entity entity, float v) {
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
void Internals_BoxCollider_SetHalfExtentsX(Scene* scene, entt::entity entity, float v) {
  scene->GetComponent<BoxColliderComponent>(entity).half_extents.x = v;
}
void Internals_BoxCollider_SetHalfExtentsY(Scene* scene, entt::entity entity, float v) {
  scene->GetComponent<BoxColliderComponent>(entity).half_extents.y = v;
}
void Internals_BoxCollider_SetHalfExtentsZ(Scene* scene, entt::entity entity, float v) {
  scene->GetComponent<BoxColliderComponent>(entity).half_extents.z = v;
}
bool Internals_BoxCollider_GetIsTrigger(Scene* scene, entt::entity entity) {
  return scene->GetComponent<BoxColliderComponent>(entity).is_trigger;
}
void Internals_BoxCollider_SetIsTrigger(Scene* scene, entt::entity entity, bool v) {
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
void Internals_SphereCollider_SetOffsetX(Scene* scene, entt::entity entity, float v) {
  scene->GetComponent<SphereColliderComponent>(entity).offset.x = v;
}
void Internals_SphereCollider_SetOffsetY(Scene* scene, entt::entity entity, float v) {
  scene->GetComponent<SphereColliderComponent>(entity).offset.y = v;
}
void Internals_SphereCollider_SetOffsetZ(Scene* scene, entt::entity entity, float v) {
  scene->GetComponent<SphereColliderComponent>(entity).offset.z = v;
}
float Internals_SphereCollider_GetRadius(Scene* scene, entt::entity entity) {
  return scene->GetComponent<SphereColliderComponent>(entity).radius;
}
void Internals_SphereCollider_SetRadius(Scene* scene, entt::entity entity, float v) {
  scene->GetComponent<SphereColliderComponent>(entity).radius = v;
}
bool Internals_SphereCollider_GetIsTrigger(Scene* scene, entt::entity entity) {
  return scene->GetComponent<SphereColliderComponent>(entity).is_trigger;
}
void Internals_SphereCollider_SetIsTrigger(Scene* scene, entt::entity entity, bool v) {
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
float Internals_RigidBody_GetLinearVelocityX(Scene* scene, entt::entity entity) {
  return scene->GetComponent<RigidBodyComponent>(entity).GetLinearVelocity().x;
}
float Internals_RigidBody_GetLinearVelocityY(Scene* scene, entt::entity entity) {
  return scene->GetComponent<RigidBodyComponent>(entity).GetLinearVelocity().y;
}
float Internals_RigidBody_GetLinearVelocityZ(Scene* scene, entt::entity entity) {
  return scene->GetComponent<RigidBodyComponent>(entity).GetLinearVelocity().z;
}
void Internals_RigidBody_SetLinearVelocity(Scene* scene, entt::entity entity,
                                           float x, float y, float z) {
  scene->GetComponent<RigidBodyComponent>(entity).SetLinearVelocity({x, y, z});
}
float Internals_RigidBody_GetAngularVelocityX(Scene* scene, entt::entity entity) {
  return scene->GetComponent<RigidBodyComponent>(entity).GetAngularVelocity().x;
}
float Internals_RigidBody_GetAngularVelocityY(Scene* scene, entt::entity entity) {
  return scene->GetComponent<RigidBodyComponent>(entity).GetAngularVelocity().y;
}
float Internals_RigidBody_GetAngularVelocityZ(Scene* scene, entt::entity entity) {
  return scene->GetComponent<RigidBodyComponent>(entity).GetAngularVelocity().z;
}
void Internals_RigidBody_SetAngularVelocity(Scene* scene, entt::entity entity,
                                            float x, float y, float z) {
  scene->GetComponent<RigidBodyComponent>(entity).SetAngularVelocity({x, y, z});
}
float Internals_RigidBody_GetFriction(Scene* scene, entt::entity entity) {
  return scene->GetComponent<RigidBodyComponent>(entity).friction;
}
void Internals_RigidBody_SetFriction(Scene* scene, entt::entity entity, float v) {
  scene->GetComponent<RigidBodyComponent>(entity).SetFrictionRuntime(v);
}
float Internals_RigidBody_GetRestitution(Scene* scene, entt::entity entity) {
  return scene->GetComponent<RigidBodyComponent>(entity).restitution;
}
void Internals_RigidBody_SetRestitution(Scene* scene, entt::entity entity, float v) {
  scene->GetComponent<RigidBodyComponent>(entity).SetRestitutionRuntime(v);
}
float Internals_RigidBody_GetLinearDamping(Scene* scene, entt::entity entity) {
  return scene->GetComponent<RigidBodyComponent>(entity).linear_damping;
}
void Internals_RigidBody_SetLinearDamping(Scene* scene, entt::entity entity, float v) {
  scene->GetComponent<RigidBodyComponent>(entity).SetLinearDampingRuntime(v);
}
float Internals_RigidBody_GetAngularDamping(Scene* scene, entt::entity entity) {
  return scene->GetComponent<RigidBodyComponent>(entity).angular_damping;
}
void Internals_RigidBody_SetAngularDamping(Scene* scene, entt::entity entity, float v) {
  scene->GetComponent<RigidBodyComponent>(entity).SetAngularDampingRuntime(v);
}
void Internals_RigidBody_AddForce(Scene* scene, entt::entity entity,
                                  float x, float y, float z) {
  scene->GetComponent<RigidBodyComponent>(entity).AddForce({x, y, z});
}
void Internals_RigidBody_AddImpulse(Scene* scene, entt::entity entity,
                                    float x, float y, float z) {
  scene->GetComponent<RigidBodyComponent>(entity).AddImpulse({x, y, z});
}

// --- Physics queries (backed by PhysicsWorld) ---
bool Internals_Physics_Raycast(Scene* scene,
    float ox, float oy, float oz,
    float dx, float dy, float dz, float maxDist,
    uint64_t ignoreEntity,
    uint64_t* outEntity, float* outPx, float* outPy, float* outPz,
    float* outNx, float* outNy, float* outNz, float* outDist) {
  glm::vec3 origin(ox, oy, oz);
  glm::vec3 dir = glm::normalize(glm::vec3(dx, dy, dz));
  glm::vec3 to = origin + dir * maxDist;

  entt::entity ignore = (ignoreEntity != 0)
      ? static_cast<entt::entity>(static_cast<uint32_t>(ignoreEntity))
      : entt::null;

  RaycastHit hit;
  if (scene->GetPhysicsWorld().Raycast(origin, to, hit, ignore)) {
    *outEntity = (uint64_t)hit.entity;
    *outPx = hit.point.x; *outPy = hit.point.y; *outPz = hit.point.z;
    *outNx = hit.normal.x; *outNy = hit.normal.y; *outNz = hit.normal.z;
    *outDist = hit.distance;
    return true;
  }
  return false;
}

MonoArray* Internals_Physics_OverlapBox(Scene* scene,
    float cx, float cy, float cz, float hx, float hy, float hz) {
  auto entities = scene->GetPhysicsWorld().OverlapBox(
      glm::vec3(cx, cy, cz), glm::vec3(hx, hy, hz));

  MonoArray* arr = mono_array_new(ScriptManager::app_domain(),
      mono_get_uint64_class(), entities.size());
  for (size_t i = 0; i < entities.size(); i++) {
    mono_array_set(arr, uint64_t, i, (uint64_t)entities[i]);
  }
  return arr;
}

MonoArray* Internals_Physics_OverlapSphere(Scene* scene,
    float cx, float cy, float cz, float radius) {
  auto entities = scene->GetPhysicsWorld().OverlapSphere(
      glm::vec3(cx, cy, cz), radius);

  MonoArray* arr = mono_array_new(ScriptManager::app_domain(),
      mono_get_uint64_class(), entities.size());
  for (size_t i = 0; i < entities.size(); i++) {
    mono_array_set(arr, uint64_t, i, (uint64_t)entities[i]);
  }
  return arr;
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

// --- RectangleTransformComponent ---
float Internals_RectTransform_GetPositionX(Scene* s, entt::entity e) { return s->GetComponent<RectangleTransformComponent>(e).position.x; }
float Internals_RectTransform_GetPositionY(Scene* s, entt::entity e) { return s->GetComponent<RectangleTransformComponent>(e).position.y; }
void Internals_RectTransform_SetPositionX(Scene* s, entt::entity e, float v) { auto& c = s->GetComponent<RectangleTransformComponent>(e); c.position.x = v; c.is_changed = true; }
void Internals_RectTransform_SetPositionY(Scene* s, entt::entity e, float v) { auto& c = s->GetComponent<RectangleTransformComponent>(e); c.position.y = v; c.is_changed = true; }
float Internals_RectTransform_GetRotation(Scene* s, entt::entity e) { return s->GetComponent<RectangleTransformComponent>(e).rotation; }
void Internals_RectTransform_SetRotation(Scene* s, entt::entity e, float v) { auto& c = s->GetComponent<RectangleTransformComponent>(e); c.rotation = v; c.is_changed = true; }
float Internals_RectTransform_GetSizeX(Scene* s, entt::entity e) { return s->GetComponent<RectangleTransformComponent>(e).size.x; }
float Internals_RectTransform_GetSizeY(Scene* s, entt::entity e) { return s->GetComponent<RectangleTransformComponent>(e).size.y; }
void Internals_RectTransform_SetSizeX(Scene* s, entt::entity e, float v) { auto& c = s->GetComponent<RectangleTransformComponent>(e); c.size.x = v; c.is_changed = true; }
void Internals_RectTransform_SetSizeY(Scene* s, entt::entity e, float v) { auto& c = s->GetComponent<RectangleTransformComponent>(e); c.size.y = v; c.is_changed = true; }
float Internals_RectTransform_GetScaleX(Scene* s, entt::entity e) { return s->GetComponent<RectangleTransformComponent>(e).scale.x; }
float Internals_RectTransform_GetScaleY(Scene* s, entt::entity e) { return s->GetComponent<RectangleTransformComponent>(e).scale.y; }
void Internals_RectTransform_SetScaleX(Scene* s, entt::entity e, float v) { auto& c = s->GetComponent<RectangleTransformComponent>(e); c.scale.x = v; c.is_changed = true; }
void Internals_RectTransform_SetScaleY(Scene* s, entt::entity e, float v) { auto& c = s->GetComponent<RectangleTransformComponent>(e); c.scale.y = v; c.is_changed = true; }
int32_t Internals_RectTransform_GetAnchor(Scene* s, entt::entity e) { return static_cast<int32_t>(s->GetComponent<RectangleTransformComponent>(e).anchor); }
void Internals_RectTransform_SetAnchor(Scene* s, entt::entity e, int32_t v) { auto& c = s->GetComponent<RectangleTransformComponent>(e); c.anchor = static_cast<AnchorPreset>(v); c.is_changed = true; }
int32_t Internals_RectTransform_GetPivot(Scene* s, entt::entity e) { return static_cast<int32_t>(s->GetComponent<RectangleTransformComponent>(e).pivot); }
void Internals_RectTransform_SetPivot(Scene* s, entt::entity e, int32_t v) { auto& c = s->GetComponent<RectangleTransformComponent>(e); c.pivot = static_cast<AnchorPreset>(v); c.is_changed = true; }
int32_t Internals_RectTransform_GetSizeModeX(Scene* s, entt::entity e) { return static_cast<int32_t>(s->GetComponent<RectangleTransformComponent>(e).size_mode_x); }
void Internals_RectTransform_SetSizeModeX(Scene* s, entt::entity e, int32_t v) { auto& c = s->GetComponent<RectangleTransformComponent>(e); c.size_mode_x = static_cast<SizeMode>(v); c.is_changed = true; }
int32_t Internals_RectTransform_GetSizeModeY(Scene* s, entt::entity e) { return static_cast<int32_t>(s->GetComponent<RectangleTransformComponent>(e).size_mode_y); }
void Internals_RectTransform_SetSizeModeY(Scene* s, entt::entity e, int32_t v) { auto& c = s->GetComponent<RectangleTransformComponent>(e); c.size_mode_y = static_cast<SizeMode>(v); c.is_changed = true; }
float Internals_RectTransform_GetPaddingLeft(Scene* s, entt::entity e) { return s->GetComponent<RectangleTransformComponent>(e).padding.x; }
float Internals_RectTransform_GetPaddingTop(Scene* s, entt::entity e) { return s->GetComponent<RectangleTransformComponent>(e).padding.y; }
float Internals_RectTransform_GetPaddingRight(Scene* s, entt::entity e) { return s->GetComponent<RectangleTransformComponent>(e).padding.z; }
float Internals_RectTransform_GetPaddingBottom(Scene* s, entt::entity e) { return s->GetComponent<RectangleTransformComponent>(e).padding.w; }
void Internals_RectTransform_SetPaddingLeft(Scene* s, entt::entity e, float v) { auto& c = s->GetComponent<RectangleTransformComponent>(e); c.padding.x = v; c.is_changed = true; }
void Internals_RectTransform_SetPaddingTop(Scene* s, entt::entity e, float v) { auto& c = s->GetComponent<RectangleTransformComponent>(e); c.padding.y = v; c.is_changed = true; }
void Internals_RectTransform_SetPaddingRight(Scene* s, entt::entity e, float v) { auto& c = s->GetComponent<RectangleTransformComponent>(e); c.padding.z = v; c.is_changed = true; }
void Internals_RectTransform_SetPaddingBottom(Scene* s, entt::entity e, float v) { auto& c = s->GetComponent<RectangleTransformComponent>(e); c.padding.w = v; c.is_changed = true; }
float Internals_RectTransform_GetComputedPositionX(Scene* s, entt::entity e) { return s->GetComponent<RectangleTransformComponent>(e).computed_position.x; }
float Internals_RectTransform_GetComputedPositionY(Scene* s, entt::entity e) { return s->GetComponent<RectangleTransformComponent>(e).computed_position.y; }
float Internals_RectTransform_GetComputedSizeX(Scene* s, entt::entity e) { return s->GetComponent<RectangleTransformComponent>(e).computed_size.x; }
float Internals_RectTransform_GetComputedSizeY(Scene* s, entt::entity e) { return s->GetComponent<RectangleTransformComponent>(e).computed_size.y; }

// --- CanvasComponent ---
int32_t Internals_Canvas_GetDirection(Scene* s, entt::entity e) { return static_cast<int32_t>(s->GetComponent<CanvasComponent>(e).direction); }
void Internals_Canvas_SetDirection(Scene* s, entt::entity e, int32_t v) { s->GetComponent<CanvasComponent>(e).direction = static_cast<LayoutDirection>(v); }
int32_t Internals_Canvas_GetAlignment(Scene* s, entt::entity e) { return static_cast<int32_t>(s->GetComponent<CanvasComponent>(e).alignment); }
void Internals_Canvas_SetAlignment(Scene* s, entt::entity e, int32_t v) { s->GetComponent<CanvasComponent>(e).alignment = static_cast<ChildAlignment>(v); }
float Internals_Canvas_GetSpacing(Scene* s, entt::entity e) { return s->GetComponent<CanvasComponent>(e).spacing; }
void Internals_Canvas_SetSpacing(Scene* s, entt::entity e, float v) { s->GetComponent<CanvasComponent>(e).spacing = v; }
int32_t Internals_Canvas_GetSortOrder(Scene* s, entt::entity e) { return s->GetComponent<CanvasComponent>(e).sort_order; }
void Internals_Canvas_SetSortOrder(Scene* s, entt::entity e, int32_t v) { s->GetComponent<CanvasComponent>(e).sort_order = v; }

// --- CanvasRectComponent ---
float Internals_CanvasRect_GetColorR(Scene* s, entt::entity e) { return s->GetComponent<CanvasRectComponent>(e).color.r; }
float Internals_CanvasRect_GetColorG(Scene* s, entt::entity e) { return s->GetComponent<CanvasRectComponent>(e).color.g; }
float Internals_CanvasRect_GetColorB(Scene* s, entt::entity e) { return s->GetComponent<CanvasRectComponent>(e).color.b; }
float Internals_CanvasRect_GetColorA(Scene* s, entt::entity e) { return s->GetComponent<CanvasRectComponent>(e).color.a; }
void Internals_CanvasRect_SetColorR(Scene* s, entt::entity e, float v) { s->GetComponent<CanvasRectComponent>(e).color.r = v; }
void Internals_CanvasRect_SetColorG(Scene* s, entt::entity e, float v) { s->GetComponent<CanvasRectComponent>(e).color.g = v; }
void Internals_CanvasRect_SetColorB(Scene* s, entt::entity e, float v) { s->GetComponent<CanvasRectComponent>(e).color.b = v; }
void Internals_CanvasRect_SetColorA(Scene* s, entt::entity e, float v) { s->GetComponent<CanvasRectComponent>(e).color.a = v; }

// --- CanvasImageComponent ---
float Internals_CanvasImage_GetTintR(Scene* s, entt::entity e) { return s->GetComponent<CanvasImageComponent>(e).tint.r; }
float Internals_CanvasImage_GetTintG(Scene* s, entt::entity e) { return s->GetComponent<CanvasImageComponent>(e).tint.g; }
float Internals_CanvasImage_GetTintB(Scene* s, entt::entity e) { return s->GetComponent<CanvasImageComponent>(e).tint.b; }
float Internals_CanvasImage_GetTintA(Scene* s, entt::entity e) { return s->GetComponent<CanvasImageComponent>(e).tint.a; }
void Internals_CanvasImage_SetTintR(Scene* s, entt::entity e, float v) { s->GetComponent<CanvasImageComponent>(e).tint.r = v; s->GetComponent<CanvasImageComponent>(e).gpu_dirty_ = true; }
void Internals_CanvasImage_SetTintG(Scene* s, entt::entity e, float v) { s->GetComponent<CanvasImageComponent>(e).tint.g = v; s->GetComponent<CanvasImageComponent>(e).gpu_dirty_ = true; }
void Internals_CanvasImage_SetTintB(Scene* s, entt::entity e, float v) { s->GetComponent<CanvasImageComponent>(e).tint.b = v; s->GetComponent<CanvasImageComponent>(e).gpu_dirty_ = true; }
void Internals_CanvasImage_SetTintA(Scene* s, entt::entity e, float v) { s->GetComponent<CanvasImageComponent>(e).tint.a = v; s->GetComponent<CanvasImageComponent>(e).gpu_dirty_ = true; }
float Internals_CanvasImage_GetUVRectX(Scene* s, entt::entity e) { return s->GetComponent<CanvasImageComponent>(e).uv_rect.x; }
float Internals_CanvasImage_GetUVRectY(Scene* s, entt::entity e) { return s->GetComponent<CanvasImageComponent>(e).uv_rect.y; }
float Internals_CanvasImage_GetUVRectZ(Scene* s, entt::entity e) { return s->GetComponent<CanvasImageComponent>(e).uv_rect.z; }
float Internals_CanvasImage_GetUVRectW(Scene* s, entt::entity e) { return s->GetComponent<CanvasImageComponent>(e).uv_rect.w; }
void Internals_CanvasImage_SetUVRectX(Scene* s, entt::entity e, float v) { s->GetComponent<CanvasImageComponent>(e).uv_rect.x = v; }
void Internals_CanvasImage_SetUVRectY(Scene* s, entt::entity e, float v) { s->GetComponent<CanvasImageComponent>(e).uv_rect.y = v; }
void Internals_CanvasImage_SetUVRectZ(Scene* s, entt::entity e, float v) { s->GetComponent<CanvasImageComponent>(e).uv_rect.z = v; }
void Internals_CanvasImage_SetUVRectW(Scene* s, entt::entity e, float v) { s->GetComponent<CanvasImageComponent>(e).uv_rect.w = v; }

// --- TextComponent ---
MonoString* Internals_Text_GetText(Scene* s, entt::entity e) {
  return mono_string_new(ScriptManager::app_domain(), s->GetComponent<TextComponent>(e).text.c_str());
}
void Internals_Text_SetText(Scene* s, entt::entity e, MonoString* v) {
  const char* str = mono_string_to_utf8(v);
  auto& c = s->GetComponent<TextComponent>(e);
  c.text = str;
  c.gpu_dirty_ = true;
  mono_free((void*)str);
}
MonoString* Internals_Text_GetFontPath(Scene* s, entt::entity e) {
  return mono_string_new(ScriptManager::app_domain(), s->GetComponent<TextComponent>(e).font_path.c_str());
}
void Internals_Text_SetFontPath(Scene* s, entt::entity e, MonoString* v) {
  const char* str = mono_string_to_utf8(v);
  auto& c = s->GetComponent<TextComponent>(e);
  c.font_path = str;
  c.gpu_dirty_ = true;
  mono_free((void*)str);
}
float Internals_Text_GetFontSize(Scene* s, entt::entity e) { return s->GetComponent<TextComponent>(e).font_size; }
void Internals_Text_SetFontSize(Scene* s, entt::entity e, float v) { auto& c = s->GetComponent<TextComponent>(e); c.font_size = v; c.gpu_dirty_ = true; }
float Internals_Text_GetColorR(Scene* s, entt::entity e) { return s->GetComponent<TextComponent>(e).color.r; }
float Internals_Text_GetColorG(Scene* s, entt::entity e) { return s->GetComponent<TextComponent>(e).color.g; }
float Internals_Text_GetColorB(Scene* s, entt::entity e) { return s->GetComponent<TextComponent>(e).color.b; }
float Internals_Text_GetColorA(Scene* s, entt::entity e) { return s->GetComponent<TextComponent>(e).color.a; }
void Internals_Text_SetColorR(Scene* s, entt::entity e, float v) { auto& c = s->GetComponent<TextComponent>(e); c.color.r = v; c.gpu_dirty_ = true; }
void Internals_Text_SetColorG(Scene* s, entt::entity e, float v) { auto& c = s->GetComponent<TextComponent>(e); c.color.g = v; c.gpu_dirty_ = true; }
void Internals_Text_SetColorB(Scene* s, entt::entity e, float v) { auto& c = s->GetComponent<TextComponent>(e); c.color.b = v; c.gpu_dirty_ = true; }
void Internals_Text_SetColorA(Scene* s, entt::entity e, float v) { auto& c = s->GetComponent<TextComponent>(e); c.color.a = v; c.gpu_dirty_ = true; }

// --- AnimatorComponent ---
void Internals_Animator_SetBool(Scene* s, entt::entity e, MonoString* name, bool value) {
  const char* cstr = mono_string_to_utf8(name);
  s->GetComponent<AnimatorComponent>(e).SetBool(cstr, value);
  mono_free((void*)cstr);
}
void Internals_Animator_SetInt(Scene* s, entt::entity e, MonoString* name, int value) {
  const char* cstr = mono_string_to_utf8(name);
  s->GetComponent<AnimatorComponent>(e).SetInt(cstr, value);
  mono_free((void*)cstr);
}
void Internals_Animator_SetFloat(Scene* s, entt::entity e, MonoString* name, float value) {
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
void Internals_Animator_Play(Scene* s, entt::entity e, MonoString* stateName, float blendTime) {
  const char* cstr = mono_string_to_utf8(stateName);
  s->GetComponent<AnimatorComponent>(e).Play(cstr, blendTime);
  mono_free((void*)cstr);
}
MonoString* Internals_Animator_GetCurrentState(Scene* s, entt::entity e) {
  auto& anim = s->GetComponent<AnimatorComponent>(e);
  return mono_string_new(ScriptManager::app_domain(), anim.current_state_name.c_str());
}
bool Internals_Animator_GetIsPlaying(Scene* s, entt::entity e) {
  return s->GetComponent<AnimatorComponent>(e).playing;
}
void Internals_Animator_SetIsPlaying(Scene* s, entt::entity e, bool value) {
  s->GetComponent<AnimatorComponent>(e).playing = value;
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

void ScriptInstance::OnUpdate(float_t delta_time) {
  PROFILE_ZONE_SCOPED_N("ScriptInstance::OnUpdate");
  if (!has_started_) {
    OnStart();
    has_started_ = true;
  }

  mono_domain_set(ScriptManager::app_domain(), true);
  void* args[1];
  args[0] = &delta_time;
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

void ScriptInstance::OnTriggerEnter(entt::entity other) {
  if (!script_data_->on_trigger_enter_method()) return;
  mono_domain_set(ScriptManager::app_domain(), true);
  void* args[1];
  uint64_t otherId = (uint64_t)other;
  args[0] = &otherId;
  mono_runtime_invoke(script_data_->on_trigger_enter_method(), handle_, args,
                      nullptr);
}

void ScriptInstance::OnTriggerStay(entt::entity other) {
  if (!script_data_->on_trigger_stay_method()) return;
  mono_domain_set(ScriptManager::app_domain(), true);
  void* args[1];
  uint64_t otherId = (uint64_t)other;
  args[0] = &otherId;
  mono_runtime_invoke(script_data_->on_trigger_stay_method(), handle_, args,
                      nullptr);
}

void ScriptInstance::OnTriggerExit(entt::entity other) {
  if (!script_data_->on_trigger_exit_method()) return;
  mono_domain_set(ScriptManager::app_domain(), true);
  void* args[1];
  uint64_t otherId = (uint64_t)other;
  args[0] = &otherId;
  mono_runtime_invoke(script_data_->on_trigger_exit_method(), handle_, args,
                      nullptr);
}

void ScriptInstance::OnCollisionEnter(entt::entity other) {
  if (!script_data_->on_collision_enter_method()) return;
  mono_domain_set(ScriptManager::app_domain(), true);
  void* args[1];
  uint64_t otherId = (uint64_t)other;
  args[0] = &otherId;
  mono_runtime_invoke(script_data_->on_collision_enter_method(), handle_, args,
                      nullptr);
}

void ScriptInstance::OnCollisionStay(entt::entity other) {
  if (!script_data_->on_collision_stay_method()) return;
  mono_domain_set(ScriptManager::app_domain(), true);
  void* args[1];
  uint64_t otherId = (uint64_t)other;
  args[0] = &otherId;
  mono_runtime_invoke(script_data_->on_collision_stay_method(), handle_, args,
                      nullptr);
}

void ScriptInstance::OnCollisionExit(entt::entity other) {
  if (!script_data_->on_collision_exit_method()) return;
  mono_domain_set(ScriptManager::app_domain(), true);
  void* args[1];
  uint64_t otherId = (uint64_t)other;
  args[0] = &otherId;
  mono_runtime_invoke(script_data_->on_collision_exit_method(), handle_, args,
                      nullptr);
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

MonoDomain* ScriptManager::root_domain_ = nullptr;
MonoAssembly* ScriptManager::core_assembly_ = nullptr;
MonoImage* ScriptManager::core_assembly_image_ = nullptr;
MonoDomain* ScriptManager::app_domain_ = nullptr;
MonoAssembly* ScriptManager::app_assembly_ = nullptr;
MonoImage* ScriptManager::app_assembly_image_ = nullptr;
MonoClass* ScriptManager::behavior_class_ = nullptr;
MonoClass* ScriptManager::transform_component_class_ = nullptr;
MonoClass* ScriptManager::model_component_class_ = nullptr;
MonoClass* ScriptManager::box_collider_class_ = nullptr;
MonoClass* ScriptManager::sphere_collider_class_ = nullptr;
MonoClass* ScriptManager::rigidbody_class_ = nullptr;
MonoClass* ScriptManager::rect_transform_class_ = nullptr;
MonoClass* ScriptManager::canvas_component_class_ = nullptr;
MonoClass* ScriptManager::canvas_rect_class_ = nullptr;
MonoClass* ScriptManager::canvas_image_class_ = nullptr;
MonoClass* ScriptManager::text_component_class_ = nullptr;
MonoClass* ScriptManager::animator_component_class_ = nullptr;
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

  // Unregister old script assets before re-registering
  AssetManager& mgr = AssetManager::Get();
  for (AssetHandle handle : mgr.GetAllOfType(AssetType::Script)) {
    mgr.Unregister(handle);
  }

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
  std::string dll_path = "obj/Core.dll";

  if (Engine::GetEngineProperties().dev_mode) {
    LOG_INFO("Compiling core scripts...");
    std::vector<std::string> sourceFiles;
    std::optional<std::filesystem::path> physical = Engine::GetVirtualFileSystem()->GetPhysicalPath("/engine/internal_scripts");
    assert(physical.has_value());
    for (const auto& entry : std::filesystem::recursive_directory_iterator(*physical)) {
      if (entry.is_regular_file() && entry.path().extension() == ".cs") {
        std::string name = entry.path().string();
        LOG_INFO("Found internal script {}", name);
        sourceFiles.push_back(name);

        std::filesystem::path rel = std::filesystem::relative(entry.path(), *physical);
        std::string vfs_path = "/engine/internal_scripts/" + rel.generic_string();
        std::string script_name = entry.path().stem().string();
        AssetManager::Get().Register(script_name, AssetType::Script, vfs_path);
      }
    }
    CompileToDLL(dll_path, sourceFiles, "", {}, enable_debugger_);
  } else {
    // Release: Core.dll should be pre-compiled and placed next to the executable
    dll_path = "Core.dll";
    LOG_INFO("Loading pre-compiled core scripts from {}", dll_path);
  }

  core_assembly_ = mono_domain_assembly_open(root_domain_, dll_path.c_str());
  assert(core_assembly_);

  core_assembly_image_ = mono_assembly_get_image(core_assembly_);
  behavior_class_ =
      mono_class_from_name(core_assembly_image_, "WieselEngine", "MonoBehavior");
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
  rigidbody_class_ = mono_class_from_name(
      core_assembly_image_, "WieselEngine", "RigidBodyComponent");
  rect_transform_class_ = mono_class_from_name(
      core_assembly_image_, "WieselEngine", "RectTransformComponent");
  canvas_component_class_ = mono_class_from_name(
      core_assembly_image_, "WieselEngine", "CanvasComponent");
  canvas_rect_class_ = mono_class_from_name(
      core_assembly_image_, "WieselEngine", "CanvasRectComponent");
  canvas_image_class_ = mono_class_from_name(
      core_assembly_image_, "WieselEngine", "CanvasImageComponent");
  text_component_class_ = mono_class_from_name(
      core_assembly_image_, "WieselEngine", "TextComponent");
  animator_component_class_ = mono_class_from_name(
      core_assembly_image_, "WieselEngine", "AnimatorComponent");
  vector3f_class_ =
      mono_class_from_name(core_assembly_image_, "WieselEngine", "Vector3f");
}

void ScriptManager::LoadApp() {
  std::string dll_path = "obj/App.dll";

  if (Engine::GetEngineProperties().dev_mode) {
    // Dev mode: compile from source
    std::optional<std::filesystem::path> physical = Engine::GetVirtualFileSystem()->GetPhysicalPath("/app/scripts");
    if (!physical.has_value() || !std::filesystem::exists(*physical)) {
      LOG_WARN("No app scripts directory found, skipping app script loading");
      return;
    }
    std::vector<std::string> sourceFiles;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(*physical)) {
      if (entry.is_regular_file() && entry.path().extension() == ".cs") {
        std::string name = entry.path().string();
        LOG_INFO("Found user script {}", name);
        sourceFiles.push_back(name);

        std::filesystem::path rel = std::filesystem::relative(entry.path(), *physical);
        std::string vfs_path = "/app/scripts/" + rel.generic_string();
        std::string script_name = entry.path().stem().string();
        AssetManager::Get().Register(script_name, AssetType::Script, vfs_path);
      }
    }
    std::vector<std::string> link_libs;
    for (const auto& entry : std::filesystem::recursive_directory_iterator("obj")) {
      if (entry.is_regular_file() && entry.path().extension() == ".dll") {
        std::string name = entry.path().string();
        link_libs.push_back(name);
        LOG_INFO("Found DLL to link {}", name);
      }
    }
    if (!CompileToDLL(dll_path, sourceFiles, "obj", link_libs, enable_debugger_)) {
      return;
    }
  } else {
    // Release: App.dll should be pre-compiled and placed next to the executable
    dll_path = "App.dll";
    if (!std::filesystem::exists(dll_path)) {
      LOG_WARN("No pre-compiled App.dll found, skipping app scripts");
      return;
    }
    LOG_INFO("Loading pre-compiled app scripts from {}", dll_path);
  }

  app_domain_ = mono_domain_create_appdomain(const_cast<char*>("WieselApp"), nullptr);
  mono_domain_set(app_domain_, true);
  app_assembly_ = mono_domain_assembly_open(app_domain_, dll_path.c_str());
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
    MonoMethod* onTriggerEnterMethod =
        mono_class_get_method_from_name(klass, "OnTriggerEnter", 1);
    MonoMethod* onTriggerStayMethod =
        mono_class_get_method_from_name(klass, "OnTriggerStay", 1);
    MonoMethod* onTriggerExitMethod =
        mono_class_get_method_from_name(klass, "OnTriggerExit", 1);
    MonoMethod* onCollisionEnterMethod =
        mono_class_get_method_from_name(klass, "OnCollisionEnter", 1);
    MonoMethod* onCollisionStayMethod =
        mono_class_get_method_from_name(klass, "OnCollisionStay", 1);
    MonoMethod* onCollisionExitMethod =
        mono_class_get_method_from_name(klass, "OnCollisionExit", 1);
    script_data_.insert(std::pair(
        className,
        std::make_shared<ScriptData>(klass, onStartMethod, onUpdateMethod, set_handle_method_,
                       onKeyPressedMethod, onKeyReleasedMethod,
                       onMouseMovedMethod,
                       onTriggerEnterMethod, onTriggerStayMethod,
                       onTriggerExitMethod,
                       onCollisionEnterMethod, onCollisionStayMethod,
                       onCollisionExitMethod, fields)));
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
  WIESEL_ADD_INTERNAL_CALL(ModelComponent_GetEnableRendering);
  WIESEL_ADD_INTERNAL_CALL(ModelComponent_SetEnableRendering);
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
  WIESEL_ADD_INTERNAL_CALL(Text_GetFontPath);
  WIESEL_ADD_INTERNAL_CALL(Text_SetFontPath);
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

  RegisterComponent<ModelComponent>(
      "ModelComponent",
      [](Scene* scene, entt::entity entity) -> MonoObject* {
        MonoObject* obj =
            mono_object_new(app_domain_, model_component_class_);
        void* args[2];
        uint64_t scenePtr = (uint64_t)scene;
        uint64_t entityId = (uint64_t)entity;
        args[0] = &scenePtr;
        args[1] = &entityId;

        MonoMethod* method = mono_class_get_method_from_name(
            model_component_class_, ".ctor", 2);
        mono_runtime_invoke(method, obj, args, nullptr);
        return obj;
      },
      [](Scene* scene, entt::entity entity) -> bool {
        return scene->HasComponent<ModelComponent>(entity);
      });

  RegisterComponent<BoxColliderComponent>(
      "BoxColliderComponent",
      [](Scene* scene, entt::entity entity) -> MonoObject* {
        MonoObject* obj =
            mono_object_new(app_domain_, box_collider_class_);
        void* args[2];
        uint64_t scenePtr = (uint64_t)scene;
        uint64_t entityId = (uint64_t)entity;
        args[0] = &scenePtr;
        args[1] = &entityId;

        MonoMethod* method = mono_class_get_method_from_name(
            box_collider_class_, ".ctor", 2);
        mono_runtime_invoke(method, obj, args, nullptr);
        return obj;
      },
      [](Scene* scene, entt::entity entity) -> bool {
        return scene->HasComponent<BoxColliderComponent>(entity);
      });

  RegisterComponent<SphereColliderComponent>(
      "SphereColliderComponent",
      [](Scene* scene, entt::entity entity) -> MonoObject* {
        MonoObject* obj =
            mono_object_new(app_domain_, sphere_collider_class_);
        void* args[2];
        uint64_t scenePtr = (uint64_t)scene;
        uint64_t entityId = (uint64_t)entity;
        args[0] = &scenePtr;
        args[1] = &entityId;

        MonoMethod* method = mono_class_get_method_from_name(
            sphere_collider_class_, ".ctor", 2);
        mono_runtime_invoke(method, obj, args, nullptr);
        return obj;
      },
      [](Scene* scene, entt::entity entity) -> bool {
        return scene->HasComponent<SphereColliderComponent>(entity);
      });

  RegisterComponent<RigidBodyComponent>(
      "RigidBodyComponent",
      [](Scene* scene, entt::entity entity) -> MonoObject* {
        MonoObject* obj =
            mono_object_new(app_domain_, rigidbody_class_);
        void* args[2];
        uint64_t scenePtr = (uint64_t)scene;
        uint64_t entityId = (uint64_t)entity;
        args[0] = &scenePtr;
        args[1] = &entityId;

        MonoMethod* method = mono_class_get_method_from_name(
            rigidbody_class_, ".ctor", 2);
        mono_runtime_invoke(method, obj, args, nullptr);
        return obj;
      },
      [](Scene* scene, entt::entity entity) -> bool {
        return scene->HasComponent<RigidBodyComponent>(entity);
      });

  RegisterComponent<RectangleTransformComponent>(
      "RectTransformComponent",
      [](Scene* scene, entt::entity entity) -> MonoObject* {
        MonoObject* obj = mono_object_new(app_domain_, rect_transform_class_);
        void* args[2];
        uint64_t scenePtr = (uint64_t)scene;
        uint64_t entityId = (uint64_t)entity;
        args[0] = &scenePtr;
        args[1] = &entityId;
        MonoMethod* method = mono_class_get_method_from_name(
            rect_transform_class_, ".ctor", 2);
        mono_runtime_invoke(method, obj, args, nullptr);
        return obj;
      },
      [](Scene* scene, entt::entity entity) -> bool {
        return scene->HasComponent<RectangleTransformComponent>(entity);
      });

  RegisterComponent<CanvasComponent>(
      "CanvasComponent",
      [](Scene* scene, entt::entity entity) -> MonoObject* {
        MonoObject* obj = mono_object_new(app_domain_, canvas_component_class_);
        void* args[2];
        uint64_t scenePtr = (uint64_t)scene;
        uint64_t entityId = (uint64_t)entity;
        args[0] = &scenePtr;
        args[1] = &entityId;
        MonoMethod* method = mono_class_get_method_from_name(
            canvas_component_class_, ".ctor", 2);
        mono_runtime_invoke(method, obj, args, nullptr);
        return obj;
      },
      [](Scene* scene, entt::entity entity) -> bool {
        return scene->HasComponent<CanvasComponent>(entity);
      });

  RegisterComponent<CanvasRectComponent>(
      "CanvasRectComponent",
      [](Scene* scene, entt::entity entity) -> MonoObject* {
        MonoObject* obj = mono_object_new(app_domain_, canvas_rect_class_);
        void* args[2];
        uint64_t scenePtr = (uint64_t)scene;
        uint64_t entityId = (uint64_t)entity;
        args[0] = &scenePtr;
        args[1] = &entityId;
        MonoMethod* method = mono_class_get_method_from_name(
            canvas_rect_class_, ".ctor", 2);
        mono_runtime_invoke(method, obj, args, nullptr);
        return obj;
      },
      [](Scene* scene, entt::entity entity) -> bool {
        return scene->HasComponent<CanvasRectComponent>(entity);
      });

  RegisterComponent<CanvasImageComponent>(
      "CanvasImageComponent",
      [](Scene* scene, entt::entity entity) -> MonoObject* {
        MonoObject* obj = mono_object_new(app_domain_, canvas_image_class_);
        void* args[2];
        uint64_t scenePtr = (uint64_t)scene;
        uint64_t entityId = (uint64_t)entity;
        args[0] = &scenePtr;
        args[1] = &entityId;
        MonoMethod* method = mono_class_get_method_from_name(
            canvas_image_class_, ".ctor", 2);
        mono_runtime_invoke(method, obj, args, nullptr);
        return obj;
      },
      [](Scene* scene, entt::entity entity) -> bool {
        return scene->HasComponent<CanvasImageComponent>(entity);
      });

  RegisterComponent<TextComponent>(
      "TextComponent",
      [](Scene* scene, entt::entity entity) -> MonoObject* {
        MonoObject* obj = mono_object_new(app_domain_, text_component_class_);
        void* args[2];
        uint64_t scenePtr = (uint64_t)scene;
        uint64_t entityId = (uint64_t)entity;
        args[0] = &scenePtr;
        args[1] = &entityId;
        MonoMethod* method = mono_class_get_method_from_name(
            text_component_class_, ".ctor", 2);
        mono_runtime_invoke(method, obj, args, nullptr);
        return obj;
      },
      [](Scene* scene, entt::entity entity) -> bool {
        return scene->HasComponent<TextComponent>(entity);
      });

  if (animator_component_class_) {
    RegisterComponent<AnimatorComponent>(
        "AnimatorComponent",
        [](Scene* scene, entt::entity entity) -> MonoObject* {
          MonoObject* obj =
              mono_object_new(app_domain_, animator_component_class_);
          void* args[2];
          uint64_t scenePtr = (uint64_t)scene;
          uint64_t entityId = (uint64_t)entity;
          args[0] = &scenePtr;
          args[1] = &entityId;
          MonoMethod* method = mono_class_get_method_from_name(
              animator_component_class_, ".ctor", 2);
          mono_runtime_invoke(method, obj, args, nullptr);
          return obj;
        },
        [](Scene* scene, entt::entity entity) -> bool {
          return scene->HasComponent<AnimatorComponent>(entity);
        });
  }
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