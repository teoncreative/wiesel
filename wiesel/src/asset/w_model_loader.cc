
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "asset/w_model_loader.h"

#include <assimp/IOStream.hpp>
#include <assimp/IOSystem.hpp>
#include <chrono>
#include <future>
#include <set>

#include "animation/w_animation_clip_asset.h"
#include "asset/w_asset_manager.h"
#include "asset/w_asset_properties.h"
#include "asset/w_asset_registry.h"
#include "rendering/w_mesh.h"
#include "rendering/w_renderer.h"
#include "w_engine.h"

namespace wiesel {

// ---- VFS-backed Assimp IO ----

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

  size_t Write(const void* /*pvBuffer*/, size_t /*pSize*/,
               size_t /*pCount*/) override {
    return 0;  // Read-only
  }

  aiReturn Seek(size_t pOffset, aiOrigin pOrigin) override {
    switch (pOrigin) {
      case aiOrigin_SET:
        file_.Seek(pOffset);
        break;
      case aiOrigin_CUR:
        file_.SeekRelative(static_cast<int64_t>(pOffset));
        break;
      case aiOrigin_END:
        file_.Seek(file_.Size() - pOffset);
        break;
      default:
        return aiReturn_FAILURE;
    }
    return aiReturn_SUCCESS;
  }

  size_t Tell() const override { return file_.Tell(); }

  size_t FileSize() const override { return file_.Size(); }

  void Flush() override {}

 private:
  VfsFile file_;
};

class VfsAssimpIOSystem : public Assimp::IOSystem {
 public:
  VfsAssimpIOSystem(std::shared_ptr<VirtualFileSystem> vfs,
                    const std::string& base_dir)
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

  void Close(Assimp::IOStream* pFile) override { delete pFile; }

 private:
  std::string ResolvePath(const char* pFile) const {
    std::string path(pFile);
    // If already a VFS path (has :// scheme), use as-is
    if (path.find("://") != std::string::npos) {
      return path;
    }
    // If absolute path starting with /, use as-is
    if (!path.empty() && path[0] == '/') {
      return path;
    }
    // Relative path: resolve against the model's base directory
    return base_dir_ + "/" + path;
  }

  std::shared_ptr<VirtualFileSystem> vfs_;
  std::string base_dir_;
};

// Populated by PreDecodeTextures before calling LoadSync on child textures,
// so the texture loader grabs pre-decoded pixels without re-importing Assimp.
static thread_local std::unordered_map<std::string,
                                       std::shared_ptr<DecodedTextureData>>
    tl_decoded_texture_cache;

static unsigned char* ConvertBGRAtoRGBA(void* bgra_data, int width,
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

static glm::mat4 ConvertMatrix(const aiMatrix4x4& from) {
  return glm::transpose(glm::make_mat4(&from.a1));
}

static std::string NormalizeTexturePath(const std::string& raw,
                                        const std::string& textures_dir) {
  std::string s = raw;
  size_t last_sep = s.find_last_of("/\\");
  if (last_sep != std::string::npos &&
      (s.find(':') != std::string::npos || s[0] == '/')) {
    s = s.substr(last_sep + 1);
  }
  return textures_dir + "/" + s;
}

static void PreDecodeTextures(Model& model, const aiScene& scene);
static bool LoadTexture(Model& model, std::shared_ptr<Mesh> mesh,
                        aiMaterial* mat, aiTextureType type,
                        const aiScene& scene);
static std::shared_ptr<Mesh> ProcessMesh(Model& model, aiMesh* aiMesh,
                                         const aiScene& aiScene);
static void ProcessNode(Model& model, aiNode* node, const aiScene& scene,
                        std::vector<std::shared_ptr<Mesh>>& meshes,
                        std::unordered_map<aiMesh*, int32_t>& mesh_cache,
                        int32_t parent_node_index);
static void ExtractAnimations(Model& model, const aiScene& scene);

std::unique_ptr<aiScene> LoadAssimpScene(const std::string& path,
                                         bool convert_to_left_handed) {
  LOG_INFO("Loading model: {}", path);

  std::string base_dir = path;
  size_t last_slash = base_dir.rfind('/');
  if (last_slash != std::string::npos) {
    base_dir = base_dir.substr(0, last_slash);
  }

  Assimp::Importer importer;
  importer.SetIOHandler(new VfsAssimpIOSystem(Engine::vfs(), base_dir));
  importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE,
                              aiPrimitiveType_LINE | aiPrimitiveType_POINT);
  // Normalize FBX scale (Blender exports in cm, we use meters)
  importer.SetPropertyFloat(AI_CONFIG_GLOBAL_SCALE_FACTOR_KEY, 1.0f);
  uint32_t flags = aiProcess_Triangulate | aiProcess_CalcTangentSpace |
                   aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices |
                   aiProcess_LimitBoneWeights | aiProcess_GlobalScale |
                   aiProcessPreset_TargetRealtime_Fast;
  if (convert_to_left_handed) {
    flags |= aiProcess_ConvertToLeftHanded;
  }
  importer.ReadFile(path.c_str(), flags);
  aiScene* scene = importer.GetOrphanedScene();

  if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) ||
      !scene->mRootNode) {
    DCON_LOG_ERROR("Failed to load model {}: {} (scene={}, flags={}, root={})",
                   path, importer.GetErrorString(), (void*)scene,
                   scene ? scene->mFlags : 0,
              scene ? (void*)scene->mRootNode : nullptr);
    return nullptr;
  }

  return std::unique_ptr<aiScene>(scene);
}

