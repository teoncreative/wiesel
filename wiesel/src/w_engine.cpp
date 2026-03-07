
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

#include <thread>
#include <assimp/IOSystem.hpp>
#include <assimp/IOStream.hpp>
#include "asset/w_asset_manager.hpp"
#include "input/w_input.hpp"
#include "scene/w_componentutil.hpp"
#include "scene/w_entity.hpp"
#include "scene/w_lights.hpp"
#include "script/w_scriptmanager.hpp"
#include "util/w_dialogs.hpp"
#include "util/w_platform.hpp"
#ifdef WIESEL_BACKEND_SDL3
#include "window/w_sdlwindow.hpp"
#else
#include "window/w_glfwwindow.hpp"
#endif
#include <cxxopts.hpp>

namespace Wiesel {

// Assimp IOStream backed by a VfsFile
class VfsAssimpIOStream : public Assimp::IOStream {
  friend class VfsAssimpIOSystem;
public:
  explicit VfsAssimpIOStream(VfsFile file) : file_(std::move(file)) {}
  ~VfsAssimpIOStream() override = default;

  size_t Read(void* pvBuffer, size_t pSize, size_t pCount) override {
    size_t bytes_requested = pSize * pCount;
    size_t bytes_read = file_.Read(pvBuffer, bytes_requested);
    return bytes_read / pSize;
  }

  size_t Write(const void* /*pvBuffer*/, size_t /*pSize*/, size_t /*pCount*/) override {
    return 0; // Read-only
  }

  aiReturn Seek(size_t pOffset, aiOrigin pOrigin) override {
    switch (pOrigin) {
      case aiOrigin_SET: file_.Seek(pOffset); break;
      case aiOrigin_CUR: file_.SeekRelative(static_cast<int64_t>(pOffset)); break;
      case aiOrigin_END: file_.Seek(file_.Size() - pOffset); break;
      default: return aiReturn_FAILURE;
    }
    return aiReturn_SUCCESS;
  }

  size_t Tell() const override { return file_.Tell(); }
  size_t FileSize() const override { return file_.Size(); }
  void Flush() override {}

private:
  VfsFile file_;
};

// Assimp IOSystem backed by VFS
class VfsAssimpIOSystem : public Assimp::IOSystem {
public:
  VfsAssimpIOSystem(std::shared_ptr<VirtualFileSystem> vfs, const std::string& base_dir)
      : vfs_(std::move(vfs)), base_dir_(base_dir) {}

  ~VfsAssimpIOSystem() override = default;

  bool Exists(const char* pFile) const override {
    return vfs_->FileExists(ResolvePath(pFile));
  }

  char getOsSeparator() const override { return '/'; }

  Assimp::IOStream* Open(const char* pFile, const char* /*pMode*/) override {
    std::string resolved = ResolvePath(pFile);
    if (!vfs_->FileExists(resolved)) {
      return nullptr;
    }
    VfsFile file = vfs_->Open(resolved);
    return new VfsAssimpIOStream(std::move(file));
  }

  void Close(Assimp::IOStream* pFile) override {
    delete pFile;
  }

private:
  std::string ResolvePath(const char* pFile) const {
    std::string path(pFile);
    // If already an absolute VFS path, use as-is
    if (!path.empty() && path[0] == '/') {
      return path;
    }
    // Relative path: resolve against the model's base directory
    return base_dir_ + "/" + path;
  }

  std::shared_ptr<VirtualFileSystem> vfs_;
  std::string base_dir_;
};

EngineProperties EngineProperties::Parse(int argc, char** argv) {
    EngineProperties config;
    std::filesystem::path exe_dir = GetExecutableDirectory();

    // Setup cxxopts
    cxxopts::Options options(argv[0], "Wiesel Game Engine");

    options.add_options()
        ("e,editor", "Enable editor layer", cxxopts::value<bool>()->default_value("false"))
        ("dev", "Enable development mode", cxxopts::value<bool>()->default_value("false"))
        ("engine-assets", "Path to engine assets directory or pak file",
            cxxopts::value<std::string>())
        ("editor-assets", "Path to editor assets directory or pak file",
            cxxopts::value<std::string>())
        ("app-assets", "Path to application assets directory",
            cxxopts::value<std::string>())
        ("project", "Path to project directory",
            cxxopts::value<std::string>())
        ("h,help", "Print usage information");

    try {
        auto result = options.parse(argc, argv);

        // Handle help
        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            std::exit(0);
        }

        // Parse flags
        config.editor_enabled = result["editor"].as<bool>();
        config.dev_mode = result["dev"].as<bool>();

        // Parse optional paths
        if (result.count("engine-assets")) {
            config.engine_assets_path = result["engine-assets"].as<std::string>();
        }

        if (result.count("editor-assets")) {
            config.editor_assets_path = result["editor-assets"].as<std::string>();
        }

        if (result.count("app-assets")) {
            config.app_assets_path = result["app-assets"].as<std::string>();
        }

        if (result.count("project")) {
            config.project_path = result["project"].as<std::string>();
        }

    } catch (const cxxopts::exceptions::exception& e) {
        std::cerr << "Error parsing arguments: " << e.what() << std::endl;
        std::cerr << options.help() << std::endl;
        std::exit(1);
    }

