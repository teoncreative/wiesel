
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/w_mesh.h"

#include "rendering/w_renderer.h"
#include "w_engine.h"

namespace wiesel {

Mesh::Mesh() {
  mat = std::make_shared<Material>();
  allocated_ = false;
}

Mesh::Mesh(const std::vector<Vertex3D>& vertices,
           const std::vector<Index>& indices)
    : vertices(vertices), indices(indices) {
  mat = std::make_shared<Material>();
  allocated_ = false;
}

Mesh::~Mesh() {
  Deallocate();
}

void Mesh::Allocate() {
  if (allocated_) {
    Deallocate();
  }

  vertex_buffer =
      Engine::renderer()->CreateVertexBuffer("Mesh::vertex_buffer", vertices);
  index_buffer =
      Engine::renderer()->CreateIndexBuffer("Mesh::index_buffer", indices);
  allocated_ = true;
}

void Mesh::ComputeBounds() {
  bounds = {};
  for (auto& v : vertices) {
    bounds.Expand(v.ppos);
  }
}

void Mesh::Deallocate() {
  if (!allocated_) {
    return;
  }
  vertex_buffer = nullptr;
  index_buffer = nullptr;
  allocated_ = false;
}

}  // namespace wiesel