bool LoadTextureAsset(AssetHandle handle) {
  auto& asset_manager = Engine::asset_manager();
  auto renderer = Engine::renderer();
  auto vfs = Engine::vfs();

  const AssetMetadata* meta = asset_manager.GetMetadata(handle);
  if (!meta) {
    return false;
  }

  meta->load_progress.store(0.1f);

  std::string path = meta->virtual_source_path;
  int w = 0;
  int h = 0;
  int channels = 0;
  stbi_uc* pixels = nullptr;
  bool free_pixels = true;

  // Check for embedded texture fragment URI: "path/to/model.glb#texture_N"
  size_t hash_pos = path.find('#');
  if (hash_pos != std::string::npos) {
    // First check the thread-local decoded cache (populated by model loader)
    auto cache_it = tl_decoded_texture_cache.find(path);
    if (cache_it != tl_decoded_texture_cache.end() && cache_it->second &&
        cache_it->second->pixels) {
      auto& decoded = cache_it->second;
      w = decoded->width;
      h = decoded->height;
      pixels = decoded->pixels;
      // Take ownership - null out source so DecodedTextureData destructor
      // doesn't free it
      decoded->pixels = nullptr;
      tl_decoded_texture_cache.erase(cache_it);
    } else {
      // Cold reload path: re-import via Assimp and extract the texture
      std::string model_path = path.substr(0, hash_pos);
      std::string fragment = path.substr(hash_pos + 1);
      int embedded_index = -1;
      if (fragment.starts_with("texture_")) {
        embedded_index = std::atoi(fragment.c_str() + 8);
      }
      if (embedded_index < 0) {
        DCON_LOG_ERROR("LoadTextureAsset: invalid fragment URI: {}", path);
        return false;
      }

      std::string base_dir = model_path;
      size_t slash = base_dir.rfind('/');
      if (slash != std::string::npos) {
        base_dir = base_dir.substr(0, slash);
      }

      Assimp::Importer importer;
      importer.SetIOHandler(new VfsAssimpIOSystem(vfs, base_dir));
      importer.ReadFile(model_path.c_str(), aiProcess_Triangulate);
      const aiScene* scene = importer.GetScene();
      if (!scene || embedded_index >= static_cast<int>(scene->mNumTextures)) {
        DCON_LOG_ERROR(
            "LoadTextureAsset: failed to extract embedded texture {} "
            "from {}",
            embedded_index, model_path);
        return false;
      }

      aiTexture* ai_tex = scene->mTextures[embedded_index];
      if (ai_tex->mHeight == 0) {
        pixels = stbi_load_from_memory(
            reinterpret_cast<unsigned char*>(ai_tex->pcData), ai_tex->mWidth,
            &w, &h, &channels, STBI_rgb_alpha);
      } else {
        w = ai_tex->mWidth;
        h = ai_tex->mHeight;
        pixels = ConvertBGRAtoRGBA(ai_tex->pcData, w, h);
      }
    }
  } else {
    // Standalone or external texture: load from VFS path
    VfsFile file = vfs->Open(path);
    if (!file) {
      DCON_LOG_ERROR("LoadTextureAsset: file not found: {}", path);
      return false;
    }
    pixels = stbi_load_from_memory(file.Data(), static_cast<int>(file.Size()),
                                   &w, &h, &channels, STBI_rgb_alpha);
  }

  if (!pixels) {
    DCON_LOG_ERROR("LoadTextureAsset: decode failed for {}", path);
    return false;
  }

  // Scan for semi-transparent pixels (0 < alpha < 255)
  bool has_semi_transparency = false;
  int pixel_count = w * h;
  for (int p = 0; p < pixel_count; p++) {
    uint8_t a = pixels[p * 4 + 3];
    if (a > 0 && a < 255) {
      has_semi_transparency = true;
      break;
    }
  }

  meta->load_progress.store(0.5f);

  // Read asset properties (filtering, mipmaps, etc.)
  const auto* tex_props = meta->GetProperties<TextureAssetProperties>();
  TextureAssetProperties defaults;
  const auto& ap = tex_props ? *tex_props : defaults;

  // Determine format from asset properties
  bool use_srgb = (ap.asset_type == TextureAssetType::Default);

  // Reuse existing command pool if one is already active on this thread
  // (e.g., when called from within the model loader's batch upload)
  bool owns_pool = !Renderer::HasThreadCommandPool();
  VkCommandPool pool = VK_NULL_HANDLE;
  if (owns_pool) {
    pool = renderer->CreateTransientCommandPool();
    Renderer::SetThreadCommandPool(pool);
  }
  renderer->BeginBatchUpload();

  TextureProps props;
  props.width = w;
  props.height = h;
  props.image_format =
      use_srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
  props.generate_mipmaps = ap.generate_mipmaps;

  VkFilter vk_filter = (ap.filter_mode == TextureFilterMode::Nearest)
                           ? VK_FILTER_NEAREST
                           : VK_FILTER_LINEAR;
  VkSamplerAddressMode vk_wrap;
  switch (ap.wrap_mode) {
    case TextureWrapMode::Clamp:
      vk_wrap = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
      break;
    case TextureWrapMode::Mirror:
      vk_wrap = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
      break;
    default:
      vk_wrap = VK_SAMPLER_ADDRESS_MODE_REPEAT;
      break;
  }
  SamplerProps sampler;
  sampler.mag_filter = vk_filter;
  sampler.min_filter = vk_filter;
  sampler.address_mode = vk_wrap;

  auto texture = renderer->CreateTexture(pixels, 4, props, sampler);
  if (texture) {
    texture->has_semi_transparency_ = has_semi_transparency;
  }

  if (free_pixels) {
    stbi_image_free(pixels);
  }

  if (owns_pool) {
    renderer->EndBatchUpload();
    Renderer::SetThreadCommandPool(VK_NULL_HANDLE);
    vkDestroyCommandPool(renderer->GetLogicalDevice(), pool, nullptr);
  }

  if (!texture) {
    return false;
  }

  meta->load_progress.store(0.95f);
  asset_manager.Store<Texture>(handle, texture);
  return true;
}

