
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include <variant>

#include <nlohmann/json.hpp>

#include "asset/w_asset_handle.hpp"
#include "rendering/w_texture.hpp"
#include "util/w_color.hpp"
#include "util/w_utils.hpp"
#include "w_pch.hpp"

namespace Wiesel {
static constexpr int kMaterialTextureCount = 7;

// --- Material Property System ---

enum class MaterialPropertyType { Float, Vec2, Vec3, Vec4, Texture, Bool, Int };

using MaterialPropertyValue =
    std::variant<float, glm::vec2, glm::vec3, glm::vec4,
                 std::shared_ptr<Texture>, bool, int32_t>;

struct MaterialProperty {
  MaterialProperty() = default;

  MaterialProperty(std::string name, std::string display_name,
                   MaterialPropertyType type,
                   MaterialPropertyValue default_value, float min_value = 0.0f,
                   float max_value = 1.0f)
      : name(std::move(name)),
        display_name(std::move(display_name)),
        type(type),
        default_value(std::move(default_value)),
        min_value(min_value),
        max_value(max_value) {}

  std::string name;
  std::string display_name;  // for editor UI
  MaterialPropertyType type = MaterialPropertyType::Float;
  MaterialPropertyValue default_value = 0.0f;
  float min_value = 0.0f;  // for float/int slider range
  float max_value = 1.0f;
};

// --- Material Features ---

struct MaterialFeature {
  MaterialFeature() = default;

  MaterialFeature(std::string name, std::string define_name,
                  std::vector<MaterialProperty> properties)
      : name(std::move(name)),
        define_name(std::move(define_name)),
        properties(std::move(properties)) {}

  std::string name;         // e.g. "Emission"
  std::string define_name;  // e.g. "FEATURE_EMISSION" - used for shader #ifdef
  std::vector<MaterialProperty> properties;  // feature-specific defaults
};

// Built-in feature registry
namespace MaterialFeatures {
inline MaterialFeature Emission() {
  return {"Emission",
          "FEATURE_EMISSION",
          {
              {"emission_color", "Emission Color", MaterialPropertyType::Vec4,
               glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)},
              {"emission_strength", "Emission Strength",
               MaterialPropertyType::Float, 1.0f, 0.0f, 10.0f},
          }};
}

inline MaterialFeature AlphaClip() {
  return {"AlphaClip",
          "FEATURE_ALPHA_CLIP",
          {
              {"alpha_cutoff", "Alpha Cutoff", MaterialPropertyType::Float,
               0.5f, 0.0f, 1.0f},
          }};
}
}  // namespace MaterialFeatures

// --- Material ---

struct Material {
  Material();
  ~Material();

  std::string name;
  AssetHandle asset_handle;  // handle in AssetManager (set during registration)
  uint32_t version = 0;      // bumped when textures/properties change

  void MarkDirty() { version++; }

  // Texture slots (backward compatible with existing mesh import)
  std::shared_ptr<Texture> base_texture;
  std::shared_ptr<Texture> normal_map;
  std::shared_ptr<Texture> specular_map;
  std::shared_ptr<Texture> height_map;
  std::shared_ptr<Texture> albedo_map;
  std::shared_ptr<Texture> roughness_map;
  std::shared_ptr<Texture> metallic_map;

  // Material properties (scalar/vector values)
  std::unordered_map<std::string, MaterialPropertyValue> properties;

  // Active features
  std::set<std::string> enabled_features;

  // Set/get typed properties
  void SetProperty(const std::string& name, MaterialPropertyValue value);
  MaterialPropertyValue GetProperty(const std::string& name) const;
  bool HasProperty(const std::string& name) const;

  // Feature management
  void EnableFeature(const MaterialFeature& feature);
  void DisableFeature(const std::string& feature_name);
  bool HasFeature(const std::string& feature_name) const;

  // Get shader defines from enabled features
  std::vector<std::string> GetShaderDefines() const;

  // Convenience accessors for common PBR properties
  glm::vec4 GetColorTint() const;
  float GetRoughness() const;
  float GetMetallic() const;
  float GetSpecular() const;

  void SetColorTint(const glm::vec4& color);
  void SetRoughness(float value);
  void SetMetallic(float value);
  void SetSpecular(float value);

  // Initialize with default PBR properties
  void InitDefaults();

  // Serialize/deserialize to/from JSON
  nlohmann::json Serialize() const;
  static std::shared_ptr<Material> Deserialize(const nlohmann::json& j);

  static void Set(std::shared_ptr<Material> material,
                  std::shared_ptr<Texture> texture, TextureType type);
};

// --- Material Instance ---
// Per-entity overrides on top of a base Material

struct MaterialInstance {
  AssetHandle base_material_handle;  // references a Material asset
  std::unordered_map<std::string, MaterialPropertyValue> overrides;

  // Resolve the base material from AssetManager
  std::shared_ptr<Material> GetBaseMaterial() const;

  MaterialPropertyValue GetEffectiveProperty(const std::string& name) const;
  float GetEffectiveFloat(const std::string& name) const;
  glm::vec4 GetEffectiveVec4(const std::string& name) const;

  void SetOverride(const std::string& name, MaterialPropertyValue value);
  void ClearOverride(const std::string& name);
  bool HasOverride(const std::string& name) const;

  // Convenience accessors (read effective, write to override)
  glm::vec4 GetColorTint() const;
  float GetRoughness() const;
  float GetMetallic() const;
  float GetSpecular() const;

  void SetColorTint(const glm::vec4& color);
  void SetRoughness(float value);
  void SetMetallic(float value);
  void SetSpecular(float value);
};

}  // namespace Wiesel
