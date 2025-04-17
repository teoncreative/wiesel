
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "w_engine.hpp"

#include "input/w_input.hpp"
#include "scene/w_componentutil.hpp"
#include "scene/w_lights.hpp"
#include "util/w_dialogs.hpp"
#include "window/w_glfwwindow.hpp"

namespace Wiesel {
Ref<Renderer> Engine::s_Renderer;
Ref<AppWindow> Engine::s_Window;

void Engine::InitEngine() {
  InitializeComponents();
  InputManager::Init();
  ScriptManager::Init();
}

void Engine::InitWindow(WindowProperties props) {
  s_Window = CreateReference<GlfwAppWindow>(props);
  Dialogs::Init();
}

void Engine::InitRenderer() {
  if (s_Window == nullptr) {
    LOG_ERROR("Window should be initialized before renderer!");
    abort();
  }
  s_Renderer = CreateReference<Renderer>(s_Window);
  s_Renderer->Initialize({});
}

void Engine::CleanupRenderer() {
  s_Renderer->Cleanup();
  s_Renderer = nullptr;
}

void Engine::CleanupWindow() {
  s_Window = nullptr;
  Dialogs::Destroy();
}

void Engine::CleanupEngine() {
  ScriptManager::Destroy();
  //InputManager::Destroy();
  //CleanupComponents();
}

Ref<Renderer> Engine::GetRenderer() {
  if (s_Renderer == nullptr) {
    throw std::runtime_error("Renderer is not initialized!");
  }
  return s_Renderer;
}

Ref<AppWindow> Engine::GetWindow() {
  return s_Window;
}

aiScene* Engine::LoadAssimpModel(ModelComponent& modelComponent,
                                 const std::string& path) {
  auto& model = modelComponent.Data;
  model.ModelPath = path;
  auto fsPath = std::filesystem::relative(path);
  model.TexturesPath = fsPath.parent_path().string();
  LOG_INFO("Loading model: {}", path);

  Assimp::Importer importer;
  importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE,
                              aiPrimitiveType_LINE | aiPrimitiveType_POINT);
  importer.SetPropertyBool(AI_CONFIG_PP_PTV_NORMALIZE, true);
  importer.ReadFile(model.ModelPath, aiProcess_Triangulate |
                                         aiProcess_CalcTangentSpace |
                                         aiProcess_JoinIdenticalVertices |
                                         aiProcess_ConvertToLeftHanded |
                                         aiProcessPreset_TargetRealtime_Fast);
  aiScene* scene = importer.GetOrphanedScene();

  if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) ||
      !scene->mRootNode) {
    LOG_ERROR("Failed to load model {}: {}", path, importer.GetErrorString());
    return nullptr;
  }

  return scene;
}

void Engine::LoadModel(TransformComponent& transform,
                       ModelComponent& modelComponent,
                       const std::string& path) {
  aiScene* scene = LoadAssimpModel(modelComponent, path);
  LoadModel(scene, transform, modelComponent, path);
}

void Engine::LoadModel(aiScene* scene, Wiesel::TransformComponent& transform,
                       Wiesel::ModelComponent& modelComponent,
                       const std::string& path) {
  modelComponent.Data.Meshes.clear();
  modelComponent.Data.Textures.clear();
  ProcessNode(modelComponent.Data, scene->mRootNode, *scene,
              modelComponent.Data.Meshes);
  uint64_t vertices = 0;
  for (const auto& item : modelComponent.Data.Meshes) {
    item->Allocate();
    vertices += item->Vertices.size();
  }
  LOG_INFO("Loaded {} meshes!", modelComponent.Data.Meshes.size());
  LOG_INFO("Loaded {} textures!", modelComponent.Data.Textures.size());
  LOG_INFO("Loaded {} vertices!", vertices);
}

bool Engine::LoadTexture(Model& model, Ref<Mesh> mesh, aiMaterial* mat,
                         aiTextureType type) {
  for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
    aiString str;
    mat->GetTexture(type, i, &str);
    std::string s = std::string(str.C_Str());
    if (s.empty()) {
      continue;
    }
    std::string textureFullPath = model.TexturesPath + "/" + s;
    if (model.Textures.contains(textureFullPath)) {
      Material::Set(mesh->Mat, model.Textures[textureFullPath],
                    static_cast<TextureType>(type));
    } else {
      Ref<Texture> texture = Engine::GetRenderer()->CreateTexture(
          textureFullPath, {static_cast<TextureType>(type)}, {});
      Material::Set(mesh->Mat, texture, static_cast<TextureType>(type));
      model.Textures.insert(std::pair(textureFullPath, texture));
    }
    return true;
  }
  return false;
}

