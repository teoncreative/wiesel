
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "physics/w_physics_world.h"

#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include "asset/w_asset_manager.h"
#include "behavior/w_behavior.h"
#include "physics/w_collider.h"
#include "physics/w_jolt_layers.h"
#include "physics/w_mesh_collider_asset.h"
#include "physics/w_rigidbody.h"
#include "rendering/w_mesh.h"
#include "scene/w_components.h"
#include "scene/w_scene.h"
#include "script/mono/w_monobehavior.h"
#include "script/w_scriptmanager.h"
#include "w_engine.h"

JPH_SUPPRESS_WARNINGS

using namespace JPH;
using namespace JPH::literals;

namespace wiesel {

// Jolt global init guard
static bool s_jolt_initialized = false;

static void InitJoltOnce() {
  if (s_jolt_initialized) {
    return;
  }
  JPH::RegisterDefaultAllocator();
  JPH::Factory::sInstance = new JPH::Factory();
  JPH::RegisterTypes();
  s_jolt_initialized = true;
}

// Conversion helpers
static JPH::Vec3 ToJolt(const glm::vec3& v) {
  return JPH::Vec3(v.x, v.y, v.z);
}

static glm::vec3 ToGlm(JPH::Vec3 v) {
  return glm::vec3(v.GetX(), v.GetY(), v.GetZ());
}

static JPH::Quat ToJoltQuat(const glm::quat& q) {
  return JPH::Quat(q.x, q.y, q.z, q.w);
}

static glm::quat ToGlmQuat(JPH::Quat q) {
  return glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ());
}

// Contact event types
enum class ContactEventType { Added, Persisted, Removed };

struct ContactEvent {
  uint32_t body_id_a;
  uint32_t body_id_b;
  ContactEventType type;
  bool is_sensor;
};

class WieselContactListener : public JPH::ContactListener {
 public:
  // Track which body IDs are one-way platforms
  std::mutex one_way_mutex_;
  std::set<uint32_t> one_way_bodies_;

  void SetOneWay(uint32_t body_id_raw, bool enabled) {
    std::lock_guard<std::mutex> lock(one_way_mutex_);
    if (enabled) {
      one_way_bodies_.insert(body_id_raw);
    } else {
      one_way_bodies_.erase(body_id_raw);
    }
  }

  ValidateResult OnContactValidate(
      const Body& inBody1, const Body& inBody2, RVec3Arg inBaseOffset,
      const CollideShapeResult& inCollisionResult) override {
    // One-way platform: only allow contact if the other body is above
    // (contact normal points upward from the platform's perspective)
    {
      std::lock_guard<std::mutex> lock(one_way_mutex_);
      bool b1_one_way =
          one_way_bodies_.contains(inBody1.GetID().GetIndexAndSequenceNumber());
      bool b2_one_way =
          one_way_bodies_.contains(inBody2.GetID().GetIndexAndSequenceNumber());
      if (b1_one_way || b2_one_way) {
        // The penetration axis points from body 1 to body 2.
        // For a one-way platform, only accept if the normal has a
        // significant upward component (object is on top).
        Vec3 normal = inCollisionResult.mPenetrationAxis.Normalized();
        // If the one-way body is body1, the normal points from it to body2.
        // Accept only if normal points up (body2 is above body1).
        // If one-way is body2, normal points from body1 to body2,
        // accept only if normal points down (body1 is above body2).
        float up_component = b1_one_way ? normal.GetY() : -normal.GetY();
        if (up_component < 0.3f) {
          return ValidateResult::RejectContact;
        }
      }
    }
    return ValidateResult::AcceptAllContactsForThisBodyPair;
  }

  void OnContactAdded(const Body& inBody1, const Body& inBody2,
                      const ContactManifold& inManifold,
                      ContactSettings& ioSettings) override {
    bool sensor = inBody1.IsSensor() || inBody2.IsSensor();
    std::lock_guard<std::mutex> lock(mutex_);
    events_.push_back({inBody1.GetID().GetIndexAndSequenceNumber(),
                       inBody2.GetID().GetIndexAndSequenceNumber(),
                       ContactEventType::Added, sensor});
  }

  void OnContactPersisted(const Body& inBody1, const Body& inBody2,
                          const ContactManifold& inManifold,
                          ContactSettings& ioSettings) override {
    // We track enter/exit via sets, so persisted is implicitly handled
  }

