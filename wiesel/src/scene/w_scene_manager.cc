//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

//
// Created by Metehan Gezer on 05.03.2026.
//

#include "scene/w_scene_manager.h"

#include <unordered_set>

#include "asset/w_asset_manager.h"
#include "rendering/features/w_billboard_feature.h"
#include "rendering/features/w_bloom_feature.h"
#include "rendering/features/w_canvas_feature.h"
#include "rendering/features/w_composite_feature.h"
#include "rendering/features/w_debug_collider_feature.h"
#include "rendering/features/w_selection_outline_feature.h"
#include "rendering/features/w_fxaa_feature.h"
#include "rendering/features/w_geometry_feature.h"
#include "rendering/features/w_grid_feature.h"
#include "rendering/features/w_ibl_feature.h"
#include "rendering/features/w_lighting_feature.h"
#include "rendering/features/w_motion_blur_feature.h"
#include "rendering/features/w_rt_shadow_feature.h"
#include "rendering/features/w_shadow_feature.h"
#include "rendering/features/w_sprite_feature.h"
#include "rendering/features/w_ssao_feature.h"
#include "rendering/features/w_taa_feature.h"
#include "rendering/features/w_transparency_feature.h"
#include "rendering/w_renderer.h"
#include "scene/w_component_serializer.h"
#include "scene/w_lights.h"
#include "scene/w_scene_serializer.h"
#include "systems/w_canvas_system.h"
#include "util/w_logger.h"
#include "w_engine.h"

