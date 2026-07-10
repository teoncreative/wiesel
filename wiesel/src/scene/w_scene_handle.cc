
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "scene/w_scene_handle.h"

#include "scene/w_scene_manager.h"
#include "w_engine.h"

namespace wiesel {

Scene* SceneHandle::Resolve() const {
  if (id == 0) {
    return nullptr;
  }
  return Engine::scene_manager().Get(*this);
}

}  // namespace wiesel