  void OnContactRemoved(const SubShapeIDPair& inSubShapePair) override {
    // During OnContactRemoved we cannot access bodies, so we just record
    // the body IDs. We determine sensor status from our tracking sets.
    std::lock_guard<std::mutex> lock(mutex_);
    events_.push_back({inSubShapePair.GetBody1ID().GetIndexAndSequenceNumber(),
                       inSubShapePair.GetBody2ID().GetIndexAndSequenceNumber(),
                       ContactEventType::Removed, false});
  }

  std::vector<ContactEvent> SwapEvents() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ContactEvent> out;
    out.swap(events_);
    return out;
  }

 private:
  std::mutex mutex_;
  std::vector<ContactEvent> events_;
};

// Constants
static constexpr uint kMaxBodies = 65536;
static constexpr uint kNumBodyMutexes = 0;
static constexpr uint kMaxBodyPairs = 65536;
static constexpr uint kMaxContactConstraints = 16384;

PhysicsWorld::PhysicsWorld(Scene* scene) : scene_(scene) {
  InitJoltOnce();

  temp_allocator_ = new TempAllocatorImpl(32 * 1024 * 1024);
  job_system_ = new JobSystemThreadPool(
      cMaxPhysicsJobs, cMaxPhysicsBarriers,
      static_cast<int>(std::max(1u, std::thread::hardware_concurrency() - 1)));

  bp_layer_interface_ = std::make_unique<BroadPhaseLayerInterfaceImpl>();
  obj_vs_bp_filter_ = std::make_unique<ObjectVsBroadPhaseLayerFilterImpl>();
  obj_layer_pair_filter_ = std::make_unique<ObjectLayerPairFilterImpl>();

  physics_system_ = new PhysicsSystem();
  physics_system_->Init(kMaxBodies, kNumBodyMutexes, kMaxBodyPairs,
                        kMaxContactConstraints, *bp_layer_interface_,
                        *obj_vs_bp_filter_, *obj_layer_pair_filter_);

  physics_system_->SetGravity(Vec3(0.0f, -20.0f, 0.0f));

  contact_listener_ = std::make_unique<WieselContactListener>();
  physics_system_->SetContactListener(contact_listener_.get());
}

PhysicsWorld::~PhysicsWorld() {
  BodyInterface& body_interface = physics_system_->GetBodyInterface();

  for (auto& [entity, data] : bodies_) {
    BodyID id(data.body_id_raw);
    body_interface.RemoveBody(id);
    body_interface.DestroyBody(id);
  }
  bodies_.clear();

  delete physics_system_;
  delete job_system_;
  delete temp_allocator_;
}

glm::vec3 PhysicsWorld::GetColliderOffset(entt::entity entity) const {
  auto& registry = scene_->GetRegistry();
  if (registry.any_of<BoxColliderComponent>(entity)) {
    return registry.get<BoxColliderComponent>(entity).offset;
  }
  if (registry.any_of<SphereColliderComponent>(entity)) {
    return registry.get<SphereColliderComponent>(entity).offset;
  }
  if (registry.any_of<CapsuleColliderComponent>(entity)) {
    return registry.get<CapsuleColliderComponent>(entity).offset;
  }
  if (registry.any_of<MeshColliderComponent>(entity)) {
    return registry.get<MeshColliderComponent>(entity).offset;
  }
  if (registry.any_of<HeightfieldColliderComponent>(entity)) {
    auto& hf = registry.get<HeightfieldColliderComponent>(entity);
    return glm::vec3(0.0f, (hf.min_height + hf.max_height) * 0.5f, 0.0f);
  }
  return glm::vec3(0.0f);
}