bool LoadModelAsset(AssetHandle handle) {
  auto& asset_manager = Engine::asset_manager();
  auto renderer = Engine::renderer();
  auto vfs = Engine::vfs();

  const AssetMetadata* meta = asset_manager.GetMetadata(handle);
  if (!meta) {
    DCON_LOG_ERROR("LoadModel: invalid handle");
    return false;
  }

  std::string path = meta->virtual_source_path;
  std::string textures_dir = path;
  size_t last_slash = textures_dir.rfind('/');
  if (last_slash != std::string::npos) {
    textures_dir = textures_dir.substr(0, last_slash);
  }

  {
    using Clock = std::chrono::high_resolution_clock;
    auto t0 = Clock::now();

    auto assimp_scene = LoadAssimpScene(path);
    meta->load_progress.store(0.2f);

    auto t1 = Clock::now();
    LOG_INFO("Model import (Assimp): {:.1f}s",
             std::chrono::duration<double>(t1 - t0).count());

    if (!assimp_scene) {
      asset_manager.SetLoadState(handle, AssetLoadState::Loading,
                                 AssetLoadState::Failed);
      return false;
    }

    VkCommandPool upload_pool = renderer->CreateTransientCommandPool();
    Renderer::SetThreadCommandPool(upload_pool);

    std::shared_ptr<Model> model = std::make_shared<Model>();
    model->model_path = path;
    model->textures_path = textures_dir;
    model->meshes.clear();
    model->node_hierarchy.nodes.clear();
    model->node_hierarchy.node_name_to_index.clear();

    // Pre-decode all embedded textures in parallel (CPU-bound stbi_load).
    // Results go into tl_decoded_texture_cache, consumed by LoadTextureAsset.
    auto t_predecode = Clock::now();
    PreDecodeTextures(*model, *assimp_scene);
    auto t_predecode_end = Clock::now();
    LOG_INFO(
        "Parallel texture decode: {:.1f}s ({} textures)",
        std::chrono::duration<double>(t_predecode_end - t_predecode).count(),
        tl_decoded_texture_cache.size());
    meta->load_progress.store(0.5f);

    // Batch all GPU uploads (textures + vertex/index buffers) into a single
    // command buffer submission to avoid per-resource GPU sync overhead.
    renderer->BeginBatchUpload();

    auto t2 = Clock::now();
    // Cache of aiMesh* -> mesh_index so identical submeshes referenced by
    // multiple nodes share one wiesel::Mesh (and thus one vertex/index buffer).
    // This is what lets the GPU instancer batch them at render time.
    std::unordered_map<aiMesh*, int32_t> mesh_cache;
    ProcessNode(*model, assimp_scene->mRootNode, *assimp_scene, model->meshes,
                mesh_cache, -1);
    auto t3 = Clock::now();
    LOG_INFO("ProcessNode (meshes + textures): {:.1f}s",
             std::chrono::duration<double>(t3 - t2).count());
    meta->load_progress.store(0.8f);

    model->ComputeMeshNodeTransforms();
    model->ComputeBounds();

    // FBX files store properties in scene units (often centimeters).
    // aiProcess_GlobalScale scales geometry but not camera/light values,
    // so we read the unit scale factor and apply it manually.
    float unit_scale = 0.01f;  // default: assume FBX cm -> meters
    if (assimp_scene->mMetaData) {
      // UnitScaleFactor tells us how many cm one FBX unit is.
      // e.g. 1.0 means 1 unit = 1 cm, 2.54 means 1 unit = 1 inch
      float float_val = 0.0f;
      double double_val = 0.0;
      if (assimp_scene->mMetaData->Get("UnitScaleFactor", double_val) &&
          double_val > 0.0) {
        unit_scale = static_cast<float>(double_val) * 0.01f;
      } else if (assimp_scene->mMetaData->Get("UnitScaleFactor", float_val) &&
                 float_val > 0.0f) {
        unit_scale = float_val * 0.01f;
      }
    }

    // Extract cameras from the scene
    for (unsigned int i = 0; i < assimp_scene->mNumCameras; i++) {
      const aiCamera* cam = assimp_scene->mCameras[i];
      Model::ImportedCamera ic;
      ic.node_name = cam->mName.C_Str();
      ic.fov = glm::degrees(cam->mHorizontalFOV * 2.0f);
      ic.near_plane = cam->mClipPlaneNear * unit_scale;
      ic.far_plane = cam->mClipPlaneFar * unit_scale;
      ic.aspect_ratio = cam->mAspect;
      ic.look_at = {cam->mLookAt.x, cam->mLookAt.y, cam->mLookAt.z};
      ic.up = {cam->mUp.x, cam->mUp.y, cam->mUp.z};
      model->imported_cameras.push_back(ic);
    }

    // Extract lights from the scene
    for (unsigned int i = 0; i < assimp_scene->mNumLights; i++) {
      const aiLight* light = assimp_scene->mLights[i];
      Model::ImportedLight il;
      il.node_name = light->mName.C_Str();

      // FBX light colors can be unnormalized (e.g. 1000).
      // Normalize color to 0-1 and store intensity separately.
      glm::vec3 raw_color = {light->mColorDiffuse.r, light->mColorDiffuse.g,
                             light->mColorDiffuse.b};
      float max_channel =
          glm::max(raw_color.r, glm::max(raw_color.g, raw_color.b));
      if (max_channel > 0.0f) {
        il.color = raw_color / max_channel;
      } else {
        il.color = {1.0f, 1.0f, 1.0f};
      }
      // Store raw intensity but the engine's density is typically 0-10 range,
      // so we don't pass the raw FBX value directly
      il.intensity = glm::clamp(max_channel, 0.0f, 10.0f);

      il.attenuation_constant = light->mAttenuationConstant;
      il.attenuation_linear = light->mAttenuationLinear;
      il.attenuation_quadratic = light->mAttenuationQuadratic;
      // FBX often stores no meaningful attenuation. Use sensible defaults
      // when both falloff terms are effectively zero.
      if (il.attenuation_linear < 1e-6f && il.attenuation_quadratic < 1e-6f) {
        il.attenuation_constant = 1.0f;
        il.attenuation_linear = 0.09f;
        il.attenuation_quadratic = 0.032f;
      }

      switch (light->mType) {
        case aiLightSource_DIRECTIONAL:
          il.type = Model::ImportedLight::Type::Directional;
          break;
        case aiLightSource_POINT:
          il.type = Model::ImportedLight::Type::Point;
          break;
        case aiLightSource_SPOT:
          il.type = Model::ImportedLight::Type::Spot;
          il.inner_cone_angle = glm::degrees(light->mAngleInnerCone);
          il.outer_cone_angle = glm::degrees(light->mAngleOuterCone);
          break;
        default:
          il.type = Model::ImportedLight::Type::Point;
          break;
      }
      model->imported_lights.push_back(il);
    }

    for (const auto& m : model->meshes) {
      if (m->has_transparency) {
        model->has_transparent_meshes = true;
        break;
      }
    }

    tl_decoded_texture_cache.clear();

    ExtractAnimations(*model, *assimp_scene);
    uint64_t vertices = 0;
    for (const std::shared_ptr<Mesh>& item : model->meshes) {
      item->Allocate();
      vertices += item->vertices.size();
    }
    auto t4 = Clock::now();
    LOG_INFO("Mesh allocation: {:.1f}s",
             std::chrono::duration<double>(t4 - t3).count());

    renderer->EndBatchUpload();

    auto t5 = Clock::now();
    LOG_INFO("Batch GPU upload: {:.1f}s",
             std::chrono::duration<double>(t5 - t4).count());
    meta->load_progress.store(0.95f);

    Renderer::SetThreadCommandPool(VK_NULL_HANDLE);
    vkDestroyCommandPool(renderer->GetLogicalDevice(), upload_pool, nullptr);

    LOG_INFO("Loaded {} meshes!", model->meshes.size());
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

    // Register materials on main thread
    Engine::app().SubmitToMainThread([model, handle, vfs]() {
      auto& mgr = Engine::asset_manager();

      std::string model_dir;
      std::string model_stem;
      {
        size_t sl = model->model_path.rfind('/');
        model_dir =
            (sl != std::string::npos) ? model->model_path.substr(0, sl) : "";
        std::string filename = (sl != std::string::npos)
                                   ? model->model_path.substr(sl + 1)
                                   : model->model_path;
        size_t dot = filename.rfind('.');
        model_stem =
            (dot != std::string::npos) ? filename.substr(0, dot) : filename;
      }

      for (size_t i = 0; i < model->meshes.size(); i++) {
        auto& mesh = model->meshes[i];
        if (mesh->mat) {
          std::string mat_name = mesh->mat->name.empty()
                                     ? "Material_" + std::to_string(i)
                                     : mesh->mat->name;

          std::string wmat_vfs_path =
              model_dir + "/" + model_stem + "_" + mat_name + ".wmat";

          AssetHandle existing = mgr.FindBySourcePath(wmat_vfs_path);
          if (existing.IsValid()) {
            // Load from disk to preserve user edits
            mesh->material_handle = existing;
            AssetRegistry::LoadJson(existing);
            auto loaded_mat = mgr.Get<Material>(existing);
            if (loaded_mat) {
              mesh->mat = loaded_mat;
            }
          } else {
            AssetHandle mat_handle = AssetRegistry::Create<Material>(
                mat_name, AssetType::Material, wmat_vfs_path, mesh->mat);
            mesh->material_handle = mat_handle;
            mesh->mat->asset_handle = mat_handle;
          }
        }
      }
      // Extract animation clips to .wanimclip files
      if (model->has_animations) {
        for (const auto& clip : model->animation_clips) {
          std::string clip_name =
              clip.name.empty()
                  ? "Clip_" + std::to_string(&clip - &model->animation_clips[0])
                  : clip.name;
          std::string clip_vfs =
              model_dir + "/" + model_stem + "_" + clip_name + ".wanimclip";

          AssetHandle existing = mgr.FindBySourcePath(clip_vfs);
          if (existing.IsValid()) {
            continue;  // already extracted
          }

          auto clip_data = std::make_shared<AnimClipAssetData>();
          clip_data->duration = clip.duration;
          clip_data->ticks_per_second = clip.ticks_per_second;
          clip_data->loop = true;
          clip_data->bone_channels = clip.channels;

          // Compute max bone reach from all position keyframes
          float max_reach = 0.0f;
          for (const auto& ch : clip.channels) {
            for (const auto& [time, value] : ch.position_keys) {
              float dist = glm::length(value);
              if (dist > max_reach) {
                max_reach = dist;
              }
            }
          }
          clip_data->max_bone_reach = max_reach;

          AssetHandle clip_handle =
              AssetRegistry::Create<AnimClipAssetData>(
              clip_name, AssetType::AnimClip, clip_vfs, clip_data);
          if (clip_handle.IsValid()) {
            LOG_INFO("Extracted animation clip: {}", clip_vfs);
          }
        }
      }

      mgr.SetLoadState(handle, AssetLoadState::Loading, AssetLoadState::Loaded);
      mgr.Store(handle, model);
    });
  }
  return true;
}

