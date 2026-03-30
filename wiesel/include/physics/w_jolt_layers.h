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

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

namespace Wiesel {

namespace ObjectLayers {
static constexpr JPH::ObjectLayer kDefault = 0;
static constexpr JPH::ObjectLayer kTerrain = 1;
static constexpr JPH::ObjectLayer kBuilding = 2;
static constexpr JPH::ObjectLayer kCharacter = 3;
static constexpr JPH::ObjectLayer kSensor = 4;
static constexpr JPH::ObjectLayer kNumLayers = 5;
}  // namespace ObjectLayers

namespace BroadPhaseLayers {
static constexpr JPH::BroadPhaseLayer kNonMoving(0);
static constexpr JPH::BroadPhaseLayer kMoving(1);
static constexpr uint32_t kNumLayers = 2;
}  // namespace BroadPhaseLayers

// Convert collision group bitmask to Jolt object layer
inline JPH::ObjectLayer ObjectLayerFromCollisionGroup(uint16_t group) {
  // Map bitmask to layer index via lowest set bit
  if (group & (1 << 1)) {
    return ObjectLayers::kTerrain;
  }
  if (group & (1 << 2)) {
    return ObjectLayers::kBuilding;
  }
  if (group & (1 << 3)) {
    return ObjectLayers::kCharacter;
  }
  return ObjectLayers::kDefault;
}

class BroadPhaseLayerInterfaceImpl final
    : public JPH::BroadPhaseLayerInterface {
 public:
  BroadPhaseLayerInterfaceImpl() {
    object_to_broad_phase_[ObjectLayers::kDefault] = BroadPhaseLayers::kMoving;
    object_to_broad_phase_[ObjectLayers::kTerrain] =
        BroadPhaseLayers::kNonMoving;
    object_to_broad_phase_[ObjectLayers::kBuilding] =
        BroadPhaseLayers::kNonMoving;
    object_to_broad_phase_[ObjectLayers::kCharacter] =
        BroadPhaseLayers::kMoving;
    object_to_broad_phase_[ObjectLayers::kSensor] = BroadPhaseLayers::kMoving;
  }

  JPH::uint GetNumBroadPhaseLayers() const override {
    return BroadPhaseLayers::kNumLayers;
  }

  JPH::BroadPhaseLayer GetBroadPhaseLayer(
      JPH::ObjectLayer inLayer) const override {
    JPH_ASSERT(inLayer < ObjectLayers::kNumLayers);
    return object_to_broad_phase_[inLayer];
  }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
  const char* GetBroadPhaseLayerName(
      JPH::BroadPhaseLayer inLayer) const override {
    switch (static_cast<JPH::BroadPhaseLayer::Type>(inLayer)) {
      case static_cast<JPH::BroadPhaseLayer::Type>(
          BroadPhaseLayers::kNonMoving):
        return "NON_MOVING";
      case static_cast<JPH::BroadPhaseLayer::Type>(BroadPhaseLayers::kMoving):
        return "MOVING";
      default:
        JPH_ASSERT(false);
        return "INVALID";
    }
  }
#endif

 private:
  JPH::BroadPhaseLayer object_to_broad_phase_[ObjectLayers::kNumLayers];
};

class ObjectVsBroadPhaseLayerFilterImpl
    : public JPH::ObjectVsBroadPhaseLayerFilter {
 public:
  bool ShouldCollide(JPH::ObjectLayer inLayer1,
                     JPH::BroadPhaseLayer inLayer2) const override {
    switch (inLayer1) {
      case ObjectLayers::kTerrain:
      case ObjectLayers::kBuilding:
        return inLayer2 == BroadPhaseLayers::kMoving;
      default:
        return true;
    }
  }
};

class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter {
 public:
  bool ShouldCollide(JPH::ObjectLayer inObject1,
                     JPH::ObjectLayer inObject2) const override {
    // Static vs static never collide
    if ((inObject1 == ObjectLayers::kTerrain ||
         inObject1 == ObjectLayers::kBuilding) &&
        (inObject2 == ObjectLayers::kTerrain ||
         inObject2 == ObjectLayers::kBuilding)) {
      return false;
    }
    return true;
  }
};

}  // namespace Wiesel
