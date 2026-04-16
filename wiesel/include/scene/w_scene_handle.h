
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

#include "w_pch.h"

namespace wiesel {

class Scene;

// Lightweight handle to a Scene. Safe to copy and store
// Unlike raw pointers, a SceneHandle never dangles - it's just an ID.
// Call Resolve() to get the Scene*, returns nullptr if destroyed.
struct SceneHandle {
  uint32_t id = 0;

  // Resolve to Scene*. Returns nullptr if the scene was destroyed.
  // Implemented in w_scene_handle.cc to avoid circular includes.
  Scene* Resolve() const;

  operator bool() const { return id != 0; }

  bool operator==(const SceneHandle& other) const { return id == other.id; }

  bool operator!=(const SceneHandle& other) const { return id != other.id; }
};

}  // namespace wiesel

namespace std {
template <>
struct hash<wiesel::SceneHandle> {
  size_t operator()(const wiesel::SceneHandle& h) const noexcept {
    return std::hash<uint32_t>{}(h.id);
  }
};
}  // namespace std