static void PreDecodeTextures(Model& model, const aiScene& scene) {
  const aiTextureType texture_types[] = {
      aiTextureType_DIFFUSE,           aiTextureType_NORMALS,
      aiTextureType_SPECULAR,          aiTextureType_BASE_COLOR,
      aiTextureType_DIFFUSE_ROUGHNESS, aiTextureType_METALNESS};

  struct EmbeddedTask {
    int index;
    std::string key;
    aiTexture* tex;
  };

  std::set<int> seen_indices;
  std::vector<EmbeddedTask> embedded_tasks;
  for (unsigned int m = 0; m < scene.mNumMaterials; m++) {
    aiMaterial* mat = scene.mMaterials[m];
    for (auto tex_type : texture_types) {
      for (unsigned int i = 0; i < mat->GetTextureCount(tex_type); i++) {
        aiString str;
        mat->GetTexture(tex_type, i, &str);
        std::string s = str.C_Str();
        if (s.empty() || s[0] != '*') {
          continue;
        }
        int idx = std::atoi(s.c_str() + 1);
        if (idx < 0 || static_cast<unsigned>(idx) >= scene.mNumTextures) {
          continue;
        }
        if (seen_indices.contains(idx)) {
          continue;
        }
        seen_indices.insert(idx);
        std::string key = model.model_path + "#texture_" + std::to_string(idx);
        embedded_tasks.push_back({idx, key, scene.mTextures[idx]});
      }
    }
  }

  if (embedded_tasks.empty()) {
    return;
  }

  std::vector<
      std::future<std::pair<std::string, std::shared_ptr<DecodedTextureData>>>>
      futures;
  for (auto& task : embedded_tasks) {
    futures.push_back(std::async(
        std::launch::async,
        [task]()
            -> std::pair<std::string, std::shared_ptr<DecodedTextureData>> {
          auto decoded = std::make_shared<DecodedTextureData>();
          if (task.tex->mHeight == 0) {
            decoded->pixels = stbi_load_from_memory(
                reinterpret_cast<unsigned char*>(task.tex->pcData),
                task.tex->mWidth, &decoded->width, &decoded->height,
                &decoded->channels, STBI_rgb_alpha);
          } else {
            decoded->width = task.tex->mWidth;
            decoded->height = task.tex->mHeight;
            decoded->channels = 4;
            int size = decoded->width * decoded->height;
            decoded->pixels = static_cast<stbi_uc*>(malloc(size * 4));
            if (!decoded->pixels) {
              return {task.key, nullptr};
            }
            aiTexel* texels = task.tex->pcData;
            for (int i = 0; i < size; i++) {
              decoded->pixels[i * 4 + 0] = texels[i].r;
              decoded->pixels[i * 4 + 1] = texels[i].g;
              decoded->pixels[i * 4 + 2] = texels[i].b;
              decoded->pixels[i * 4 + 3] = texels[i].a;
            }
          }
          if (!decoded->pixels) {
            return {task.key, nullptr};
          }
          int pixel_count = decoded->width * decoded->height;
          for (int p = 0; p < pixel_count; p++) {
            uint8_t a = decoded->pixels[p * 4 + 3];
            if (a > 0 && a < 255) {
              decoded->has_semi_transparency = true;
              break;
            }
          }
          return {task.key, decoded};
        }));
  }

  for (auto& f : futures) {
    auto [key, data] = f.get();
    if (data) {
      tl_decoded_texture_cache[key] = std::move(data);
    }
  }
}