    // Fallback to environment variables
    if (config.engine_assets_path.empty()) {
        if (const char* env = std::getenv("WIESEL_ENGINE_ASSETS")) {
            config.engine_assets_path = env;
        }
    }

    if (config.editor_assets_path.empty() && config.editor_enabled) {
        if (const char* env = std::getenv("WIESEL_EDITOR_ASSETS")) {
            config.editor_assets_path = env;
        }
    }

    if (config.app_assets_path.empty()) {
        if (const char* env = std::getenv("WIESEL_APP_ASSETS")) {
            config.app_assets_path = env;
        }
    }

    if (config.project_path.empty()) {
        if (const char* env = std::getenv("WIESEL_PROJECT_PATH")) {
            config.project_path = env;
        }
    }

    // Final fallback to default locations
    if (config.engine_assets_path.empty()) {
        // Development: Look for source tree
        std::filesystem::path dev_path = exe_dir / "../../../engine/assets";
        if (std::filesystem::exists(dev_path)) {
            config.engine_assets_path = dev_path;
            config.dev_mode = true;
        } else {
            // Release: Look for pak or embedded
            config.engine_assets_path = exe_dir / "engine.pak";
        }
    }

    if (config.editor_assets_path.empty() && config.editor_enabled) {
        std::filesystem::path dev_path = exe_dir / "../../../editor/assets";
        if (std::filesystem::exists(dev_path)) {
            config.editor_assets_path = dev_path;
        } else {
            config.editor_assets_path = exe_dir / "editor.pak";
        }
    }

    if (config.app_assets_path.empty()) {
        // Default to assets/ relative to current working directory
        std::filesystem::path default_app = std::filesystem::current_path() / "assets";
        if (std::filesystem::exists(default_app)) {
            config.app_assets_path = default_app;
        }
    }

    if (config.user_data_path.empty()) {
        config.user_data_path = GetUserDataDirectory();
    }

    return config;
}
EngineProperties Engine::properties_;
std::shared_ptr<Renderer> Engine::renderer_;
std::shared_ptr<AppWindow> Engine::window_;
std::shared_ptr<VirtualFileSystem> Engine::vfs_;
DeveloperConsole Engine::console_;

void Engine::InitEngine(const EngineProperties& props) {
  properties_ = props;
  LOG_INFO("Current work directory: {}",
           std::filesystem::current_path().string());
  LOG_INFO(" - engine_assets_path: {}", props.engine_assets_path.string());
  LOG_INFO(" - editor_enabled: {}", props.editor_enabled);
  LOG_INFO(" - editor_assets_path: {}", props.editor_assets_path.string());
  LOG_INFO(" - app_assets_path: {}", props.app_assets_path.string());
  LOG_INFO(" - project_path: {}", props.project_path.string());
  LOG_INFO(" - user_data_path: {}", props.user_data_path.string());
  LOG_INFO(" - dev_mode: {}", props.dev_mode);
  InitializeVfs();
  InitializeComponents();
  InputManager::Init();
  ScriptManager::Init({.EnableDebugger = true});
}

void Engine::InitWindow(const WindowProperties&& props) {
#ifdef WIESEL_BACKEND_SDL3
  window_ = CreateReference<SdlAppWindow>(std::move(props));
#else
  window_ = CreateReference<GlfwAppWindow>(std::move(props));
#endif
  Dialogs::Init();
}

void Engine::InitRenderer(const RendererProperties&& props) {
  if (window_ == nullptr) {
    LOG_ERROR("Window should be initialized before renderer!");
    abort();
  }
  renderer_ = CreateReference<Renderer>(window_);
  renderer_->Initialize(std::move(props));
}

