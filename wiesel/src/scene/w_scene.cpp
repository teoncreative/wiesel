//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "scene/w_scene.hpp"
#include <rendering/w_sprite.hpp>

#include "behavior/w_behavior.hpp"
#include "rendering/w_renderer.hpp"
#include "scene/w_entity.hpp"
#include "w_engine.hpp"

namespace Wiesel {

Scene::Scene() {
  current_camera_ = CreateReference<CameraData>();
}

Scene::~Scene() {}

Entity Scene::CreateEntity(const std::string& name) {
  return CreateEntityWithUUID(UUID(), name);
}

Entity Scene::CreateEntityWithUUID(UUID uuid, const std::string& name) {
  Entity entity = {registry_.create(), this};
  entity.AddComponent<IdComponent>(uuid);
  entity.AddComponent<TransformComponent>();
  entity.AddComponent<TagComponent>(name.empty() ? "Entity" : name);

  entities_[uuid] = entity;
  scene_hierarchy_.push_back(entity);
  return entity;
}

void Scene::RemoveEntity(Entity entity) {
  entities_.erase(entity.GetUUID());
  destroy_queue_.push_back(entity.handle());
  scene_hierarchy_.erase(std::ranges::remove_if(scene_hierarchy_, [&](auto& e) {
                           return e == entity;
                         }).begin());
}

void Scene::DestroyEntity(entt::entity handle) {
  registry_.destroy(handle);
}

void Scene::OnUpdate(float_t deltaTime) {
  PROFILE_ZONE_SCOPED();
  if (!first_update_) [[likely]] {
    for (const auto& entity : registry_.view<BehaviorsComponent>()) {
      auto& component = registry_.get<BehaviorsComponent>(entity);
      for (const auto& entry : component.behaviors_) {
        entry.second->OnUpdate(deltaTime);
      }
    }
  } else {
    first_update_ = false;
  }

  for (const auto& entity : registry_.view<TransformComponent>()) {
    auto& transform = registry_.get<TransformComponent>(entity);
    if (transform.is_changed) {
      UpdateMatrices(entity);
      transform.is_changed = false;
      // todo this is a bit hacky
      // set the camera as changed if transform has changed
      if (registry_.any_of<CameraComponent>(entity)) {
        auto& camera = registry_.get<CameraComponent>(entity);
        camera.pos_changed = true;
      }
    }
  }
  auto& lights = Engine::GetRenderer()->lights_uniform_data_;
  lights.direct_light_count = 0;
  lights.point_light_count = 0;
  for (const auto& entity : registry_.view<LightDirectComponent>()) {
    auto& light = registry_.get<LightDirectComponent>(entity);
    auto& transform = registry_.get<TransformComponent>(entity);
    UpdateLight(lights, light.light_data, transform);
  }
  for (const auto& entity : registry_.view<LightPointComponent>()) {
    auto& light = registry_.get<LightPointComponent>(entity);
    auto& transform = registry_.get<TransformComponent>(entity);
    UpdateLight(lights, light.light_data, transform);
  }

  for (const auto& entity :
       registry_.view<CameraComponent, TransformComponent>()) {
    auto& camera = registry_.get<CameraComponent>(entity);
    auto& transform = registry_.get<TransformComponent>(entity);
    if (!camera.enabled) {
      continue;
    }
    if (camera.view_changed) {
      camera.UpdateProjection();
      camera.view_changed = false;
    }
    if (camera.pos_changed) {
      camera.UpdateView(transform.transform_matrix);
      camera.pos_changed = false;
    }
    if (camera.any_changed) {
      camera.UpdateAll();
      camera.any_changed = false;
    }
    if (lights.direct_light_count > 0) {
      camera.ComputeCascades(glm::normalize(lights.direct_lights[0].direction));
    } else {
      camera.does_shadow_pass = false;
    }
  }
}

void Scene::OnEvent(Event& event) {
  EventDispatcher dispatcher{event};
  dispatcher.Dispatch<WindowResizeEvent>(WIESEL_BIND_FN(OnWindowResizeEvent));

  for (const auto& entity : registry_.view<BehaviorsComponent>()) {
    auto& component = registry_.get<BehaviorsComponent>(entity);
    component.OnEvent(event);
  }
}

void Scene::LinkEntities(entt::entity parent, entt::entity child) {
  entt::entity loop_entity = parent;
  while (loop_entity != entt::null) {
    if (loop_entity == child) {
      return;
    }
    if (!registry_.any_of<TreeComponent>(loop_entity)) {
      break;
    }
    auto& tree = registry_.get_or_emplace<TreeComponent>(loop_entity);
    loop_entity = tree.parent;
  }
  auto& parent_tree = registry_.get_or_emplace<TreeComponent>(parent);
  auto& child_tree = registry_.get_or_emplace<TreeComponent>(child);
  if (child_tree.parent != entt::null) {
    UnlinkEntities(child_tree.parent, child);
  }
  parent_tree.childs.push_back(child);
  child_tree.parent = parent;
  auto& child_transform = registry_.get<TransformComponent>(child);
  auto& parent_transform = registry_.get<TransformComponent>(parent);
  glm::vec3 posDiff = child_transform.position - parent_transform.position;
  glm::vec3 rotDiff = child_transform.rotation - parent_transform.rotation;

  child_transform.position = posDiff;
  child_transform.rotation = rotDiff;
  child_transform.is_changed = true;
}

void Scene::UnlinkEntities(entt::entity parent, entt::entity child) {
  auto& parent_tree = registry_.get_or_emplace<TreeComponent>(parent);
  auto& child_tree = registry_.get_or_emplace<TreeComponent>(child);
  if (child_tree.parent == entt::null) {
    return;
  }
  parent_tree.childs.erase(
      std::ranges::remove(parent_tree.childs, child).begin(),
      parent_tree.childs.end());
  child_tree.parent = entt::null;
  auto& child_transform = registry_.get<TransformComponent>(child);
  auto& parent_transform = registry_.get<TransformComponent>(parent);
  glm::vec3 pos_diff = child_transform.position + parent_transform.position;
  glm::vec3 rot_diff = child_transform.rotation + parent_transform.rotation;

  child_transform.position = pos_diff;
  child_transform.rotation = rot_diff;
  child_transform.is_changed = true;
}

void Scene::ProcessDestroyQueue() {
  PROFILE_ZONE_SCOPED();
  for (const auto& item : destroy_queue_) {
    DestroyEntity(item);
  }
  destroy_queue_.clear();
}

bool Scene::OnWindowResizeEvent(WindowResizeEvent& event) {
  /*for (const auto& entity : registry_.view<CameraComponent>()) {
    auto& component = registry_.get<CameraComponent>(entity);
    component.aspect_ratio = event.aspect_ratio();
    component.viewport_size.x = event.window_size().width;
    component.viewport_size.y = event.window_size().height;
    component.resources_dirty = true;
    component.view_changed = true;
  }
  return false;*/
  return false;
}

glm::mat4 Scene::MakeLocal(const TransformComponent& t) {
  PROFILE_ZONE_SCOPED();
  glm::vec3 rotRad = glm::radians(t.rotation);
  glm::mat4 R = glm::toMat4(glm::quat(rotRad));
  glm::mat4 T = glm::translate(glm::mat4(1.0f), t.position);
  glm::mat4 Tp = glm::translate(glm::mat4(1.0f), t.pivot);
  glm::mat4 Tn = glm::translate(glm::mat4(1.0f), -t.pivot);
  glm::mat4 S = glm::scale(glm::mat4(1.0f), t.scale);

  // move to Position, shift to Pivot, rotate+scale, shift back
  return T * Tp * R * S * Tn;
}

glm::mat4 Scene::GetWorldMatrix(entt::entity entity) {
  PROFILE_ZONE_SCOPED();
  auto& transform = registry_.get<TransformComponent>(entity);
  glm::mat4 local = MakeLocal(transform);

  if (auto* tree = registry_.try_get<TreeComponent>(entity);
      tree && tree->parent != entt::null) {
    return GetWorldMatrix(tree->parent) * local;
  }
  return local;
}

void Scene::UpdateMatrices(entt::entity entity) {
  PROFILE_ZONE_SCOPED();
  auto& tc = registry_.get<TransformComponent>(entity);
  tc.transform_matrix = GetWorldMatrix(entity);
  tc.normal_matrix = glm::inverseTranspose(glm::mat3(tc.transform_matrix));
}

void Scene::InvalidateRenderGraphs() {
  render_graphs_.clear();
}

void Scene::BuildRenderGraph(entt::entity camera_entity) {
  Ref<Renderer> renderer = Engine::GetRenderer();
  std::shared_ptr<RenderGraph>& graph = render_graphs_[camera_entity];
  graph = CreateReference<RenderGraph>(*renderer);
  RenderGraph& g = *graph;
  std::shared_ptr<CameraData> cam = current_camera_;

  // Import all existing resources (initial layout UNDEFINED - graph handles transitions)
  RGResource geo_view_pos = g.ImportTexture("GeoViewPos", cam->geometry_view_pos_resolve_image);
  RGResource geo_world_pos = g.ImportTexture("GeoWorldPos", cam->geometry_world_pos_resolve_image);
  RGResource geo_depth = g.ImportTexture("GeoDepth", cam->geometry_depth_resolve_image);
  RGResource geo_normal = g.ImportTexture("GeoNormal", cam->geometry_normal_resolve_image);
  RGResource geo_albedo = g.ImportTexture("GeoAlbedo", cam->geometry_albedo_resolve_image);
  RGResource geo_material = g.ImportTexture("GeoMaterial", cam->geometry_material_resolve_image);
  RGResource ssao_noise = g.ImportTexture("SSAONoise", renderer->ssao_noise_);
  RGResource ssao_out = g.ImportTexture("SSAOOut", cam->ssao_color_image);
  RGResource ssao_blur_h = g.ImportTexture("SSAOBlurH", cam->ssao_blur_horz_color_image);
  RGResource ssao_blur_v = g.ImportTexture("SSAOBlurV", cam->ssao_blur_vert_color_image);
  RGResource lighting_out = g.ImportTexture("LightingOut", cam->lighting_color_resolve_image);
  RGResource sprite_out = g.ImportTexture("SpriteOut", cam->sprite_color_image);
  RGResource composite_out = g.ImportTexture("CompositeOut", cam->composite_color_resolve_image);

  RGResource shadow_depth;
  if (cam->shadow_depth_stencil) {
    shadow_depth = g.ImportTexture("ShadowDepth", cam->shadow_depth_stencil);
  }

  // --- Shadow Passes ---
  for (int i = 0; i < WIESEL_SHADOW_CASCADE_COUNT; ++i) {
    uint32_t shadow = g.AddPass("Shadow " + std::to_string(i), renderer->shadow_render_pass_,
        [this, renderer, i](VkCommandBuffer) {
          if (!current_camera_->does_shadow_pass) {
            return;
          }
          memcpy(renderer->shadow_camera_uniform_buffer_->data_,
                 &renderer->shadow_camera_uniform_data_,
                 sizeof(renderer->shadow_camera_uniform_data_));
          renderer->shadow_pipeline_push_constant_->cascade_index = i;
          renderer->shadow_pipeline_->Bind(PipelineBindPointGraphics);
          for (const auto& entity : GetAllEntitiesWith<ModelComponent, TransformComponent>()) {
            auto& model = registry_.get<ModelComponent>(entity);
            auto& transform = registry_.get<TransformComponent>(entity);
            if (!model.receive_shadows || !model.enable_rendering || !model.model_handle) continue;
            renderer->DrawModel(model, transform, true);
          }
        });
    if (shadow_depth.IsValid()) {
      g.PassWritesDepth(shadow, shadow_depth);
    }
    g.SetPassFramebuffer(shadow, cam->shadow_framebuffers[i]);
    g.SetPassViewport(shadow, {WIESEL_SHADOWMAP_DIM, WIESEL_SHADOWMAP_DIM});
    g.SetPassClearColor(shadow, {0, 0, 0, 1});
  }

  // --- Geometry Pass ---
  uint32_t geo = g.AddPass("Geometry", renderer->geometry_render_pass_,
      [this, renderer](VkCommandBuffer) {
        renderer->geometry_pipeline_->Bind(PipelineBindPointGraphics);
        for (const auto& entity : GetAllEntitiesWith<ModelComponent, TransformComponent>()) {
          auto& model = registry_.get<ModelComponent>(entity);
          auto& transform = registry_.get<TransformComponent>(entity);
          if (!model.enable_rendering || !model.model_handle) continue;
          renderer->DrawModel(model, transform, false);
        }
      });
  g.PassWritesColor(geo, geo_view_pos);
  g.PassWritesColor(geo, geo_world_pos);
  g.PassWritesColor(geo, geo_depth);
  g.PassWritesColor(geo, geo_normal);
  g.PassWritesColor(geo, geo_albedo);
  g.PassWritesColor(geo, geo_material);
  g.SetPassFramebuffer(geo, cam->geometry_framebuffer);
  g.SetPassViewport(geo, cam->viewport_size);
  g.SetPassClearColor(geo, {0, 0, 0, 0});

  // --- SSAO Gen Pass ---
  uint32_t ssao_gen = g.AddPass("SSAO Gen", renderer->ssao_gen_render_pass_,
      [renderer](VkCommandBuffer) {
        renderer->GetSSAOGenPipeline()->Bind(PipelineBindPointGraphics);
        renderer->DrawFullscreen(
            renderer->GetSSAOGenPipeline(),
            {renderer->GetCameraData()->ssao_gen_descriptor,
             renderer->GetCameraData()->global_descriptor});
      });
  g.PassReadsTexture(ssao_gen, geo_view_pos);
  g.PassReadsTexture(ssao_gen, geo_normal);
  g.PassReadsTexture(ssao_gen, geo_depth);
  g.PassReadsTexture(ssao_gen, ssao_noise);
  g.PassWritesColor(ssao_gen, ssao_out);
  g.SetPassFramebuffer(ssao_gen, cam->ssao_gen_framebuffer);
  g.SetPassViewport(ssao_gen, {cam->viewport_size.x / 2, cam->viewport_size.y / 2});
  g.SetPassClearColor(ssao_gen, {0, 0, 0, 0});
  auto& settings = renderer->options();
  g.SetPassEnabled(ssao_gen, settings.ssao_enabled);

  // --- SSAO Blur Horizontal ---
  uint32_t ssao_blur_horz = g.AddPass("SSAO Blur H", renderer->ssao_blur_horz_render_pass_,
      [renderer](VkCommandBuffer) {
        renderer->GetSSAOBlurHorzPipeline()->Bind(PipelineBindPointGraphics);
        renderer->DrawFullscreen(
            renderer->GetSSAOBlurHorzPipeline(),
            {renderer->GetCameraData()->ssao_output_descriptor});
      });
  g.PassReadsTexture(ssao_blur_horz, ssao_out);
  g.PassReadsTexture(ssao_blur_horz, geo_depth);
  g.PassWritesColor(ssao_blur_horz, ssao_blur_h);
  g.SetPassFramebuffer(ssao_blur_horz, cam->ssao_blur_horz_framebuffer);
  g.SetPassViewport(ssao_blur_horz, cam->viewport_size);
  g.SetPassClearColor(ssao_blur_horz, {0, 0, 0, 0});
  g.SetPassEnabled(ssao_blur_horz, settings.ssao_enabled);

  // --- SSAO Blur Vertical ---
  uint32_t ssao_blur_vert = g.AddPass("SSAO Blur V", renderer->ssao_blur_vert_render_pass_,
      [renderer](VkCommandBuffer) {
        renderer->GetSSAOBlurVertPipeline()->Bind(PipelineBindPointGraphics);
        renderer->DrawFullscreen(
            renderer->GetSSAOBlurVertPipeline(),
            {renderer->GetCameraData()->ssao_blur_horz_output_descriptor});
      });
  g.PassReadsTexture(ssao_blur_vert, ssao_blur_h);
  g.PassReadsTexture(ssao_blur_vert, geo_depth);
  g.PassWritesColor(ssao_blur_vert, ssao_blur_v);
  g.SetPassFramebuffer(ssao_blur_vert, cam->ssao_blur_vert_framebuffer);
  g.SetPassViewport(ssao_blur_vert, cam->viewport_size);
  g.SetPassClearColor(ssao_blur_vert, {0, 0, 0, 0});
  g.SetPassEnabled(ssao_blur_vert, settings.ssao_enabled);

  // --- Lighting Pass ---
  uint32_t lighting = g.AddPass("Lighting", renderer->lighting_render_pass_,
      [this, renderer](VkCommandBuffer) {
        renderer->GetSkyboxPipeline()->Bind(PipelineBindPointGraphics);
        if (skybox_) {
          renderer->DrawSkybox(skybox_);
        }
        renderer->GetLightingPipeline()->Bind(PipelineBindPointGraphics);
        renderer->DrawFullscreen(
            renderer->GetLightingPipeline(),
            {renderer->GetCameraData()->geometry_output_descriptor,
             renderer->GetCameraData()->ssao_blur_vert_output_descriptor,
             renderer->GetCameraData()->global_descriptor});
      });
  g.PassReadsTexture(lighting, geo_view_pos);
  g.PassReadsTexture(lighting, geo_world_pos);
  g.PassReadsTexture(lighting, geo_normal);
  g.PassReadsTexture(lighting, geo_albedo);
  g.PassReadsTexture(lighting, geo_material);
  g.PassReadsTexture(lighting, ssao_blur_v);
  if (shadow_depth.IsValid()) {
    g.PassReadsTexture(lighting, shadow_depth);
  }
  g.PassWritesColor(lighting, lighting_out);
  g.SetPassFramebuffer(lighting, cam->lighting_framebuffer);
  g.SetPassViewport(lighting, cam->viewport_size);
  g.SetPassClearColor(lighting, renderer->GetClearColor());

  // --- Sprite Pass ---
  uint32_t sprite = g.AddPass("Sprite", renderer->sprite_render_pass_,
      [this, renderer](VkCommandBuffer) {
        renderer->GetSpritePipeline()->Bind(PipelineBindPointGraphics);
        for (const auto& entity : GetAllEntitiesWith<SpriteComponent, TransformComponent>()) {
          auto& spr = registry_.get<SpriteComponent>(entity);
          auto& transform = registry_.get<TransformComponent>(entity);
          renderer->DrawSprite(spr, transform);
        }
      });
  g.PassWritesColor(sprite, sprite_out);
  g.SetPassFramebuffer(sprite, cam->sprite_framebuffer);
  g.SetPassViewport(sprite, cam->viewport_size);
  g.SetPassClearColor(sprite, {0, 0, 0, 0});

  // --- Composite Pass ---
  uint32_t composite = g.AddPass("Composite", renderer->composite_render_pass_,
      [renderer](VkCommandBuffer) {
        renderer->GetCompositePipeline()->Bind(PipelineBindPointGraphics);
        if (renderer->options().only_ssao) {
          renderer->DrawFullscreen(
              renderer->GetCompositePipeline(),
              {renderer->GetCameraData()->ssao_blur_vert_output_descriptor});
        } else {
          renderer->DrawFullscreen(
              renderer->GetCompositePipeline(),
              {renderer->GetCameraData()->lighting_output_descriptor});
          renderer->DrawFullscreen(
              renderer->GetCompositePipeline(),
              {renderer->GetCameraData()->sprite_output_descriptor});
        }
      });
  g.PassReadsTexture(composite, lighting_out);
  g.PassReadsTexture(composite, sprite_out);
  g.PassWritesColor(composite, composite_out);
  g.SetPassFramebuffer(composite, cam->composite_framebuffer);
  g.SetPassViewport(composite, cam->viewport_size);
  g.SetPassClearColor(composite, renderer->GetClearColor());

  // --- Post-process resources ---
  RGResource bloom_extract_out = g.ImportTexture("BloomExtract", cam->bloom_extract_image);
  RGResource bloom_blur_h_out = g.ImportTexture("BloomBlurH", cam->bloom_blur_h_image);
  RGResource bloom_blur_v_out = g.ImportTexture("BloomBlurV", cam->bloom_blur_v_image);
  RGResource bloom_composite_out = g.ImportTexture("BloomComposite", cam->bloom_composite_image);
  RGResource motion_blur_out = g.ImportTexture("MotionBlur", cam->motion_blur_image);

  // --- Bloom Extract (half-res) ---
  uint32_t bloom_extract = g.AddPass("Bloom Extract", renderer->postprocess_render_pass_,
      [renderer](VkCommandBuffer) {
        auto& s = renderer->options();
        renderer->bloom_push_constants_->threshold = s.bloom_threshold;
        renderer->bloom_push_constants_->intensity = s.bloom_intensity;
        renderer->GetBloomExtractPipeline()->Bind(PipelineBindPointGraphics);
        renderer->DrawFullscreen(
            renderer->GetBloomExtractPipeline(),
            {renderer->GetCameraData()->bloom_extract_descriptor});
      });
  g.PassReadsTexture(bloom_extract, composite_out);
  g.PassWritesColor(bloom_extract, bloom_extract_out);
  g.SetPassFramebuffer(bloom_extract, cam->bloom_extract_framebuffer);
  g.SetPassViewport(bloom_extract, {cam->viewport_size.x / 2, cam->viewport_size.y / 2});
  g.SetPassClearColor(bloom_extract, {0, 0, 0, 0});
  g.SetPassEnabled(bloom_extract, settings.bloom_enabled);

  // --- Bloom Blur Horizontal (half-res) ---
  uint32_t bloom_blur_h = g.AddPass("Bloom Blur H", renderer->postprocess_render_pass_,
      [renderer](VkCommandBuffer) {
        renderer->GetBloomBlurHPipeline()->Bind(PipelineBindPointGraphics);
        renderer->DrawFullscreen(
            renderer->GetBloomBlurHPipeline(),
            {renderer->GetCameraData()->bloom_blur_h_descriptor});
      });
  g.PassReadsTexture(bloom_blur_h, bloom_extract_out);
  g.PassWritesColor(bloom_blur_h, bloom_blur_h_out);
  g.SetPassFramebuffer(bloom_blur_h, cam->bloom_blur_h_framebuffer);
  g.SetPassViewport(bloom_blur_h, {cam->viewport_size.x / 2, cam->viewport_size.y / 2});
  g.SetPassClearColor(bloom_blur_h, {0, 0, 0, 0});
  g.SetPassEnabled(bloom_blur_h, settings.bloom_enabled);

  // --- Bloom Blur Vertical (half-res) ---
  uint32_t bloom_blur_v = g.AddPass("Bloom Blur V", renderer->postprocess_render_pass_,
      [renderer](VkCommandBuffer) {
        renderer->GetBloomBlurVPipeline()->Bind(PipelineBindPointGraphics);
        renderer->DrawFullscreen(
            renderer->GetBloomBlurVPipeline(),
            {renderer->GetCameraData()->bloom_blur_v_descriptor});
      });
  g.PassReadsTexture(bloom_blur_v, bloom_blur_h_out);
  g.PassWritesColor(bloom_blur_v, bloom_blur_v_out);
  g.SetPassFramebuffer(bloom_blur_v, cam->bloom_blur_v_framebuffer);
  g.SetPassViewport(bloom_blur_v, {cam->viewport_size.x / 2, cam->viewport_size.y / 2});
  g.SetPassClearColor(bloom_blur_v, {0, 0, 0, 0});
  g.SetPassEnabled(bloom_blur_v, settings.bloom_enabled);

  // --- Bloom Composite (full-res) ---
  uint32_t bloom_comp = g.AddPass("Bloom Composite", renderer->postprocess_render_pass_,
      [renderer](VkCommandBuffer) {
        auto& s = renderer->options();
        renderer->bloom_push_constants_->threshold = s.bloom_threshold;
        renderer->bloom_push_constants_->intensity = s.bloom_intensity;
        renderer->GetBloomCompositePipeline()->Bind(PipelineBindPointGraphics);
        renderer->DrawFullscreen(
            renderer->GetBloomCompositePipeline(),
            {renderer->GetCameraData()->bloom_composite_descriptor});
      });
  g.PassReadsTexture(bloom_comp, composite_out);
  g.PassReadsTexture(bloom_comp, bloom_blur_v_out);
  g.PassWritesColor(bloom_comp, bloom_composite_out);
  g.SetPassFramebuffer(bloom_comp, cam->bloom_composite_framebuffer);
  g.SetPassViewport(bloom_comp, cam->viewport_size);
  g.SetPassClearColor(bloom_comp, {0, 0, 0, 0});
  g.SetPassEnabled(bloom_comp, settings.bloom_enabled);

  // --- Motion Blur (full-res) ---
  uint32_t motion_blur = g.AddPass("Motion Blur", renderer->postprocess_render_pass_,
      [renderer](VkCommandBuffer) {
        auto& s = renderer->options();
        renderer->motion_blur_push_constants_->strength = s.motion_blur_strength;
        renderer->motion_blur_push_constants_->num_samples = s.motion_blur_samples;
        auto mb_desc = s.bloom_enabled
            ? renderer->GetCameraData()->motion_blur_after_bloom_desc
            : renderer->GetCameraData()->motion_blur_after_composite_desc;
        renderer->GetMotionBlurPipeline()->Bind(PipelineBindPointGraphics);
        renderer->DrawFullscreen(
            renderer->GetMotionBlurPipeline(),
            {mb_desc, renderer->GetCameraData()->global_descriptor});
      });
  g.PassReadsTexture(motion_blur, composite_out);
  if (settings.bloom_enabled) {
    g.PassReadsTexture(motion_blur, bloom_composite_out);
  }
  g.PassReadsTexture(motion_blur, geo_world_pos);
  g.PassWritesColor(motion_blur, motion_blur_out);
  g.SetPassFramebuffer(motion_blur, cam->motion_blur_framebuffer);
  g.SetPassViewport(motion_blur, cam->viewport_size);
  g.SetPassClearColor(motion_blur, {0, 0, 0, 0});
  g.SetPassEnabled(motion_blur, settings.motion_blur_enabled);

  g.Compile();
}

bool Scene::Render() {
  PROFILE_ZONE_SCOPED();
  bool hasCamera = false;
  Ref<Renderer> renderer = Engine::GetRenderer();

  for (const auto& cameraEntity : GetAllEntitiesWith<CameraComponent>()) {
    auto& camera = registry_.get<CameraComponent>(cameraEntity);
    auto& camera_transform = registry_.get<TransformComponent>(cameraEntity);
    if (!camera.enabled) continue;

    if (camera.resources_dirty) {
      vkDeviceWaitIdle(renderer->GetLogicalDevice());
      renderer->SetupCameraComponent(camera);
      render_graphs_.erase(cameraEntity);
    }

    current_camera_->TransferFrom(camera, camera_transform);
    renderer->SetCameraData(current_camera_);
    renderer->UpdateUniformData();

    // Rebuild render graph each frame to pick up settings changes
    BuildRenderGraph(cameraEntity);
    render_graphs_[cameraEntity]->Execute(renderer->GetCommandBuffer().handle_);

    // Store current VP for next frame's motion blur
    camera.prev_view_projection = camera.projection * camera.view_matrix;

    hasCamera = true;
  }
  return hasCamera;
}

}  // namespace Wiesel