static bool LoadTexture(Model& model, std::shared_ptr<Mesh> mesh,
                        aiMaterial* mat, aiTextureType type,
                        const aiScene& scene) {
  auto& asset_manager = Engine::asset_manager();

  size_t count = mat->GetTextureCount(type);
  if (count > 1) {
    LOG_WARN("Mesh has more than one texture for type {}",
             std::to_string(type));
  }

  TextureType tex_type = static_cast<TextureType>(type);

  for (unsigned int i = 0; i < count; i++) {
    aiString str;
    mat->GetTexture(type, i, &str);
    std::string s = std::string(str.C_Str());
    if (s.empty()) {
      continue;
    }

    // Build VFS path for this texture
    std::string vfs_path;
    if (s[0] == '*') {
      int tex_index = std::atoi(s.c_str() + 1);
      if (tex_index < 0 || tex_index >= static_cast<int>(scene.mNumTextures)) {
        continue;
      }
      vfs_path = model.model_path + "#texture_" + std::to_string(tex_index);
    } else {
      vfs_path = NormalizeTexturePath(s, model.textures_path);
    }

    // Register texture asset if not already registered
    AssetHandle tex_handle = asset_manager.FindBySourcePath(vfs_path);
    if (!tex_handle.IsValid()) {
      tex_handle =
          asset_manager.Register(vfs_path, AssetType::Texture, vfs_path);
    }

    // Set texture asset properties (sRGB vs UNORM) based on Assimp type
    if (tex_handle.IsValid()) {
      auto* meta =
          const_cast<AssetMetadata*>(asset_manager.GetMetadata(tex_handle));
      if (meta) {
        bool is_color =
            tex_type == TextureTypeDiffuse || tex_type == TextureTypeBaseColor;
        auto& props = meta->GetOrCreateProperties<TextureAssetProperties>();
        props.asset_type =
            is_color ? TextureAssetType::Default : TextureAssetType::NormalMap;
      }
    }

    // Load texture through the unified texture loader
    if (tex_handle.IsValid() &&
        asset_manager.GetLoadState(tex_handle) != AssetLoadState::Loaded) {
      asset_manager.LoadSync(tex_handle);
    }

    // Check loaded texture for semi-transparency
    if (tex_type == TextureTypeDiffuse || tex_type == TextureTypeBaseColor) {
      auto tex = asset_manager.Get<Texture>(tex_handle);
      if (tex && tex->has_semi_transparency_) {
        mesh->has_transparency = true;
      }
    }
    if (tex_type == TextureTypeOpacity) {
      mesh->has_transparency = true;
    }

    Material::Set(mesh->mat, tex_handle, tex_type);
    return true;
  }
  return false;
}

