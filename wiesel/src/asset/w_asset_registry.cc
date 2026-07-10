//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "asset/w_asset_registry.h"

#include <nlohmann/json.hpp>

#include "animation/w_animation_clip_asset.h"
#include "animation/w_animation_controller.h"
#include "animation/w_animation_controller_asset.h"
#include "asset/w_asset_properties.h"
#include "asset/w_model_loader.h"
#include "cursor/w_cursor.h"
#include "physics/w_mesh_collider_asset.h"
#include "rendering/w_material.h"
#include "rendering/w_renderer.h"
#include "rendering/w_skybox.h"
#include "rendering/w_sprite.h"
#include "rendering/w_sprite_asset.h"
#include "ui/w_font.h"
#include "ui/w_ui_document.h"
#include "util/w_logger.h"
#include "util/w_vfs.h"
#include "w_engine.h"

namespace wiesel {

// --- AssetRegistry core methods ---

std::unordered_map<AssetType, AssetTypeDesc>& AssetRegistry::Registry() {
  static std::unordered_map<AssetType, AssetTypeDesc> registry;
  return registry;
}

void AssetRegistry::Register(AssetType type, AssetTypeDesc desc) {
  Registry()[type] = std::move(desc);
}

const AssetTypeDesc* AssetRegistry::Get(AssetType type) {
  auto& reg = Registry();
  auto it = reg.find(type);
  if (it != reg.end()) {
    return &it->second;
  }
  return nullptr;
}

bool AssetRegistry::HasLoader(AssetType type) {
  const auto* desc = Get(type);
  return desc && desc->Load;
}

bool AssetRegistry::HasSerializer(AssetType type) {
  const auto* desc = Get(type);
  return desc && desc->IsJsonAsset();
}

bool AssetRegistry::HasProperties(AssetType type) {
  const auto* desc = Get(type);
  return desc && desc->HasProperties();
}

bool AssetRegistry::Save(AssetHandle handle) {
  const auto* meta = Engine::asset_manager().GetMetadata(handle);
  if (!meta) {
    return false;
  }
  return Save(handle, meta->virtual_source_path);
}

bool AssetRegistry::Save(AssetHandle handle, const std::string& vfs_path) {
  const auto* meta = Engine::asset_manager().GetMetadata(handle);
  if (!meta) {
    return false;
  }

  const auto* desc = Get(meta->type);
  if (!desc) {
    LOG_ERROR("No descriptor registered for asset type {}",
              static_cast<int>(meta->type));
    return false;
  }

  if (desc->SerializeBinary) {
    urkern::Buffer buf;
    desc->SerializeBinary(handle, buf);
    return Engine::vfs()->WriteFile(vfs_path,
                                    std::string(buf.data(), buf.size()));
  }

  if (!desc->Serialize) {
    LOG_ERROR("No serializer registered for asset type {}",
              static_cast<int>(meta->type));
    return false;
  }

  nlohmann::json j = desc->Serialize(handle);
  return Engine::vfs()->WriteFile(vfs_path, j.dump(2));
}

bool AssetRegistry::LoadBinary(AssetHandle handle) {
  const auto* meta = Engine::asset_manager().GetMetadata(handle);
  if (!meta) {
    return false;
  }

  const auto* desc = Get(meta->type);
  if (!desc || !desc->DeserializeBinary) {
    return false;
  }

  VfsFile file = Engine::vfs()->Open(meta->virtual_source_path);
  if (!file) {
    return false;
  }

  std::string content((std::istreambuf_iterator<char>(file.Stream())),
                      std::istreambuf_iterator<char>());
  urkern::Buffer buf(content.data(), content.size());
  return desc->DeserializeBinary(handle, buf);
}

bool AssetRegistry::LoadJson(AssetHandle handle) {
  const auto* meta = Engine::asset_manager().GetMetadata(handle);
  if (!meta) {
    return false;
  }

  const auto* desc = Get(meta->type);
  if (!desc || !desc->Deserialize) {
    return false;
  }

  VfsFile file = Engine::vfs()->Open(meta->virtual_source_path);
  if (!file) {
    return false;
  }

  try {
    std::string content((std::istreambuf_iterator<char>(file.Stream())),
                        std::istreambuf_iterator<char>());
    nlohmann::json j = nlohmann::json::parse(content);
    return desc->Deserialize(handle, j);
  } catch (const std::exception& e) {
    LOG_ERROR("Failed to load asset '{}': {}", meta->virtual_source_path,
              e.what());
    return false;
  }
}

// --- Meta file operations ---

AssetRegistry::MetaFileData AssetRegistry::ReadMetaFile(
    const nlohmann::json& j) {
  MetaFileData result;
  result.handle = AssetHandle::FromString(j.value("handle", ""));
  if (j.contains("type")) {
    result.type = AssetTypeFromString(j["type"].get<std::string>());
  }
  if (j.contains("properties") && j["properties"].is_object()) {
    result.properties = j["properties"];
  }
  return result;
}

AssetRegistry::MetaFileData AssetRegistry::ReadMetaFile(
    const std::filesystem::path& path) {
  if (!std::filesystem::exists(path)) {
    return {};
  }
  std::ifstream file(path);
  if (!file.is_open()) {
    return {};
  }
  try {
    nlohmann::json j;
    file >> j;
    return ReadMetaFile(j);
  } catch (...) {
    return {};
  }
}

void AssetRegistry::WriteMetaFile(const std::filesystem::path& path,
                                  const AssetHandle& handle, AssetType type,
                                  const void* properties) {
  nlohmann::json j;
  j["handle"] = handle.ToString();
  if (type != AssetType::None) {
    j["type"] = AssetTypeToString(type);
  }
  if (properties) {
    const auto* desc = Get(type);
    if (desc && desc->SerializeProperties) {
      j["properties"] = desc->SerializeProperties(properties);
    }
  }
  std::ofstream file(path);
  if (file.is_open()) {
    file << j.dump(2);
  }
}

// --- Helper functions - transition conditions (animation controller) ---

static void ParseTransitionConditions(const nlohmann::json& conds_json,
                                      std::vector<TransitionCondition>& out) {
  for (const auto& condj : conds_json) {
    TransitionCondition cond;
    cond.param_name = condj.value("param", "");

    std::string type_str = condj.value("type", "Bool");
    if (type_str == "Bool") {
      cond.param_type = AnimParamType::Bool;
    } else if (type_str == "Int") {
      cond.param_type = AnimParamType::Int;
    } else if (type_str == "Float") {
      cond.param_type = AnimParamType::Float;
    } else if (type_str == "Trigger") {
      cond.param_type = AnimParamType::Trigger;
    }

    std::string op_str = condj.value("op", "Equals");
    if (op_str == "Equals") {
      cond.op = ConditionOp::Equals;
    } else if (op_str == "NotEquals") {
      cond.op = ConditionOp::NotEquals;
    } else if (op_str == "Greater") {
      cond.op = ConditionOp::Greater;
    } else if (op_str == "Less") {
      cond.op = ConditionOp::Less;
    }

    if (cond.param_type == AnimParamType::Bool ||
        cond.param_type == AnimParamType::Trigger) {
      cond.value.b = condj.value("value", true);
    } else if (cond.param_type == AnimParamType::Int) {
      cond.value.i = condj.value("value", 0);
    } else if (cond.param_type == AnimParamType::Float) {
      cond.value.f = condj.value("value", 0.0f);
    }

    if (!cond.param_name.empty()) {
      out.push_back(std::move(cond));
    }
  }
}

static nlohmann::json SerializeTransitionConditions(
    const std::vector<TransitionCondition>& conditions) {
  nlohmann::json arr = nlohmann::json::array();
  for (const auto& cond : conditions) {
    nlohmann::json cj;
    cj["param"] = cond.param_name;

    switch (cond.param_type) {
      case AnimParamType::Bool:
        cj["type"] = "Bool";
        cj["value"] = cond.value.b;
        break;
      case AnimParamType::Int:
        cj["type"] = "Int";
        cj["value"] = cond.value.i;
        break;
      case AnimParamType::Float:
        cj["type"] = "Float";
        cj["value"] = cond.value.f;
        break;
      case AnimParamType::Trigger:
        cj["type"] = "Trigger";
        cj["value"] = cond.value.b;
        break;
    }

    switch (cond.op) {
      case ConditionOp::Equals:
        cj["op"] = "Equals";
        break;
      case ConditionOp::NotEquals:
        cj["op"] = "NotEquals";
        break;
      case ConditionOp::Greater:
        cj["op"] = "Greater";
        break;
      case ConditionOp::Less:
        cj["op"] = "Less";
        break;
    }
    arr.push_back(cj);
  }
  return arr;
}

// --- Helper functions - property curves (animation clips) ---

static nlohmann::json SerializePropertyCurve(const PropertyCurve& curve) {
  nlohmann::json cj;
  cj["component"] = curve.target_component;
  cj["field"] = curve.target_field;
  cj["interp"] = (curve.interp == CurveInterp::Step) ? "Step" : "Linear";

  auto serialize_keys = [](const auto& keys) {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& k : keys) {
      nlohmann::json kj;
      kj["t"] = k.time;
      if constexpr (std::is_same_v<std::decay_t<decltype(k.value)>, float>) {
        kj["v"] = k.value;
      } else if constexpr (std::is_same_v<std::decay_t<decltype(k.value)>,
                                          int>) {
        kj["v"] = k.value;
      } else if constexpr (std::is_same_v<std::decay_t<decltype(k.value)>,
                                          bool>) {
        kj["v"] = k.value;
      } else if constexpr (std::is_same_v<std::decay_t<decltype(k.value)>,
                                          AssetHandle>) {
        kj["v"] = k.value.ToString();
      } else if constexpr (std::is_same_v<std::decay_t<decltype(k.value)>,
                                          glm::vec2>) {
        kj["v"] = {k.value.x, k.value.y};
      } else if constexpr (std::is_same_v<std::decay_t<decltype(k.value)>,
                                          glm::vec3>) {
        kj["v"] = {k.value.x, k.value.y, k.value.z};
      } else if constexpr (std::is_same_v<std::decay_t<decltype(k.value)>,
                                          glm::vec4>) {
        kj["v"] = {k.value.x, k.value.y, k.value.z, k.value.w};
      } else if constexpr (std::is_same_v<std::decay_t<decltype(k.value)>,
                                          glm::quat>) {
        kj["v"] = {k.value.w, k.value.x, k.value.y, k.value.z};
      }
      arr.push_back(kj);
    }
    return arr;
  };