JPH::Shape* PhysicsWorld::CreateShapeForEntity(entt::entity entity) const {
  auto& registry = scene_->GetRegistry();

  if (registry.any_of<BoxColliderComponent>(entity)) {
    auto& box = registry.get<BoxColliderComponent>(entity);
    auto result = BoxShapeSettings(ToJolt(box.half_extents)).Create();
    if (result.HasError()) {
      return nullptr;
    }
    // AddRef since we return a raw pointer that the caller will manage
    Shape* shape = const_cast<Shape*>(result.Get().GetPtr());
    shape->AddRef();
    return shape;
  }

  if (registry.any_of<SphereColliderComponent>(entity)) {
    auto& sphere = registry.get<SphereColliderComponent>(entity);
    auto result = SphereShapeSettings(sphere.radius).Create();
    if (result.HasError()) {
      return nullptr;
    }
    Shape* shape = const_cast<Shape*>(result.Get().GetPtr());
    shape->AddRef();
    return shape;
  }

  if (registry.any_of<CapsuleColliderComponent>(entity)) {
    auto& cap = registry.get<CapsuleColliderComponent>(entity);
    float half_height = cap.height * 0.5f;

    RefConst<Shape> base_shape;
    {
      auto result = CapsuleShapeSettings(half_height, cap.radius).Create();
      if (result.HasError()) {
        return nullptr;
      }
      base_shape = result.Get();
    }

    // Jolt capsules are Y-axis by default. Rotate for X or Z.
    if (cap.axis == CapsuleAxis::X) {
      auto result =
          RotatedTranslatedShapeSettings(
              Vec3::sZero(), Quat::sRotation(Vec3::sAxisZ(), 0.5f * JPH_PI),
              base_shape)
              .Create();
      if (result.HasError()) {
        return nullptr;
      }
      Shape* shape = const_cast<Shape*>(result.Get().GetPtr());
      shape->AddRef();
      return shape;
    }
    if (cap.axis == CapsuleAxis::Z) {
      auto result =
          RotatedTranslatedShapeSettings(
              Vec3::sZero(), Quat::sRotation(Vec3::sAxisX(), 0.5f * JPH_PI),
              base_shape)
              .Create();
      if (result.HasError()) {
        return nullptr;
      }
      Shape* shape = const_cast<Shape*>(result.Get().GetPtr());
      shape->AddRef();
      return shape;
    }

    // Y axis - use directly
    Shape* shape = const_cast<Shape*>(base_shape.GetPtr());
    shape->AddRef();
    return shape;
  }

  if (registry.any_of<MeshColliderComponent>(entity)) {
    auto& mesh_comp = registry.get<MeshColliderComponent>(entity);
    auto& mgr = Engine::asset_manager();

    std::shared_ptr<MeshColliderAssetData> baked;

    if (mesh_comp.collider_handle.IsValid()) {
      if (!mgr.IsLoaded(mesh_comp.collider_handle)) {
        mgr.LoadSync(mesh_comp.collider_handle);
      }
      baked = mgr.Get<MeshColliderAssetData>(mesh_comp.collider_handle);
    }

    // Fallback: extract geometry from MeshRendererComponent
    if ((!baked || !baked->cached_shape) &&
        registry.any_of<MeshRendererComponent>(entity)) {
      auto& mr = registry.get<MeshRendererComponent>(entity);
      if (mr.model_handle.IsValid()) {
        baked = BakeMeshColliderFromModel(mr.model_handle);
      }
    }

    if (!baked || !baked->cached_shape) {
      LOG_WARN(
          "MeshCollider on entity {}: no collider asset and no model to "
          "fall back on",
          static_cast<uint32_t>(entity));
      return nullptr;
    }

    // Retrieve the pre-built Jolt shape from the asset
    RefConst<Shape> mesh_shape(
        static_cast<const Shape*>(baked->cached_shape.get()));

    // Apply entity scale
    if (registry.any_of<TransformComponent>(entity)) {
      glm::vec3 scale = registry.get<TransformComponent>(entity).GetScale();
      if (scale != glm::vec3(1.0f)) {
        auto scaled_result =
            ScaledShapeSettings(mesh_shape, Vec3(scale.x, scale.y, scale.z))
                .Create();
        if (!scaled_result.HasError()) {
          mesh_shape = scaled_result.Get();
        }
      }
    }

    Shape* shape = const_cast<Shape*>(mesh_shape.GetPtr());
    shape->AddRef();
    return shape;
  }

  if (registry.any_of<HeightfieldColliderComponent>(entity)) {
    auto& hf = registry.get<HeightfieldColliderComponent>(entity);
    if (hf.width < 2 || hf.length < 2 || hf.height_data.empty()) {
      return nullptr;
    }

    HeightFieldShapeSettings settings(
        hf.height_data.data(), Vec3(0.0f, 0.0f, 0.0f),
        Vec3(hf.scale.x, hf.scale.y, hf.scale.z), hf.width);

    auto result = settings.Create();
    if (result.HasError()) {
      return nullptr;
    }
    Shape* shape = const_cast<Shape*>(result.Get().GetPtr());
    shape->AddRef();
    return shape;
  }

  return nullptr;
}

