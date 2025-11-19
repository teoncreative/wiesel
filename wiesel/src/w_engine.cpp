
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
#include "script/w_scriptmanager.hpp"

#ifdef WIN32
#include <windows.h>

void EnableAnsiColors() {
  HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD dwMode = 0;
  GetConsoleMode(hOut, &dwMode);
  dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
  SetConsoleMode(hOut, dwMode);
}
#endif

namespace Wiesel {
Ref<Renderer> Engine::kRenderer;
Ref<AppWindow> Engine::kWindow;

void Engine::InitEngine() {
  LOG_INFO("Current work directory: {}", std::filesystem::current_path().string());
#ifdef WIN32
  EnableAnsiColors();
#endif
  InitializeComponents();
  InputManager::Init();
  ScriptManager::Init({
      .EnableDebugger = true
  });
}

void Engine::InitWindow(const WindowProperties&& props) {
  kWindow = CreateReference<GlfwAppWindow>(std::move(props));
  Dialogs::Init();
}

void Engine::InitRenderer(const RendererProperties&& props) {
  if (kWindow == nullptr) {
    LOG_ERROR("Window should be initialized before renderer!");
    abort();
  }
  kRenderer = CreateReference<Renderer>(kWindow);
  kRenderer->Initialize(std::move(props));
}

void Engine::CleanupRenderer() {
  kRenderer->Cleanup();
  kRenderer = nullptr;
}

void Engine::CleanupWindow() {
  kWindow = nullptr;
  Dialogs::Destroy();
}

void Engine::CleanupEngine() {
  ScriptManager::Destroy();
  //InputManager::Destroy();
  //CleanupComponents();
}

std::shared_ptr<Renderer> Engine::GetRenderer() {
  if (kRenderer == nullptr) {
    throw std::runtime_error("Renderer is not initialized!");
  }
  return kRenderer;
}

std::shared_ptr<AppWindow> Engine::GetWindow() {
  return kWindow;
}

aiScene* Engine::LoadAssimpModel(ModelComponent& modelComponent,
                                 const std::string& path,
                                 bool convertToLeftHanded) {
  auto& model = modelComponent.data;
  model.model_path = path;
  auto fsPath = std::filesystem::relative(path);
  model.textures_path = fsPath.parent_path().string();
  LOG_INFO("Loading model: {}", path);

  Assimp::Importer importer;
  importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE,
                              aiPrimitiveType_LINE | aiPrimitiveType_POINT);
  importer.SetPropertyBool(AI_CONFIG_PP_PTV_NORMALIZE, true);
  uint32_t flags = aiProcess_Triangulate |
                   aiProcess_CalcTangentSpace |
                   aiProcess_FlipUVs |
                   aiProcess_JoinIdenticalVertices |
                   aiProcessPreset_TargetRealtime_Fast;
  if (convertToLeftHanded) {
    flags |= aiProcess_ConvertToLeftHanded;
  }
  importer.ReadFile(model.model_path, flags);
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
                       const std::string& path,
                       bool convertToLeftHanded) {
  aiScene* scene = LoadAssimpModel(modelComponent, path, convertToLeftHanded);
  LoadModel(scene, transform, modelComponent, path);
}

void Engine::LoadModel(aiScene* scene, Wiesel::TransformComponent& transform,
                       Wiesel::ModelComponent& modelComponent,
                       const std::string& path) {
  modelComponent.data.meshes.clear();
  modelComponent.data.textures.clear();
  ProcessNode(modelComponent.data, scene->mRootNode, *scene,
              modelComponent.data.meshes, glm::mat4(1.0f));
  uint64_t vertices = 0;
  for (const auto& item : modelComponent.data.meshes) {
    item->Allocate();
    vertices += item->vertices.size();
  }
  LOG_INFO("Loaded {} meshes!", modelComponent.data.meshes.size());
  LOG_INFO("Loaded {} textures!", modelComponent.data.textures.size());
  LOG_INFO("Loaded {} vertices!", vertices);
}