namespace Wiesel {

std::shared_ptr<Scene> SceneManager::CreateScene() {
  active_scene_ = std::make_shared<Scene>();
  loaded_scenes_.clear();
  loaded_scenes_.push_back(active_scene_);
  return active_scene_;
}

std::shared_ptr<Scene> SceneManager::FindScene(const std::string& name) const {
  for (auto& scene : loaded_scenes_) {
    if (scene->GetName() == name) {
      return scene;
    }
  }
  return nullptr;
}

std::shared_ptr<Scene> SceneManager::FindSceneByPtr(Scene* raw) const {
  for (auto& scene : loaded_scenes_) {
    if (scene.get() == raw) {
      return scene;
    }
  }
  return nullptr;
}

void SceneManager::RegisterScene(const std::string& name,
                                 const std::string& vfs_path) {
  registered_scenes_[name] = vfs_path;
  LOG_INFO("Registered scene '{}' at {}", name, vfs_path);
}

void SceneManager::UnregisterScene(const std::string& name) {
  registered_scenes_.erase(name);
}

std::string SceneManager::DeriveNameFromPath(const std::string& vfs_path) {
  std::string name = vfs_path;
  auto slash = name.find_last_of('/');
  if (slash != std::string::npos) {
    name = name.substr(slash + 1);
  }
  auto dot = name.find_last_of('.');
  if (dot != std::string::npos) {
    name = name.substr(0, dot);
  }
  return name;
}

// Synchronous loading
std::shared_ptr<Scene> SceneManager::LoadScene(const std::string& name,
                                               LoadSceneMode mode) {
  auto it = registered_scenes_.find(name);
  if (it == registered_scenes_.end()) {
    LOG_ERROR("Scene '{}' not registered", name);
    return nullptr;
  }
  return LoadSceneFromPath(it->second, mode);
}

std::shared_ptr<Scene> SceneManager::LoadSceneFromPath(
    const std::string& vfs_path, LoadSceneMode mode) {
  if (mode == LoadSceneMode::Single) {
    if (ReplacePrimaryScene(vfs_path)) {
      return active_scene_;
    }
    return nullptr;
  }
  return LoadAdditiveFromPath(vfs_path, DeriveNameFromPath(vfs_path));
}

// Async loading (queued for next BeginFrame)
void SceneManager::LoadSceneAsync(const std::string& name, LoadSceneMode mode) {
  auto it = registered_scenes_.find(name);
  if (it == registered_scenes_.end()) {
    LOG_ERROR("Scene '{}' not registered", name);
    return;
  }
  pending_async_loads_.push_back({it->second, name, mode});
  LOG_INFO("Queued async scene load: '{}' ({})", name,
           mode == LoadSceneMode::Single ? "single" : "additive");
}

void SceneManager::LoadSceneAsyncFromPath(const std::string& vfs_path,
                                          LoadSceneMode mode) {
  pending_async_loads_.push_back(
      {vfs_path, DeriveNameFromPath(vfs_path), mode});
  LOG_INFO("Queued async scene load from path: {} ({})", vfs_path,
           mode == LoadSceneMode::Single ? "single" : "additive");
}

void SceneManager::UnloadScene(std::shared_ptr<Scene> scene) {
  if (!scene) {
    return;
  }
  if (scene == active_scene_) {
    LOG_WARN(
        "Cannot unload the active scene via UnloadScene, use LoadScene to "
        "replace it");
    return;
  }
  pending_unloads_.push_back(scene);
  LOG_INFO("Queued scene unload: '{}'", scene->GetName());
}

void SceneManager::UnloadScene(const std::string& name) {
  auto scene = FindScene(name);
  if (!scene) {
    LOG_ERROR("Scene '{}' not found in loaded scenes", name);
    return;
  }
  UnloadScene(scene);
}

bool SceneManager::ReplacePrimaryScene(const std::string& vfs_path) {
  auto scene = active_scene_;
  if (!scene) {
    return false;
  }

  // Unload all additive scenes before replacing the primary scene
  UnloadAllAdditiveScenes();

  // Save old scene's asset list and keep-loaded flag before clearing
  std::vector<AssetHandle> old_assets = scene->GetRequestedAssets();
  bool old_keep_loaded = scene->GetKeepAssetsLoaded();

  // Clear the current scene
  std::vector<entt::entity>& hierarchy = scene->GetSceneHierarchy();
  std::vector<entt::entity> to_remove(hierarchy.begin(), hierarchy.end());
  for (entt::entity entity_id : to_remove) {
    Entity entity{entity_id, scene.get()};
    scene->RemoveEntity(entity);
  }
  scene->ProcessDestroyQueue();
  scene->ResetPhysicsWorld();
  scene->ClearRequestedAssets();
  default_pipeline_ = nullptr;
  external_render_graph_ = nullptr;

  // Load the new scene via VFS
  VfsFile file = Engine::vfs()->Open(vfs_path);
  if (!file) {
    LOG_ERROR("Failed to open scene: {}", vfs_path);
    return false;
  }
  std::string content((std::istreambuf_iterator<char>(file.Stream())),
                      std::istreambuf_iterator<char>());
  SceneSerializer serializer(scene);
  if (!serializer.DeserializeFromString(content)) {
    LOG_ERROR("Failed to load scene: {}", vfs_path);
    return false;
  }

  scene->SetSourcePath(vfs_path);

  // Unload assets the old scene used but the new scene doesn't,
  // unless the old scene had keep_assets_loaded set
  if (!old_keep_loaded) {
    UnloadUnusedAssets(old_assets, scene->GetRequestedAssets());
  }

  // Setup cameras
  for (entt::entity entity : scene->GetAllEntitiesWith<CameraComponent>()) {
    auto& cam = scene->GetComponent<CameraComponent>(entity);
    Engine::renderer()->SetupCameraComponent(cam);
  }

  scene->ResetFirstUpdate();
  LOG_INFO("Scene loaded: {}", vfs_path);
  return true;
}

std::shared_ptr<Scene> SceneManager::LoadAdditiveFromPath(
    const std::string& vfs_path, const std::string& name) {
  auto new_scene = std::make_shared<Scene>();
  new_scene->SetSourcePath(vfs_path);
  new_scene->SetName(name);

  VfsFile file = Engine::vfs()->Open(vfs_path);
  if (!file) {
    LOG_ERROR("Failed to open additive scene: {}", vfs_path);
    return nullptr;
  }
  std::string content((std::istreambuf_iterator<char>(file.Stream())),
                      std::istreambuf_iterator<char>());
  SceneSerializer serializer(new_scene);
  if (!serializer.DeserializeFromString(content)) {
    LOG_ERROR("Failed to load additive scene: {}", vfs_path);
    return nullptr;
  }

  // Setup cameras
  for (entt::entity entity : new_scene->GetAllEntitiesWith<CameraComponent>()) {
    auto& cam = new_scene->GetComponent<CameraComponent>(entity);
    Engine::renderer()->SetupCameraComponent(cam);
  }

  new_scene->ResetFirstUpdate();
  loaded_scenes_.push_back(new_scene);
  LOG_INFO("Additive scene loaded: '{}' ({})", name, vfs_path);
  return new_scene;
}

bool SceneManager::BeginFrame() {
  if (!active_scene_) {
    return false;
  }

  // Poll async asset loading progress via target scene's own tracking
  if (target_scene_ && !scene_ready_) {
    load_progress_ = target_scene_->GetAssetLoadProgress();
    if (target_scene_->AreAssetsReady()) {
      scene_ready_ = true;
      target_scene_.reset();
      LOG_INFO("Target scene assets pre-loaded");
    }
  }

  // Auto-activate target scene when pre-loading is done
  if (auto_activate_ && scene_ready_ && !target_scene_path_.empty()) {
    ActivateLoadedScene();
  }

  ProcessPendingAsyncLoads();
  return false;
}

void SceneManager::LoadSceneWithLoading(const std::string& target_scene,
                                        const std::string& loading_scene) {
  std::map<std::string, std::string>::iterator target_it =
      registered_scenes_.find(target_scene);
  std::map<std::string, std::string>::iterator loading_it =
      registered_scenes_.find(loading_scene);
  if (target_it == registered_scenes_.end()) {
    LOG_ERROR("Target scene '{}' not registered", target_scene);
    return;
  }
  if (loading_it == registered_scenes_.end()) {
    LOG_ERROR("Loading scene '{}' not registered", loading_scene);
    return;
  }

  target_scene_path_ = target_it->second;
  load_progress_ = 0.0f;
  scene_ready_ = false;
  auto_activate_ = true;

  // Switch to loading scene immediately (async single)
  pending_async_loads_.push_back(
      {loading_it->second, loading_scene, LoadSceneMode::Single});
  LOG_INFO("Loading via intermediate scene: {} -> {}", loading_it->second,
           target_it->second);

  // Deserialize the target scene into a temporary scene to discover and
  // kick off async loads for all asset dependencies. RequestAsset() is
  // called during deserialization for every asset handle, so we don't
  // need to manually scan the JSON for specific component types.
  target_scene_ = std::make_shared<Scene>();
  VfsFile target_file = Engine::vfs()->Open(target_scene_path_);
  std::string target_content;
  if (target_file) {
    target_content =
        std::string((std::istreambuf_iterator<char>(target_file.Stream())),
                    std::istreambuf_iterator<char>());
  }
  SceneSerializer serializer(target_scene_);
  if (target_content.empty() ||
      !serializer.DeserializeFromString(target_content)) {
    LOG_ERROR("Failed to pre-parse target scene: {}", target_scene_path_);
    target_scene_.reset();
    load_progress_ = 1.0f;
    scene_ready_ = true;
    return;
  }

  if (target_scene_->AreAssetsReady()) {
    // All assets already loaded (or no async assets)
    target_scene_.reset();
    load_progress_ = 1.0f;
    scene_ready_ = true;
    return;
  }

  LOG_INFO("Pre-loading assets for target scene (progress: {:.0f}%)",
           target_scene_->GetAssetLoadProgress() * 100.0f);
}

void SceneManager::ActivateLoadedScene() {
  if (!scene_ready_ || target_scene_path_.empty()) {
    LOG_WARN("No scene ready to activate");
    return;
  }

  pending_async_loads_.push_back({target_scene_path_,
                                  DeriveNameFromPath(target_scene_path_),
                                  LoadSceneMode::Single});
  target_scene_path_.clear();
  load_progress_ = 0.0f;
  scene_ready_ = false;
  auto_activate_ = false;
  LOG_INFO("Activating loaded scene");
}

void SceneManager::EndFrame() {
  for (auto& scene : loaded_scenes_) {
    scene->ProcessDestroyQueue();
  }
  ProcessPendingUnloads();
}

void SceneManager::Cleanup() {
  for (auto& scene : loaded_scenes_) {
    scene->Cleanup();
  }
  loaded_scenes_.clear();
  active_scene_.reset();
  default_pipeline_ = nullptr;
  external_render_graph_ = nullptr;
}

void SceneManager::AccumulateLights() {
  auto& lights = Engine::renderer()->lights_uniform_data_;
  lights.direct_light_count = 0;
  lights.point_light_count = 0;
  for (auto& scene : loaded_scenes_) {
    for (auto entity :
         scene
             ->GetAllEntitiesWith<LightDirectComponent, TransformComponent>()) {
      auto& light = scene->GetComponent<LightDirectComponent>(entity);
      auto& transform = scene->GetComponent<TransformComponent>(entity);
      UpdateLight(lights, light.light_data, transform);
    }
    for (auto entity :
         scene->GetAllEntitiesWith<LightPointComponent, TransformComponent>()) {
      auto& light = scene->GetComponent<LightPointComponent>(entity);
      auto& transform = scene->GetComponent<TransformComponent>(entity);
      UpdateLight(lights, light.light_data, transform);
    }
  }
}

void SceneManager::EnsureDefaultResources() {
  if (!active_scene_) {
    return;
  }
  active_scene_->EnsureDefaultSkybox();
  auto renderer = Engine::renderer();
  if (!default_pipeline_) {
    CreateDefaultPipeline();
  }
  if (renderer->NeedsRecreateResources()) {
    renderer->ClearRecreateResources();
    CreateDefaultPipeline();
  }
}

void SceneManager::CreateDefaultPipeline() {
  auto renderer = Engine::renderer();
  ++pipeline_version_;
  external_render_graph_ = nullptr;
  default_pipeline_ = std::make_shared<RenderPipeline>(renderer);
  default_pipeline_->AddFeature<ShadowFeature>(renderer);
  default_pipeline_->AddFeature<GeometryFeature>(renderer);
  if (renderer->IsRayTracingSupported()) {
    default_pipeline_->AddFeature<RTShadowFeature>(renderer);
  }
  default_pipeline_->AddFeature<SSAOFeature>(renderer);
  default_pipeline_->AddFeature<IBLFeature>(renderer);
  default_pipeline_->AddFeature<LightingFeature>(renderer);
  default_pipeline_->AddFeature<TransparencyFeature>(renderer);
  default_pipeline_->AddFeature<GridFeature>(renderer);
  default_pipeline_->AddFeature<SpriteFeature>(renderer);
  default_pipeline_->AddFeature<CompositeFeature>(renderer);
  default_pipeline_->AddFeature<SelectionOutlineFeature>(renderer);
  default_pipeline_->AddFeature<TAAFeature>(renderer);
  default_pipeline_->AddFeature<BloomFeature>(renderer);
  default_pipeline_->AddFeature<MotionBlurFeature>(renderer);
  default_pipeline_->AddFeature<FXAAFeature>(renderer);
  default_pipeline_->AddFeature<CanvasFeature>(renderer);
  default_pipeline_->AddFeature<DebugColliderFeature>(renderer);
  default_pipeline_->AddFeature<BillboardFeature>(renderer);
}

bool SceneManager::RenderGameView() {
  if (loaded_scenes_.empty()) {
    return false;
  }

  PROFILE_ZONE_SCOPED_N("SceneManager::RenderGameView");
  auto renderer = Engine::renderer();
  EnsureDefaultResources();
  AccumulateLights();
  auto& default_pipeline = *default_pipeline_;
  bool has_camera = false;

  // Find the first enabled camera's viewport size for canvas layout
  glm::vec2 canvas_viewport = {0, 0};
  for (auto& scene : loaded_scenes_) {
    for (entt::entity e : scene->GetAllEntitiesWith<CameraComponent>()) {
      auto& cam = scene->GetComponent<CameraComponent>(e);
      if (cam.enabled) {
        canvas_viewport = cam.viewport_size;
        break;
      }
    }
    if (canvas_viewport.x > 0) {
      break;
    }
  }

  // Compute canvas layout for ALL scenes using the camera viewport
  if (canvas_viewport.x > 0) {
    for (auto& scene : loaded_scenes_) {
      CanvasSystem canvas_system;
      canvas_system.Update(*scene, canvas_viewport);
    }
  }

  // Render each camera across all loaded scenes
  for (auto& scene : loaded_scenes_) {
    for (entt::entity camera_entity :
         scene->GetAllEntitiesWith<CameraComponent, TransformComponent>()) {
      auto& camera = scene->GetComponent<CameraComponent>(camera_entity);
      auto& camera_transform =
          scene->GetComponent<TransformComponent>(camera_entity);
      if (!camera.enabled) {
        continue;
      }

      // Apply scene render resolution if set
      glm::vec2 res = scene->GetRenderResolution();
      if (res.x > 0 && res.y > 0) {
        if (res.x != camera.viewport_size.x ||
            res.y != camera.viewport_size.y) {
          camera.viewport_size = res;
          camera.aspect_ratio = res.x / res.y;
          camera.view_changed = true;
        }
      }

      // Rebuild resources if pipeline version or viewport changed
      if (camera.resource_pipeline_version != pipeline_version_ ||
          camera.resource_viewport_size != camera.viewport_size) {
        bool use_resolve =
            renderer->options().msaa_mode > SamplingMode::DISABLED;
        RenderContext ctx{*renderer,
                          multi_scene_,
                          camera,
                          camera.resource_pool,
                          camera.viewport_size,
                          use_resolve,
                          false,
                          false,
                          camera_entity};
        auto& pipeline =
            camera.render_pipeline ? *camera.render_pipeline : default_pipeline;
        pipeline.SetupResources(ctx);
        camera.resource_pipeline_version = pipeline_version_;
        camera.resource_viewport_size = camera.viewport_size;
        camera.render_graph = nullptr;
      }

      scene->GetCurrentCamera()->TransferFrom(camera, camera_transform);
      renderer->SetCameraData(scene->GetCurrentCamera());
      renderer->UpdateUniformData();

      // Build render graph with all scenes visible to this camera
      if (!camera.render_graph) {
        camera.render_graph = std::make_shared<RenderGraph>(*renderer);
      } else {
        camera.render_graph->Clear();
      }
      {
        bool use_resolve =
            renderer->options().msaa_mode > SamplingMode::DISABLED;
        RenderContext graph_ctx{*renderer,
                                multi_scene_,
                                camera,
                                camera.resource_pool,
                                camera.viewport_size,
                                use_resolve,
                                false,
                                false,
                                camera_entity};
        auto& pipeline =
            camera.render_pipeline ? *camera.render_pipeline : default_pipeline;
        pipeline.BuildRenderGraph(*camera.render_graph, graph_ctx);
        camera.render_graph->Compile();
      }
      camera.render_graph->Execute(renderer->GetCommandBuffer().handle_);

      camera.prev_view_projection = camera.projection * camera.view_matrix;
      has_camera = true;
    }
  }
  return has_camera;
}

bool SceneManager::RenderEditorView(CameraComponent& camera,
                                    TransformComponent& transform,
                                    bool show_grid) {
  if (loaded_scenes_.empty()) {
    return false;
  }
  if (camera.viewport_size.x <= 0 || camera.viewport_size.y <= 0) {
    return false;
  }

  PROFILE_ZONE_SCOPED_N("SceneManager::RenderEditorView");
  auto renderer = Engine::renderer();
  EnsureDefaultResources();
  AccumulateLights();
  auto& default_pipeline = *default_pipeline_;

  // If the pipeline was recreated, the editor camera's resource pool
  // Compute canvas layout for all scenes
  for (auto& scene : loaded_scenes_) {
    CanvasSystem canvas_system;
    canvas_system.Update(*scene, camera.viewport_size);
  }

  // Compute transform matrix (no entity hierarchy for external camera)
  glm::vec3 rot_rad = glm::radians(transform.GetRotation());
  glm::mat4 R = glm::toMat4(glm::quat(rot_rad));
  glm::mat4 T = glm::translate(glm::mat4(1.0f), transform.GetPosition());
  glm::mat4 S = glm::scale(glm::mat4(1.0f), transform.GetScale());
  transform.SetTransformMatrix(T * R * S);

  if (camera.view_changed) {
    camera.UpdateProjection();
    camera.view_changed = false;
  }
  camera.UpdateView(transform.GetTransformMatrix());
  camera.UpdateAll();

  auto& lights = renderer->lights_uniform_data_;
  if (lights.direct_light_count > 0 && renderer->options().shadows_enabled) {
    camera.ComputeCascades(-glm::normalize(lights.direct_lights[0].direction));
  } else {
    camera.does_shadow_pass = false;
  }

  // Rebuild resources if pipeline version or viewport changed
  if (camera.resource_pipeline_version != pipeline_version_ ||
      camera.resource_viewport_size != camera.viewport_size) {
    bool use_resolve = renderer->options().msaa_mode > SamplingMode::DISABLED;
    RenderContext ctx{*renderer,
                      multi_scene_,
                      camera,
                      camera.resource_pool,
                      camera.viewport_size,
                      use_resolve,
                      true,
                      show_grid};
    auto& pipeline =
        camera.render_pipeline ? *camera.render_pipeline : default_pipeline;
    pipeline.SetupResources(ctx);
    camera.resource_pipeline_version = pipeline_version_;
    camera.resource_viewport_size = camera.viewport_size;
    external_render_graph_ = nullptr;
  }

  active_scene_->GetCurrentCamera()->TransferFrom(camera, transform);
  renderer->SetCameraData(active_scene_->GetCurrentCamera());
  renderer->UpdateUniformData();

  if (!external_render_graph_) {
    external_render_graph_ = std::make_shared<RenderGraph>(*renderer);
  } else {
    external_render_graph_->Clear();
  }
  bool use_resolve = renderer->options().msaa_mode > SamplingMode::DISABLED;
  RenderContext ctx{*renderer,
                    multi_scene_,
                    camera,
                    camera.resource_pool,
                    camera.viewport_size,
                    use_resolve,
                    true,
                    show_grid};
  auto& pipeline =
      camera.render_pipeline ? *camera.render_pipeline : default_pipeline;
  pipeline.BuildRenderGraph(*external_render_graph_, ctx);
  external_render_graph_->Compile();
  external_render_graph_->Execute(renderer->GetCommandBuffer().handle_);

  camera.prev_view_projection = camera.projection * camera.view_matrix;
  return true;
}

void SceneManager::OnUpdate(float_t delta_time) {
  for (auto& loaded_scene : loaded_scenes_) {
    loaded_scene->OnUpdate(delta_time);
  }
}

void SceneManager::OnUpdateEditor(float_t delta_time) {
  for (auto& loaded_scene : loaded_scenes_) {
    loaded_scene->OnUpdateEditor(delta_time);
  }
}

void SceneManager::UnloadUnusedAssets(
    const std::vector<AssetHandle>& old_assets,
    const std::vector<AssetHandle>& new_assets) {
  int unloaded = 0;
  for (const auto& old_handle : old_assets) {
    // Check if new scene still needs this asset
    bool still_needed = false;
    for (const auto& new_handle : new_assets) {
      if (old_handle == new_handle) {
        still_needed = true;
        break;
      }
    }
    if (still_needed) {
      continue;
    }

    // Only unload if actually loaded
    auto state = Engine::asset_manager().GetLoadState(old_handle);
    if (state == AssetLoadState::Loaded) {
      Engine::asset_manager().Unload(old_handle);
      unloaded++;
    }
  }
  if (unloaded > 0) {
    LOG_INFO("Unloaded {} unused assets from previous scene", unloaded);
  }
}

void SceneManager::UnloadUnusedAssetsForScene(
    const std::vector<AssetHandle>& scene_assets) {
  // Collect all assets still needed by remaining loaded scenes
  std::unordered_set<UUID> still_needed;
  for (auto& loaded : loaded_scenes_) {
    for (auto& handle : loaded->GetRequestedAssets()) {
      still_needed.insert(handle.id);
    }
  }

  int unloaded = 0;
  for (const auto& handle : scene_assets) {
    if (still_needed.contains(handle.id)) {
      continue;
    }
    auto state = Engine::asset_manager().GetLoadState(handle);
    if (state == AssetLoadState::Loaded) {
      Engine::asset_manager().Unload(handle);
      unloaded++;
    }
  }
  if (unloaded > 0) {
    LOG_INFO("Unloaded {} unused assets from removed scene", unloaded);
  }
}

void SceneManager::UnloadAllAdditiveScenes() {
  if (loaded_scenes_.size() <= 1) {
    return;
  }

  // Collect all assets from additive scenes for cleanup
  std::vector<AssetHandle> additive_assets;
  for (auto& scene : loaded_scenes_) {
    if (scene == active_scene_) {
      continue;
    }
    for (auto& handle : scene->GetRequestedAssets()) {
      additive_assets.push_back(handle);
    }
    scene->Cleanup();
  }

  // Remove all but the active scene
  loaded_scenes_.clear();
  loaded_scenes_.push_back(active_scene_);
  pending_unloads_.clear();

  LOG_INFO("Unloaded all additive scenes");
}

void SceneManager::ProcessPendingAsyncLoads() {
  if (pending_async_loads_.empty()) {
    return;
  }

  std::vector<PendingAsyncLoad> loads;
  loads.swap(pending_async_loads_);

  for (const auto& load : loads) {
    if (load.mode == LoadSceneMode::Single) {
      ReplacePrimaryScene(load.vfs_path);
    } else {
      LoadAdditiveFromPath(load.vfs_path, load.name);
    }
  }
}

void SceneManager::ProcessPendingUnloads() {
  if (pending_unloads_.empty()) {
    return;
  }

  std::vector<std::shared_ptr<Scene>> to_unload;
  to_unload.swap(pending_unloads_);

  for (auto& scene : to_unload) {
    // Collect assets from this scene before cleanup
    std::vector<AssetHandle> scene_assets = scene->GetRequestedAssets();
    bool keep_loaded = scene->GetKeepAssetsLoaded();

    scene->Cleanup();

    // Remove from loaded_scenes_
    std::erase(loaded_scenes_, scene);

    // Unload assets exclusive to the removed scene
    if (!keep_loaded) {
      UnloadUnusedAssetsForScene(scene_assets);
    }

    LOG_INFO("Unloaded scene: '{}'", scene->GetName());
  }
}

Entity SceneManager::MoveEntityToScene(Entity entity,
                                       std::shared_ptr<Scene> target_scene,
                                       bool move_children) {
  Scene* source_scene = entity.GetScene();
  if (!source_scene) {
    LOG_ERROR("MoveEntityToScene: entity has no source scene");
    return {entt::null, nullptr};
  }
  if (source_scene == target_scene.get()) {
    LOG_WARN("MoveEntityToScene: entity is already in the target scene");
    return entity;
  }

  // Collect entity subtree in depth-first order
  struct EntityData {
    UUID uuid;
    std::string name;
    std::vector<std::string> tags;
    UUID parent_uuid;
    nlohmann::json components;
  };

  std::vector<EntityData> entity_list;
  std::function<void(entt::entity, UUID)> collect;
  collect = [&](entt::entity handle, UUID parent_uuid) {
    EntityData data;
    data.uuid = source_scene->GetComponent<IdComponent>(handle).Id;
    auto& tag_comp = source_scene->GetComponent<TagComponent>(handle);
    data.name = tag_comp.name;
    data.tags = tag_comp.tags;
    data.parent_uuid = parent_uuid;

    Entity ent{handle, source_scene};
    ComponentSerializerRegistry::SerializeAll(ent, data.components);

    entity_list.push_back(std::move(data));

    if (move_children && source_scene->HasComponent<TreeComponent>(handle)) {
      auto& tree = source_scene->GetComponent<TreeComponent>(handle);
      for (auto child : tree.childs) {
        collect(child, entity_list.back().uuid);
      }
    }
  };

  UUID root_uuid = entity.GetUUID();
  collect(entity.handle(), UUID());

  // Compute world-space transform for the root entity
  glm::vec3 world_position = {};
  glm::vec3 world_rotation = {};
  glm::vec3 world_scale = {1.0f, 1.0f, 1.0f};
  {
    auto& transform =
        source_scene->GetComponent<TransformComponent>(entity.handle());
    world_position = transform.GetPosition();
    world_rotation = transform.GetRotation();
    world_scale = transform.GetScale();

    // Walk up the parent chain to get world-space values
    entt::entity current = entity.handle();
    while (source_scene->HasComponent<TreeComponent>(current)) {
      auto& tree = source_scene->GetComponent<TreeComponent>(current);
      if (tree.parent == entt::null) {
        break;
      }
      auto& parent_t =
          source_scene->GetComponent<TransformComponent>(tree.parent);
      world_position += parent_t.GetPosition();
      world_rotation += parent_t.GetRotation();
      world_scale *= parent_t.GetScale();
      current = tree.parent;
    }
  }

  // Remove entities from source scene
  {
    entt::entity root_handle = entity.handle();
    // Unlink root from its parent in the source scene if any
    if (source_scene->HasComponent<TreeComponent>(root_handle)) {
      auto& tree = source_scene->GetComponent<TreeComponent>(root_handle);
      if (tree.parent != entt::null) {
        source_scene->UnlinkEntities(tree.parent, root_handle);
      }
    }

    // RemoveEntity queues the root + all children recursively, then
    // ProcessDestroyQueue handles physics body destruction, UUID map
    // removal, hierarchy removal, and registry destruction.
    source_scene->RemoveEntity(Entity{root_handle, source_scene});
    source_scene->ProcessDestroyQueue();
  }

  // Recreate entities in target scene
  Entity new_root{entt::null, target_scene.get()};
  std::unordered_map<UUID, entt::entity> uuid_to_new_handle;

  for (size_t i = 0; i < entity_list.size(); ++i) {
    auto& data = entity_list[i];
    Entity new_entity =
        target_scene->CreateEntityWithUUID(data.uuid, data.name);
    uuid_to_new_handle[data.uuid] = new_entity.handle();

    // Restore tags
    auto& tag_comp = new_entity.GetComponent<TagComponent>();
    tag_comp.tags = data.tags;

    // Deserialize components
    ComponentSerializerRegistry::DeserializeAll(new_entity, data.components,
                                                target_scene.get());

    // Set world-space transform for root
    if (i == 0) {
      auto& transform = new_entity.GetComponent<TransformComponent>();
      transform.SetPosition(world_position);
      transform.SetRotation(world_rotation);
      transform.SetScale(world_scale);
      new_root = new_entity;
    }

    // Restore parent-child link
    if (!data.parent_uuid.IsNil()) {
      auto parent_it = uuid_to_new_handle.find(data.parent_uuid);
      if (parent_it != uuid_to_new_handle.end()) {
        target_scene->LinkEntities(parent_it->second, new_entity.handle(),
                                   false);
      }
    }
  }

  return new_root;
}

}  // namespace Wiesel