  if (!curve.float_keys.empty()) {
    cj["value_type"] = "float";
    cj["keys"] = serialize_keys(curve.float_keys);
  } else if (!curve.vec2_keys.empty()) {
    cj["value_type"] = "vec2";
    cj["keys"] = serialize_keys(curve.vec2_keys);
  } else if (!curve.vec3_keys.empty()) {
    cj["value_type"] = "vec3";
    cj["keys"] = serialize_keys(curve.vec3_keys);
  } else if (!curve.vec4_keys.empty()) {
    cj["value_type"] = "vec4";
    cj["keys"] = serialize_keys(curve.vec4_keys);
  } else if (!curve.quat_keys.empty()) {
    cj["value_type"] = "quat";
    cj["keys"] = serialize_keys(curve.quat_keys);
  } else if (!curve.int_keys.empty()) {
    cj["value_type"] = "int";
    cj["keys"] = serialize_keys(curve.int_keys);
  } else if (!curve.bool_keys.empty()) {
    cj["value_type"] = "bool";
    cj["keys"] = serialize_keys(curve.bool_keys);
  } else if (!curve.asset_keys.empty()) {
    cj["value_type"] = "asset";
    cj["keys"] = serialize_keys(curve.asset_keys);
  }
  return cj;
}

static PropertyCurve ParsePropertyCurve(const nlohmann::json& cj) {
  PropertyCurve curve;
  curve.target_component = cj.value("component", "");
  curve.target_field = cj.value("field", "");
  std::string interp_str = cj.value("interp", "Linear");
  curve.interp =
      (interp_str == "Step") ? CurveInterp::Step : CurveInterp::Linear;

  std::string value_type = cj.value("value_type", "");
  if (!cj.contains("keys") || !cj["keys"].is_array()) {
    return curve;
  }

  for (const auto& kj : cj["keys"]) {
    float t = kj.value("t", 0.0f);
    if (value_type == "float") {
      curve.float_keys.push_back({t, kj.value("v", 0.0f)});
    } else if (value_type == "vec2") {
      auto v = kj["v"];
      curve.vec2_keys.push_back({t, {v[0].get<float>(), v[1].get<float>()}});
    } else if (value_type == "vec3") {
      auto v = kj["v"];
      curve.vec3_keys.push_back(
          {t, {v[0].get<float>(), v[1].get<float>(), v[2].get<float>()}});
    } else if (value_type == "vec4") {
      auto v = kj["v"];
      curve.vec4_keys.push_back({t,
                                 {v[0].get<float>(), v[1].get<float>(),
                                  v[2].get<float>(), v[3].get<float>()}});
    } else if (value_type == "quat") {
      auto v = kj["v"];
      curve.quat_keys.push_back(
          {t, glm::quat(v[0].get<float>(), v[1].get<float>(), v[2].get<float>(),
                        v[3].get<float>())});
    } else if (value_type == "int") {
      curve.int_keys.push_back({t, kj.value("v", 0)});
    } else if (value_type == "bool") {
      curve.bool_keys.push_back({t, kj.value("v", false)});
    } else if (value_type == "asset") {
      std::string ref = kj.value("v", "");
      curve.asset_keys.push_back({t, AssetHandle::FromString(ref)});
    }
  }
  return curve;
}