static std::shared_ptr<Mesh> ProcessMesh(Model& model, aiMesh* aiMesh,
                                         const aiScene& aiScene) {
  std::vector<Vertex3D> vertices;
  std::vector<Index> indices;

  aiMaterial* material = aiScene.mMaterials[aiMesh->mMaterialIndex];

  std::shared_ptr<Mesh> mesh = std::make_shared<Mesh>();

  aiString ai_mat_name;
  if (material->Get(AI_MATKEY_NAME, ai_mat_name) == AI_SUCCESS) {
    mesh->mat->name = ai_mat_name.C_Str();
  }

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
  flags |= VertexFlagHasOpacityMap *
           LoadTexture(model, mesh, material, aiTextureType_OPACITY, aiScene);

  // Read material base color from assimp.
  bool has_color_texture =
      (flags & VertexFlagHasTexture) || (flags & VertexFlagHasAlbedoMap);
  if (!has_color_texture) {
    aiColor4D diffuse_color;
    if (material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse_color) == AI_SUCCESS) {
      mesh->mat->SetColorTint(glm::vec4(diffuse_color.r, diffuse_color.g,
                                        diffuse_color.b, diffuse_color.a));
    } else {
      aiColor4D base_color;
      if (material->Get(AI_MATKEY_BASE_COLOR, base_color) == AI_SUCCESS) {
        mesh->mat->SetColorTint(
            glm::vec4(base_color.r, base_color.g, base_color.b, base_color.a));
      }
    }
  }

  // Read PBR material factors from assimp (glTF roughness/metallic workflow)
  float roughness_factor;
  if (material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness_factor) !=
      AI_SUCCESS) {
    roughness_factor = (flags & VertexFlagHasRoughnessMap) ? 1.0f : 0.5f;
  }
  float metallic_factor;
  if (material->Get(AI_MATKEY_METALLIC_FACTOR, metallic_factor) != AI_SUCCESS) {
    metallic_factor = (flags & VertexFlagHasMetallicMap) ? 1.0f : 0.0f;
  }
  mesh->mat->SetProperty("roughness", roughness_factor);
  mesh->mat->SetProperty("metallic", metallic_factor);

  float specular_factor;
  if (material->Get(AI_MATKEY_SPECULAR_FACTOR, specular_factor) != AI_SUCCESS) {
    specular_factor = (flags & VertexFlagHasSpecularMap) ? 1.0f : 0.08f;
  }
  mesh->mat->SetProperty("specular", specular_factor);

  int two_sided = 0;
  if (material->Get(AI_MATKEY_TWOSIDED, two_sided) == AI_SUCCESS && two_sided) {
    mesh->mat->double_sided = true;
  }
  // Materials with opacity maps are typically double-sided (hair, skirts, etc.)
  if (mesh->mat->opacity_map.HasTexture()) {
    mesh->mat->double_sided = true;
  }

  for (unsigned int i = 0; i < aiMesh->mNumVertices; i++) {
    Vertex3D vertex{};
    glm::vec3 vector;
    vector.x = aiMesh->mVertices[i].x;
    vector.y = aiMesh->mVertices[i].y;
    vector.z = aiMesh->mVertices[i].z;
    vertex.ppos = vector;

    vector.x = aiMesh->mNormals[i].x;
    vector.y = aiMesh->mNormals[i].y;
    vector.z = aiMesh->mNormals[i].z;
    vertex.normal = vector;

    if (aiMesh->mTextureCoords[0]) {
      glm::vec2 vec;
      vec.x = aiMesh->mTextureCoords[0][i].x;
      vec.y = aiMesh->mTextureCoords[0][i].y;
      vertex.uv = vec;
    } else {
      vertex.uv = glm::vec2(0.0f, 0.0f);
    }
    vertex.flags = flags;

    if (aiMesh->mColors[0]) {
      vertex.color = {aiMesh->mColors[0][i].r, aiMesh->mColors[0][i].g,
                      aiMesh->mColors[0][i].b};
    } else {
      vertex.color = {1.0f, 1.0f, 1.0f};
    }

    if (aiMesh->mTangents) {
      vertex.tangent = {aiMesh->mTangents[i].x, aiMesh->mTangents[i].y,
                        aiMesh->mTangents[i].z};
    }

    vertices.push_back(vertex);
  }

  // Bone weights
  if (aiMesh->mNumBones > 0) {
    model.has_skeleton = true;
    for (uint32_t b = 0; b < aiMesh->mNumBones; b++) {
      aiBone* bone = aiMesh->mBones[b];
      std::string bone_name = bone->mName.C_Str();

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

      for (uint32_t w = 0; w < bone->mNumWeights; w++) {
        uint32_t vid = bone->mWeights[w].mVertexId;
        float weight = bone->mWeights[w].mWeight;

        auto& v = vertices[vid];
        for (int s = 0; s < 4; s++) {
          if (v.bone_weights[s] == 0.0f) {
            v.bone_indices[s] = bone_index;
            v.bone_weights[s] = weight;
            break;
          }
        }
      }
    }
  }

  for (unsigned int i = 0; i < aiMesh->mNumFaces; i++) {
    aiFace face = aiMesh->mFaces[i];
    for (unsigned int j = 0; j < face.mNumIndices; j++) {
      indices.push_back(face.mIndices[j]);
    }
  }

  mesh->vertices = std::move(vertices);
  mesh->indices = std::move(indices);
  mesh->model_path = model.model_path;

  return mesh;
}

