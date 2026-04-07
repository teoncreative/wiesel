//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "systems/w_physics_system.h"

#include "physics/w_physics_world.h"
#include "scene/w_scene.h"

namespace Wiesel {

void PhysicsBodySystem::Update(Scene& scene, float delta_time) {
  PROFILE_ZONE_SCOPED_N("PhysicsBodySystem::Update");
  scene.GetPhysicsWorld().EnsureBodiesExist();
}

void PhysicsSimulationSystem::Update(Scene& scene, float delta_time) {
  PROFILE_ZONE_SCOPED_N("PhysicsSimulationSystem::Update");
  PhysicsWorld& physics = scene.GetPhysicsWorld();
  physics.SyncTransformsFromECS();
  physics.StepSimulation(delta_time);
  physics.SyncTransformsToECS();
  physics.DetectContacts();
}

}  // namespace Wiesel
