
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "rendering/w_material.hpp"

#include "asset/w_asset_manager.hpp"
#include "w_engine.hpp"

namespace Wiesel {

// --- Material ---

Material::Material() {
  InitDefaults();
}

Material::~Material() {
  base_texture = nullptr;
  normal_map = nullptr;
  specular_map = nullptr;
  height_map = nullptr;
  albedo_map = nullptr;
  roughness_map = nullptr;
  metallic_map = nullptr;
}

void Material::InitDefaults() {
  properties["color_tint"] = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
  properties["roughness"] = 0.5f;
  properties["metallic"] = 0.0f;
  properties["specular"] = 0.5f;
}

void Material::SetProperty(const std::string& name,
                           MaterialPropertyValue value) {
  properties[name] = std::move(value);
  MarkDirty();
}

MaterialPropertyValue Material::GetProperty(const std::string& name) const {
  auto it = properties.find(name);
  if (it != properties.end()) {
    return it->second;
  }
  return 0.0f;  // default
}

bool Material::HasProperty(const std::string& name) const {
  return properties.contains(name);
}

void Material::EnableFeature(const MaterialFeature& feature) {
  enabled_features.insert(feature.name);
  // Add feature's default properties if not already set
  for (const auto& prop : feature.properties) {
    if (!HasProperty(prop.name)) {
      SetProperty(prop.name, prop.default_value);
    }
  }
}

void Material::DisableFeature(const std::string& feature_name) {
  enabled_features.erase(feature_name);
}

bool Material::HasFeature(const std::string& feature_name) const {
  return enabled_features.contains(feature_name);
}

std::vector<std::string> Material::GetShaderDefines() const {
  std::vector<std::string> defines;
  // Map feature names to their shader defines
  static const std::unordered_map<std::string, std::string> feature_defines = {
      {"Emission", "FEATURE_EMISSION"},
      {"AlphaClip", "FEATURE_ALPHA_CLIP"},
  };
  for (const auto& feature : enabled_features) {
    auto it = feature_defines.find(feature);
    if (it != feature_defines.end()) {
      defines.push_back(it->second);
    }
  }
  return defines;
}

glm::vec4 Material::GetColorTint() const {
  auto it = properties.find("color_tint");
  if (it != properties.end() && std::holds_alternative<glm::vec4>(it->second)) {
    return std::get<glm::vec4>(it->second);
  }
  return glm::vec4(1.0f);
}

float Material::GetRoughness() const {
  auto it = properties.find("roughness");
  if (it != properties.end() && std::holds_alternative<float>(it->second)) {
    return std::get<float>(it->second);
  }
  return 0.5f;
}

float Material::GetMetallic() const {
  auto it = properties.find("metallic");
  if (it != properties.end() && std::holds_alternative<float>(it->second)) {
    return std::get<float>(it->second);
  }
  return 0.0f;
}

float Material::GetSpecular() const {
  auto it = properties.find("specular");
  if (it != properties.end() && std::holds_alternative<float>(it->second)) {
    return std::get<float>(it->second);
  }
  return 0.5f;
}

void Material::SetColorTint(const glm::vec4& color) {
  properties["color_tint"] = color;
  MarkDirty();
}

void Material::SetRoughness(float value) {
  properties["roughness"] = value;
  MarkDirty();
}

void Material::SetMetallic(float value) {
  properties["metallic"] = value;
  MarkDirty();
}

void Material::SetSpecular(float value) {
  properties["specular"] = value;
  MarkDirty();
}

nlohmann::json Material::Serialize() const {
  nlohmann::json j;
  j["name"] = name;
  j["color_tint"] = {GetColorTint().x, GetColorTint().y, GetColorTint().z,
                     GetColorTint().w};
  j["roughness"] = GetRoughness();
  j["metallic"] = GetMetallic();
  j["specular"] = GetSpecular();

  // Serialize texture references as asset paths
  nlohmann::json textures;
  auto& mgr = Engine::asset_manager();
  auto serialize_tex = [&](const char* key,
                           const std::shared_ptr<Texture>& tex) {
    if (tex && !tex->path_.empty()) {
      textures[key] = tex->path_;
    }
  };
  serialize_tex("diffuse", base_texture);
  serialize_tex("normal", normal_map);
  serialize_tex("specular_map", specular_map);
  serialize_tex("height", height_map);
  serialize_tex("albedo", albedo_map);
  serialize_tex("roughness_map", roughness_map);
  serialize_tex("metallic_map", metallic_map);
  if (!textures.empty()) {
    j["textures"] = textures;
  }

  if (!enabled_features.empty()) {
    j["features"] = nlohmann::json::array();
    for (const auto& f : enabled_features) {
      j["features"].push_back(f);
    }
  }

  return j;
}

std::shared_ptr<Material> Material::Deserialize(const nlohmann::json& j) {
  auto mat = std::make_shared<Material>();
  mat->name = j.value("name", "");

  if (j.contains("color_tint") && j["color_tint"].is_array() &&
      j["color_tint"].size() >= 4) {
    mat->SetColorTint(glm::vec4(j["color_tint"][0], j["color_tint"][1],
                                j["color_tint"][2], j["color_tint"][3]));
  }
  if (j.contains("roughness")) {
    mat->SetRoughness(j["roughness"].get<float>());
  }
  if (j.contains("metallic")) {
    mat->SetMetallic(j["metallic"].get<float>());
  }
  if (j.contains("specular")) {
    mat->SetSpecular(j["specular"].get<float>());
  }

  // Texture paths are stored but loading is deferred until the material is used
  // (textures are loaded by the model import pipeline or resolved separately)

  if (j.contains("features") && j["features"].is_array()) {
    for (const auto& f : j["features"]) {
      mat->enabled_features.insert(f.get<std::string>());
    }
  }

  return mat;
}

void Material::Set(std::shared_ptr<Material> material,
                   std::shared_ptr<Texture> texture, TextureType type) {
  switch (type) {
    case TextureTypeNone:
      break;
    case TextureTypeDiffuse:
      material->base_texture = texture;
      break;
    case TextureTypeSpecular:
      material->specular_map = texture;
      break;
    case TextureTypeAmbient:
      break;
    case TextureTypeEmissive:
      break;
    case TextureTypeHeight:
      material->height_map = texture;
      break;
    case TextureTypeNormals:
      material->normal_map = texture;
      break;
    case TextureTypeShininess:
      break;
    case TextureTypeOpacty:
      break;
    case TextureTypeDisplacement:
      break;
    case TextureTypeLightmap:
      break;
    case TextureTypeReflection:
      break;
    case TextureTypeBaseColor:
      material->albedo_map = texture;
      break;
    case TextureTypeNormalCamera:
      material->normal_map = texture;
      break;
    case TextureTypeEmissionColor:
      break;
    case TextureTypeMetalness:
      material->metallic_map = texture;
      break;
    case TextureTypeDiffuseRoughness:
      material->roughness_map = texture;
      break;
    case TextureTypeAmbientOcclusion:
      break;
    case TextureTypeSheen:
      break;
    case TextureTypeClearcoat:
      break;
    case TextureTypeTransmission:
      break;
  }
  material->MarkDirty();
}

// --- MaterialInstance ---

std::shared_ptr<Material> MaterialInstance::GetBaseMaterial() const {
  if (!base_material_handle.IsValid()) {
    return nullptr;
  }
  return Engine::asset_manager().Get<Material>(base_material_handle);
}

MaterialPropertyValue MaterialInstance::GetEffectiveProperty(
    const std::string& name) const {
  auto it = overrides.find(name);
  if (it != overrides.end()) {
    return it->second;
  }
  auto base = GetBaseMaterial();
  if (base) {
    return base->GetProperty(name);
  }
  return 0.0f;
}

float MaterialInstance::GetEffectiveFloat(const std::string& name) const {
  auto val = GetEffectiveProperty(name);
  if (std::holds_alternative<float>(val)) {
    return std::get<float>(val);
  }
  return 0.0f;
}

glm::vec4 MaterialInstance::GetEffectiveVec4(const std::string& name) const {
  auto val = GetEffectiveProperty(name);
  if (std::holds_alternative<glm::vec4>(val)) {
    return std::get<glm::vec4>(val);
  }
  return glm::vec4(0.0f);
}

void MaterialInstance::SetOverride(const std::string& name,
                                   MaterialPropertyValue value) {
  overrides[name] = std::move(value);
}

void MaterialInstance::ClearOverride(const std::string& name) {
  overrides.erase(name);
}

bool MaterialInstance::HasOverride(const std::string& name) const {
  return overrides.contains(name);
}

glm::vec4 MaterialInstance::GetColorTint() const {
  return GetEffectiveVec4("color_tint");
}

float MaterialInstance::GetRoughness() const {
  return GetEffectiveFloat("roughness");
}

float MaterialInstance::GetMetallic() const {
  return GetEffectiveFloat("metallic");
}

float MaterialInstance::GetSpecular() const {
  return GetEffectiveFloat("specular");
}

void MaterialInstance::SetColorTint(const glm::vec4& color) {
  overrides["color_tint"] = color;
}

void MaterialInstance::SetRoughness(float value) {
  overrides["roughness"] = value;
}

void MaterialInstance::SetMetallic(float value) {
  overrides["metallic"] = value;
}

void MaterialInstance::SetSpecular(float value) {
  overrides["specular"] = value;
}

}  // namespace Wiesel