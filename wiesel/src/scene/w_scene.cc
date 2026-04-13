//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "scene/w_scene.h"

#include <rendering/w_sprite.h>
#include <rendering/w_sprite_asset.h>
#include <nlohmann/json.hpp>
#include <ranges>
#include "input/w_input.h"

#include <RmlUi/Core.h>
#include "ai/w_agent_controller.h"
#include "animation/w_animation.h"
#include "animation/w_animation_controller.h"
#include "animation/w_animator.h"
#include "asset/w_asset_manager.h"
#include "asset/w_asset_registry.h"
#include "audio/w_audio.h"
#include "behavior/w_behavior.h"
#include "cursor/w_cursor.h"
#include "events/w_keyevents.h"
#include "events/w_mouseevents.h"
#include "rendering/features/w_canvas_feature.h"
#include "rendering/w_camera.h"
#include "rendering/w_mesh_renderer.h"
#include "rendering/w_render_feature.h"
#include "rendering/w_renderer.h"
#include "scene/w_entity.h"
#include "scene/w_lights.h"
#include "script/mono/w_monobehavior.h"
#include "systems/w_agent_system.h"
#include "systems/w_animation_system.h"
#include "systems/w_audio_listener_system.h"
#include "systems/w_audio_source_system.h"
#include "systems/w_behavior_system.h"
#include "systems/w_camera_system.h"
#include "systems/w_canvas_system.h"
#include "systems/w_light_system.h"
#include "systems/w_physics_system.h"
#include "systems/w_skinned_mesh_system.h"
#include "systems/w_text_input_system.h"
#include "systems/w_transform_system.h"
#include "systems/w_ui_document_system.h"
#include "ui/w_ui_document.h"
#include "ui/w_ui_manager.h"
#include "util/w_keycodes.h"
#include "w_engine.h"