// --- Texture property helpers ---

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

// --- Font property helpers ---

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

// --- UIDocument property helpers ---

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
  if (!props->events.empty()) {
    j["events"] = props->events;
  }
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
  if (j.contains("events") && j["events"].is_array()) {
    for (const auto& ev : j["events"]) {
      props->events.push_back(ev.get<std::string>());
    }
  }
  return props;
}


// --- InitializeAssetRegistry - register all asset types ---

void InitializeAssetRegistry() {
  auto& mgr = Engine::asset_manager();

  // --- Texture ---
  AssetRegistry::Register(
      AssetType::Texture,
      {
          .Load = [](AssetHandle handle) { return LoadTextureAsset(handle); },
          .Unload = nullptr,
          .CreateProperties = []() -> std::shared_ptr<void> {
            return std::make_shared<TextureAssetProperties>();
          },
          .SerializeProperties = SerializeTextureProperties,
          .DeserializeProperties = DeserializeTextureProperties,
      });

  // --- Model ---
  AssetRegistry::Register(
      AssetType::Model,
      {
          .Load = [](AssetHandle handle) { return LoadModelAsset(handle); },
          .Unload = nullptr,
      });

  // --- Material ---
  AssetRegistry::Register(
      AssetType::Material,
      {
          .Load =
              [](AssetHandle handle) {
                const auto* meta = Engine::asset_manager().GetMetadata(handle);
                if (!meta || meta->virtual_source_path.empty()) {
                  return false;
                }
                VfsFile file = Engine::vfs()->Open(meta->virtual_source_path);
                if (!file) {
                  return false;
                }
                auto chars = file.AsChars();
                std::string content(chars.begin(), chars.end());
                auto j = nlohmann::json::parse(content, nullptr, false);
                if (j.is_discarded()) {
                  LOG_ERROR("Invalid .wmat file: {}",
                            meta->virtual_source_path);
                  return false;
                }
                auto mat = Material::Deserialize(j);
                mat->name = meta->name;
                mat->asset_handle = handle;
                Engine::asset_manager().Store<Material>(handle, mat);
                return true;
              },
          .Unload = nullptr,
          .Serialize = [](AssetHandle handle) -> nlohmann::json {
            auto mat = Engine::asset_manager().Get<Material>(handle);
            if (!mat) {
              return {};
            }
            return mat->Serialize();
          },
          .Deserialize = [](AssetHandle handle,
                            const nlohmann::json& j) -> bool {
            auto mat = Material::Deserialize(j);
            mat->asset_handle = handle;
            const auto* meta = Engine::asset_manager().GetMetadata(handle);
            if (meta) {
              mat->name = meta->name;
            }
            Engine::asset_manager().Store<Material>(handle, mat);
            return true;
          },
      });

  // --- Font ---
  AssetRegistry::Register(
      AssetType::Font,
      {
          .Load =
              [](AssetHandle handle) {
                const auto* meta = Engine::asset_manager().GetMetadata(handle);
                if (!meta) {
                  return false;
                }
                auto asset =
                    std::make_shared<FontAsset>(meta->virtual_source_path);
                if (!asset->IsLoaded()) {
                  return false;
                }
                const auto* props = meta->GetProperties<FontAssetProperties>();
                if (props) {
                  asset->SetAAMode(props->aa_mode);
                }
                Engine::asset_manager().Store<FontAsset>(handle, asset);
                return true;
              },
          .Unload =
              [](AssetHandle handle) {
                FontCache::Invalidate(handle);
                Engine::asset_manager().Unload(handle);
              },
          .CreateProperties = []() -> std::shared_ptr<void> {
            return std::make_shared<FontAssetProperties>();
          },
          .SerializeProperties = SerializeFontProperties,
          .DeserializeProperties = DeserializeFontProperties,
      });

  // --- Sprite ---
  AssetRegistry::Register(
      AssetType::Sprite,
      {
          .Load =
              [](AssetHandle handle) {
                const auto* meta = Engine::asset_manager().GetMetadata(handle);
                if (!meta) {
                  return false;
                }
                auto file = Engine::vfs()->Open(meta->virtual_source_path);
                if (!file) {
                  return false;
                }
                auto chars = file.AsChars();
                std::string json_str(chars.begin(), chars.end());
                nlohmann::json j =
                    nlohmann::json::parse(json_str, nullptr, false);
                if (j.is_discarded() || !j.contains("texture") ||
                    !j.contains("rect")) {
                  LOG_ERROR("Invalid .wsprite file: {}",
                            meta->virtual_source_path);
                  return false;
                }

                auto data = std::make_shared<SpriteAssetData>();
                data->texture_handle =
                    AssetHandle::FromString(j["texture"].get<std::string>());
                data->rect = {
                    j["rect"][0].get<float>(), j["rect"][1].get<float>(),
                    j["rect"][2].get<float>(), j["rect"][3].get<float>()};
                if (j.contains("pivot") && j["pivot"].is_array()) {
                  data->pivot = {j["pivot"][0].get<float>(),
                                 j["pivot"][1].get<float>()};
                }

                // Ensure the backing texture is loaded
                if (data->texture_handle.IsValid()) {
                  auto tex = Engine::asset_manager().Get<Texture>(
                      data->texture_handle);
                  if (!tex) {
                    Engine::asset_manager().LoadSync(data->texture_handle);
                  }
                }

                Engine::asset_manager().Store(handle, data);
                Engine::asset_manager().AddDependency(handle,
                                                      data->texture_handle);

                // Build GPU resources for rendering
                auto tex =
                    Engine::asset_manager().Get<Texture>(data->texture_handle);
                if (tex && tex->is_allocated_ && tex->image_view_) {
                  auto gpu = std::make_shared<SpriteGpuData>();
                  gpu->view = tex->image_view_;
                  gpu->sampler =
                      tex->sampler_
                          ? tex->sampler_
                          : Engine::renderer()->GetDefaultLinearSampler();
                  gpu->pixel_size = {data->rect.z, data->rect.w};

                  float tw = static_cast<float>(tex->width_);
                  float th = static_cast<float>(tex->height_);
                  glm::vec4 uv = data->GetUVRect(tw, th);

                  float u0 = uv.x;
                  float v0 = uv.y;
                  float u1 = uv.x + uv.z;
                  float v1 = uv.y + uv.w;

                  std::vector<VertexSprite> uvs = {
                      {{u0, v0}}, {{u1, v0}}, {{u1, v1}},
                      {{u0, v0}}, {{u1, v1}}, {{u0, v1}},
                  };
                  gpu->vertex_buffer = Engine::renderer()->CreateVertexBuffer(
                      "SpriteGpuData::vertex_buffer", uvs);

                  Engine::asset_manager().Store(handle, gpu);
                }

                return true;
              },
          .Unload = nullptr,
          .Serialize = [](AssetHandle handle) -> nlohmann::json {
            auto data = Engine::asset_manager().Get<SpriteAssetData>(handle);
            nlohmann::json j;
            if (data) {
              j["texture"] = data->texture_handle.ToString();
              j["rect"] = {data->rect.x, data->rect.y, data->rect.z,
                           data->rect.w};
              j["pivot"] = {data->pivot.x, data->pivot.y};
            }
            return j;
          },
          .Deserialize = [](AssetHandle handle,
                            const nlohmann::json& j) -> bool {
            if (!j.contains("texture") || !j.contains("rect")) {
              return false;
            }
            auto data = std::make_shared<SpriteAssetData>();
            data->texture_handle =
                AssetHandle::FromString(j["texture"].get<std::string>());
            data->rect = {j["rect"][0].get<float>(), j["rect"][1].get<float>(),
                          j["rect"][2].get<float>(), j["rect"][3].get<float>()};
            if (j.contains("pivot") && j["pivot"].is_array()) {
              data->pivot = {j["pivot"][0].get<float>(),
                             j["pivot"][1].get<float>()};
            }
            Engine::asset_manager().Store(handle, data);
            return true;
          },
      });

  // --- Skybox ---
  AssetRegistry::Register(
      AssetType::Skybox,
      {
          .Load =
              [](AssetHandle handle) {
                return AssetRegistry::LoadJson(handle);
              },
          .Unload = nullptr,
          .Serialize = [](AssetHandle handle) -> nlohmann::json {
            auto data = Engine::asset_manager().Get<SkyboxAssetData>(handle);
            nlohmann::json j;
            if (data) {
              auto resolve = [](AssetHandle h) -> std::string {
                if (!h.IsValid()) {
                  return "";
                }
                const auto* meta = Engine::asset_manager().GetMetadata(h);
                return meta ? meta->virtual_source_path : h.ToString();
              };

              switch (data->type) {
                case SkyboxType::Panorama:
                  j["type"] = "panorama";
                  j["source"] = resolve(data->source_handle);
                  break;
                case SkyboxType::Cubemap:
                  j["type"] = "cubemap";
                  j["faces"] = {
                      {"right", resolve(data->face_handles[0])},
                      {"left", resolve(data->face_handles[1])},
                      {"top", resolve(data->face_handles[2])},
                      {"bottom", resolve(data->face_handles[3])},
                      {"front", resolve(data->face_handles[4])},
                      {"back", resolve(data->face_handles[5])},
                  };
                  break;
                case SkyboxType::Cross:
                  j["type"] = "cross";
                  j["source"] = resolve(data->source_handle);
                  break;
              }
            }
            return j;
          },
          .Deserialize = [](AssetHandle handle,
                            const nlohmann::json& j) -> bool {
            auto data = std::make_shared<SkyboxAssetData>();
            std::string type_str = j.value("type", "panorama");

            auto resolve_handle = [](const std::string& ref) -> AssetHandle {
              if (ref.empty()) {
                return {};
              }
              AssetHandle h = Engine::asset_manager().FindBySourcePath(ref);
              if (h.IsValid()) {
                return h;
              }
              return AssetHandle::FromString(ref);
            };

            if (type_str == "panorama") {
              data->type = SkyboxType::Panorama;
              data->source_handle = resolve_handle(j.value("source", ""));
            } else if (type_str == "cubemap") {
              data->type = SkyboxType::Cubemap;
              if (j.contains("faces") && j["faces"].is_object()) {
                auto& f = j["faces"];
                data->face_handles[0] = resolve_handle(f.value("right", ""));
                data->face_handles[1] = resolve_handle(f.value("left", ""));
                data->face_handles[2] = resolve_handle(f.value("top", ""));
                data->face_handles[3] = resolve_handle(f.value("bottom", ""));
                data->face_handles[4] = resolve_handle(f.value("front", ""));
                data->face_handles[5] = resolve_handle(f.value("back", ""));
              }
            } else if (type_str == "cross") {
              data->type = SkyboxType::Cross;
              data->source_handle = resolve_handle(j.value("source", ""));
            }

            Engine::asset_manager().Store(handle, data);
            return true;
          },
      });

  // --- AnimClip ---
  AssetRegistry::Register(
      AssetType::AnimClip,
      {
          .Load =
              [](AssetHandle handle) {
                return AssetRegistry::LoadJson(handle);
              },
          .Unload = nullptr,
          .Serialize = [](AssetHandle handle) -> nlohmann::json {
            auto data = Engine::asset_manager().Get<AnimClipAssetData>(handle);
            nlohmann::json j;
            if (!data) {
              return j;
            }
            j["version"] = 1;
            j["duration"] = data->duration;
            j["ticks_per_second"] = data->ticks_per_second;
            j["loop"] = data->loop;
            if (data->max_bone_reach > 0.0f) {
              j["max_bone_reach"] = data->max_bone_reach;
            }

            if (!data->property_curves.empty()) {
              nlohmann::json curves = nlohmann::json::array();
              for (const auto& c : data->property_curves) {
                curves.push_back(SerializePropertyCurve(c));
              }
              j["property_curves"] = curves;
            }

            if (!data->bone_channels.empty()) {
              nlohmann::json channels = nlohmann::json::array();
              for (const auto& ch : data->bone_channels) {
                nlohmann::json chj;
                chj["node"] = ch.node_name;
                nlohmann::json pos = nlohmann::json::array();
                for (const auto& k : ch.position_keys) {
                  pos.push_back({{"t", k.time},
                                 {"v", {k.value.x, k.value.y, k.value.z}}});
                }
                chj["position"] = pos;
                nlohmann::json rot = nlohmann::json::array();
                for (const auto& k : ch.rotation_keys) {
                  rot.push_back(
                      {{"t", k.time},
                       {"v", {k.value.w, k.value.x, k.value.y, k.value.z}}});
                }
                chj["rotation"] = rot;
                nlohmann::json scl = nlohmann::json::array();
                for (const auto& k : ch.scale_keys) {
                  scl.push_back({{"t", k.time},
                                 {"v", {k.value.x, k.value.y, k.value.z}}});
                }
                chj["scale"] = scl;
                channels.push_back(chj);
              }
              j["bone_channels"] = channels;
            }
            return j;
          },
          .Deserialize = [](AssetHandle handle,
                            const nlohmann::json& j) -> bool {
            auto data = std::make_shared<AnimClipAssetData>();
            data->duration = j.value("duration", 0.0f);
            data->ticks_per_second = j.value("ticks_per_second", 25.0f);
            data->loop = j.value("loop", true);
            data->max_bone_reach = j.value("max_bone_reach", 0.0f);

            if (j.contains("property_curves") &&
                j["property_curves"].is_array()) {
              for (const auto& cj : j["property_curves"]) {
                data->property_curves.push_back(ParsePropertyCurve(cj));
              }
            }

            if (j.contains("bone_channels") && j["bone_channels"].is_array()) {
              for (const auto& chj : j["bone_channels"]) {
                AnimationChannel ch;
                ch.node_name = chj.value("node", "");
                if (chj.contains("position") && chj["position"].is_array()) {
                  for (const auto& kj : chj["position"]) {
                    auto v = kj["v"];
                    ch.position_keys.push_back(
                        {kj["t"].get<float>(),
                         {v[0].get<float>(), v[1].get<float>(),
                          v[2].get<float>()}});
                  }
                }
                if (chj.contains("rotation") && chj["rotation"].is_array()) {
                  for (const auto& kj : chj["rotation"]) {
                    auto v = kj["v"];
                    ch.rotation_keys.push_back(
                        {kj["t"].get<float>(),
                         glm::quat(v[0].get<float>(), v[1].get<float>(),
                                   v[2].get<float>(), v[3].get<float>())});
                  }
                }
                if (chj.contains("scale") && chj["scale"].is_array()) {
                  for (const auto& kj : chj["scale"]) {
                    auto v = kj["v"];
                    ch.scale_keys.push_back(
                        {kj["t"].get<float>(),
                         {v[0].get<float>(), v[1].get<float>(),
                          v[2].get<float>()}});
                  }
                }
                data->bone_channels.push_back(std::move(ch));
              }
            }

            Engine::asset_manager().Store(handle, data);
            return true;
          },
      });

  // --- AnimController ---
  AssetRegistry::Register(
      AssetType::AnimController,
      {
          .Load =
              [](AssetHandle handle) {
                return AssetRegistry::LoadJson(handle);
              },
          .Unload = nullptr,
          .Serialize = [](AssetHandle handle) -> nlohmann::json {
            auto data =
                Engine::asset_manager().Get<AnimControllerAssetData>(handle);
            nlohmann::json j;
            if (!data) {
              return j;
            }
            j["version"] = 1;
            j["default_state"] = data->default_state;
            nlohmann::json states = nlohmann::json::array();
            for (const auto& s : data->states) {
              nlohmann::json sj;
              sj["name"] = s.name;
              if (s.clip_handle.IsValid()) {
                sj["clip"] = s.clip_handle.ToString();
              }
              sj["speed"] = s.speed;
              sj["editor_pos"] = {s.editor_pos.x, s.editor_pos.y};
              sj["editor_id"] = s.editor_id;
              states.push_back(sj);
            }
            j["states"] = states;
            nlohmann::json transitions = nlohmann::json::array();
            for (const auto& t : data->transitions) {
              nlohmann::json tj;
              tj["from"] = t.from_state;
              tj["to"] = t.to_state;
              tj["blend"] = t.blend_duration;
              if (!t.conditions.empty()) {
                tj["conditions"] = SerializeTransitionConditions(t.conditions);
              }
              tj["editor_id"] = t.editor_id;
              transitions.push_back(tj);
            }
            j["transitions"] = transitions;

            if (!data->default_parameters.empty()) {
              nlohmann::json params;
              for (const auto& [name, param] : data->default_parameters) {
                nlohmann::json pj;
                switch (param.type) {
                  case AnimParamType::Bool:
                    pj["type"] = "Bool";
                    pj["value"] = param.b;
                    break;
                  case AnimParamType::Int:
                    pj["type"] = "Int";
                    pj["value"] = param.i;
                    break;
                  case AnimParamType::Float:
                    pj["type"] = "Float";
                    pj["value"] = param.f;
                    break;
                  case AnimParamType::Trigger:
                    pj["type"] = "Trigger";
                    pj["value"] = false;
                    break;
                }
                params[name] = pj;
              }
              j["parameters"] = params;
            }
            return j;
          },
          .Deserialize = [](AssetHandle handle,
                            const nlohmann::json& j) -> bool {
            auto data = std::make_shared<AnimControllerAssetData>();
            data->default_state = j.value("default_state", "");

            if (j.contains("states") && j["states"].is_array()) {
              for (const auto& sj : j["states"]) {
                AnimControllerAssetData::State state;
                state.name = sj.value("name", "");
                std::string clip_ref = sj.value("clip", "");
                if (!clip_ref.empty()) {
                  state.clip_handle = AssetHandle::FromString(clip_ref);
                }
                state.speed = sj.value("speed", 1.0f);
                if (sj.contains("editor_pos") && sj["editor_pos"].is_array()) {
                  state.editor_pos = {sj["editor_pos"][0].get<float>(),
                                      sj["editor_pos"][1].get<float>()};
                }
                state.editor_id = sj.value("editor_id", -1);
                if (!state.name.empty()) {
                  if (state.clip_handle.IsValid()) {
                    Engine::asset_manager().AddDependency(handle,
                                                          state.clip_handle);
                  }
                  data->states.push_back(std::move(state));
                }
              }
            }

            if (j.contains("transitions") && j["transitions"].is_array()) {
              for (const auto& tj : j["transitions"]) {
                AnimationTransition trans;
                trans.from_state = tj.value("from", "");
                trans.to_state = tj.value("to", "");
                trans.blend_duration = tj.value("blend", 0.0f);
                if (tj.contains("conditions") && tj["conditions"].is_array()) {
                  ParseTransitionConditions(tj["conditions"], trans.conditions);
                }
                trans.editor_id = tj.value("editor_id", -1);
                if (!trans.to_state.empty()) {
                  data->transitions.push_back(std::move(trans));
                }
              }
            }

            if (j.contains("parameters") && j["parameters"].is_object()) {
              for (auto& [name, pj] : j["parameters"].items()) {
                std::string type_str = pj.value("type", "Bool");
                if (type_str == "Bool") {
                  data->default_parameters[name] =
                      AnimParam::MakeBool(pj.value("value", false));
                } else if (type_str == "Int") {
                  data->default_parameters[name] =
                      AnimParam::MakeInt(pj.value("value", 0));
                } else if (type_str == "Float") {
                  data->default_parameters[name] =
                      AnimParam::MakeFloat(pj.value("value", 0.0f));
                } else if (type_str == "Trigger") {
                  data->default_parameters[name] = AnimParam::MakeTrigger();
                }
              }
            }

            Engine::asset_manager().Store(handle, data);
            return true;
          },
      });

  // --- CursorSet ---
  AssetRegistry::Register(
      AssetType::CursorSet,
      {
          .Load =
              [](AssetHandle handle) {
                return AssetRegistry::LoadJson(handle);
              },
          .Unload = nullptr,
          .Serialize = [](AssetHandle handle) -> nlohmann::json {
            auto data = Engine::asset_manager().Get<CursorSetData>(handle);
            nlohmann::json j;
            if (!data) {
              return j;
            }
            j["mode"] = data->mode == CursorSetMode::ForceSoftware ? "software"
                                                                   : "auto";
            nlohmann::json states_j = nlohmann::json::object();
            for (const auto& [name, entry] : data->states) {
              nlohmann::json state_j;
              state_j["hotspot"] = {entry.hotspot.x, entry.hotspot.y};
              state_j["size"] = {entry.size.x, entry.size.y};
              if (entry.scale != 1) {
                state_j["scale"] = entry.scale;
              }
              if (entry.frames.size() == 1) {
                state_j["texture"] = entry.frames[0].texture.ToString();
              } else if (entry.frames.size() > 1) {
                state_j["frame_duration"] = entry.frame_duration;
                nlohmann::json frames_j = nlohmann::json::array();
                for (const auto& frame : entry.frames) {
                  frames_j.push_back(frame.texture.ToString());
                }
                state_j["frames"] = frames_j;
              }
              states_j[name] = state_j;
            }
            j["states"] = states_j;
            return j;
          },
          .Deserialize = [](AssetHandle handle,
                            const nlohmann::json& j) -> bool {
            auto data = std::make_shared<CursorSetData>();

            if (j.contains("mode")) {
              std::string mode_str = j["mode"].get<std::string>();
              if (mode_str == "software") {
                data->mode = CursorSetMode::ForceSoftware;
              }
            }

            if (j.contains("states") && j["states"].is_object()) {
              for (auto& [name, state_j] : j["states"].items()) {
                CursorStateEntry entry;

                if (state_j.contains("hotspot") &&
                    state_j["hotspot"].is_array() &&
                    state_j["hotspot"].size() >= 2) {
                  entry.hotspot = {state_j["hotspot"][0].get<int>(),
                                   state_j["hotspot"][1].get<int>()};
                }
                if (state_j.contains("size") && state_j["size"].is_array() &&
                    state_j["size"].size() >= 2) {
                  entry.size = {state_j["size"][0].get<int>(),
                                state_j["size"][1].get<int>()};
                }
                if (state_j.contains("frame_duration")) {
                  entry.frame_duration = state_j["frame_duration"].get<float>();
                }
                if (state_j.contains("scale")) {
                  entry.scale = std::clamp(
                      state_j["scale"].get<int>(), 1, 8);
                }

                // Single texture
                if (state_j.contains("texture")) {
                  CursorFrame frame;
                  frame.texture = AssetHandle::FromString(
                      state_j["texture"].get<std::string>());
                  if (frame.texture.IsValid()) {
                    Engine::asset_manager().AddDependency(handle,
                                                          frame.texture);
                  }
                  entry.frames.push_back(frame);
                }

                // Multiple frames
                if (state_j.contains("frames") &&
                    state_j["frames"].is_array()) {
                  for (const auto& frame_j : state_j["frames"]) {
                    CursorFrame frame;
                    if (frame_j.is_string()) {
                      frame.texture =
                          AssetHandle::FromString(frame_j.get<std::string>());
                    } else if (frame_j.is_object() &&
                               frame_j.contains("texture")) {
                      frame.texture = AssetHandle::FromString(
                          frame_j["texture"].get<std::string>());
                    }
                    if (frame.texture.IsValid()) {
                      Engine::asset_manager().AddDependency(handle,
                                                            frame.texture);
                    }
                    entry.frames.push_back(frame);
                  }
                }

                data->states[name] = std::move(entry);
              }
            }

            Engine::asset_manager().Store(handle, data);
            return true;
          },
      });

  // --- MeshCollider ---
  // Binary format: magic "WMCL" + uint32 version + varint source_model length +
  // source_model string + uint64 vertex_count + raw vertex floats +
  // uint64 index_count + raw uint32 indices.
  AssetRegistry::Register(
      AssetType::MeshCollider,
      {
          .Load =
              [](AssetHandle handle) {
                if (!AssetRegistry::LoadBinary(handle)) {
                  return false;
                }
                auto data =
                    Engine::asset_manager().Get<MeshColliderAssetData>(handle);
                if (data) {
                  BuildCollisionShape(*data);
                }
                return data != nullptr;
              },
          .Unload = nullptr,
          .SerializeBinary =
              [](AssetHandle handle, urkern::Buffer& buf) {
                auto data = Engine::asset_manager()
                                .Get<MeshColliderAssetData>(handle);
                if (!data) {
                  return;
                }
                buf.Write("WMCL", 4);
                buf.WriteInt<uint32_t>(kMeshColliderBinaryVersion);
                buf.WriteString(data->source_model.ToString());
                buf.WriteInt<uint64_t>(data->vertices.size());
                if (!data->vertices.empty()) {
                  buf.Write(reinterpret_cast<const float*>(
                                data->vertices.data()),
                            data->vertices.size() * 3);
                }
                buf.WriteInt<uint64_t>(data->indices.size());
                if (!data->indices.empty()) {
                  buf.Write(data->indices.data(), data->indices.size());
                }
              },
          .DeserializeBinary =
              [](AssetHandle handle, urkern::Buffer& buf) -> bool {
            char magic[4];
            buf.Read(magic, 4);
            if (std::strncmp(magic, "WMCL", 4) != 0) {
              DCON_LOG_ERROR(
                  "MeshCollider: bad magic, expected WMCL (re-bake via "
                  "editor)");
              return false;
            }
            uint32_t version = buf.ReadInt<uint32_t>();
            if (version != kMeshColliderBinaryVersion) {
              DCON_LOG_ERROR("MeshCollider: unsupported version {}", version);
              return false;
            }

            auto data = std::make_shared<MeshColliderAssetData>();
            data->source_model =
                AssetHandle::FromString(buf.ReadString());

            uint64_t vert_count = buf.ReadInt<uint64_t>();
            data->vertices.resize(vert_count);
            if (vert_count > 0) {
              buf.Read(reinterpret_cast<float*>(data->vertices.data()),
                       vert_count * 3);
            }

            uint64_t idx_count = buf.ReadInt<uint64_t>();
            data->indices.resize(idx_count);
            if (idx_count > 0) {
              buf.Read(data->indices.data(), idx_count);
            }

            if (buf.GetAndClearLastError() != urkern::BufferError::None) {
              DCON_LOG_ERROR("MeshCollider: corrupt/truncated binary asset");
              return false;
            }

            Engine::asset_manager().Store(handle, data);
            return true;
          },
      });

  // --- UIDocument ---
  AssetRegistry::Register(
      AssetType::UIDocument,
      {
          .Load =
              [](AssetHandle handle) {
                const auto* meta = Engine::asset_manager().GetMetadata(handle);
                if (!meta || meta->virtual_source_path.empty()) {
                  return false;
                }
                auto asset = std::make_shared<UIDocumentAsset>();
                asset->vfs_path = meta->virtual_source_path;
                Engine::asset_manager().Store<UIDocumentAsset>(handle, asset);
                return true;
              },
          .Unload = nullptr,
          .CreateProperties = []() -> std::shared_ptr<void> {
            return std::make_shared<UIDocumentAssetProperties>();
          },
          .SerializeProperties = SerializeUIDocumentProperties,
          .DeserializeProperties = DeserializeUIDocumentProperties,
      });

  // --- UIStylesheet ---
  AssetRegistry::Register(
      AssetType::UIStylesheet,
      {
          .Load =
              [](AssetHandle handle) {
                const auto* meta = Engine::asset_manager().GetMetadata(handle);
                if (!meta || meta->virtual_source_path.empty()) {
                  return false;
                }
                auto asset = std::make_shared<UIStylesheetAsset>();
                asset->vfs_path = meta->virtual_source_path;
                Engine::asset_manager().Store<UIStylesheetAsset>(handle, asset);
                return true;
              },
          .Unload = nullptr,
      });
}

}  // namespace wiesel