void Engine::InitializeVfs() {
  vfs_ = std::make_shared<VirtualFileSystem>();
  vfs_->Mount("/engine", properties_.engine_assets_path.string(), 100);
  if (properties_.editor_enabled && !properties_.editor_assets_path.empty()) {
    vfs_->Mount("/editor", properties_.editor_assets_path.string(), 90);
  }
  if (!properties_.app_assets_path.empty()) {
    vfs_->Mount("/app", properties_.app_assets_path.string(), 80);
  }
  if (!properties_.user_data_path.empty()) {
    vfs_->Mount("/user", properties_.user_data_path.string(), 0);
  }
}

void Engine::CleanupRenderer() {
  renderer_->Cleanup();
  renderer_ = nullptr;
}

void Engine::CleanupWindow() {
  window_ = nullptr;
  Dialogs::Destroy();
}

void Engine::CleanupEngine() {
  ScriptManager::Destroy();
  //InputManager::Destroy();
  //CleanupComponents();
}

std::shared_ptr<Renderer> Engine::GetRenderer() {
  if (renderer_ == nullptr) {
    throw std::runtime_error("Renderer is not initialized!");
  }
  return renderer_;
}

std::shared_ptr<AppWindow> Engine::GetWindow() {
  return window_;
}

std::shared_ptr<VirtualFileSystem> Engine::GetVirtualFileSystem() {
  return vfs_;
}

DeveloperConsole& Engine::GetConsole() {
  return console_;
}

aiScene* Engine::LoadAssimpModel(const std::string& path,
                                 bool convert_to_left_handed) {
  LOG_INFO("Loading model: {}", path);

  // Derive base directory for resolving relative references (e.g. .bin, textures)
  std::string base_dir = path;
  size_t last_slash = base_dir.rfind('/');
  if (last_slash != std::string::npos) {
    base_dir = base_dir.substr(0, last_slash);
  }

  Assimp::Importer importer;
  importer.SetIOHandler(new VfsAssimpIOSystem(vfs_, base_dir));
  importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE,
                              aiPrimitiveType_LINE | aiPrimitiveType_POINT);
  importer.SetPropertyBool(AI_CONFIG_PP_PTV_NORMALIZE, true);
  uint32_t flags = aiProcess_Triangulate | aiProcess_CalcTangentSpace |
                   aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices |
                   aiProcess_LimitBoneWeights |
                   aiProcessPreset_TargetRealtime_Fast;
  if (convert_to_left_handed) {
    flags |= aiProcess_ConvertToLeftHanded;
  }
  importer.ReadFile(path.c_str(), flags);
  aiScene* scene = importer.GetOrphanedScene();

  if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) ||
      !scene->mRootNode) {
    LOG_ERROR("Failed to load model {}: {}", path, importer.GetErrorString());
    return nullptr;
  }

  return scene;
}