void PhysicsWorld::CreateBody(entt::entity entity) {
  if (bodies_.count(entity)) {
    return;
  }

  auto& registry = scene_->GetRegistry();

  // Create the collision shape
  Shape* raw_shape = CreateShapeForEntity(entity);
  if (!raw_shape) {
    return;
  }
  RefConst<Shape> shape(raw_shape);
  raw_shape->Release();  // RefConst now owns it

  // Apply collider offset
  glm::vec3 offset = GetColliderOffset(entity);
  if (glm::length(offset) > 0.001f) {
    auto result =
        RotatedTranslatedShapeSettings(ToJolt(offset), Quat::sIdentity(), shape)
            .Create();
    if (!result.HasError()) {
      shape = result.Get();
    }
  }

  // Determine body properties
  bool is_trigger = false;
  uint16_t collision_group = CollisionGroupDefault;

  if (registry.any_of<BoxColliderComponent>(entity)) {
    auto& box = registry.get<BoxColliderComponent>(entity);
    is_trigger = box.is_trigger;
    collision_group = box.collision_group;
  } else if (registry.any_of<SphereColliderComponent>(entity)) {
    auto& sphere = registry.get<SphereColliderComponent>(entity);
    is_trigger = sphere.is_trigger;
    collision_group = sphere.collision_group;
  } else if (registry.any_of<CapsuleColliderComponent>(entity)) {
    auto& cap = registry.get<CapsuleColliderComponent>(entity);
    is_trigger = cap.is_trigger;
    collision_group = cap.collision_group;
  } else if (registry.any_of<MeshColliderComponent>(entity)) {
    auto& mc = registry.get<MeshColliderComponent>(entity);
    is_trigger = mc.is_trigger;
    collision_group = mc.collision_group;
  } else if (registry.any_of<HeightfieldColliderComponent>(entity)) {
    auto& hf = registry.get<HeightfieldColliderComponent>(entity);
    collision_group = hf.collision_group;
  }

  // Check one-way flag
  bool is_one_way = false;
  if (registry.any_of<BoxColliderComponent>(entity)) {
    is_one_way = registry.get<BoxColliderComponent>(entity).is_one_way;
  } else if (registry.any_of<SphereColliderComponent>(entity)) {
    is_one_way = registry.get<SphereColliderComponent>(entity).is_one_way;
  } else if (registry.any_of<CapsuleColliderComponent>(entity)) {
    is_one_way = registry.get<CapsuleColliderComponent>(entity).is_one_way;
  } else if (registry.any_of<MeshColliderComponent>(entity)) {
    is_one_way = registry.get<MeshColliderComponent>(entity).is_one_way;
  }

  RigidBodyType rb_type = RigidBodyType::Static;
  float mass_val = 1.0f;
  float friction_val = 0.5f;
  float restitution_val = 0.0f;
  float linear_damping_val = 0.0f;
  float angular_damping_val = 0.05f;

  bool force_static = registry.any_of<HeightfieldColliderComponent>(entity) ||
                      registry.any_of<MeshColliderComponent>(entity);

  if (!force_static && registry.any_of<RigidBodyComponent>(entity)) {
    auto& rb = registry.get<RigidBodyComponent>(entity);
    rb_type = rb.type;
    mass_val = rb.mass;
    friction_val = rb.friction;
    restitution_val = rb.restitution;
    linear_damping_val = rb.linear_damping;
    angular_damping_val = rb.angular_damping;
  }

  // Jolt motion type
  EMotionType motion_type;
  switch (rb_type) {
    case RigidBodyType::Dynamic:
      motion_type = EMotionType::Dynamic;
      break;
    case RigidBodyType::Kinematic:
      motion_type = EMotionType::Kinematic;
      break;
    default:
      motion_type = EMotionType::Static;
      break;
  }

  // Object layer
  ObjectLayer obj_layer = is_trigger
                              ? ObjectLayers::kSensor
                              : ObjectLayerFromCollisionGroup(collision_group);

  // Get initial transform
  auto& tc = registry.get<TransformComponent>(entity);
  glm::quat q = glm::quat(glm::radians(tc.GetRotation()));

  BodyCreationSettings body_settings(
      shape, RVec3(tc.GetPosition().x, tc.GetPosition().y, tc.GetPosition().z),
      ToJoltQuat(q), motion_type, obj_layer);

  body_settings.mIsSensor = is_trigger;
  body_settings.mFriction = friction_val;
  body_settings.mRestitution = restitution_val;
  body_settings.mLinearDamping = linear_damping_val;
  body_settings.mAngularDamping = angular_damping_val;
  body_settings.mAllowSleeping = false;

  // Mass override for dynamic bodies
  if (motion_type == EMotionType::Dynamic) {
    body_settings.mOverrideMassProperties =
        EOverrideMassProperties::CalculateInertia;
    body_settings.mMassPropertiesOverride.mMass = mass_val;
  }

  // DOF constraints
  if (!force_static && registry.any_of<RigidBodyComponent>(entity)) {
    auto& rb = registry.get<RigidBodyComponent>(entity);
    EAllowedDOFs dofs = EAllowedDOFs::All;
    if (rb.lock_position_x) {
      dofs = dofs & ~EAllowedDOFs::TranslationX;
    }
    if (rb.lock_position_y) {
      dofs = dofs & ~EAllowedDOFs::TranslationY;
    }
    if (rb.lock_position_z) {
      dofs = dofs & ~EAllowedDOFs::TranslationZ;
    }
    if (rb.lock_rotation_x) {
      dofs = dofs & ~EAllowedDOFs::RotationX;
    }
    if (rb.lock_rotation_y) {
      dofs = dofs & ~EAllowedDOFs::RotationY;
    }
    if (rb.lock_rotation_z) {
      dofs = dofs & ~EAllowedDOFs::RotationZ;
    }
    body_settings.mAllowedDOFs = dofs;
  }

  // Create and add the body
  BodyInterface& body_interface = physics_system_->GetBodyInterface();
  BodyID body_id = body_interface.CreateAndAddBody(
      body_settings,
      is_trigger ? EActivation::DontActivate : EActivation::Activate);

  if (body_id.IsInvalid()) {
    LOG_ERROR("Failed to create physics body for entity {}",
              static_cast<uint32_t>(entity));
    return;
  }

  // Store entity handle as user data
  body_interface.SetUserData(
      body_id, static_cast<uint64>(static_cast<uint32_t>(entity)));

  bodies_[entity] = {body_id.GetIndexAndSequenceNumber(), is_trigger};

  // Register one-way platform
  if (is_one_way) {
    contact_listener_->SetOneWay(body_id.GetIndexAndSequenceNumber(), true);
  }

  // Update RigidBodyComponent with Jolt handles
  if (registry.any_of<RigidBodyComponent>(entity)) {
    auto& rb = registry.get<RigidBodyComponent>(entity);
    rb.body_id_raw = body_id.GetIndexAndSequenceNumber();
    rb.physics_system_ptr = physics_system_;
    rb.needs_recreate = false;
  }
}

