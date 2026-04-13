
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "game/w_game_application.h"

#include "game/w_game_info.h"
#include "game/w_game_loader.h"
#include "scene/w_scene_manager.h"

namespace wiesel {

bool GameApplication::LoadGame(const std::filesystem::path& game_info_path,
                               const std::filesystem::path& assets_dir) {
  std::unique_ptr<GameInfo> info = GameInfo::Load(game_info_path);
  if (!info) {
    LOG_ERROR("Failed to load game info: {}", game_info_path.string());
    return false;
  }

  auto shared_info = std::shared_ptr<GameInfo>(std::move(info));
  Engine::SetGameInfo(shared_info);

  SceneManager& sm = Engine::scene_manager();
  if (!sm.GetActiveScene()) {
    sm.CreateScene();
  }

  return GameLoader::LoadAll(*shared_info, assets_dir);
}

}  // namespace wiesel
