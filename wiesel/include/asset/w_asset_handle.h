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
// Created by Metehan Gezer on 10.02.2026.
//

#pragma once

#include "util/w_uuid.h"
#include "w_pch.h"

namespace Wiesel {

enum class AssetLoadState : uint8_t { Unloaded = 0, Loading, Loaded, Failed };

enum class AssetType : uint8_t {
  None = 0,
  Texture,
  Model,
  Material,
  Shader,
  Sprite,
  Skybox,
  Font,
  Script,
  Scene,
  Prefab,
  Audio,
  AnimClip,
  AnimController,
  UIDocument,
  UIStylesheet,
  CursorSet,
  MeshCollider,
  Count
};

const char* AssetTypeToString(AssetType type);
AssetType AssetTypeFromString(std::string_view s);

struct AssetHandle {
  UUID id;

  AssetHandle() : id() {}

  explicit AssetHandle(UUID uuid) : id(uuid) {}

  static AssetHandle Generate() { return AssetHandle(UUID::GenerateV4()); }

  bool IsValid() const { return !id.IsNil(); }

  std::string ToString() const { return id.ToString(); }

  static AssetHandle FromString(std::string_view s) {
    return AssetHandle(UUID::FromString(s));
  }

  friend bool operator==(const AssetHandle& a, const AssetHandle& b) {
    return a.id == b.id;
  }

  friend bool operator!=(const AssetHandle& a, const AssetHandle& b) {
    return !(a == b);
  }

  friend bool operator<(const AssetHandle& a, const AssetHandle& b) {
    return a.id < b.id;
  }

  operator bool() const { return IsValid(); }
};

inline const AssetHandle kNullAssetHandle{};

// Fixed primitive asset handles - deterministic across sessions
inline const AssetHandle kPrimitiveCube{
    UUID::FromString("00000000-0000-4000-8000-000000000001")};
inline const AssetHandle kPrimitiveSphere{
    UUID::FromString("00000000-0000-4000-8000-000000000002")};
inline const AssetHandle kPrimitivePlane{
    UUID::FromString("00000000-0000-4000-8000-000000000003")};
inline const AssetHandle kPrimitiveCylinder{
    UUID::FromString("00000000-0000-4000-8000-000000000004")};
inline const AssetHandle kPrimitiveCapsule{
    UUID::FromString("00000000-0000-4000-8000-000000000005")};

}  // namespace Wiesel

namespace std {
template <>
struct hash<Wiesel::AssetHandle> {
  size_t operator()(const Wiesel::AssetHandle& h) const noexcept {
    return std::hash<Wiesel::UUID>{}(h.id);
  }
};
}  // namespace std
