
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "networking/w_replication_manager.h"

#include "behavior/w_behavior.h"
#include "networking/w_network.h"
#include "networking/w_network_component_serializer.h"
#include "networking/w_network_scene_manager.h"
#include "scene/w_scene.h"
#include "scene/w_scene_manager.h"
#include "util/w_logger.h"
#include "w_engine.h"
#include "znet/buffer.h"
#include "znet/packet_handler.h"
#include "znet/peer_session.h"

namespace wiesel {

void ReplicationManager::Update(float delta_time) {
  auto& network = Engine::network();
  if (network.role() == NetworkRole::kNone) {
    return;
  }

  if (!handlers_registered_) {
    RegisterPacketHandlers();
    handlers_registered_ = true;
  }

  ProcessIncomingCommands();

  if (network.is_server()) {
    ServerHandleSpawnsAndDestroys();
  }

  SendOwnedEntityState(delta_time);
  InterpolateRemoteEntities(delta_time);
}

void ReplicationManager::ProcessIncomingCommands() {
  std::vector<ReplicationCommand> commands;
  {
    std::scoped_lock lock(command_mutex_);
    commands.swap(pending_commands_);
  }

  // Process deferred spawns for scenes that finished loading
  {
    std::vector<std::string> loaded;
    for (const std::string& scene_name : loading_scenes_) {
      if (Engine::scene_manager().FindScene(scene_name)) {
        loaded.push_back(scene_name);
      }
    }
    for (std::string& scene_name : loaded) {
      loading_scenes_.erase(scene_name);
      auto it = deferred_spawns_.find(scene_name);
      if (it != deferred_spawns_.end()) {
        LOG_INFO("Processing {} deferred spawns for scene '{}'",
                 it->second.size(), scene_name);
        for (auto& deferred_cmd : it->second) {
          commands.push_back(std::move(deferred_cmd));
        }
        deferred_spawns_.erase(it);
      }
    }
  }

  for (ReplicationCommand& cmd : commands) {
    switch (cmd.type) {
      case ReplicationCommand::kSceneLoad: {
        auto packet = std::static_pointer_cast<SceneLoadPacket>(cmd.packet);
        LOG_INFO("Server requested scene load: '{}' (mode {})",
                 packet->scene_name, static_cast<int>(packet->load_mode));

        loading_scenes_.insert(packet->scene_name);

        if (packet->load_mode == 2) {
          Engine::scene_manager().LoadSceneWithLoading(
              packet->scene_name, packet->loading_scene);
        } else {
          LoadSceneMode mode = (packet->load_mode == 1)
                                   ? LoadSceneMode::Additive
                                   : LoadSceneMode::Single;
          Engine::scene_manager().LoadSceneAsync(packet->scene_name, mode);
        }
        break;
      }

      case ReplicationCommand::kSpawn: {
        auto packet =
            std::static_pointer_cast<EntitySpawnPacket>(cmd.packet);

        if (!packet->scene_name.empty() &&
            !Engine::scene_manager().FindScene(packet->scene_name)) {
          deferred_spawns_[packet->scene_name].push_back(cmd);
          LOG_DEBUG("Deferred entity spawn net_id={} for scene '{}'",
                    packet->net_id, packet->scene_name);
          break;
        }

        SpawnEntityFromPacket(packet);
        break;
      }

      case ReplicationCommand::kDestroy: {
        auto packet =
            std::static_pointer_cast<EntityDestroyPacket>(cmd.packet);

        auto it = net_id_to_entity_.find(packet->net_id);
        if (it != net_id_to_entity_.end()) {
          EntityRef ref = it->second;
          Entity entity = ref.Resolve();
          if (entity) {
            entity.RemoveFromScene();
          }
          net_id_to_entity_.erase(it);
          LOG_DEBUG("Replicated entity destroy: net_id={}", packet->net_id);
        }
        break;
      }

      case ReplicationCommand::kState: {
        auto packet =
            std::static_pointer_cast<EntityStatePacket>(cmd.packet);

        for (auto& update : packet->updates) {
          auto it = net_id_to_entity_.find(update.net_id);
          if (it == net_id_to_entity_.end()) {
            continue;
          }

          Entity entity = it->second.Resolve();
          if (!entity) {
            continue;
          }
          if (!update.component_data || update.component_data->size() == 0) {
            continue;
          }

          auto& net_id = entity.GetComponent<NetworkIdentityComponent>();

          // Only apply state for entities we don't own
          if (!net_id.is_remote) {
            continue;
          }

          // Capture pre-snapshot for interpolation
          TransformSnapshot pre_snapshot;
          bool has_transform = entity.HasComponent<TransformComponent>();
          if (has_transform) {
            auto& tc = entity.GetComponent<TransformComponent>();
            pre_snapshot.position = tc.GetPosition();
            pre_snapshot.rotation = tc.GetRotation();
            pre_snapshot.scale = tc.GetScale();
          }

          NetworkComponentSerializerRegistry::DeserializeAll(
              entity, *update.component_data);

          // Set up interpolation
          if (has_transform) {
            auto& tc = entity.GetComponent<TransformComponent>();
            net_id.from_snapshot = pre_snapshot;
            net_id.to_snapshot.position = tc.GetPosition();
            net_id.to_snapshot.rotation = tc.GetRotation();
            net_id.to_snapshot.scale = tc.GetScale();
            net_id.interp_elapsed = 0.0f;
            net_id.has_snapshots = true;

            tc.SetPosition(pre_snapshot.position);
            tc.SetRotation(pre_snapshot.rotation);
            tc.SetScale(pre_snapshot.scale);
          }

          // On the server, mark dirty so we rebroadcast to other clients
          if (Engine::network().is_server()) {
            net_id.dirty_components.set();
          }
        }
        break;
      }

      case ReplicationCommand::kOwnership: {
        auto packet =
            std::static_pointer_cast<EntityOwnershipPacket>(cmd.packet);

        auto it = net_id_to_entity_.find(packet->net_id);
        if (it != net_id_to_entity_.end()) {
          Entity entity = it->second.Resolve();
          if (!entity) {
            break;
          }
          auto& net_id = entity.GetComponent<NetworkIdentityComponent>();
          net_id.authority =
              static_cast<NetworkAuthority>(packet->authority);
          net_id.owner_session_id = packet->owner_session_id;
        }
        break;
      }

      case ReplicationCommand::kRpc: {
        auto packet = std::static_pointer_cast<RpcPacket>(cmd.packet);

        auto it = net_id_to_entity_.find(packet->net_id);
        if (it == net_id_to_entity_.end()) {
          break;
        }

        Entity entity = it->second.Resolve();
        if (!entity) {
          break;
        }
        Scene* scene = entity.GetScene();
        if (!scene->HasComponent<BehaviorsComponent>(entity.handle())) {
          break;
        }

        std::string args_json;
        if (packet->args_data && packet->args_data->size() > 0) {
          packet->args_data->set_read_cursor(0);
          args_json = packet->args_data->ReadString();
        }

        auto& behaviors =
            scene->GetComponent<BehaviorsComponent>(entity.handle());
        for (auto& [name, behavior] : behaviors.behaviors_) {
          if (behavior && behavior->IsEnabled()) {
            if (packet->direction == RpcDirection::kToServer) {
              behavior->OnServerRpc(packet->rpc_name, args_json);
            } else {
              behavior->OnClientRpc(packet->rpc_name, args_json);
            }
          }
        }
        break;
      }

      case ReplicationCommand::kSyncVar: {
        auto packet =
            std::static_pointer_cast<SyncVarUpdatePacket>(cmd.packet);

        auto it = net_id_to_entity_.find(packet->net_id);
        if (it == net_id_to_entity_.end()) {
          break;
        }

        Entity entity = it->second.Resolve();
        if (!entity) {
          break;
        }
        auto& net_id = entity.GetComponent<NetworkIdentityComponent>();

        for (auto& var : packet->vars) {
          auto parsed = nlohmann::json::parse(var.json_value, nullptr, false);
          net_id.sync_vars[var.name] = parsed;

          // Mark as sent so we don't echo it back to the sender
          net_id.sync_vars_last_sent[var.name] = parsed;

          Scene* scene = entity.GetScene();
          if (scene->HasComponent<BehaviorsComponent>(entity.handle())) {
            auto& behaviors =
                scene->GetComponent<BehaviorsComponent>(entity.handle());
            for (auto& [bname, behavior] : behaviors.behaviors_) {
              if (behavior && behavior->IsEnabled()) {
                behavior->OnSyncVarChanged(var.name);
              }
            }
          }
        }

        // Server: rebroadcast to other clients
        if (Engine::network().is_server()) {
          Engine::network().Broadcast(packet);
        }
        break;
      }
    }
  }
}

void ReplicationManager::ServerHandleSpawnsAndDestroys() {
  auto& network = Engine::network();

  // Late joiner full state syncs
  {
    std::vector<uint64_t> sessions_to_sync;
    {
      std::scoped_lock lock(sync_mutex_);
      sessions_to_sync.swap(pending_full_sync_);
    }
    for (uint64_t session_id : sessions_to_sync) {
      SendFullStateToSession(session_id);
    }
  }

  // Pending entity destroys
  {
    std::vector<uint32_t> destroys;
    {
      std::scoped_lock lock(destroy_mutex_);
      destroys.swap(pending_destroys_);
    }
    for (uint32_t net_id : destroys) {
      auto packet = std::make_shared<EntityDestroyPacket>();
      packet->net_id = net_id;
      network.Broadcast(packet);
      net_id_to_entity_.erase(net_id);
    }
  }

  // Pending entity spawns - two passes so parents have net_ids before children
  // build their packets (BuildSpawnPacket reads the parent's net_id).
  for (auto& scene_ptr : Engine::scene_manager().GetLoadedScenes()) {
    auto& registry = scene_ptr->GetRegistry();
    auto spawn_view = registry.view<NetworkIdentityComponent>();

    // Pass 1: assign net_ids
    for (entt::entity ent : spawn_view) {
      auto& net_id = spawn_view.get<NetworkIdentityComponent>(ent);
      if (!net_id.pending_spawn || net_id.net_id != 0) {
        continue;
      }
      net_id.net_id = AllocateNetId();
      net_id_to_entity_.emplace(net_id.net_id,
                                EntityRef(ent, scene_ptr->GetHandle()));
      if (net_id.authority == NetworkAuthority::kClient) {
        net_id.is_remote = true;
      }
    }

    // Pass 2: build and broadcast spawn packets
    for (entt::entity ent : spawn_view) {
      auto& net_id = spawn_view.get<NetworkIdentityComponent>(ent);
      if (!net_id.pending_spawn) {
        continue;
      }
      Entity entity(ent, scene_ptr.get());
      auto packet = BuildSpawnPacket(entity, net_id);
      network.Broadcast(packet);
      net_id.pending_spawn = false;
      LOG_DEBUG("Replicated entity spawn: net_id={} name={} entity={}",
                net_id.net_id, packet->entity_name, entity);
    }
  }
}

void ReplicationManager::SendOwnedEntityState(float delta_time) {
  auto& network = Engine::network();
  float tick_interval = network.tick_interval();

  send_accumulator_ += delta_time;
  if (send_accumulator_ < tick_interval) {
    return;
  }
  send_accumulator_ -= tick_interval;

  auto state_packet = std::make_shared<EntityStatePacket>();

  for (auto& scene_ptr : Engine::scene_manager().GetLoadedScenes()) {
    auto& registry = scene_ptr->GetRegistry();
    auto view = registry.view<NetworkIdentityComponent>();

    for (auto ent : view) {
      auto& net_id = registry.get<NetworkIdentityComponent>(ent);

      if (net_id.pending_spawn || net_id.net_id == 0) {
        continue;
      }

      if (net_id.dirty_components.none()) {
        continue;
      }

      Entity entity{ent, scene_ptr.get()};
      auto buf = std::make_shared<znet::Buffer>();
      NetworkComponentSerializerRegistry::SerializeDirty(entity, net_id, *buf);

      if (buf->size() > 0) {
        EntityStatePacket::EntityUpdate update;
        update.net_id = net_id.net_id;
        update.component_data = buf;
        state_packet->updates.push_back(std::move(update));
      }

      net_id.dirty_components.reset();
    }

    // Send dirty sync vars
    for (auto ent : view) {
      auto& net_id = registry.get<NetworkIdentityComponent>(ent);
      if (net_id.pending_spawn || net_id.net_id == 0) {
        continue;
      }

      auto sync_var_packet = std::make_shared<SyncVarUpdatePacket>();
      sync_var_packet->net_id = net_id.net_id;

      for (auto& [name, value] : net_id.sync_vars) {
        auto last_it = net_id.sync_vars_last_sent.find(name);
        if (last_it != net_id.sync_vars_last_sent.end() &&
            last_it->second == value) {
          continue;
        }

        SyncVarUpdatePacket::VarEntry entry;
        entry.name = name;
        entry.json_value = value.dump();
        sync_var_packet->vars.push_back(std::move(entry));
        net_id.sync_vars_last_sent[name] = value;
      }

      if (!sync_var_packet->vars.empty()) {
        if (network.is_server()) {
          network.Broadcast(sync_var_packet);
        } else {
          auto session = network.GetServerSession();
          if (session) {
            session->SendPacket(sync_var_packet);
          }
        }
      }
    }
  }

  if (!state_packet->updates.empty()) {
    if (network.is_server()) {
      network.Broadcast(state_packet);
    } else {
      auto session = network.GetServerSession();
      if (session) {
        session->SendPacket(state_packet);
      }
    }
  }
}

void ReplicationManager::InterpolateRemoteEntities(float delta_time) {
  float tick_interval = Engine::network().tick_interval();

  for (auto& scene_ptr : Engine::scene_manager().GetLoadedScenes()) {
    auto& registry = scene_ptr->GetRegistry();

    auto view =
        registry.view<NetworkIdentityComponent, TransformComponent>();
    for (entt::entity ent : view) {
      auto& net_id = view.get<NetworkIdentityComponent>(ent);

      if (!net_id.has_snapshots) {
        continue;
      }

      net_id.interp_elapsed += delta_time;
      float t = (tick_interval > 0.0f)
                    ? std::min(net_id.interp_elapsed / tick_interval, 1.0f)
                    : 1.0f;

      auto& tc = view.get<TransformComponent>(ent);
      tc.SetPosition(glm::mix(net_id.from_snapshot.position,
                              net_id.to_snapshot.position, t));
      tc.SetRotation(glm::mix(net_id.from_snapshot.rotation,
                              net_id.to_snapshot.rotation, t));
      tc.SetScale(glm::mix(net_id.from_snapshot.scale,
                           net_id.to_snapshot.scale, t));

      if (t >= 1.0f) {
        net_id.has_snapshots = false;
      }
    }
  }
}

void ReplicationManager::SendFullStateToSession(uint64_t session_id) {
  auto& network = Engine::network();

  for (auto& scene_ptr : Engine::scene_manager().GetLoadedScenes()) {
    auto& registry = scene_ptr->GetRegistry();

    auto view =
        registry.view<NetworkIdentityComponent, IdComponent, TagComponent>();
    for (entt::entity ent : view) {
      auto& net_id = view.get<NetworkIdentityComponent>(ent);

      if (net_id.pending_spawn || net_id.net_id == 0) {
        continue;
      }

      Entity entity{ent, scene_ptr.get()};
      auto packet = BuildSpawnPacket(entity, net_id);
      network.SendTo(session_id, packet);
    }
  }

  LOG_INFO("Sent full state sync to session {}", session_id);
}

std::shared_ptr<EntitySpawnPacket> ReplicationManager::BuildSpawnPacket(
    Entity& entity, const NetworkIdentityComponent& net_id) {
  auto& id_comp = entity.GetComponent<IdComponent>();
  auto& tag_comp = entity.GetComponent<TagComponent>();

  auto component_buffer = std::make_shared<znet::Buffer>();
  NetworkComponentSerializerRegistry::SerializeAll(entity, *component_buffer);

  auto packet = std::make_shared<EntitySpawnPacket>();
  packet->net_id = net_id.net_id;
  packet->uuid_hi = id_comp.Id.hi();
  packet->uuid_lo = id_comp.Id.lo();
  packet->entity_name = tag_comp.name;
  packet->authority = static_cast<uint8_t>(net_id.authority);
  packet->owner_session_id = net_id.owner_session_id;
  packet->scene_name = entity.GetScene()->GetName();
  packet->component_data = component_buffer;

  // Include parent's net_id if parent has a NetworkIdentityComponent
  auto& registry = entity.GetScene()->GetRegistry();
  if (registry.any_of<TreeComponent>(entity.handle())) {
    auto& tree = registry.get<TreeComponent>(entity.handle());
    if (tree.parent != entt::null &&
        registry.any_of<NetworkIdentityComponent>(tree.parent)) {
      packet->parent_net_id =
          registry.get<NetworkIdentityComponent>(tree.parent).net_id;
    }
  }
  return packet;
}

void ReplicationManager::SpawnEntityFromPacket(
    std::shared_ptr<EntitySpawnPacket> packet) {
  if (net_id_to_entity_.contains(packet->net_id)) {
    return;
  }

  Scene* target_scene = nullptr;
  if (!packet->scene_name.empty()) {
    target_scene = Engine::scene_manager().FindScene(packet->scene_name);
  }
  if (!target_scene) {
    target_scene = Engine::scene_manager().GetActiveScene();
  }

  if (!target_scene) {
    if (!packet->scene_name.empty()) {
      deferred_spawns_[packet->scene_name].push_back(
          {ReplicationCommand::kSpawn, packet});
    }
    return;
  }

  urkern::UUID::bytes uuid_bytes;
  for (int i = 0; i < 8; i++) {
    uuid_bytes[i] = static_cast<uint8_t>(
        (packet->uuid_hi >> (56 - i * 8)) & 0xFF);
  }
  for (int i = 0; i < 8; i++) {
    uuid_bytes[8 + i] = static_cast<uint8_t>(
        (packet->uuid_lo >> (56 - i * 8)) & 0xFF);
  }

  urkern::UUID uuid(uuid_bytes);
  Entity entity = target_scene->CreateEntityWithUUID(uuid, packet->entity_name);

  auto& net_id = entity.AddComponent<NetworkIdentityComponent>();
  net_id.net_id = packet->net_id;
  net_id.authority = static_cast<NetworkAuthority>(packet->authority);
  net_id.owner_session_id = packet->owner_session_id;
  net_id.pending_spawn = false;

  // Determine ownership: we own it if our session matches, otherwise remote
  auto& network = Engine::network();
  if (network.is_client() && !network.is_server()) {
    net_id.is_remote =
        (net_id.owner_session_id != network.local_session_id());
  }

  if (packet->component_data && packet->component_data->size() > 0) {
    NetworkComponentSerializerRegistry::DeserializeAll(entity,
                                                       *packet->component_data);
  }

  // Initialize interpolation snapshots
  if (entity.HasComponent<TransformComponent>()) {
    auto& tc = entity.GetComponent<TransformComponent>();
    net_id.from_snapshot.position = tc.GetPosition();
    net_id.from_snapshot.rotation = tc.GetRotation();
    net_id.from_snapshot.scale = tc.GetScale();
    net_id.to_snapshot = net_id.from_snapshot;
  }

  net_id_to_entity_.emplace(packet->net_id, entity.ToRef());

  // Link to parent if specified
  if (packet->parent_net_id != 0) {
    auto parent_it = net_id_to_entity_.find(packet->parent_net_id);
    if (parent_it != net_id_to_entity_.end()) {
      Entity parent = parent_it->second.Resolve();
      if (parent && parent.GetScene() == target_scene) {
        target_scene->LinkEntities(parent.handle(), entity.handle(), false);
      }
    } else {
      LOG_WARN("Parent net_id {} not found for entity net_id {}",
               packet->parent_net_id, packet->net_id);
    }
  }

  LOG_DEBUG("Replicated entity spawn: net_id={} name={} entity={} parent={}",
            packet->net_id, packet->entity_name, entity, packet->parent_net_id);
}

void ReplicationManager::PushCommand(ReplicationCommand command) {
  std::scoped_lock lock(command_mutex_);
  pending_commands_.push_back(std::move(command));
}

void ReplicationManager::QueueFullSyncForSession(uint64_t session_id) {
  std::scoped_lock lock(sync_mutex_);
  pending_full_sync_.push_back(session_id);
}

void ReplicationManager::RegisterPacketHandlers() {
  auto& network = Engine::network();

  network.SetSessionSetupCallback(
      [this](std::shared_ptr<znet::PeerSession> session) {
        auto handler = std::make_shared<znet::CallbackPacketHandler>();

        handler->AddShared<EntitySpawnPacket>(
            [this](std::shared_ptr<EntitySpawnPacket> packet) {
              PushCommand({ReplicationCommand::kSpawn, packet});
            });

        handler->AddShared<EntityDestroyPacket>(
            [this](std::shared_ptr<EntityDestroyPacket> packet) {
              PushCommand({ReplicationCommand::kDestroy, packet});
            });

        handler->AddShared<EntityStatePacket>(
            [this](std::shared_ptr<EntityStatePacket> packet) {
              PushCommand({ReplicationCommand::kState, packet});
            });

        handler->AddShared<EntityOwnershipPacket>(
            [this](std::shared_ptr<EntityOwnershipPacket> packet) {
              PushCommand({ReplicationCommand::kOwnership, packet});
            });

        handler->AddShared<RpcPacket>(
            [this](std::shared_ptr<RpcPacket> packet) {
              PushCommand({ReplicationCommand::kRpc, packet});
            });

        handler->AddShared<SyncVarUpdatePacket>(
            [this](std::shared_ptr<SyncVarUpdatePacket> packet) {
              PushCommand({ReplicationCommand::kSyncVar, packet});
            });

        handler->AddShared<SceneLoadPacket>(
            [this](std::shared_ptr<SceneLoadPacket> packet) {
              PushCommand({ReplicationCommand::kSceneLoad, packet});
            });

        session->SetHandler(handler);

        if (Engine::network().is_server()) {
          uint64_t sid = session->id();
          Engine::app().SubmitToMainThread([this, sid]() {
            Engine::network_scene_manager().SyncToSession(sid);
            QueueFullSyncForSession(sid);
          });
        }
      });
}

uint32_t ReplicationManager::AllocateNetId() {
  return next_net_id_++;
}

}  // namespace wiesel
