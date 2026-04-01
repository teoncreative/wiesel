
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "asset/w_asset_property_registry.h"

#include <imgui.h>
#include <nlohmann/json.hpp>
#include "asset/w_asset_properties.h"

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

// --- UIDocument properties ---

static nlohmann::json SerializeUIDocumentProperties(const void* p) {
  auto* props = static_cast<const UIDocumentAssetProperties*>(p);
  nlohmann::json j;
  nlohmann::json vars = nlohmann::json::array();
  for (const auto& v : props->variables) {
    nlohmann::json var;
    var["name"] = v.name;
    var["type"] = static_cast<int>(v.type);
    var["mode"] = static_cast<int>(v.mode);
    var["default"] = v.default_value;
    vars.push_back(var);
  }
  j["variables"] = vars;
  return j;
}

static std::shared_ptr<void> DeserializeUIDocumentProperties(
    const nlohmann::json& j) {
  auto props = std::make_shared<UIDocumentAssetProperties>();
  if (j.contains("variables") && j["variables"].is_array()) {
    for (const auto& var : j["variables"]) {
      UIVariableDecl decl;
      if (var.contains("name")) {
        decl.name = var["name"].get<std::string>();
      }
      if (var.contains("type")) {
        decl.type = static_cast<UIVariableType>(var["type"].get<int>());
      }
      if (var.contains("mode")) {
        decl.mode = static_cast<UIVariableMode>(var["mode"].get<int>());
      }
      if (var.contains("default")) {
        decl.default_value = var["default"].get<std::string>();
      }
      props->variables.push_back(decl);
    }
  }
  return props;
}

static bool RenderUIDocumentPropertiesImGui(void* p) {
  auto* props = static_cast<UIDocumentAssetProperties*>(p);
  bool changed = false;

  ImGui::SeparatorText("Data Variables");

  int remove_idx = -1;
  for (int i = 0; i < static_cast<int>(props->variables.size()); i++) {
    auto& v = props->variables[i];
    ImGui::PushID(i);

    // Name
    char name_buf[128];
    strncpy(name_buf, v.name.c_str(), sizeof(name_buf) - 1);
    name_buf[sizeof(name_buf) - 1] = '\0';
    ImGui::SetNextItemWidth(120);
    if (ImGui::InputText("##name", name_buf, sizeof(name_buf))) {
      v.name = name_buf;
      changed = true;
    }
    ImGui::SameLine();

    // Type
    const char* type_names[] = {"Int", "Float", "String", "Bool"};
    int type_idx = static_cast<int>(v.type);
    ImGui::SetNextItemWidth(70);
    if (ImGui::Combo("##type", &type_idx, type_names, 4)) {
      v.type = static_cast<UIVariableType>(type_idx);
      changed = true;
    }
    ImGui::SameLine();

    // Mode
    const char* mode_names[] = {"TwoWay", "ReadOnly"};
    int mode_idx = static_cast<int>(v.mode);
    ImGui::SetNextItemWidth(80);
    if (ImGui::Combo("##mode", &mode_idx, mode_names, 2)) {
      v.mode = static_cast<UIVariableMode>(mode_idx);
      changed = true;
    }
    ImGui::SameLine();

    // Default value
    char def_buf[128];
    strncpy(def_buf, v.default_value.c_str(), sizeof(def_buf) - 1);
    def_buf[sizeof(def_buf) - 1] = '\0';
    ImGui::SetNextItemWidth(80);
    if (ImGui::InputText("##default", def_buf, sizeof(def_buf))) {
      v.default_value = def_buf;
      changed = true;
    }
    ImGui::SameLine();

    // Remove button
    if (ImGui::SmallButton("X")) {
      remove_idx = i;
      changed = true;
    }

    ImGui::PopID();
  }

  if (remove_idx >= 0) {
    props->variables.erase(props->variables.begin() + remove_idx);
  }

  if (ImGui::Button("+ Add Variable")) {
    UIVariableDecl decl;
    decl.name = "var_" + std::to_string(props->variables.size());
    props->variables.push_back(decl);
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

  AssetPropertyRegistry::Register(
      AssetType::UIDocument,
      {[]() -> std::shared_ptr<void> {
         return std::make_shared<UIDocumentAssetProperties>();
       },
       SerializeUIDocumentProperties, DeserializeUIDocumentProperties,
       RenderUIDocumentPropertiesImGui});
}

}  // namespace Wiesel