static void ProcessNode(Model& model, aiNode* node, const aiScene& scene,
                        std::vector<std::shared_ptr<Mesh>>& meshes,
                        std::unordered_map<aiMesh*, int32_t>& mesh_cache,
                        int32_t parent_node_index) {
  int32_t node_index = static_cast<int32_t>(model.node_hierarchy.nodes.size());

  NodeInfo node_info;
  node_info.name = node->mName.C_Str();
  node_info.parent_index = parent_node_index;
  node_info.local_transform = ConvertMatrix(node->mTransformation);
  node_info.bone_index = -1;

  model.node_hierarchy.node_name_to_index[node_info.name] = node_index;

  for (uint32_t i = 0; i < node->mNumMeshes; i++) {
    aiMesh* mesh_ptr = scene.mMeshes[node->mMeshes[i]];
    auto cached = mesh_cache.find(mesh_ptr);
    if (cached != mesh_cache.end()) {
      node_info.mesh_indices.push_back(cached->second);
      continue;
    }
    std::shared_ptr<Mesh> mesh = ProcessMesh(model, mesh_ptr, scene);
    if (mesh == nullptr) {
      continue;
    }
    mesh->node_index = node_index;
    int32_t mesh_index = static_cast<int32_t>(meshes.size());
    meshes.push_back(mesh);
    mesh_cache[mesh_ptr] = mesh_index;
    node_info.mesh_indices.push_back(mesh_index);
  }

  for (uint32_t i = 0; i < node->mNumChildren; i++) {
    node_info.children.push_back(-1);
  }

  model.node_hierarchy.nodes.push_back(std::move(node_info));

  for (uint32_t i = 0; i < node->mNumChildren; i++) {
    int32_t child_index =
        static_cast<int32_t>(model.node_hierarchy.nodes.size());
    model.node_hierarchy.nodes[node_index].children[i] = child_index;
    ProcessNode(model, node->mChildren[i], scene, meshes, mesh_cache,
                node_index);
  }
}

