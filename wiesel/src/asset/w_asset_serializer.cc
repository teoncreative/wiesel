//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "asset/w_asset_serializer.h"

#include "animation/w_animation_controller.h"
#include "cursor/w_cursor.h"
#include "rendering/w_skybox.h"
#include "rendering/w_sprite_asset.h"
#include "util/w_logger.h"
#include "util/w_vfs.h"
#include "w_engine.h"

namespace Wiesel {

std::vector<AssetSerializerDesc>& AssetSerializerRegistry::Registry() {
  static std::vector<AssetSerializerDesc> registry;
  return registry;
}

void AssetSerializerRegistry::Register(AssetSerializerDesc desc) {
  Registry().push_back(std::move(desc));
}

AssetSerializerDesc* AssetSerializerRegistry::Find(AssetType type) {
  for (auto& desc : Registry()) {
    if (desc.type == type) {
      return &desc;
    }
  }
  return nullptr;
}

bool AssetSerializerRegistry::HasSerializer(AssetType type) {
  return Find(type) != nullptr;
}

bool AssetSerializerRegistry::Save(AssetHandle handle) {
  const auto* meta = Engine::asset_manager().GetMetadata(handle);
  if (!meta) {
    return false;
  }
  return Save(handle, meta->virtual_source_path);
}

bool AssetSerializerRegistry::Save(AssetHandle handle,
                                   const std::string& vfs_path) {
  const auto* meta = Engine::asset_manager().GetMetadata(handle);
  if (!meta) {
    return false;
  }

  auto* desc = Find(meta->type);
  if (!desc || !desc->Serialize) {
    LOG_ERROR("No serializer registered for asset type {}",
              static_cast<int>(meta->type));
    return false;
  }

  nlohmann::json j = desc->Serialize(handle);
  j["asset_handle"] = handle.ToString();

  return Engine::vfs()->WriteFile(vfs_path, j.dump(2));
}

bool AssetSerializerRegistry::Load(AssetHandle handle) {
  const auto* meta = Engine::asset_manager().GetMetadata(handle);
  if (!meta) {
    return false;
  }

  auto* desc = Find(meta->type);
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

// ---------------------------------------------------------------------------
// Serializer registrations
// ---------------------------------------------------------------------------

static void RegisterSpriteSerializer() {
  AssetSerializerRegistry::Register({
      AssetType::Sprite,
      // Serialize
      [](AssetHandle handle) -> nlohmann::json {
        auto data = Engine::asset_manager().Get<SpriteAssetData>(handle);
        nlohmann::json j;
        if (data) {
          j["texture"] = data->texture_handle.ToString();
          j["rect"] = {data->rect.x, data->rect.y, data->rect.z, data->rect.w};
          j["pivot"] = {data->pivot.x, data->pivot.y};
        }
        return j;
      },
      // Deserialize
      [](AssetHandle handle, const nlohmann::json& j) -> bool {
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
}

static void RegisterSpriteAnimSerializer() {
  AssetSerializerRegistry::Register({
      AssetType::SpriteAnim,
      // Serialize
      [](AssetHandle handle) -> nlohmann::json {
        auto data = Engine::asset_manager().Get<SpriteAnimAssetData>(handle);
        nlohmann::json j;
        if (data) {
          j["loop"] = data->loop;
          nlohmann::json frames = nlohmann::json::array();
          for (const auto& frame : data->frames) {
            nlohmann::json fj;
            fj["sprite"] = frame.sprite_handle.ToString();
            fj["duration"] = frame.duration;
            frames.push_back(fj);
          }
          j["frames"] = frames;
        }
        return j;
      },
      // Deserialize
      [](AssetHandle handle, const nlohmann::json& j) -> bool {
        if (!j.contains("frames") || !j["frames"].is_array()) {
          return false;
        }
        auto data = std::make_shared<SpriteAnimAssetData>();
        data->loop = j.value("loop", true);
        for (const auto& fj : j["frames"]) {
          SpriteAnimAssetData::Frame frame;
          std::string sprite_ref;
          if (fj.is_object()) {
            sprite_ref = fj.value("sprite", "");
            frame.duration = fj.value("duration", 0.1f);
          } else if (fj.is_string()) {
            sprite_ref = fj.get<std::string>();
            frame.duration = 0.1f;
          } else {
            continue;
          }
          if (sprite_ref.empty()) {
            continue;
          }
          frame.sprite_handle = AssetHandle::FromString(sprite_ref);
          if (frame.sprite_handle.IsValid()) {
            Engine::asset_manager().AddDependency(handle, frame.sprite_handle);
            data->frames.push_back(std::move(frame));
          }
        }
        if (data->frames.empty()) {
          return false;
        }
        Engine::asset_manager().Store(handle, data);
        return true;
      },
  });
}

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

static void RegisterSpriteControllerSerializer() {
  AssetSerializerRegistry::Register({
      AssetType::SpriteController,
      // Serialize
      [](AssetHandle handle) -> nlohmann::json {
        auto data =
            Engine::asset_manager().Get<SpriteControllerAssetData>(handle);
        nlohmann::json j;
        if (data) {
          j["default_state"] = data->default_state;
          nlohmann::json states = nlohmann::json::array();
          for (const auto& s : data->states) {
            nlohmann::json sj;
            sj["name"] = s.name;
            if (s.animation_handle.IsValid()) {
              sj["animation"] = s.animation_handle.ToString();
            }
            sj["speed"] = s.speed;
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
            transitions.push_back(tj);
          }
          j["transitions"] = transitions;
        }
        return j;
      },
      // Deserialize
      [](AssetHandle handle, const nlohmann::json& j) -> bool {
        auto data = std::make_shared<SpriteControllerAssetData>();
        data->default_state = j.value("default_state", "");
        if (j.contains("states") && j["states"].is_array()) {
          for (const auto& sj : j["states"]) {
            SpriteControllerAssetData::State state;
            state.name = sj.value("name", "");
            std::string anim_ref = sj.value("animation", "");
            if (!anim_ref.empty()) {
              state.animation_handle = AssetHandle::FromString(anim_ref);
            }
            state.speed = sj.value("speed", 1.0f);
            if (!state.name.empty()) {
              if (state.animation_handle.IsValid()) {
                Engine::asset_manager().AddDependency(handle,
                                                      state.animation_handle);
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
            if (!trans.to_state.empty()) {
              data->transitions.push_back(std::move(trans));
            }
          }
        }
        Engine::asset_manager().Store(handle, data);
        return true;
      },
  });
}

static void RegisterSkyboxSerializer() {
  AssetSerializerRegistry::Register({
      AssetType::Skybox,
      // Serialize
      [](AssetHandle handle) -> nlohmann::json {
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
      // Deserialize
      [](AssetHandle handle, const nlohmann::json& j) -> bool {
        auto data = std::make_shared<SkyboxAssetData>();
        std::string type_str = j.value("type", "panorama");

        auto resolve_handle = [](const std::string& ref) -> AssetHandle {
          if (ref.empty()) {
            return {};
          }
          // Try as VFS path first
          AssetHandle h = Engine::asset_manager().FindBySourcePath(ref);
          if (h.IsValid()) {
            return h;
          }
          // Try as UUID
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
}

static void RegisterCursorSetSerializer() {
  AssetSerializerRegistry::Register({
      AssetType::CursorSet,
      // Serialize
      [](AssetHandle handle) -> nlohmann::json {
        auto data = Engine::asset_manager().Get<CursorSetData>(handle);
        nlohmann::json j;
        if (!data) {
          return j;
        }
        j["mode"] =
            data->mode == CursorSetMode::ForceSoftware ? "software" : "auto";
        nlohmann::json states_j = nlohmann::json::object();
        for (const auto& [name, entry] : data->states) {
          nlohmann::json state_j;
          state_j["hotspot"] = {entry.hotspot.x, entry.hotspot.y};
          state_j["size"] = {entry.size.x, entry.size.y};
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
      // Deserialize
      [](AssetHandle handle, const nlohmann::json& j) -> bool {
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

            if (state_j.contains("hotspot") && state_j["hotspot"].is_array() &&
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

            // Single texture
            if (state_j.contains("texture")) {
              CursorFrame frame;
              frame.texture = AssetHandle::FromString(
                  state_j["texture"].get<std::string>());
              if (frame.texture.IsValid()) {
                Engine::asset_manager().AddDependency(handle, frame.texture);
              }
              entry.frames.push_back(frame);
            }

            // Multiple frames
            if (state_j.contains("frames") && state_j["frames"].is_array()) {
              for (const auto& frame_j : state_j["frames"]) {
                CursorFrame frame;
                if (frame_j.is_string()) {
                  frame.texture =
                      AssetHandle::FromString(frame_j.get<std::string>());
                } else if (frame_j.is_object() && frame_j.contains("texture")) {
                  frame.texture = AssetHandle::FromString(
                      frame_j["texture"].get<std::string>());
                }
                if (frame.texture.IsValid()) {
                  Engine::asset_manager().AddDependency(handle, frame.texture);
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
}

void InitializeAssetSerializers() {
  RegisterSpriteSerializer();
  RegisterSpriteAnimSerializer();
  RegisterSpriteControllerSerializer();
  RegisterSkyboxSerializer();
  RegisterCursorSetSerializer();
}

}  // namespace Wiesel
