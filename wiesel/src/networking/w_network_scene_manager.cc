
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "networking/w_network_scene_manager.h"

#include "networking/w_network.h"
#include "networking/w_replication_packets.h"
#include "util/w_logger.h"
#include "w_engine.h"

namespace wiesel {

void NetworkSceneManager::LoadScene(const std::string& name,
                                    LoadSceneMode mode) {
  auto& network = Engine::network();
  if (!network.is_server()) {
    LOG_WARN("NetworkSceneManager::LoadScene can only be called on the server");
    return;
  }

  Engine::scene_manager().LoadScene(name, mode);

  // Track for late joiners
  network_loaded_scenes_.push_back({name, mode, ""});

  // Broadcast to all clients
  auto packet = std::make_shared<SceneLoadPacket>();
  packet->scene_name = name;
  packet->load_mode = ToInt(mode);
  network.Broadcast(packet);

  LOG_INFO("NetworkSceneManager: loaded scene '{}' (mode {}), broadcast to clients",
           name, ToString(mode));
}

void NetworkSceneManager::LoadSceneWithLoading(
    const std::string& target_scene, const std::string& loading_scene) {
  auto& network = Engine::network();
  if (!network.is_server()) {
    LOG_WARN("NetworkSceneManager::LoadSceneWithLoading can only be called on the server");
    return;
  }

  // Load locally on the server with loading screen
  Engine::scene_manager().LoadSceneWithLoading(target_scene, loading_scene);

  // Track for late joiners
  network_loaded_scenes_.push_back({target_scene, LoadSceneMode::Single, loading_scene});

  // Broadcast to all clients
  auto packet = std::make_shared<SceneLoadPacket>();
  packet->scene_name = target_scene;
  packet->loading_scene = loading_scene;
  packet->load_mode = 0; // ignored
  network.Broadcast(packet);

  LOG_INFO("NetworkSceneManager: loading scene '{}' with loading screen '{}', broadcast to clients",
           target_scene, loading_scene);
}

void NetworkSceneManager::SyncToSession(uint64_t session_id) {
  auto& network = Engine::network();

  for (auto& entry : network_loaded_scenes_) {
    auto packet = std::make_shared<SceneLoadPacket>();
    packet->scene_name = entry.scene_name;
    packet->load_mode = ToInt(entry.load_mode);
    packet->loading_scene = entry.loading_scene;
    network.SendTo(session_id, packet);
  }

  LOG_DEBUG("NetworkSceneManager: synced {} scene loads to session {}",
            network_loaded_scenes_.size(), session_id);
}

bool NetworkSceneManager::IsNetworkScene(const std::string& name) const {
  for (auto& entry : network_loaded_scenes_) {
    if (entry.scene_name == name) {
      return true;
    }
  }
  return false;
}

}  // namespace wiesel