namespace Wiesel {
class PipelineRecreatedEvent;

Scene::Scene() {
  current_camera_ = std::make_shared<CameraData>();
  physics_world_ = std::make_unique<PhysicsWorld>(this);

  // Register built-in systems
  AddSystem<AudioListenerSystem>();
  AddSystem<PhysicsBodySystem>();
  AddSystem<BehaviorSystem>();
  AddSystem<AgentSystem>();
  AddSystem<TextInputSystem>();
  AddSystem<AudioSourceSystem>();
  AddSystem<PhysicsSimulationSystem>();
  AddSystem<TransformSystem>();
  AddSystem<LightSystem>();
  AddSystem<SkinnedMeshSystem>();
  AddSystem<AnimationSystem>();
  AddSystem<CameraSystem>();
  AddSystem<UIDocumentSystem>();
}

Scene::~Scene() {
  LOG_DEBUG("~Scene destructor");
  Cleanup();
}

std::shared_ptr<Skybox> Scene::GetSkybox() {
  if (skybox_) {
    return skybox_;
  }
  return default_skybox_;
}

void Scene::SetCursorSetAsset(AssetHandle handle) {
  cursor_set_asset_ = handle;
  Engine::cursor_manager().SetCursorSet(handle);
}

void Scene::SetSkyboxAsset(AssetHandle handle) {
  skybox_asset_ = handle;
  skybox_ = nullptr;

  Engine::renderer()->SetRecreateResources(true);
  if (!handle.IsValid()) {
    return;
  }

  auto& mgr = Engine::asset_manager();
  const auto* meta = mgr.GetMetadata(handle);
  if (!meta || meta->type != AssetType::Skybox) {
    return;
  }

  VfsFile file = Engine::vfs()->Open(meta->virtual_source_path);
  if (!file) {
    return;
  }

  try {
    std::string content((std::istreambuf_iterator<char>(file.Stream())),
                        std::istreambuf_iterator<char>());
    auto j = nlohmann::json::parse(content);

    std::string type = j.value("type", "");
    auto renderer = Engine::renderer();
    std::shared_ptr<Texture> tex;

    if (type == "panorama") {
      std::string source = j.value("source", "");
      if (!source.empty()) {
        tex = renderer->CreateCubemapTextureFromSingle(source, {}, {});
      }
    } else if (type == "cubemap") {
      if (j.contains("faces") && j["faces"].is_object()) {
        auto& f = j["faces"];
        std::array<std::string, 6> paths = {
            f.value("right", ""),  f.value("left", ""),  f.value("top", ""),
            f.value("bottom", ""), f.value("front", ""), f.value("back", ""),
        };
        tex = renderer->CreateCubemapTexture(paths, {}, {});
      }
    } else if (type == "cross") {
      std::string source = j.value("source", "");
      if (!source.empty()) {
        tex = renderer->CreateCubemapTextureFromSingle(source, {}, {});
      }
    }

    if (tex) {
      skybox_ = std::make_shared<Skybox>(tex);
      LOG_INFO("Loaded skybox asset: {}", meta->name);
    }
  } catch (const std::exception& e) {
    LOG_ERROR("Failed to load skybox asset: {}", e.what());
  }
}

void Scene::EnsureDefaultSkybox() {
  if (default_skybox_) {
    return;
  }
  auto renderer = Engine::renderer();
  if (!renderer) {
    return;
  }
  auto tex = renderer->CreateCubemapTextureFromSingle(
      "engine://textures/default_skybox.png", {}, {});
  if (tex) {
    default_skybox_ = std::make_shared<Skybox>(tex);
  }
}

Entity Scene::CreateEntity(const std::string& name) {
  return CreateEntityWithUUID(UUID::GenerateV4(), name);
}

Entity Scene::CreateEntityWithUUID(UUID uuid, const std::string& name) {
  if (uuid.IsNil()) {
    uuid = UUID::GenerateV4();
  }
  Entity entity = {registry_.create(), this};
  entity.AddComponent<IdComponent>(uuid);
  entity.AddComponent<TransformComponent>();
  entity.AddComponent<TagComponent>(name.empty() ? "Entity" : name);

  entities_[uuid] = entity;
  scene_hierarchy_.push_back(entity);
  return entity;
}

void Scene::RemoveEntity(Entity entity) {
  entt::entity handle = entity.handle();

  // Skip if already queued
  for (entt::entity& queued : destroy_queue_) {
    if (queued == handle) {
      return;
    }
  }

  // Queue children recursively
  if (registry_.any_of<TreeComponent>(handle)) {
    auto& tree = registry_.get<TreeComponent>(handle);
    std::vector<entt::entity> children = tree.children;
    for (auto child : children) {
      RemoveEntity(Entity{child, this});
    }
  }

  destroy_queue_.push_back(handle);
}

entt::entity Scene::FindEntityByName(const std::string& name) {
  for (entt::entity entity : registry_.view<TagComponent>()) {
    if (registry_.get<TagComponent>(entity).name == name) {
      return entity;
    }
  }
  return entt::null;
}

entt::entity Scene::FindEntityByUUID(const UUID& uuid) {
  auto it = entities_.find(uuid);
  if (it != entities_.end()) {
    return it->second;
  }
  return entt::null;
}

std::vector<entt::entity> Scene::FindEntitiesByTag(const std::string& tag) {
  std::vector<entt::entity> result;
  for (entt::entity entity : registry_.view<TagComponent>()) {
    if (registry_.get<TagComponent>(entity).HasTag(tag)) {
      result.push_back(entity);
    }
  }
  return result;
}

void Scene::RequestAsset(AssetHandle handle) {
  if (!handle.IsValid()) {
    return;
  }
  if (!Engine::asset_manager().HasAsset(handle)) {
    return;
  }

  // Only track asset types that have a registered loader
  const AssetMetadata* meta = Engine::asset_manager().GetMetadata(handle);
  if (!meta || !AssetRegistry::HasLoader(meta->type)) {
    return;
  }

  // Avoid duplicates
  for (AssetHandle& h : requested_assets_) {
    if (h == handle) {
      return;
    }
  }
  requested_assets_.push_back(handle);

  // Kick off async load if not already loaded
  auto state = Engine::asset_manager().GetLoadState(handle);
  if (state != AssetLoadState::Loaded && state != AssetLoadState::Loading) {
    Engine::asset_manager().LoadAsync(handle);
  }
}

bool Scene::AreAssetsReady() const {
  for (auto& handle : requested_assets_) {
    auto state = Engine::asset_manager().GetLoadState(handle);
    if (state != AssetLoadState::Loaded && state != AssetLoadState::Failed) {
      return false;
    }
  }
  return true;
}

float Scene::GetAssetLoadProgress() const {
  if (requested_assets_.empty()) {
    return 1.0f;
  }
  float total = 0.0f;
  for (auto& handle : requested_assets_) {
    auto state = Engine::asset_manager().GetLoadState(handle);
    if (state == AssetLoadState::Loaded || state == AssetLoadState::Failed) {
      total += 1.0f;
    } else {
      // Use sub-progress for assets currently loading
      const auto* meta = Engine::asset_manager().GetMetadata(handle);
      if (meta) {
        total += meta->load_progress.load();
      }
    }
  }
  return total / static_cast<float>(requested_assets_.size());
}

void Scene::ClearRequestedAssets() {
  requested_assets_.clear();
}

void Scene::AddSystem(std::unique_ptr<ISystem> system) {
  int priority = system->GetPriority();
  auto it = std::ranges::find_if(systems_, [priority](const auto& s) {
    return s->GetPriority() > priority;
  });
  systems_.insert(it, std::move(system));
}

void Scene::OnUpdate(float_t delta_time) {
  PROFILE_ZONE_SCOPED();
  for (auto& system : systems_) {
    if (first_update_ && !system->RunOnFirstUpdate()) {
      continue;
    }
    system->Update(*this, delta_time);
  }

  if (!first_update_) [[likely]] {
    // UI events
    ui_event_system_.Update(*this, delta_time);
  } else {
    first_update_ = false;
  }
}

void Scene::OnUpdateEditor(float_t delta_time) {
  PROFILE_ZONE_SCOPED();
  // Only run systems marked as editor-safe.
  for (auto& system : systems_) {
    if (!system->RunInEditor()) {
      continue;
    }
    system->Update(*this, delta_time);
  }
}

void Scene::OnEvent(Event& event) {
  PROFILE_ZONE_SCOPED_N("Scene::OnEvent");
  EventDispatcher dispatcher{event};
  dispatcher.Dispatch<WindowResizedEvent>(WIESEL_BIND_FN(OnWindowResizeEvent));

  // F8: Toggle RmlUi debugger on the first visible UIDocument context
  if (event.GetEventType() == KeyPressedEvent::GetStaticType()) {
    auto& key = static_cast<KeyPressedEvent&>(event);
    if (key.GetKeyCode() == KeyF8) {
      for (auto entity : registry_.view<UIDocumentComponent>()) {
        auto& doc = registry_.get<UIDocumentComponent>(entity);
        if (doc.rml_context_ && doc.visible) {
          Engine::ui_manager().ToggleDebugger(doc.rml_context_);
          break;
        }
      }
    }
  }

  // Forward scroll events to the focused RmlUi document.
  if (event.GetEventType() == MouseScrolledEvent::GetStaticType()) {
    auto& scroll = static_cast<MouseScrolledEvent&>(event);
    if (ui_event_system_.ProcessMouseScroll(*this, scroll.GetYOffset())) {
      event.handled_ = true;
      return;
    }
  }

  // Forward key events to the focused RmlUi document.
  // Only consume events if RmlUi has an actual text input focused.
  if (ui_event_system_.HasRmlFocus()) {
    bool consume = ui_event_system_.HasRmlTextInputFocus(*this);

    if (event.GetEventType() == KeyPressedEvent::GetStaticType()) {
      auto& key = static_cast<KeyPressedEvent&>(event);
      ui_event_system_.ProcessKeyDown(*this, key.GetKeyCode(), 0);
      if (consume) {
        event.handled_ = true;
        return;
      }
    }
    if (event.GetEventType() == KeyReleasedEvent::GetStaticType()) {
      auto& key = static_cast<KeyReleasedEvent&>(event);
      ui_event_system_.ProcessKeyUp(*this, key.GetKeyCode(), 0);
      // Never consume releases so held keys get properly released
    }
    if (event.GetEventType() == KeyTypedEvent::GetStaticType()) {
      auto& typed = static_cast<KeyTypedEvent&>(event);
      std::string text(1, static_cast<char>(typed.GetKeyCode()));
      ui_event_system_.ProcessTextInput(*this, text);
      if (consume) {
        event.handled_ = true;
        return;
      }
    }
  }

  // Route keyboard input to focused text input
  entt::entity focused = ui_event_system_.GetFocusedEntity();
  if (focused != entt::null && registry_.valid(focused) &&
      registry_.any_of<TextInputComponent>(focused)) {
    auto& input = registry_.get<TextInputComponent>(focused);

    if (event.GetEventType() == KeyTypedEvent::GetStaticType()) {
      auto& typed = static_cast<KeyTypedEvent&>(event);
      uint32_t codepoint = static_cast<uint32_t>(typed.GetKeyCode());
      if (codepoint >= 32) {
        if (input.max_length <= 0 ||
            static_cast<int>(input.text.size()) < input.max_length) {
          input.text.insert(input.text.begin() + input.cursor_pos_,
                            static_cast<char>(codepoint));
          input.cursor_pos_++;
          input.cursor_visible_ = true;
          input.cursor_timer_ = 0.0f;
        }
      }
      event.handled_ = true;
      return;
    }

    if (event.GetEventType() == KeyPressedEvent::GetStaticType()) {
      auto& key = static_cast<KeyPressedEvent&>(event);
      bool handled = true;
      switch (key.GetKeyCode()) {
        case KeyBackspace:
          if (input.cursor_pos_ > 0) {
            input.text.erase(input.cursor_pos_ - 1, 1);
            input.cursor_pos_--;
          }
          break;
        case KeyDelete:
          if (input.cursor_pos_ < static_cast<int>(input.text.size())) {
            input.text.erase(input.cursor_pos_, 1);
          }
          break;
        case KeyArrowLeft:
          if (input.cursor_pos_ > 0) {
            input.cursor_pos_--;
          }
          break;
        case KeyArrowRight:
          if (input.cursor_pos_ < static_cast<int>(input.text.size())) {
            input.cursor_pos_++;
          }
          break;
        case KeyHome:
          input.cursor_pos_ = 0;
          break;
        case KeyEnd:
          input.cursor_pos_ = static_cast<int>(input.text.size());
          break;
        default:
          handled = false;
          break;
      }
      if (handled) {
        input.cursor_visible_ = true;
        input.cursor_timer_ = 0.0f;
        event.handled_ = true;
        return;
      }
    }
  }

  for (const auto& entity : registry_.view<BehaviorsComponent>()) {
    auto& component = registry_.get<BehaviorsComponent>(entity);
    component.OnEvent(event);
  }
}

void Scene::LinkEntities(entt::entity parent, entt::entity child,
                         bool convert_to_local) {
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
  parent_tree.children.push_back(child);
  child_tree.parent = parent;
  if (convert_to_local) {
    auto& child_transform = registry_.get<TransformComponent>(child);
    auto& parent_transform = registry_.get<TransformComponent>(parent);
    glm::vec3 posDiff =
        child_transform.GetPosition() - parent_transform.GetPosition();
    glm::vec3 rotDiff =
        child_transform.GetRotation() - parent_transform.GetRotation();

    child_transform.SetPosition(posDiff);
    child_transform.SetRotation(rotDiff);
  }
}

void Scene::UnlinkEntities(entt::entity parent, entt::entity child) {
  auto& parent_tree = registry_.get_or_emplace<TreeComponent>(parent);
  auto& child_tree = registry_.get_or_emplace<TreeComponent>(child);
  if (child_tree.parent == entt::null) {
    return;
  }
  parent_tree.children.erase(
      std::ranges::remove(parent_tree.children, child).begin(),
      parent_tree.children.end());
  child_tree.parent = entt::null;
  auto& child_transform = registry_.get<TransformComponent>(child);
  auto& parent_transform = registry_.get<TransformComponent>(parent);
  glm::vec3 pos_diff =
      child_transform.GetPosition() + parent_transform.GetPosition();
  glm::vec3 rot_diff =
      child_transform.GetRotation() + parent_transform.GetRotation();

  child_transform.SetPosition(pos_diff);
  child_transform.SetRotation(rot_diff);
}

void Scene::ProcessDestroyQueue() {
  PROFILE_ZONE_SCOPED();
  if (destroy_queue_.empty()) {
    return;
  }

  // Swap to local copy so new removals during cleanup don't corrupt iteration
  std::vector<entt::entity> queue;
  queue.swap(destroy_queue_);

  for (entt::entity handle : queue) {
    if (!registry_.valid(handle)) {
      continue;
    }

    // Unlink from parent
    if (registry_.any_of<TreeComponent>(handle)) {
      auto& tree = registry_.get<TreeComponent>(handle);
      if (tree.parent != entt::null && registry_.valid(tree.parent) &&
          registry_.any_of<TreeComponent>(tree.parent)) {
        auto& parent_tree = registry_.get<TreeComponent>(tree.parent);
        std::erase(parent_tree.children, handle);
      }
    }

    // Remove from UUID map and hierarchy
    if (registry_.any_of<IdComponent>(handle)) {
      entities_.erase(registry_.get<IdComponent>(handle).Id);
    }
    std::erase(scene_hierarchy_, handle);

    physics_world_->DestroyBody(handle);
    registry_.destroy(handle);
  }
}

bool Scene::OnWindowResizeEvent(WindowResizedEvent& event) {
  if (render_resolution_.x > 0 && render_resolution_.y > 0) {
    return false;  // fixed resolution, don't react to window resize
  }
  for (const auto& entity : registry_.view<CameraComponent>()) {
    auto& component = registry_.get<CameraComponent>(entity);
    component.viewport_size.x = event.window_size().width;
    component.viewport_size.y = event.window_size().height;
    component.aspect_ratio = event.aspect_ratio();
    component.view_changed = true;
  }
  return false;
}

void Scene::Cleanup() {
  // Clear camera render graphs and resource pools.
  auto camera_view = registry_.view<CameraComponent>();
  LOG_DEBUG("Scene::Cleanup - cameras: {}", camera_view.size());
  for (entt::entity entity : camera_view) {
    auto& camera = registry_.get<CameraComponent>(entity);
    camera.render_graph = nullptr;
    camera.resource_pool.Clear();
    camera.render_pipeline = nullptr;
  }

  for (entt::entity entity : registry_.view<CanvasRectComponent>()) {
    auto& rect = registry_.get<CanvasRectComponent>(entity);
    rect.descriptor_ = nullptr;
    rect.ubo_ = nullptr;
  }

  // CanvasImageComponent has no per-component GPU resources;
  // rendering uses the Renderer's transient slice pool.

  for (entt::entity entity : registry_.view<TextComponent>()) {
    auto& text = registry_.get<TextComponent>(entity);
    text.glyph_gpu_.clear();
  }

  skybox_ = nullptr;
  default_skybox_ = nullptr;
  current_camera_ = nullptr;
}

void Scene::ResetPhysicsWorld() {
  glm::vec3 gravity = physics_world_->GetGravity();
  physics_world_.reset();
  physics_world_ = std::make_unique<PhysicsWorld>(this);
  physics_world_->SetGravity(gravity);
}

void Scene::ResetScriptStates() {
  for (const auto& entity : registry_.view<BehaviorsComponent>()) {
    auto& component = registry_.get<BehaviorsComponent>(entity);
    for (auto& [name, behavior] : component.behaviors_) {
      if (auto* mono = dynamic_cast<MonoBehavior*>(behavior)) {
        if (auto* instance = mono->script_instance()) {
          instance->ResetStartState();
        }
      }
    }
  }
}

void Scene::SetRenderPipeline(entt::entity camera_entity,
                              std::shared_ptr<RenderPipeline> pipeline) {
  auto& camera = registry_.get<CameraComponent>(camera_entity);
  camera.render_pipeline = std::move(pipeline);
  camera.resource_pool.Clear();
  camera.resource_pipeline_version = 0;  // force rebuild on next render
}

void MultiScene::SetSceneIndex(uint8_t index) {
  Engine::renderer()->SetCurrentSceneIndex(index);
}

Entity Scene::InstantiateModel(AssetHandle model_handle,
                               const std::string& name) {
  auto model_data = Engine::asset_manager().GetOrLoad<Model>(model_handle);
  if (!model_data) {
    LOG_ERROR("InstantiateModel: failed to load model");
    return Entity{entt::null, this};
  }

  const auto& hierarchy = model_data->node_hierarchy;
  if (hierarchy.nodes.empty()) {
    LOG_ERROR("InstantiateModel: model has no nodes");
    return Entity{entt::null, this};
  }

  // Create one entity per node in the hierarchy
  std::vector<entt::entity> node_entities(
      hierarchy.nodes.size(), static_cast<entt::entity>(entt::null));

  for (int32_t i = 0; i < static_cast<int32_t>(hierarchy.nodes.size()); i++) {
    const auto& node = hierarchy.nodes[i];
    std::string node_name = node.name;
    if (node_name.empty()) {
      node_name = "Node_" + std::to_string(i);
    }
    // Use the provided name for the root node
    if (i == hierarchy.root_index && !name.empty()) {
      node_name = name;
    }

    Entity entity = CreateEntity(node_name);
    node_entities[i] = entity.handle();

    // Apply each node's local transform. Skip the root node - its transform
    // is the FBX axis conversion (bone matrices already include it via the
    // hierarchy). For direct children of root, rotate their position by root's
    // rotation so they end up in the correct world position.
    if (i != hierarchy.root_index) {
      auto& tc = entity.GetComponent<TransformComponent>();
      glm::vec3 pos, scale, skew;
      glm::quat rot;
      glm::vec4 persp;
      if (glm::decompose(node.local_transform, scale, rot, pos, skew, persp)) {
        if (node.parent_index == hierarchy.root_index) {
          glm::mat3 root_rot =
              glm::mat3(hierarchy.nodes[hierarchy.root_index].local_transform);
          pos = root_rot * pos;
        }
        tc.SetPosition(pos);
        tc.SetRotation(glm::degrees(glm::eulerAngles(rot)));
        tc.SetScale(scale);
      }
    }

    // Add mesh components. If a node has multiple meshes, create child
    // entities for each additional mesh (entt allows one component per type).
    for (size_t mi = 0; mi < node.mesh_indices.size(); mi++) {
      int32_t mesh_idx = node.mesh_indices[mi];
      if (mesh_idx < 0 ||
          mesh_idx >= static_cast<int32_t>(model_data->meshes.size())) {
        continue;
      }

      Entity mesh_entity = entity;
      if (mi > 0) {
        std::string mesh_name = node_name + "_mesh" + std::to_string(mi);
        mesh_entity = CreateEntity(mesh_name);
        LinkEntities(entity.handle(), mesh_entity.handle(), false);
      }

      auto& mesh = model_data->meshes[mesh_idx];
      if (model_data->has_skeleton) {
        auto& smr = mesh_entity.AddComponent<SkinnedMeshRendererComponent>();
        smr.model_handle = model_handle;
        smr.mesh_index = mesh_idx;
        if (mesh->material_handle.IsValid()) {
          smr.material_handle = mesh->material_handle;
        }
      } else {
        auto& mr = mesh_entity.AddComponent<MeshRendererComponent>();
        mr.model_handle = model_handle;
        mr.mesh_index = mesh_idx;
        if (mesh->material_handle.IsValid()) {
          mr.material_handle = mesh->material_handle;
        }
      }
    }

    // Add camera components for matching nodes
    for (const auto& cam : model_data->imported_cameras) {
      if (cam.node_name == node.name) {
        auto& cc = entity.AddComponent<CameraComponent>();
        cc.field_of_view = cam.fov;
        cc.near_plane = cam.near_plane;
        cc.far_plane = cam.far_plane;
        if (cam.aspect_ratio > 0.0f) {
          cc.aspect_ratio = cam.aspect_ratio;
        }
        cc.enabled = false;

        // Compute camera rotation from look_at/up vectors. The node's rotation
        // plus the root's axis conversion give the full world orientation.
        auto& tc = entity.GetComponent<TransformComponent>();
        glm::vec3 p, s, sk;
        glm::quat node_rot;
        glm::vec4 pr;
        glm::decompose(node.local_transform, s, node_rot, p, sk, pr);
        // Include root rotation for nodes that are direct children of root
        if (node.parent_index == hierarchy.root_index) {
          glm::quat root_rot = glm::quat_cast(
              glm::mat3(hierarchy.nodes[hierarchy.root_index].local_transform));
          node_rot = root_rot * node_rot;
        }

        glm::vec3 world_forward = glm::normalize(-(node_rot * cam.look_at));
        glm::vec3 world_up = glm::normalize(node_rot * cam.up);
        glm::vec3 right = glm::normalize(glm::cross(world_up, world_forward));
        world_up = glm::cross(world_forward, right);

        // Columns: X=right, Y=up, Z=forward matches our camera convention
        glm::mat3 cam_rot(right, world_up, world_forward);
        glm::quat cam_quat = glm::quat_cast(cam_rot);
        tc.SetRotation(glm::degrees(glm::eulerAngles(cam_quat)));

        break;
      }
    }

    // Add light components for matching nodes
    for (const auto& light : model_data->imported_lights) {
      if (light.node_name == node.name) {
        if (light.type == Model::ImportedLight::Type::Directional) {
          auto& lc = entity.AddComponent<LightDirectComponent>();
          lc.light_data.base.color = light.color;
          lc.light_data.base.density = light.intensity;
        } else if (light.type == Model::ImportedLight::Type::Point ||
                   light.type == Model::ImportedLight::Type::Spot) {
          auto& lc = entity.AddComponent<LightPointComponent>();
          lc.light_data.base.color = light.color;
          lc.light_data.base.density = light.intensity;
          lc.light_data.constant = light.attenuation_constant;
          lc.light_data.linear = light.attenuation_linear;
          lc.light_data.exp = light.attenuation_quadratic;
        }
        break;
      }
    }

    // Link to parent
    if (node.parent_index >= 0 &&
        node.parent_index < static_cast<int32_t>(node_entities.size()) &&
        node_entities[node.parent_index] != entt::null) {
      LinkEntities(node_entities[node.parent_index], entity.handle(), false);
    }
  }

  entt::entity root = node_entities[hierarchy.root_index];

  // For skeletal models: add AnimatorComponent on the root, and set
  // skeleton_root on all SkinnedMeshRendererComponents
  if (model_data->has_skeleton) {
    // Set skeleton_root on all skinned meshes that reference this model
    int skinned_count = 0;
    for (auto e : registry_.view<SkinnedMeshRendererComponent>()) {
      auto& smr = registry_.get<SkinnedMeshRendererComponent>(e);
      if (smr.model_handle == model_handle && smr.skeleton_root == entt::null) {
        smr.skeleton_root = root;
        skinned_count++;
      }
    }
  }

  return Entity{root, this};
}

}  // namespace Wiesel