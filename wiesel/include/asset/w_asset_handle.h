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

#include <urkern/uuid.h>
#include "w_pch.h"

namespace wiesel {

enum class AssetLoadState : uint8_t { Unloaded = 0, Loading, Loaded, Failed };

enum class AssetType : uint8_t {
  None = 0,
  Texture = 1,
  Model = 2,
  Material = 3,
  Shader = 4,
  Sprite = 5,
  Skybox = 6,
  Font = 7,
  Script = 8,
  Scene = 9,
  Prefab = 10,
  Audio = 11,
  AnimClip = 12,
  AnimController = 13,
  UIDocument = 14,
  UIStylesheet = 15,
  CursorSet = 16,
  MeshCollider = 17,
  Count
};

const char* AssetTypeToString(AssetType type);
AssetType AssetTypeFromString(std::string_view s);

struct AssetHandle {
  urkern::UUID id;

  AssetHandle() : id() {}

  explicit AssetHandle(urkern::UUID uuid) : id(uuid) {}

  static AssetHandle Generate() { return AssetHandle(urkern::UUID::GenerateV4()); }

  bool IsValid() const { return !id.IsNil(); }

  std::string ToString() const { return id.ToString(); }

  static AssetHandle FromString(std::string_view s) {
    return AssetHandle(urkern::UUID::FromString(s));
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
    urkern::UUID::FromString("00000000-0000-4000-8000-000000000001")};
inline const AssetHandle kPrimitiveSphere{
    urkern::UUID::FromString("00000000-0000-4000-8000-000000000002")};
inline const AssetHandle kPrimitivePlane{
    urkern::UUID::FromString("00000000-0000-4000-8000-000000000003")};
inline const AssetHandle kPrimitiveCylinder{
    urkern::UUID::FromString("00000000-0000-4000-8000-000000000004")};
inline const AssetHandle kPrimitiveCapsule{
    urkern::UUID::FromString("00000000-0000-4000-8000-000000000005")};

}  // namespace wiesel

namespace std {
template <>
struct hash<wiesel::AssetHandle> {
  size_t operator()(const wiesel::AssetHandle& h) const noexcept {
    return std::hash<urkern::UUID>{}(h.id);
  }
};
}  // namespace std
