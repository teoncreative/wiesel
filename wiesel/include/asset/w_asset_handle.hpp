//
// Created by Metehan Gezer on 10.02.2026.
//

#pragma once

#include "w_pch.hpp"
#include "util/w_uuid.hpp"

namespace Wiesel {

enum class AssetLoadState : uint8_t {
  Unloaded = 0,
  Loading,
  Loaded,
  Failed
};

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

}  // namespace Wiesel

namespace std {
template <>
struct hash<Wiesel::AssetHandle> {
  size_t operator()(const Wiesel::AssetHandle& h) const noexcept {
    return std::hash<Wiesel::UUID>{}(h.id);
  }
};
}  // namespace std
