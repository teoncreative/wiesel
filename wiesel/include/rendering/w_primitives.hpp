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

#include "rendering/w_mesh.hpp"

namespace Wiesel {

// Generates procedural Model objects for common shapes.
// Each returns a Model with a single mesh, default material, and
// proper normals/UVs/tangents for PBR rendering.
namespace Primitives {

std::shared_ptr<Model> CreateCube();
std::shared_ptr<Model> CreateSphere(int stacks = 24, int slices = 32);
std::shared_ptr<Model> CreatePlane();
std::shared_ptr<Model> CreateCylinder(int segments = 32);
std::shared_ptr<Model> CreateCapsule(int stacks = 16, int slices = 32);

}  // namespace Primitives
}  // namespace Wiesel