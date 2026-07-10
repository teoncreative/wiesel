
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "scene/w_scene_manager.h"
#include "w_pch.h"

namespace wiesel {

class NetworkManager;

class NetworkSceneManager {
 public:
  // Load a scene on server AND broadcast to all clients.
  // Must be called on the server. Clients receive a SceneLoadPacket
  // and load the scene automatically.
  void LoadScene(const std::string& name, LoadSceneMode mode);

  // Load a scene with a loading screen on server AND all clients.
  // Each side shows their own loading screen independently.
  void LoadSceneWithLoading(const std::string& target_scene,
                            const std::string& loading_scene);

  // Send all tracked scene loads to a specific session (late joiner sync).
  void SyncToSession(uint64_t session_id);

  // Check if a scene was loaded via network.
  bool IsNetworkScene(const std::string& name) const;

 private:
  struct SceneLoadEntry {
    std::string scene_name;
    LoadSceneMode load_mode;
    std::string loading_scene;
  };
  std::vector<SceneLoadEntry> network_loaded_scenes_;
};

}  // namespace wiesel