void PhysicsWorld::DestroyBody(entt::entity entity) {
  auto it = bodies_.find(entity);
  if (it == bodies_.end()) {
    return;
  }

  BodyInterface& body_interface = physics_system_->GetBodyInterface();
  BodyID body_id(it->second.body_id_raw);
  contact_listener_->SetOneWay(it->second.body_id_raw, false);
  body_interface.RemoveBody(body_id);
  body_interface.DestroyBody(body_id);
  bodies_.erase(it);

  // Clean up contact tracking
  std::erase_if(prev_solid_contacts_, [entity](const ContactPair& p) {
    return p.a == entity || p.b == entity;
  });
  std::erase_if(prev_trigger_contacts_, [entity](const ContactPair& p) {
    return p.a == entity || p.b == entity;
  });

  // Clear RigidBodyComponent handles
  auto& registry = scene_->GetRegistry();
  if (registry.valid(entity) && registry.any_of<RigidBodyComponent>(entity)) {
    auto& rb = registry.get<RigidBodyComponent>(entity);
    rb.body_id_raw = ~0u;
    rb.physics_system_ptr = nullptr;
  }
}

void PhysicsWorld::RecreateBodyIfNeeded(entt::entity entity) {
  auto& registry = scene_->GetRegistry();
  if (!registry.any_of<RigidBodyComponent>(entity)) {
    return;
  }
  auto& rb = registry.get<RigidBodyComponent>(entity);
  if (!rb.needs_recreate) {
    return;
  }
  DestroyBody(entity);
  CreateBody(entity);
}

