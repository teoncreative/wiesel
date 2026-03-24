
//
//   Copyright 2025 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/w_primitives.hpp"

#include <glm/gtc/constants.hpp>

namespace Wiesel {
namespace Primitives {

static Vertex3D MakeVertex(glm::vec3 pos, glm::vec3 normal, glm::vec2 uv) {
  // Compute tangent from normal
  glm::vec3 tangent;
  if (std::abs(normal.y) < 0.999f) {
    tangent = glm::normalize(glm::cross(glm::vec3(0, 1, 0), normal));
  } else {
    tangent = glm::normalize(glm::cross(glm::vec3(0, 0, 1), normal));
  }
  glm::vec3 bitangent = glm::cross(normal, tangent);

  Vertex3D v{};
  v.Pos = pos;
  v.Color = {1.0f, 1.0f, 1.0f};
  v.UV = uv;
  v.Normal = normal;
  v.Tangent = tangent;
  v.BiTangent = bitangent;
  v.Flags = 0;
  return v;
}

static std::shared_ptr<Model> WrapMesh(std::vector<Vertex3D>& vertices,
                                       std::vector<Index>& indices,
                                       const std::string& name) {
  auto mesh = std::make_shared<Mesh>(vertices, indices);
  mesh->model_path = "primitive://" + name;
  mesh->Allocate();

  auto model = std::make_shared<Model>();
  model->meshes.push_back(mesh);
  model->model_path = "primitive://" + name;
  model->mesh_node_transforms.push_back(glm::mat4(1.0f));
  return model;
}

std::shared_ptr<Model> CreateCube() {
  std::vector<Vertex3D> vertices;
  std::vector<Index> indices;

  // 6 faces, 4 vertices each, separate normals per face
  struct Face {
    glm::vec3 normal;
    glm::vec3 up;
    glm::vec3 right;
  };

  Face faces[] = {
      {{0, 0, 1}, {0, 1, 0}, {1, 0, 0}},    // front
      {{0, 0, -1}, {0, 1, 0}, {-1, 0, 0}},  // back
      {{1, 0, 0}, {0, 1, 0}, {0, 0, -1}},   // right
      {{-1, 0, 0}, {0, 1, 0}, {0, 0, 1}},   // left
      {{0, 1, 0}, {0, 0, -1}, {1, 0, 0}},   // top
      {{0, -1, 0}, {0, 0, 1}, {1, 0, 0}},   // bottom
  };

  for (const auto& face : faces) {
    uint32_t base = static_cast<uint32_t>(vertices.size());
    glm::vec3 center = face.normal * 0.5f;

    vertices.push_back(MakeVertex(center - face.right * 0.5f - face.up * 0.5f,
                                  face.normal, {0, 1}));
    vertices.push_back(MakeVertex(center + face.right * 0.5f - face.up * 0.5f,
                                  face.normal, {1, 1}));
    vertices.push_back(MakeVertex(center + face.right * 0.5f + face.up * 0.5f,
                                  face.normal, {1, 0}));
    vertices.push_back(MakeVertex(center - face.right * 0.5f + face.up * 0.5f,
                                  face.normal, {0, 0}));

    indices.push_back(base + 0);
    indices.push_back(base + 1);
    indices.push_back(base + 2);
    indices.push_back(base + 0);
    indices.push_back(base + 2);
    indices.push_back(base + 3);
  }

  return WrapMesh(vertices, indices, "cube");
}

std::shared_ptr<Model> CreateSphere(int stacks, int slices) {
  std::vector<Vertex3D> vertices;
  std::vector<Index> indices;

  for (int i = 0; i <= stacks; i++) {
    float phi = glm::pi<float>() * static_cast<float>(i) / stacks;
    float v = static_cast<float>(i) / stacks;

    for (int j = 0; j <= slices; j++) {
      float theta = glm::two_pi<float>() * static_cast<float>(j) / slices;
      float u = static_cast<float>(j) / slices;

      glm::vec3 normal;
      normal.x = sinf(phi) * cosf(theta);
      normal.y = cosf(phi);
      normal.z = sinf(phi) * sinf(theta);

      glm::vec3 pos = normal * 0.5f;
      vertices.push_back(MakeVertex(pos, normal, {u, v}));
    }
  }

  for (int i = 0; i < stacks; i++) {
    for (int j = 0; j < slices; j++) {
      uint32_t a = i * (slices + 1) + j;
      uint32_t b = a + slices + 1;

      indices.push_back(a);
      indices.push_back(b);
      indices.push_back(a + 1);

      indices.push_back(a + 1);
      indices.push_back(b);
      indices.push_back(b + 1);
    }
  }

  return WrapMesh(vertices, indices, "sphere");
}

std::shared_ptr<Model> CreatePlane() {
  std::vector<Vertex3D> vertices;
  std::vector<Index> indices;

  glm::vec3 normal = {0, 1, 0};

  vertices.push_back(MakeVertex({-0.5f, 0, -0.5f}, normal, {0, 0}));
  vertices.push_back(MakeVertex({0.5f, 0, -0.5f}, normal, {1, 0}));
  vertices.push_back(MakeVertex({0.5f, 0, 0.5f}, normal, {1, 1}));
  vertices.push_back(MakeVertex({-0.5f, 0, 0.5f}, normal, {0, 1}));

  indices = {0, 2, 1, 0, 3, 2};

  return WrapMesh(vertices, indices, "plane");
}

std::shared_ptr<Model> CreateCylinder(int segments) {
  std::vector<Vertex3D> vertices;
  std::vector<Index> indices;

  float height = 1.0f;
  float radius = 0.5f;

  // Side
  for (int i = 0; i <= segments; i++) {
    float theta = glm::two_pi<float>() * static_cast<float>(i) / segments;
    float u = static_cast<float>(i) / segments;
    float cx = cosf(theta);
    float cz = sinf(theta);
    glm::vec3 normal = {cx, 0, cz};

    vertices.push_back(
        MakeVertex({cx * radius, -height * 0.5f, cz * radius}, normal, {u, 1}));
    vertices.push_back(
        MakeVertex({cx * radius, height * 0.5f, cz * radius}, normal, {u, 0}));
  }

  for (int i = 0; i < segments; i++) {
    uint32_t a = i * 2;
    uint32_t b = a + 1;
    uint32_t c = a + 2;
    uint32_t d = a + 3;

    indices.push_back(a);
    indices.push_back(c);
    indices.push_back(b);

    indices.push_back(b);
    indices.push_back(c);
    indices.push_back(d);
  }

  // Top cap
  uint32_t top_center = static_cast<uint32_t>(vertices.size());
  vertices.push_back(
      MakeVertex({0, height * 0.5f, 0}, {0, 1, 0}, {0.5f, 0.5f}));
  for (int i = 0; i <= segments; i++) {
    float theta = glm::two_pi<float>() * static_cast<float>(i) / segments;
    float cx = cosf(theta);
    float cz = sinf(theta);
    vertices.push_back(MakeVertex({cx * radius, height * 0.5f, cz * radius},
                                  {0, 1, 0},
                                  {cx * 0.5f + 0.5f, cz * 0.5f + 0.5f}));
  }
  for (int i = 0; i < segments; i++) {
    indices.push_back(top_center);
    indices.push_back(top_center + 1 + i);
    indices.push_back(top_center + 2 + i);
  }

  // Bottom cap
  uint32_t bot_center = static_cast<uint32_t>(vertices.size());
  vertices.push_back(
      MakeVertex({0, -height * 0.5f, 0}, {0, -1, 0}, {0.5f, 0.5f}));
  for (int i = 0; i <= segments; i++) {
    float theta = glm::two_pi<float>() * static_cast<float>(i) / segments;
    float cx = cosf(theta);
    float cz = sinf(theta);
    vertices.push_back(MakeVertex({cx * radius, -height * 0.5f, cz * radius},
                                  {0, -1, 0},
                                  {cx * 0.5f + 0.5f, cz * 0.5f + 0.5f}));
  }
  for (int i = 0; i < segments; i++) {
    indices.push_back(bot_center);
    indices.push_back(bot_center + 2 + i);
    indices.push_back(bot_center + 1 + i);
  }

  return WrapMesh(vertices, indices, "cylinder");
}

std::shared_ptr<Model> CreateCapsule(int stacks, int slices) {
  std::vector<Vertex3D> vertices;
  std::vector<Index> indices;

  float radius = 0.5f;
  float half_height = 0.5f;  // half of the cylinder section

  // Top hemisphere
  int half_stacks = stacks / 2;
  for (int i = 0; i <= half_stacks; i++) {
    float phi = glm::half_pi<float>() * static_cast<float>(i) / half_stacks;
    float v = static_cast<float>(i) / stacks;

    for (int j = 0; j <= slices; j++) {
      float theta = glm::two_pi<float>() * static_cast<float>(j) / slices;
      float u = static_cast<float>(j) / slices;

      glm::vec3 normal;
      normal.x = sinf(phi) * cosf(theta);
      normal.y = cosf(phi);
      normal.z = sinf(phi) * sinf(theta);

      glm::vec3 pos = normal * radius + glm::vec3(0, half_height, 0);
      vertices.push_back(MakeVertex(pos, normal, {u, v}));
    }
  }

  // Bottom hemisphere
  for (int i = half_stacks; i <= stacks; i++) {
    float phi = glm::half_pi<float>() +
                glm::half_pi<float>() * static_cast<float>(i - half_stacks) /
                    half_stacks;
    float v = static_cast<float>(i) / stacks;

    for (int j = 0; j <= slices; j++) {
      float theta = glm::two_pi<float>() * static_cast<float>(j) / slices;
      float u = static_cast<float>(j) / slices;

      glm::vec3 normal;
      normal.x = sinf(phi) * cosf(theta);
      normal.y = cosf(phi);
      normal.z = sinf(phi) * sinf(theta);

      glm::vec3 pos = normal * radius - glm::vec3(0, half_height, 0);
      vertices.push_back(MakeVertex(pos, normal, {u, v}));
    }
  }

  for (int i = 0; i <= stacks; i++) {
    for (int j = 0; j < slices; j++) {
      uint32_t a = i * (slices + 1) + j;
      uint32_t b = a + slices + 1;

      indices.push_back(a);
      indices.push_back(b);
      indices.push_back(a + 1);

      indices.push_back(a + 1);
      indices.push_back(b);
      indices.push_back(b + 1);
    }
  }

  return WrapMesh(vertices, indices, "capsule");
}

}  // namespace Primitives
}  // namespace Wiesel