bool Engine::LoadTexture(Model& model, std::shared_ptr<Mesh> mesh, aiMaterial* mat,
                         aiTextureType type, const aiScene& scene) {
  size_t count = mat->GetTextureCount(type);
  if (count > 1) {
    LOG_WARN("Mesh has more than one texture for type {}", std::to_string(type));
  }
  for (unsigned int i = 0; i < count; i++) {
    aiString str;
    mat->GetTexture(type, i, &str);
    std::string s = std::string(str.C_Str());
    if (s.empty()) {
      continue;
    }

    // Check if texture is embedded (starts with '*')
    if (s[0] == '*') {
      // Extract embedded texture index
      int texIndex = std::atoi(s.c_str() + 1);

      if (texIndex >= 0 && texIndex < scene.mNumTextures) {
        aiTexture* embeddedTex = scene.mTextures[texIndex];

        // Create unique identifier for embedded texture
        std::string textureKey = model.textures_path + "/embedded_" + std::to_string(texIndex);

        if (model.textures.contains(textureKey)) {
          Material::Set(mesh->mat, model.textures[textureKey],
                        static_cast<TextureType>(type));
        } else {
          std::shared_ptr<Texture> texture = CreateTextureFromEmbedded(
              embeddedTex, static_cast<TextureType>(type));
          if (texture == nullptr) {
            continue;
          }
          Material::Set(mesh->mat, texture, static_cast<TextureType>(type));
          model.textures.insert(std::pair(textureKey, texture));
        }
        return true;
      }
    } else {
      // Handle external texture file
      std::string textureFullPath = model.textures_path + "/" + s;
      if (model.textures.contains(textureFullPath)) {
        Material::Set(mesh->mat, model.textures[textureFullPath],
                      static_cast<TextureType>(type));
      } else {
        std::shared_ptr<Texture> texture = GetRenderer()->CreateTexture(
            textureFullPath, {static_cast<TextureType>(type)},{});
        if (texture == nullptr) {
          continue;
        }
        Material::Set(mesh->mat, texture, static_cast<TextureType>(type));
        model.textures.insert(std::pair(textureFullPath, texture));
      }
      return true;
    }
  }
  return false;
}

std::shared_ptr<Texture> Engine::CreateTextureFromEmbedded(aiTexture* aiTex,
                                                            TextureType type) {
  int width, height;
  unsigned char* pixelData = nullptr;

  if (aiTex->mHeight == 0) {
    // Compressed format - decode using stb_image
    int channels;
    pixelData = stbi_load_from_memory(
        reinterpret_cast<unsigned char*>(aiTex->pcData),
        aiTex->mWidth,
        &width, &height, &channels,
        STBI_rgb_alpha);

    if (!pixelData) {
      LOG_ERROR("Failed to decode embedded texture: {}", stbi_failure_reason());
      return nullptr;
    }
  } else {
    // Uncompressed format - convert BGRA to RGBA
    width = aiTex->mWidth;
    height = aiTex->mHeight;
    pixelData = ConvertBGRAtoRGBA(aiTex->pcData, width, height);

    if (!pixelData) {
      LOG_ERROR("Failed to convert embedded texture format");
      return nullptr;
    }
  }

  TextureProps props;
  props.width = width;
  props.height = height;
  props.type = type;
  props.image_format = VK_FORMAT_R8G8B8A8_SRGB;
  props.generate_mipmaps = true;

  auto texture = GetRenderer()->CreateTexture(
      pixelData, 4, props, {});

  stbi_image_free(pixelData);  // Works for both cases

  return texture;
}

unsigned char* Engine::ConvertBGRAtoRGBA(void* bgra_data, int width, int height) {
  aiTexel* texels = reinterpret_cast<aiTexel*>(bgra_data);
  int size = width * height;

  unsigned char* rgba_data = static_cast<unsigned char*>(
      malloc(size * 4 * sizeof(unsigned char)));

  if (!rgba_data) {
    return nullptr;
  }

  for (int i = 0; i < size; i++) {
    rgba_data[i * 4 + 0] = texels[i].r;
    rgba_data[i * 4 + 1] = texels[i].g;
    rgba_data[i * 4 + 2] = texels[i].b;
    rgba_data[i * 4 + 3] = texels[i].a;
  }

  return rgba_data;
}