Ref<Mesh> Engine::ProcessMesh(Model& model, aiMesh* aiMesh,
                                    const aiScene& aiScene,
                                    aiMatrix4x4 aiMatrix) {
  std::vector<Vertex3D> vertices;
  std::vector<Index> indices;
  glm::mat4 mat = ConvertMatrix(aiMatrix);

  aiMaterial* material = aiScene.mMaterials[aiMesh->mMaterialIndex];

  Ref<Mesh> mesh = CreateReference<Mesh>();
  // todo handle materials properly within another class
  uint32_t flags = 0;
  flags |= VertexFlagHasTexture *
           LoadTexture(model, mesh, material, aiTextureType_DIFFUSE);
  flags |= VertexFlagHasNormalMap *
           LoadTexture(model, mesh, material, aiTextureType_NORMALS);
  flags |= VertexFlagHasSpecularMap *
           LoadTexture(model, mesh, material, aiTextureType_SPECULAR);
  flags |= VertexFlagHasAlbedoMap *
           LoadTexture(model, mesh, material, aiTextureType_BASE_COLOR);
  flags |= VertexFlagHasRoughnessMap *
           LoadTexture(model, mesh, material, aiTextureType_DIFFUSE_ROUGHNESS);
  flags |= VertexFlagHasMetallicMap *
           LoadTexture(model, mesh, material, aiTextureType_METALNESS);

  for (unsigned int i = 0; i < aiMesh->mNumVertices; i++) {
    Vertex3D vertex{};
    glm::vec3 vector;
    // positions
    vector.x = aiMesh->mVertices[i].x;
    vector.y = aiMesh->mVertices[i].y;
    vector.z = aiMesh->mVertices[i].z;
    vertex.Pos = vector;

    // normals
    vector.x = aiMesh->mNormals[i].x;
    vector.y = aiMesh->mNormals[i].y;
    vector.z = aiMesh->mNormals[i].z;
    vertex.Normal = vector;

    // texture coordinates
    if (aiMesh->mTextureCoords[0]) {
      glm::vec2 vec;
      vec.x = aiMesh->mTextureCoords[0][i].x;
      vec.y = aiMesh->mTextureCoords[0][i].y;
      vertex.UV = vec;
    } else {
      vertex.UV = glm::vec2(0.0f, 0.0f);
    }
    vertex.Flags = flags;

    vertex.Color = {1.0f, 1.0f, 1.0f};

    // tangent
    vector.x = aiMesh->mTangents[i].x;
    vector.y = aiMesh->mTangents[i].y;
    vector.z = aiMesh->mTangents[i].z;
    vertex.Tangent = vector;

    // bitangent
    vector.x = aiMesh->mBitangents[i].x;
    vector.y = aiMesh->mBitangents[i].y;
    vector.z = aiMesh->mBitangents[i].z;
    vertex.BiTangent = vector;

    mesh->Vertices.push_back(vertex);
  }

  // now walk through each of the mesh's faces (a face is a mesh its triangle) and retrieve the corresponding vertex indices.
  for (unsigned int i = 0; i < aiMesh->mNumFaces; i++) {
    aiFace face = aiMesh->mFaces[i];

    // retrieve all indices of the face and store them in the indices vector
    for (int j = face.mNumIndices - 1; j >= 0; j--) {
      mesh->Indices.push_back(face.mIndices[j]);
    }
  }

  return mesh;
}

void Engine::ProcessNode(Model& model, aiNode* node, const aiScene& scene,
                         std::vector<Ref<Mesh>>& meshes) {
  for (uint32_t i = 0; i < node->mNumMeshes; i++) {
    aiMesh* aiMesh = scene.mMeshes[node->mMeshes[i]];
    Ref<Mesh> mesh =
        ProcessMesh(model, aiMesh, scene, node->mTransformation);
    if (mesh == nullptr)
      continue;
    meshes.push_back(mesh);
  }
  for (uint32_t i = 0; i < node->mNumChildren; i++) {
    ProcessNode(model, node->mChildren[i], scene, meshes);
  }
}

glm::mat4 Engine::ConvertMatrix(const aiMatrix4x4& aiMat) {
  return {aiMat.a1, aiMat.b1, aiMat.c1, aiMat.d1, aiMat.a2, aiMat.b2,
          aiMat.c2, aiMat.d2, aiMat.a3, aiMat.b3, aiMat.c3, aiMat.d3,
          aiMat.a4, aiMat.b4, aiMat.c4, aiMat.d4};
}

}  // namespace Wiesel