void PhysicsWorld::EnsureBodiesExist() {
  PROFILE_ZONE_SCOPED_N("Physics::EnsureBodiesExist");
  auto& registry = scene_->GetRegistry();

  // Check for bodies that need recreation
  for (auto& [entity, data] : bodies_) {
    if (registry.valid(entity)) {
      RecreateBodyIfNeeded(entity);
    }
  }

  // Create bodies for new entities with colliders
  for (auto handle :
       registry.view<TransformComponent, BoxColliderComponent>()) {
    if (!bodies_.count(handle)) {
      CreateBody(handle);
    }
  }
  for (auto handle :
       registry.view<TransformComponent, SphereColliderComponent>()) {
    if (!bodies_.count(handle)) {
      CreateBody(handle);
    }
  }
  for (auto handle :
       registry.view<TransformComponent, CapsuleColliderComponent>()) {
    if (!bodies_.count(handle)) {
      CreateBody(handle);
    }
  }
  for (auto handle :
       registry.view<TransformComponent, MeshColliderComponent>()) {
    if (!bodies_.count(handle)) {
      CreateBody(handle);
    }
  }
  for (auto handle :
       registry.view<TransformComponent, HeightfieldColliderComponent>()) {
    if (!bodies_.count(handle)) {
      CreateBody(handle);
    }
  }
}

void PhysicsWorld::StepSimulation(float delta_time) {
  PROFILE_ZONE_SCOPED_N("Physics::StepSimulation")
  physics_system_->Update(delta_time, 1, temp_allocator_, job_system_);
}

void PhysicsWorld::SyncTransformsFromECS() {
  PROFILE_ZONE_SCOPED_N("Physics::SyncFromECS");
  auto& registry = scene_->GetRegistry();
  BodyInterface& body_interface = physics_system_->GetBodyInterface();

  for (auto& [entity, data] : bodies_) {
    if (!registry.valid(entity)) {
      continue;
    }

    BodyID body_id(data.body_id_raw);

    // For dynamic bodies, only sync if the transform was changed by game code
    if (!data.is_sensor && registry.any_of<RigidBodyComponent>(entity)) {
      auto& rb = registry.get<RigidBodyComponent>(entity);
      if (rb.type == RigidBodyType::Dynamic) {
        auto& tc = registry.get<TransformComponent>(entity);
        if (!tc.IsChanged()) {
          continue;
        }
      }
    }

    auto& tc = registry.get<TransformComponent>(entity);
    glm::quat q = glm::quat(glm::radians(tc.GetRotation()));

    EActivation activation =
        data.is_sensor ? EActivation::DontActivate : EActivation::Activate;
    body_interface.SetPositionAndRotation(
        body_id,
        RVec3(tc.GetPosition().x, tc.GetPosition().y, tc.GetPosition().z),
        ToJoltQuat(q), activation);
  }
}

void PhysicsWorld::SyncTransformsToECS() {
  PROFILE_ZONE_SCOPED_N("Physics::SyncToECS");
  auto& registry = scene_->GetRegistry();
  BodyInterface& body_interface = physics_system_->GetBodyInterface();

  for (auto& [entity, data] : bodies_) {
    if (data.is_sensor) {
      continue;
    }
    if (!registry.valid(entity)) {
      continue;
    }
    if (!registry.any_of<RigidBodyComponent>(entity)) {
      continue;
    }

    auto& rb = registry.get<RigidBodyComponent>(entity);
    if (rb.type != RigidBodyType::Dynamic) {
      continue;
    }

    BodyID body_id(data.body_id_raw);
    RVec3 pos;
    Quat rot;
    body_interface.GetPositionAndRotation(body_id, pos, rot);

    auto& tc = registry.get<TransformComponent>(entity);
    tc.SetPosition(glm::vec3(static_cast<float>(pos.GetX()),
                             static_cast<float>(pos.GetY()),
                             static_cast<float>(pos.GetZ())));

    bool skip_rotation =
        rb.lock_rotation_x && rb.lock_rotation_y && rb.lock_rotation_z;
    if (!skip_rotation) {
      glm::quat glm_q = ToGlmQuat(rot);
      tc.SetRotation(glm::degrees(glm::eulerAngles(glm_q)));
    }
    tc.MarkChanged();
  }
}