void Engine::LoadModelAsync(AssetHandle handle) {
  auto& assets = AssetManager::Get();
  const AssetMetadata* meta = assets.GetMetadata(handle);
  if (!meta) {
    LOG_ERROR("LoadModelAsync: invalid model_handle");
    return;
  }
  if (!assets.SetLoadState(handle, AssetLoadState::Unloaded, AssetLoadState::Loading)) {
    return;
  }
  std::string path = meta->virtual_source_path;

  // Derive VFS directory for relative texture resolution
  std::string textures_dir = path;
  size_t last_slash = textures_dir.rfind('/');
  if (last_slash != std::string::npos) {
    textures_dir = textures_dir.substr(0, last_slash);
  }

  // Background thread: Assimp file I/O + parsing only
  // TODO use a thread pool for this instead
  std::thread([path, handle, textures_dir]() {
    aiScene* assimp_scene = LoadAssimpModel(path);

    if (!assimp_scene) {
      AssetManager::Get().SetLoadState(handle, AssetLoadState::Loading, AssetLoadState::Failed);
      return;
    }

    // Main thread: ProcessNode (texture loading needs GPU) + Allocate
    Application::Get()->SubmitToMainThread(
        [assimp_scene, path, handle, textures_dir]() {
          std::shared_ptr<Model> model = std::make_shared<Model>();
          model->model_path = path;
          model->textures_path = textures_dir;
          model->meshes.clear();
          model->textures.clear();
          model->node_hierarchy.nodes.clear();
          model->node_hierarchy.node_name_to_index.clear();
          ProcessNode(*model, assimp_scene->mRootNode, *assimp_scene, model->meshes,
                      -1);
          ExtractAnimations(*model, *assimp_scene);
          uint64_t vertices = 0;
          for (const std::shared_ptr<Mesh>& item : model->meshes) {
            item->Allocate();
            vertices += item->vertices.size();
          }
          LOG_INFO("Loaded {} meshes!", model->meshes.size());
          LOG_INFO("Loaded {} textures!", model->textures.size());
          LOG_INFO("Loaded {} vertices!", vertices);
          if (model->has_skeleton) {
            LOG_INFO("Loaded {} bones!", model->skeleton.bones.size());
          }
          if (model->has_animations) {
            LOG_INFO("Loaded {} animation clips!", model->animation_clips.size());
            for (const auto& clip : model->animation_clips) {
              LOG_INFO("  Clip '{}': {:.1f} ticks, {:.0f} tps, {} channels",
                       clip.name, clip.duration, clip.ticks_per_second,
                       clip.channels.size());
            }
          }

          // Register textures with AssetManager
          AssetManager& assets = AssetManager::Get();
          for (auto& [texPath, tex] : model->textures) {
            assets.RegisterAndStore<Texture>(texPath, AssetType::Texture,
                                             texPath, tex);
          }
          assets.SetLoadState(handle, AssetLoadState::Loading, AssetLoadState::Loaded);
          assets.Store(handle, model);
          delete assimp_scene;
        });
  }).detach();
}

