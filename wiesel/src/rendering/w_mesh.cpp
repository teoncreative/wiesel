
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/w_mesh.hpp"

#include "rendering/w_renderer.hpp"
#include "w_engine.hpp"

namespace Wiesel {

Mesh::Mesh() {
  mat = CreateReference<Material>();
  allocated_ = false;
}

Mesh::Mesh(const std::vector<Vertex3D>& vertices, const std::vector<Index>& indices) : vertices(vertices), indices(indices) {
  mat = CreateReference<Material>();
  allocated_ = false;
}

Mesh::~Mesh() {
  Deallocate();
}

void Mesh::Allocate() {
  if (allocated_) {
    Deallocate();
  }

  vertex_buffer = Engine::renderer()->CreateVertexBuffer(vertices);
  index_buffer = Engine::renderer()->CreateIndexBuffer(indices);
  allocated_ = true;
}

void Mesh::Deallocate() {
  if (!allocated_) {
    return;
  }
  vertex_buffer = nullptr;
  index_buffer = nullptr;
  allocated_ = false;
}

}  // namespace Wiesel