void PhysicsWorld::DetectContacts() {
  PROFILE_ZONE_SCOPED_N("Physics::DetectContacts");
  auto& registry = scene_->GetRegistry();

  // Helper: resolve body ID to entity
  BodyInterface& body_interface = physics_system_->GetBodyInterface();
  auto body_id_to_entity = [&](uint32_t id_raw) -> entt::entity {
    BodyID bid(id_raw);
    if (bid.IsInvalid()) {
      return entt::null;
    }
    uint64 user_data = body_interface.GetUserData(bid);
    return static_cast<entt::entity>(static_cast<uint32_t>(user_data));
  };

  // Helper to invoke callbacks on all MonoBehaviors of an entity
  auto invoke = [&](entt::entity entity, entt::entity other,
                    void (ScriptInstance::*method)(entt::entity)) {
    if (!registry.valid(entity)) {
      return;
    }
    if (!registry.any_of<BehaviorsComponent>(entity)) {
      return;
    }
    auto& behaviors = registry.get<BehaviorsComponent>(entity);
    for (auto& [name, behavior] : behaviors.behaviors_) {
      auto* mono = dynamic_cast<MonoBehavior*>(behavior);
      if (!mono || !mono->script_instance()) {
        continue;
      }
      (mono->script_instance()->*method)(other);
    }
  };

  // Process buffered contact events
  auto events = contact_listener_->SwapEvents();

  std::set<ContactPair> current_solid = prev_solid_contacts_;
  std::set<ContactPair> current_trigger = prev_trigger_contacts_;

  for (auto& event : events) {
    entt::entity ea = body_id_to_entity(event.body_id_a);
    entt::entity eb = body_id_to_entity(event.body_id_b);
    if (ea == entt::null || eb == entt::null) {
      continue;
    }

    ContactPair pair(ea, eb);
    bool is_sensor = event.is_sensor;

    // For removed events, check if the pair was in sensor or solid tracking
    if (event.type == ContactEventType::Removed) {
      if (current_trigger.contains(pair)) {
        is_sensor = true;
      }
    }

    if (event.type == ContactEventType::Added) {
      if (is_sensor) {
        current_trigger.insert(pair);
      } else {
        current_solid.insert(pair);
      }
    } else if (event.type == ContactEventType::Removed) {
      if (is_sensor) {
        current_trigger.erase(pair);
      } else {
        current_solid.erase(pair);
      }
    }
  }

  // Dispatch solid callbacks
  for (auto& pair : current_solid) {
    if (!prev_solid_contacts_.contains(pair)) {
      invoke(pair.a, pair.b, &ScriptInstance::OnCollisionEnter);
      invoke(pair.b, pair.a, &ScriptInstance::OnCollisionEnter);
    }
  }
  for (auto& pair : prev_solid_contacts_) {
    if (!current_solid.contains(pair)) {
      invoke(pair.a, pair.b, &ScriptInstance::OnCollisionExit);
      invoke(pair.b, pair.a, &ScriptInstance::OnCollisionExit);
    }
  }

  // Dispatch trigger callbacks
  for (auto& pair : current_trigger) {
    if (!prev_trigger_contacts_.contains(pair)) {
      invoke(pair.a, pair.b, &ScriptInstance::OnTriggerEnter);
      invoke(pair.b, pair.a, &ScriptInstance::OnTriggerEnter);
    }
  }
  for (auto& pair : prev_trigger_contacts_) {
    if (!current_trigger.contains(pair)) {
      invoke(pair.a, pair.b, &ScriptInstance::OnTriggerExit);
      invoke(pair.b, pair.a, &ScriptInstance::OnTriggerExit);
    }
  }

  prev_solid_contacts_ = std::move(current_solid);
  prev_trigger_contacts_ = std::move(current_trigger);
}