static void ExtractAnimations(Model& model, const aiScene& scene) {
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
                                ? static_cast<float>(anim->mTicksPerSecond)
                                : 25.0f;

    for (uint32_t c = 0; c < anim->mNumChannels; c++) {
      aiNodeAnim* chan = anim->mChannels[c];
      AnimationChannel ch;
      ch.node_name = chan->mNodeName.C_Str();

      ch.position_keys.reserve(chan->mNumPositionKeys);
      for (uint32_t k = 0; k < chan->mNumPositionKeys; k++) {
        ch.position_keys.push_back(
            {static_cast<float>(chan->mPositionKeys[k].mTime),
             glm::vec3(chan->mPositionKeys[k].mValue.x,
                       chan->mPositionKeys[k].mValue.y,
                       chan->mPositionKeys[k].mValue.z)});
      }

      ch.rotation_keys.reserve(chan->mNumRotationKeys);
      for (uint32_t k = 0; k < chan->mNumRotationKeys; k++) {
        ch.rotation_keys.push_back(
            {static_cast<float>(chan->mRotationKeys[k].mTime),
             glm::quat(chan->mRotationKeys[k].mValue.w,
                       chan->mRotationKeys[k].mValue.x,
                       chan->mRotationKeys[k].mValue.y,
                       chan->mRotationKeys[k].mValue.z)});
      }

      ch.scale_keys.reserve(chan->mNumScalingKeys);
      for (uint32_t k = 0; k < chan->mNumScalingKeys; k++) {
        ch.scale_keys.push_back(
            {static_cast<float>(chan->mScalingKeys[k].mTime),
             glm::vec3(chan->mScalingKeys[k].mValue.x,
                       chan->mScalingKeys[k].mValue.y,
                       chan->mScalingKeys[k].mValue.z)});
      }

      clip.channels.push_back(std::move(ch));
    }

    model.animation_clips.push_back(std::move(clip));
  }

  for (auto& bone : model.skeleton.bones) {
    auto node_it = model.node_hierarchy.node_name_to_index.find(bone.name);
    if (node_it != model.node_hierarchy.node_name_to_index.end()) {
      int32_t node_idx = node_it->second;
      model.node_hierarchy.nodes[node_idx].bone_index =
          model.skeleton.bone_name_to_index[bone.name];
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

}  // namespace wiesel