bool Engine::LoadTexture(Model& model, std::shared_ptr<Mesh> mesh,
                         aiMaterial* mat, aiTextureType type,
                         const aiScene& scene) {
  size_t count = mat->GetTextureCount(type);
  if (count > 1) {
    LOG_WARN("Mesh has more than one texture for type {}",
             std::to_string(type));
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
        std::string textureKey =
            model.textures_path + "/embedded_" + std::to_string(texIndex);

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
      // Strip absolute paths (e.g. "C:/Users/.../texture.png") to just filename
      // so we resolve relative to the model's directory
      size_t last_sep = s.find_last_of("/\\");
      if (last_sep != std::string::npos && (s.find(':') != std::string::npos || s[0] == '/')) {
        s = s.substr(last_sep + 1);
      }
      std::string textureFullPath = model.textures_path + "/" + s;
      if (model.textures.contains(textureFullPath)) {
        Material::Set(mesh->mat, model.textures[textureFullPath],
                      static_cast<TextureType>(type));
      } else {
        std::shared_ptr<Texture> texture = GetRenderer()->CreateTexture(
            textureFullPath, {static_cast<TextureType>(type)}, {});
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
        reinterpret_cast<unsigned char*>(aiTex->pcData), aiTex->mWidth, &width,
        &height, &channels, STBI_rgb_alpha);

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

  auto texture = GetRenderer()->CreateTexture(pixelData, 4, props, {});

  stbi_image_free(pixelData);  // Works for both cases

  return texture;
}

unsigned char* Engine::ConvertBGRAtoRGBA(void* bgra_data, int width,
                                         int height) {
  aiTexel* texels = reinterpret_cast<aiTexel*>(bgra_data);
  int size = width * height;

  unsigned char* rgba_data =
      static_cast<unsigned char*>(malloc(size * 4 * sizeof(unsigned char)));

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
  flags |=
      VertexFlagHasAlbedoMap *
      LoadTexture(model, mesh, material, aiTextureType_BASE_COLOR, aiScene);
  flags |= VertexFlagHasRoughnessMap *
           LoadTexture(model, mesh, material, aiTextureType_DIFFUSE_ROUGHNESS,
                       aiScene);
  flags |= VertexFlagHasMetallicMap *
           LoadTexture(model, mesh, material, aiTextureType_METALNESS, aiScene);

  bool has_unsupported_textures = false;
  for (size_t type = aiTextureType_NONE; type < AI_TEXTURE_TYPE_MAX; type++) {
    if (type == aiTextureType_UNKNOWN) {
      continue;
    }
    if (type == aiTextureType_DIFFUSE || type == aiTextureType_NORMALS ||
        type == aiTextureType_SPECULAR || type == aiTextureType_BASE_COLOR ||
        type == aiTextureType_DIFFUSE_ROUGHNESS ||
        type == aiTextureType_METALNESS) {
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

    if (aiMesh->mColors[0]) {
      vertex.Color = {aiMesh->mColors[0][i].r,
                      aiMesh->mColors[0][i].g,
                      aiMesh->mColors[0][i].b};
    } else {
      // Fall back to material diffuse color when no vertex colors exist
      aiColor4D diffuse_color(1.0f, 1.0f, 1.0f, 1.0f);
      material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse_color);
      vertex.Color = {diffuse_color.r, diffuse_color.g, diffuse_color.b};
    }

    // tangent
    if (aiMesh->mTangents) {
      vector.x = aiMesh->mTangents[i].x;
      vector.y = aiMesh->mTangents[i].y;
      vector.z = aiMesh->mTangents[i].z;
      vertex.Tangent = vector;
    } else {
      vertex.Tangent = {1.0f, 0.0f, 0.0f};
    }

    // bitangent
    if (aiMesh->mBitangents) {
      vector.x = aiMesh->mBitangents[i].x;
      vector.y = aiMesh->mBitangents[i].y;
      vector.z = aiMesh->mBitangents[i].z;
      vertex.BiTangent = vector;
    } else {
      vertex.BiTangent = {0.0f, 0.0f, 1.0f};
    }

    float handedness = glm::dot(glm::cross(vertex.Normal, vertex.Tangent),
                                vertex.BiTangent) < 0.0f
                           ? -1.0f
                           : 1.0f;
    if (handedness < 0.0f) {
      vertex.Tangent *= -1.0f;
    }

    mesh->vertices.push_back(vertex);
  }

  // Extract bone weights
  if (aiMesh->mNumBones > 0) {
    for (uint32_t b = 0; b < aiMesh->mNumBones; b++) {
      aiBone* bone = aiMesh->mBones[b];
      std::string bone_name = bone->mName.C_Str();

      // Register bone in skeleton if not already present
      int32_t bone_index;
      auto it = model.skeleton.bone_name_to_index.find(bone_name);
      if (it != model.skeleton.bone_name_to_index.end()) {
        bone_index = it->second;
      } else {
        bone_index = static_cast<int32_t>(model.skeleton.bones.size());
        BoneInfo bone_info;
        bone_info.name = bone_name;
        bone_info.inverse_bind_matrix = ConvertMatrix(bone->mOffsetMatrix);
        model.skeleton.bones.push_back(bone_info);
        model.skeleton.bone_name_to_index[bone_name] = bone_index;
      }

      // Write bone weights into vertices
      for (uint32_t w = 0; w < bone->mNumWeights; w++) {
        uint32_t vertex_id = bone->mWeights[w].mVertexId;
        float weight = bone->mWeights[w].mWeight;
        auto& v = mesh->vertices[vertex_id];
        for (int s = 0; s < WIESEL_MAX_BONE_INFLUENCE; s++) {
          if (v.BoneWeights[s] == 0.0f) {
            v.BoneIndices[s] = bone_index;
            v.BoneWeights[s] = weight;
            break;
          }
        }
      }
    }
    model.has_skeleton = true;
  }

  // now walk through each of the mesh's faces (a face is a mesh its triangle) and retrieve the corresponding vertex indices.
  for (unsigned int i = 0; i < aiMesh->mNumFaces; i++) {
    aiFace face = aiMesh->mFaces[i];

    // Retrieve all indices of the face and store them in the indices vector
    for (int j = 0; j < face.mNumIndices; j++) {
      mesh->indices.push_back(face.mIndices[j]);
    }
  }

  return mesh;
}

void Engine::ProcessNode(Model& model, aiNode* node, const aiScene& scene,
                         std::vector<Ref<Mesh>>& meshes,
                         int32_t parent_node_index) {
  int32_t node_index = static_cast<int32_t>(model.node_hierarchy.nodes.size());

  NodeInfo node_info;
  node_info.name = node->mName.C_Str();
  node_info.parent_index = parent_node_index;
  node_info.local_transform = ConvertMatrix(node->mTransformation);
  node_info.bone_index = -1;

  model.node_hierarchy.node_name_to_index[node_info.name] = node_index;

  for (uint32_t i = 0; i < node->mNumMeshes; i++) {
    int32_t mesh_index = static_cast<int32_t>(meshes.size());
    aiMesh* mesh_ptr = scene.mMeshes[node->mMeshes[i]];
    Ref<Mesh> mesh = ProcessMesh(model, mesh_ptr, scene);
    if (mesh == nullptr) {
      continue;
    }
    mesh->node_index = node_index;
    node_info.mesh_indices.push_back(mesh_index);
    meshes.push_back(mesh);
  }

  // Reserve placeholder children indices
  for (uint32_t i = 0; i < node->mNumChildren; i++) {
    node_info.children.push_back(-1);
  }

  model.node_hierarchy.nodes.push_back(std::move(node_info));

  // Recurse children, filling in child indices
  for (uint32_t i = 0; i < node->mNumChildren; i++) {
    int32_t child_index = static_cast<int32_t>(model.node_hierarchy.nodes.size());
    model.node_hierarchy.nodes[node_index].children[i] = child_index;
    ProcessNode(model, node->mChildren[i], scene, meshes, node_index);
  }
}

void Engine::ExtractAnimations(Model& model, const aiScene& scene) {
  if (scene.mNumAnimations == 0) {
    return;
  }
  model.has_animations = true;

  for (uint32_t a = 0; a < scene.mNumAnimations; a++) {
    aiAnimation* anim = scene.mAnimations[a];
    AnimationClip clip;
    clip.name = anim->mName.C_Str();
    clip.duration = static_cast<float>(anim->mDuration);
    clip.ticks_per_second = anim->mTicksPerSecond > 0
        ? static_cast<float>(anim->mTicksPerSecond) : 25.0f;

    for (uint32_t c = 0; c < anim->mNumChannels; c++) {
      aiNodeAnim* chan = anim->mChannels[c];
      AnimationChannel ch;
      ch.node_name = chan->mNodeName.C_Str();

      ch.position_keys.reserve(chan->mNumPositionKeys);
      for (uint32_t k = 0; k < chan->mNumPositionKeys; k++) {
        ch.position_keys.push_back({
            static_cast<float>(chan->mPositionKeys[k].mTime),
            glm::vec3(chan->mPositionKeys[k].mValue.x,
                      chan->mPositionKeys[k].mValue.y,
                      chan->mPositionKeys[k].mValue.z)});
      }

      ch.rotation_keys.reserve(chan->mNumRotationKeys);
      for (uint32_t k = 0; k < chan->mNumRotationKeys; k++) {
        ch.rotation_keys.push_back({
            static_cast<float>(chan->mRotationKeys[k].mTime),
            glm::quat(chan->mRotationKeys[k].mValue.w,
                      chan->mRotationKeys[k].mValue.x,
                      chan->mRotationKeys[k].mValue.y,
                      chan->mRotationKeys[k].mValue.z)});
      }

      ch.scale_keys.reserve(chan->mNumScalingKeys);
      for (uint32_t k = 0; k < chan->mNumScalingKeys; k++) {
        ch.scale_keys.push_back({
            static_cast<float>(chan->mScalingKeys[k].mTime),
            glm::vec3(chan->mScalingKeys[k].mValue.x,
                      chan->mScalingKeys[k].mValue.y,
                      chan->mScalingKeys[k].mValue.z)});
      }

      clip.channels.push_back(std::move(ch));
    }

    model.animation_clips.push_back(std::move(clip));
  }

  // Link bone parent indices via node hierarchy
  for (auto& bone : model.skeleton.bones) {
    auto node_it = model.node_hierarchy.node_name_to_index.find(bone.name);
    if (node_it != model.node_hierarchy.node_name_to_index.end()) {
      int32_t node_idx = node_it->second;
      model.node_hierarchy.nodes[node_idx].bone_index =
          model.skeleton.bone_name_to_index[bone.name];
      // Walk up to find parent bone
      int32_t parent_node = model.node_hierarchy.nodes[node_idx].parent_index;
      while (parent_node >= 0) {
        auto parent_bone_it = model.skeleton.bone_name_to_index.find(
            model.node_hierarchy.nodes[parent_node].name);
        if (parent_bone_it != model.skeleton.bone_name_to_index.end()) {
          bone.parent_index = parent_bone_it->second;
          break;
        }
        parent_node = model.node_hierarchy.nodes[parent_node].parent_index;
      }
    }
  }
}

glm::mat4 Engine::ConvertMatrix(const aiMatrix4x4& from) {
  return glm::transpose(glm::make_mat4(&from.a1));
}

}  // namespace Wiesel