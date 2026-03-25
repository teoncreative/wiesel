
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "asset/w_asset_property_registry.hpp"

#include <imgui.h>
#include <nlohmann/json.hpp>
#include "asset/w_asset_properties.hpp"

namespace Wiesel {

std::unordered_map<AssetType, AssetPropertyDesc>&
AssetPropertyRegistry::Registry() {
  static std::unordered_map<AssetType, AssetPropertyDesc> registry;
  return registry;
}

void AssetPropertyRegistry::Register(AssetType type, AssetPropertyDesc desc) {
  Registry()[type] = std::move(desc);
}

const AssetPropertyDesc* AssetPropertyRegistry::Get(AssetType type) {
  auto& reg = Registry();
  auto it = reg.find(type);
  if (it != reg.end()) {
    return &it->second;
  }
  return nullptr;
}

bool AssetPropertyRegistry::HasProperties(AssetType type) {
  return Registry().contains(type);
}

// --- Texture properties ---

static nlohmann::json SerializeTextureProperties(const void* p) {
  auto* props = static_cast<const TextureAssetProperties*>(p);
  nlohmann::json j;
  j["asset_type"] = static_cast<int>(props->asset_type);
  j["filter_mode"] = static_cast<int>(props->filter_mode);
  j["wrap_mode"] = static_cast<int>(props->wrap_mode);
  j["generate_mipmaps"] = props->generate_mipmaps;
  if (props->slice_border.x != 0 || props->slice_border.y != 0 ||
      props->slice_border.z != 0 || props->slice_border.w != 0) {
    j["slice_border"] = {props->slice_border.x, props->slice_border.y,
                         props->slice_border.z, props->slice_border.w};
  }
  return j;
}

static std::shared_ptr<void> DeserializeTextureProperties(
    const nlohmann::json& j) {
  auto props = std::make_shared<TextureAssetProperties>();
  if (j.contains("asset_type")) {
    props->asset_type =
        static_cast<TextureAssetType>(j["asset_type"].get<int>());
  }
  if (j.contains("filter_mode")) {
    props->filter_mode =
        static_cast<TextureFilterMode>(j["filter_mode"].get<int>());
  }
  if (j.contains("wrap_mode")) {
    props->wrap_mode = static_cast<TextureWrapMode>(j["wrap_mode"].get<int>());
  }
  if (j.contains("generate_mipmaps")) {
    props->generate_mipmaps = j["generate_mipmaps"].get<bool>();
  }
  if (j.contains("slice_border") && j["slice_border"].is_array() &&
      j["slice_border"].size() >= 4) {
    props->slice_border = {j["slice_border"][0], j["slice_border"][1],
                           j["slice_border"][2], j["slice_border"][3]};
  }
  return props;
}

static bool RenderTexturePropertiesImGui(void* p) {
  auto* props = static_cast<TextureAssetProperties*>(p);
  bool changed = false;

  const char* asset_types[] = {"Default", "Normal Map", "Sprite (UI)"};
  int at = static_cast<int>(props->asset_type);
  if (ImGui::Combo("Type", &at, asset_types, 3)) {
    props->asset_type = static_cast<TextureAssetType>(at);
    changed = true;
  }

  const char* filter_modes[] = {"Nearest", "Linear"};
  int filter = static_cast<int>(props->filter_mode);
  if (ImGui::Combo("Filter Mode", &filter, filter_modes, 2)) {
    props->filter_mode = static_cast<TextureFilterMode>(filter);
    changed = true;
  }

  const char* wrap_modes[] = {"Repeat", "Clamp", "Mirror"};
  int wrap = static_cast<int>(props->wrap_mode);
  if (ImGui::Combo("Wrap Mode", &wrap, wrap_modes, 3)) {
    props->wrap_mode = static_cast<TextureWrapMode>(wrap);
    changed = true;
  }

  changed |= ImGui::Checkbox("Generate Mipmaps", &props->generate_mipmaps);

  ImGui::SeparatorText("9-Slice");
  changed |= ImGui::DragFloat4("Border (L,T,R,B)",
                               reinterpret_cast<float*>(&props->slice_border),
                               1.0f, 0.0f, 500.0f);

  return changed;
}

// --- Font properties ---

static nlohmann::json SerializeFontProperties(const void* p) {
  auto* props = static_cast<const FontAssetProperties*>(p);
  nlohmann::json j;
  j["aa_mode"] = static_cast<int>(props->aa_mode);
  return j;
}

static std::shared_ptr<void> DeserializeFontProperties(
    const nlohmann::json& j) {
  auto props = std::make_shared<FontAssetProperties>();
  if (j.contains("aa_mode")) {
    props->aa_mode = static_cast<FontAAMode>(j["aa_mode"].get<int>());
  }
  return props;
}

static bool RenderFontPropertiesImGui(void* p) {
  auto* props = static_cast<FontAssetProperties*>(p);
  bool changed = false;

  const char* aa_modes[] = {"None", "Grayscale"};
  int aa = static_cast<int>(props->aa_mode);
  if (ImGui::Combo("Anti-Aliasing", &aa, aa_modes, 2)) {
    props->aa_mode = static_cast<FontAAMode>(aa);
    changed = true;
  }

  return changed;
}

// --- Registration ---

void InitializeAssetProperties() {
  AssetPropertyRegistry::Register(
      AssetType::Texture,
      {[]() -> std::shared_ptr<void> {
         return std::make_shared<TextureAssetProperties>();
       },
       SerializeTextureProperties, DeserializeTextureProperties,
       RenderTexturePropertiesImGui});

  AssetPropertyRegistry::Register(
      AssetType::Font, {[]() -> std::shared_ptr<void> {
                          return std::make_shared<FontAssetProperties>();
                        },
                        SerializeFontProperties, DeserializeFontProperties,
                        RenderFontPropertiesImGui});
}

}  // namespace Wiesel