bool PhysicsWorld::Raycast(const glm::vec3& from, const glm::vec3& to,
                           RaycastHit& hit, entt::entity ignore) const {
  glm::vec3 direction = to - from;
  float length = glm::length(direction);
  if (length < 0.0001f) {
    return false;
  }

  RRayCast ray(RVec3(from.x, from.y, from.z), ToJolt(direction));

  // Custom body filter to ignore a specific entity
  class IgnoreBodyFilter : public BodyFilter {
   public:
    uint32_t ignore_body_id_raw = ~0u;

    bool ShouldCollide(const BodyID& inBodyID) const override {
      if (ignore_body_id_raw == ~0u) {
        return true;
      }
      return inBodyID.GetIndexAndSequenceNumber() != ignore_body_id_raw;
    }

    bool ShouldCollideLocked(const Body& inBody) const override {
      if (ignore_body_id_raw == ~0u) {
        return true;
      }
      return inBody.GetID().GetIndexAndSequenceNumber() != ignore_body_id_raw;
    }
  };

  IgnoreBodyFilter body_filter;
  if (ignore != entt::null) {
    auto it = bodies_.find(ignore);
    if (it != bodies_.end()) {
      body_filter.ignore_body_id_raw = it->second.body_id_raw;
    }
  }

  RayCastResult result;
  bool has_hit = physics_system_->GetNarrowPhaseQuery().CastRay(
      ray, result, {}, {}, body_filter);

  if (!has_hit) {
    return false;
  }

  BodyInterface& body_interface = physics_system_->GetBodyInterfaceNoLock();
  BodyID hit_body_id = result.mBodyID;

  uint64 user_data = body_interface.GetUserData(hit_body_id);
  hit.entity = static_cast<entt::entity>(static_cast<uint32_t>(user_data));

  // Compute hit point
  RVec3 hit_point = ray.GetPointOnRay(result.mFraction);
  hit.point = glm::vec3(static_cast<float>(hit_point.GetX()),
                        static_cast<float>(hit_point.GetY()),
                        static_cast<float>(hit_point.GetZ()));

  // Get hit normal
  BodyLockRead lock(physics_system_->GetBodyLockInterface(), hit_body_id);
  if (lock.Succeeded()) {
    Vec3 normal = lock.GetBody().GetWorldSpaceSurfaceNormal(result.mSubShapeID2,
                                                            hit_point);
    hit.normal = ToGlm(normal);
  }

  hit.distance = result.mFraction * length;
  return true;
}

bool PhysicsWorld::Raycast(const glm::vec3& from, const glm::vec3& to,
                           RaycastHit& hit, entt::entity ignore,
                           uint16_t collision_mask) const {
  // For now, delegate to the base raycast. Collision mask filtering
  // would require a custom ObjectLayerFilter. Since the current layer system
  // maps groups to layers, we can filter by checking the entity's group
  // after the hit.
  // TODO: implement proper ObjectLayerFilter for collision_mask
  return Raycast(from, to, hit, ignore);
}

std::vector<entt::entity> PhysicsWorld::OverlapBox(
    const glm::vec3& center, const glm::vec3& half_extents) const {
  BoxShapeSettings settings(ToJolt(half_extents));
  auto shape_result = settings.Create();
  if (shape_result.HasError()) {
    return {};
  }

  class EntityCollector : public CollideShapeCollector {
   public:
    const PhysicsSystem* system;
    std::vector<entt::entity> results;

    void AddHit(const CollideShapeResult& inResult) override {
      BodyID body_id = inResult.mBodyID2;
      uint64 user_data = system->GetBodyInterfaceNoLock().GetUserData(body_id);
      results.push_back(
          static_cast<entt::entity>(static_cast<uint32_t>(user_data)));
    }
  };

  EntityCollector collector;
  collector.system = physics_system_;

  RMat44 transform = RMat44::sTranslation(RVec3(center.x, center.y, center.z));
  CollideShapeSettings collide_settings;

  physics_system_->GetNarrowPhaseQuery().CollideShape(
      shape_result.Get().GetPtr(), Vec3::sReplicate(1.0f), transform,
      collide_settings, RVec3(center.x, center.y, center.z), collector);

  return collector.results;
}

std::vector<entt::entity> PhysicsWorld::OverlapSphere(const glm::vec3& center,
                                                      float radius) const {
  SphereShapeSettings settings(radius);
  auto shape_result = settings.Create();
  if (shape_result.HasError()) {
    return {};
  }

  class EntityCollector : public CollideShapeCollector {
   public:
    const PhysicsSystem* system;
    std::vector<entt::entity> results;

    void AddHit(const CollideShapeResult& inResult) override {
      BodyID body_id = inResult.mBodyID2;
      uint64 user_data = system->GetBodyInterfaceNoLock().GetUserData(body_id);
      results.push_back(
          static_cast<entt::entity>(static_cast<uint32_t>(user_data)));
    }
  };

  EntityCollector collector;
  collector.system = physics_system_;

  RMat44 transform = RMat44::sTranslation(RVec3(center.x, center.y, center.z));
  CollideShapeSettings collide_settings;

  physics_system_->GetNarrowPhaseQuery().CollideShape(
      shape_result.Get().GetPtr(), Vec3::sReplicate(1.0f), transform,
      collide_settings, RVec3(center.x, center.y, center.z), collector);

  return collector.results;
}

void PhysicsWorld::SetGravity(const glm::vec3& gravity) {
  physics_system_->SetGravity(ToJolt(gravity));
}

glm::vec3 PhysicsWorld::GetGravity() const {
  return ToGlm(physics_system_->GetGravity());
}

}  // namespace wiesel