std::shared_ptr<Mesh> Engine::ProcessMesh(Model& model, aiMesh* aiMesh,
                                    const aiScene& aiScene) {
  std::vector<Vertex3D> vertices;
  std::vector<Index> indices;

  aiMaterial* material = aiScene.mMaterials[aiMesh->mMaterialIndex];

  std::shared_ptr<Mesh> mesh = std::make_shared<Mesh>();
  // todo handle materials properly within another class
  uint32_t flags = 0;
  flags |= VertexFlagHasTexture *
           LoadTexture(model, mesh, material, aiTextureType_DIFFUSE, aiScene);
  flags |= VertexFlagHasNormalMap *
           LoadTexture(model, mesh, material, aiTextureType_NORMALS, aiScene);
  flags |= VertexFlagHasSpecularMap *
           LoadTexture(model, mesh, material, aiTextureType_SPECULAR, aiScene);
  flags |= VertexFlagHasAlbedoMap *
           LoadTexture(model, mesh, material, aiTextureType_BASE_COLOR, aiScene);
  flags |= VertexFlagHasRoughnessMap *
           LoadTexture(model, mesh, material, aiTextureType_DIFFUSE_ROUGHNESS, aiScene);
  flags |= VertexFlagHasMetallicMap *
           LoadTexture(model, mesh, material, aiTextureType_METALNESS, aiScene);

  bool has_unsupported_textures = false;
  for (size_t type = aiTextureType_NONE; type < AI_TEXTURE_TYPE_MAX; type++) {
    if (type == aiTextureType_DIFFUSE || type == aiTextureType_NORMALS || type == aiTextureType_SPECULAR
      || type == aiTextureType_BASE_COLOR || type == aiTextureType_DIFFUSE_ROUGHNESS
      || type == aiTextureType_METALNESS) {
      continue;
    }
    int count = material->GetTextureCount(static_cast<aiTextureType>(type));
    if (count > 0) {
      has_unsupported_textures = true;
    }
  }
  if (has_unsupported_textures) {
    LOG_WARN("Mesh has unsupported textures!");
  }

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

    float handedness = glm::dot(glm::cross(vertex.Normal, vertex.Tangent), vertex.BiTangent) < 0.0f ? -1.0f : 1.0f;
    if (handedness < 0.0f) {
      vertex.Tangent *= -1.0f;
    }

    mesh->vertices.push_back(vertex);
  }

  // now walk through each of the mesh's faces (a face is a mesh its triangle) and retrieve the corresponding vertex indices.
  for (unsigned int i = 0; i < aiMesh->mNumFaces; i++) {
    aiFace face = aiMesh->mFaces[i];

    // retrieve all indices of the face and store them in the indices vector
    for (int j = 0; j < face.mNumIndices; j++) {
      mesh->indices.push_back(face.mIndices[j]);
    }
  }

  return mesh;
}
void Engine::ProcessNode(Model& model, aiNode* node, const aiScene& scene,
                         std::vector<Ref<Mesh>>& meshes,
                         const glm::mat4& parentTransform) {
  // Accumulate transforms down the hierarchy
  glm::mat4 nodeTransform = parentTransform * ConvertMatrix(node->mTransformation);

  for (uint32_t i = 0; i < node->mNumMeshes; i++) {
    aiMesh* aiMesh = scene.mMeshes[node->mMeshes[i]];
    Ref<Mesh> mesh = ProcessMesh(model, aiMesh, scene);
    if (mesh == nullptr) {
      continue;
    }

    meshes.push_back(mesh);
  }

  for (uint32_t i = 0; i < node->mNumChildren; i++) {
    ProcessNode(model, node->mChildren[i], scene, meshes, nodeTransform);
  }
}

glm::mat4 Engine::ConvertMatrix(const aiMatrix4x4& from) {
  return glm::transpose(glm::make_mat4(&from.a1));
}

}  // namespace Wiesel