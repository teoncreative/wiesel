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
// Component serializer registry - single source of truth for component
// serialization used by both scene files and prefabs.
//

#include "scene/w_component_serializer.h"

#include "asset/w_asset_manager.h"
#include "audio/w_audio.h"
#include "behavior/w_behavior.h"
#include "behavior/w_native_behavior.h"
#include "mono_wrappers.h"
#include "physics/w_collider.h"
#include "physics/w_rigidbody.h"
#include "rendering/w_mesh.h"
#include "rendering/w_sprite.h"
#include "rendering/w_sprite_asset.h"
#include "rendering/w_sprite_loader.h"
#include "scene/w_lights.h"
#include "script/mono/w_monobehavior.h"
#include "ui/w_canvas.h"
#include "ui/w_interactable.h"
#include "ui/w_navigable.h"
#include "util/w_logger.h"
#include "w_engine.h"

namespace Wiesel {

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// SerializeUtil
// ---------------------------------------------------------------------------

json SerializeUtil::Vec2(const glm::vec2& v) {
  return {v.x, v.y};
}

json SerializeUtil::Vec3(const glm::vec3& v) {
  return {v.x, v.y, v.z};
}

json SerializeUtil::Vec4(const glm::vec4& v) {
  return {v.x, v.y, v.z, v.w};
}

glm::vec2 SerializeUtil::Vec2(const json& v, glm::vec2 fallback) {
  if (!v.is_array() || v.size() < 2) {
    return fallback;
  }
  return {v[0].get<float>(), v[1].get<float>()};
}

glm::vec3 SerializeUtil::Vec3(const json& v, glm::vec3 fallback) {
  if (!v.is_array() || v.size() < 3) {
    return fallback;
  }
  return {v[0].get<float>(), v[1].get<float>(), v[2].get<float>()};
}

glm::vec4 SerializeUtil::Vec4(const json& v, glm::vec4 fallback) {
  if (!v.is_array() || v.size() < 4) {
    return fallback;
  }
  return {v[0].get<float>(), v[1].get<float>(), v[2].get<float>(),
          v[3].get<float>()};
}

// ---------------------------------------------------------------------------
// ComponentSerializerRegistry
// ---------------------------------------------------------------------------

std::vector<ComponentSerializerDesc>& ComponentSerializerRegistry::Registry() {
  static std::vector<ComponentSerializerDesc> registry;
  return registry;
}

void ComponentSerializerRegistry::Register(ComponentSerializerDesc desc) {
  Registry().push_back(std::move(desc));
}

void ComponentSerializerRegistry::SerializeAll(Entity& entity, json& out) {
  for (auto& desc : Registry()) {
    if (desc.Has(entity)) {
      out[desc.json_key] = desc.Serialize(entity);
    }
  }
}

void ComponentSerializerRegistry::DeserializeAll(Entity& entity, const json& in,
                                                 Scene* scene) {
  for (auto& desc : Registry()) {
    if (in.contains(desc.json_key)) {
      desc.Deserialize(entity, in[desc.json_key], scene);
    }
  }
}

// ---------------------------------------------------------------------------
// Helper: request async load for all textures referenced by a material
// ---------------------------------------------------------------------------

static void RequestMaterialTextures(Scene* scene, AssetHandle material_handle) {
  const auto* meta = Engine::asset_manager().GetMetadata(material_handle);
  if (!meta || meta->virtual_source_path.empty()) {
    return;
  }

  VfsFile file = Engine::vfs()->Open(meta->virtual_source_path);
  if (!file) {
    return;
  }

  try {
    std::string content(reinterpret_cast<const char*>(file.Data()),
                        file.Size());
    auto j = json::parse(content);
    if (!j.contains("textures") || !j["textures"].is_object()) {
      return;
    }

    for (auto& [key, val] : j["textures"].items()) {
      if (!val.is_string()) {
        continue;
      }
      AssetHandle th =
          Engine::asset_manager().FindBySourcePath(val.get<std::string>());
      if (th.IsValid()) {
        scene->RequestAsset(th);
      }
    }
  } catch (...) {}
}

// ---------------------------------------------------------------------------
// InitializeComponentSerializers - registers every component type
// ---------------------------------------------------------------------------

void InitializeComponentSerializers() {
  // -------------------------------------------------------------------
  // 1. Transform
  // -------------------------------------------------------------------
  ComponentSerializerRegistry::Register({
      "Transform",
      // Has
      [](Entity& entity) -> bool {
        return entity.HasComponent<TransformComponent>();
      },
      // Serialize
      [](Entity& entity) -> json {
        auto& t = entity.GetComponent<TransformComponent>();
        json transform;
        transform["position"] = SerializeUtil::Vec3(t.GetPosition());
        transform["rotation"] = SerializeUtil::Vec3(t.GetRotation());
        transform["scale"] = SerializeUtil::Vec3(t.GetScale());
        transform["pivot"] = SerializeUtil::Vec3(t.GetPivot());
        return transform;
      },
      // Deserialize
      [](Entity& entity, const json& j, Scene* /*scene*/) {
        auto& t = entity.GetComponent<TransformComponent>();
        t.SetPosition(SerializeUtil::Vec3(j.value("position", json::array())));
        t.SetRotation(SerializeUtil::Vec3(j.value("rotation", json::array())));
        t.SetScale(
            SerializeUtil::Vec3(j.value("scale", json::array()), {1, 1, 1}));
        t.SetPivot(SerializeUtil::Vec3(j.value("pivot", json::array())));
      },
  });

  // -------------------------------------------------------------------
  // 2. Camera
  // -------------------------------------------------------------------
  ComponentSerializerRegistry::Register({
      "Camera",
      // Has
      [](Entity& entity) -> bool {
        return entity.HasComponent<CameraComponent>();
      },
      // Serialize
      [](Entity& entity) -> json {
        auto& c = entity.GetComponent<CameraComponent>();
        json cam;
        cam["projection_mode"] = static_cast<int>(c.projection_mode);
        cam["field_of_view"] = c.field_of_view;
        cam["ortho_size"] = c.ortho_size;
        cam["background_color"] = {c.background_color.r, c.background_color.g,
                                   c.background_color.b, c.background_color.a};
        cam["near_plane"] = c.near_plane;
        cam["far_plane"] = c.far_plane;
        cam["viewport_size"] = SerializeUtil::Vec2(c.viewport_size);
        cam["enabled"] = c.enabled;
        return cam;
      },
      // Deserialize
      [](Entity& entity, const json& cj, Scene* /*scene*/) {
        auto& c = entity.AddComponent<CameraComponent>();
        c.projection_mode =
            static_cast<ProjectionMode>(cj.value("projection_mode", 0));
        c.field_of_view = cj.value("field_of_view", 60.0f);
        c.ortho_size = cj.value("ortho_size", 5.0f);
        if (cj.contains("background_color") &&
            cj["background_color"].is_array() &&
            cj["background_color"].size() >= 4) {
          auto& bg = cj["background_color"];
          c.background_color = {bg[0], bg[1], bg[2], bg[3]};
        }
        c.near_plane = cj.value("near_plane", 0.01f);
        c.far_plane = cj.value("far_plane", 1000.0f);
        c.viewport_size = SerializeUtil::Vec2(
            cj.value("viewport_size", json::array()), {1920, 1080});
        c.enabled = cj.value("enabled", true);
      },
  });

  // -------------------------------------------------------------------
  // 3. Model
  // -------------------------------------------------------------------
  ComponentSerializerRegistry::Register({
      "Model",
      // Has
      [](Entity& entity) -> bool {
        return entity.HasComponent<ModelComponent>();
      },
      // Serialize
      [](Entity& entity) -> json {
        auto& m = entity.GetComponent<ModelComponent>();
        json model;
        if (m.model_handle.IsValid()) {
          model["asset_handle"] = m.model_handle.ToString();
        }
        model["receive_shadows"] = m.receive_shadows;
        model["enable_rendering"] = m.enable_rendering;

        // Serialize per-slot material handles and overrides
        if (!m.material_slot_handles.empty()) {
          json slots = json::array();
          for (size_t i = 0; i < m.material_slot_handles.size(); i++) {
            json slot;
            if (m.material_slot_handles[i].IsValid()) {
              slot["material_handle"] = m.material_slot_handles[i].ToString();
            }
            // Save material instance overrides
            if (i < m.material_instances.size() && m.material_instances[i] &&
                !m.material_instances[i]->overrides.empty()) {
              json overrides;
              for (const auto& [key, val] :
                   m.material_instances[i]->overrides) {
                if (std::holds_alternative<float>(val)) {
                  overrides[key] = std::get<float>(val);
                } else if (std::holds_alternative<glm::vec4>(val)) {
                  auto& v = std::get<glm::vec4>(val);
                  overrides[key] = {v.x, v.y, v.z, v.w};
                }
              }
              if (!overrides.empty()) {
                slot["overrides"] = overrides;
              }
            }
            slots.push_back(slot);
          }
          model["material_slots"] = slots;
        }

        return model;
      },
      // Deserialize
      [](Entity& entity, const json& mj, Scene* scene) {
        auto& m = entity.AddComponent<ModelComponent>();

        std::string handle_str = mj.value("asset_handle", "");
        std::string asset_name = mj.value("asset_name", "");
        std::string asset_path = mj.value("asset_path", "");

        if (!handle_str.empty()) {
          AssetHandle handle = AssetHandle::FromString(handle_str);
          if (!Engine::asset_manager().HasAsset(handle)) {
            // Handle not found, check if registered under a different
            // handle (from asset registry)
            if (!asset_path.empty()) {
              for (auto& h : Engine::asset_manager().GetAll()) {
                const auto* m2 = Engine::asset_manager().GetMetadata(h);
                if (m2 && m2->virtual_source_path == asset_path &&
                    m2->type == AssetType::Model) {
                  handle = h;
                  break;
                }
              }
            }
            // Still not found, register with the scene file's handle as
            // fallback
            if (!Engine::asset_manager().HasAsset(handle) &&
                !asset_path.empty()) {
              Engine::asset_manager().Register(handle, asset_name,
                                               AssetType::Model, asset_path);
            }
          }
          m.model_handle = handle;
          if (scene) {
            scene->RequestAsset(handle);
          }
        }

        m.receive_shadows = mj.value("receive_shadows", true);
        m.enable_rendering = mj.value("enable_rendering", true);

        // Restore per-slot material handles and overrides
        if (mj.contains("material_slots") && mj["material_slots"].is_array()) {
          for (size_t i = 0; i < mj["material_slots"].size(); i++) {
            const auto& slot = mj["material_slots"][i];
            // Resize vectors to fit
            if (m.material_slot_handles.size() <= i) {
              m.material_slot_handles.resize(i + 1);
              m.material_instances.resize(i + 1);
              m.material_versions.resize(i + 1, 0);
            }
            if (slot.contains("material_handle")) {
              m.material_slot_handles[i] = AssetHandle::FromString(
                  slot["material_handle"].get<std::string>());
              if (scene) {
                scene->RequestAsset(m.material_slot_handles[i]);
                RequestMaterialTextures(scene, m.material_slot_handles[i]);
              }
            }
            // Restore overrides
            if (slot.contains("overrides")) {
              if (!m.material_instances[i]) {
                m.material_instances[i] = std::make_shared<MaterialInstance>();
              }
              auto& inst = m.material_instances[i];
              for (auto& [key, val] : slot["overrides"].items()) {
                if (val.is_number()) {
                  inst->overrides[key] = val.get<float>();
                } else if (val.is_array() && val.size() >= 4) {
                  inst->overrides[key] =
                      glm::vec4(val[0], val[1], val[2], val[3]);
                }
              }
            }
          }
        }
      },
  });

  // -------------------------------------------------------------------
  // 4. LightDirect
  // -------------------------------------------------------------------
  ComponentSerializerRegistry::Register({
      "LightDirect",
      // Has
      [](Entity& entity) -> bool {
        return entity.HasComponent<LightDirectComponent>();
      },
      // Serialize
      [](Entity& entity) -> json {
        auto& l = entity.GetComponent<LightDirectComponent>();
        json light;
        light["color"] = SerializeUtil::Vec3(l.light_data.base.color);
        light["ambient"] = l.light_data.base.ambient;
        light["diffuse"] = l.light_data.base.diffuse;
        light["specular"] = l.light_data.base.specular;
        light["density"] = l.light_data.base.density;
        return light;
      },
      // Deserialize
      [](Entity& entity, const json& lj, Scene* /*scene*/) {
        auto& l = entity.AddComponent<LightDirectComponent>();
        l.light_data.base.color =
            SerializeUtil::Vec3(lj.value("color", json::array()), {1, 1, 1});
        l.light_data.base.ambient = lj.value("ambient", 0.2f);
        l.light_data.base.diffuse = lj.value("diffuse", 1.0f);
        l.light_data.base.specular = lj.value("specular", 0.85f);
        l.light_data.base.density = lj.value("density", 1.0f);
      },
  });

  // -------------------------------------------------------------------
  // 5. LightPoint
  // -------------------------------------------------------------------
  ComponentSerializerRegistry::Register({
      "LightPoint",
      // Has
      [](Entity& entity) -> bool {
        return entity.HasComponent<LightPointComponent>();
      },
      // Serialize
      [](Entity& entity) -> json {
        auto& l = entity.GetComponent<LightPointComponent>();
        json light;
        light["color"] = SerializeUtil::Vec3(l.light_data.base.color);
        light["ambient"] = l.light_data.base.ambient;
        light["diffuse"] = l.light_data.base.diffuse;
        light["specular"] = l.light_data.base.specular;
        light["density"] = l.light_data.base.density;
        light["constant"] = l.light_data.constant;
        light["linear"] = l.light_data.linear;
        light["exp"] = l.light_data.exp;
        return light;
      },
      // Deserialize
      [](Entity& entity, const json& lj, Scene* /*scene*/) {
        auto& l = entity.AddComponent<LightPointComponent>();
        l.light_data.base.color =
            SerializeUtil::Vec3(lj.value("color", json::array()), {1, 1, 1});
        l.light_data.base.ambient = lj.value("ambient", 0.2f);
        l.light_data.base.diffuse = lj.value("diffuse", 1.0f);
        l.light_data.base.specular = lj.value("specular", 0.85f);
        l.light_data.base.density = lj.value("density", 1.0f);
        l.light_data.constant = lj.value("constant", 1.0f);
        l.light_data.linear = lj.value("linear", 0.09f);
        l.light_data.exp = lj.value("exp", 0.032f);
      },
  });

  // -------------------------------------------------------------------
  // 6. RigidBody
  // -------------------------------------------------------------------
  ComponentSerializerRegistry::Register({
      "RigidBody",
      // Has
      [](Entity& entity) -> bool {
        return entity.HasComponent<RigidBodyComponent>();
      },
      // Serialize
      [](Entity& entity) -> json {
        auto& rb = entity.GetComponent<RigidBodyComponent>();
        json body;
        body["type"] = static_cast<int>(rb.type);
        body["mass"] = rb.mass;
        body["friction"] = rb.friction;
        body["restitution"] = rb.restitution;
        body["linear_damping"] = rb.linear_damping;
        body["angular_damping"] = rb.angular_damping;
        body["lock_position_x"] = rb.lock_position_x;
        body["lock_position_y"] = rb.lock_position_y;
        body["lock_position_z"] = rb.lock_position_z;
        body["lock_rotation_x"] = rb.lock_rotation_x;
        body["lock_rotation_y"] = rb.lock_rotation_y;
        body["lock_rotation_z"] = rb.lock_rotation_z;
        return body;
      },
      // Deserialize
      [](Entity& entity, const json& rbj, Scene* /*scene*/) {
        auto& rb = entity.AddComponent<RigidBodyComponent>();
        rb.type = static_cast<RigidBodyType>(rbj.value("type", 0));
        rb.mass = rbj.value("mass", 1.0f);
        rb.friction = rbj.value("friction", 0.5f);
        rb.restitution = rbj.value("restitution", 0.0f);
        rb.linear_damping = rbj.value("linear_damping", 0.0f);
        rb.angular_damping = rbj.value("angular_damping", 0.05f);
        rb.lock_position_x = rbj.value("lock_position_x", false);
        rb.lock_position_y = rbj.value("lock_position_y", false);
        rb.lock_position_z = rbj.value("lock_position_z", false);
        rb.lock_rotation_x = rbj.value("lock_rotation_x", false);
        rb.lock_rotation_y = rbj.value("lock_rotation_y", false);
        rb.lock_rotation_z = rbj.value("lock_rotation_z", false);
      },
  });

  // -------------------------------------------------------------------
  // 7. BoxCollider
  // -------------------------------------------------------------------
  ComponentSerializerRegistry::Register({
      "BoxCollider",
      // Has
      [](Entity& entity) -> bool {
        return entity.HasComponent<BoxColliderComponent>();
      },
      // Serialize
      [](Entity& entity) -> json {
        auto& bc = entity.GetComponent<BoxColliderComponent>();
        json collider;
        collider["offset"] = SerializeUtil::Vec3(bc.offset);
        collider["half_extents"] = SerializeUtil::Vec3(bc.half_extents);
        collider["is_trigger"] = bc.is_trigger;
        collider["collision_group"] = bc.collision_group;
        return collider;
      },
      // Deserialize
      [](Entity& entity, const json& bcj, Scene* /*scene*/) {
        auto& bc = entity.AddComponent<BoxColliderComponent>();
        bc.offset = SerializeUtil::Vec3(bcj.value("offset", json::array()));
        bc.half_extents = SerializeUtil::Vec3(
            bcj.value("half_extents", json::array()), {0.5f, 0.5f, 0.5f});
        bc.is_trigger = bcj.value("is_trigger", false);
        bc.collision_group = bcj.value("collision_group", 1);
      },
  });

  // -------------------------------------------------------------------
  // 8. SphereCollider
  // -------------------------------------------------------------------
  ComponentSerializerRegistry::Register({
      "SphereCollider",
      // Has
      [](Entity& entity) -> bool {
        return entity.HasComponent<SphereColliderComponent>();
      },
      // Serialize
      [](Entity& entity) -> json {
        auto& sc = entity.GetComponent<SphereColliderComponent>();
        json collider;
        collider["offset"] = SerializeUtil::Vec3(sc.offset);
        collider["radius"] = sc.radius;
        collider["is_trigger"] = sc.is_trigger;
        collider["collision_group"] = sc.collision_group;
        return collider;
      },
      // Deserialize
      [](Entity& entity, const json& scj, Scene* /*scene*/) {
        auto& sc = entity.AddComponent<SphereColliderComponent>();
        sc.offset = SerializeUtil::Vec3(scj.value("offset", json::array()));
        sc.radius = scj.value("radius", 0.5f);
        sc.is_trigger = scj.value("is_trigger", false);
        sc.collision_group = scj.value("collision_group", 1);
      },
  });

  // -------------------------------------------------------------------
  // 9. CapsuleCollider
  // -------------------------------------------------------------------
  ComponentSerializerRegistry::Register({
      "CapsuleCollider",
      // Has
      [](Entity& entity) -> bool {
        return entity.HasComponent<CapsuleColliderComponent>();
      },
      // Serialize
      [](Entity& entity) -> json {
        auto& cc = entity.GetComponent<CapsuleColliderComponent>();
        json collider;
        collider["offset"] = SerializeUtil::Vec3(cc.offset);
        collider["radius"] = cc.radius;
        collider["height"] = cc.height;
        collider["axis"] = static_cast<int>(cc.axis);
        collider["is_trigger"] = cc.is_trigger;
        collider["collision_group"] = cc.collision_group;
        return collider;
      },
      // Deserialize
      [](Entity& entity, const json& ccj, Scene* /*scene*/) {
        auto& cc = entity.AddComponent<CapsuleColliderComponent>();
        cc.offset = SerializeUtil::Vec3(ccj.value("offset", json::array()));
        cc.radius = ccj.value("radius", 0.3f);
        cc.height = ccj.value("height", 1.0f);
        cc.axis = static_cast<CapsuleAxis>(ccj.value("axis", 1));
        cc.is_trigger = ccj.value("is_trigger", false);
        cc.collision_group = ccj.value("collision_group", 1);
      },
  });

  // -------------------------------------------------------------------
  // 10. Behaviors (C# scripts with field values)
  // -------------------------------------------------------------------
  ComponentSerializerRegistry::Register({
      "Behaviors",
      // Has
      [](Entity& entity) -> bool {
        return entity.HasComponent<BehaviorsComponent>();
      },
      // Serialize
      [](Entity& entity) -> json {
        auto& bc = entity.GetComponent<BehaviorsComponent>();
        Scene* scene = entity.GetScene();
        json scripts = json::array();
        for (auto& [name, behavior] : bc.behaviors_) {
          json script_json;
          script_json["name"] = name;

          // Serialize field values from the ScriptInstance
          auto* mono_behavior = dynamic_cast<MonoBehavior*>(behavior);
          if (mono_behavior && mono_behavior->script_instance()) {
            ScriptInstance* instance = mono_behavior->script_instance();
            json fields_json;
            for (auto& [field_name, field_data] :
                 instance->script_data().fields()) {
              if (field_data.field_type() == FieldType::Float) {
                fields_json[field_name] =
                    field_data.Get<float>(instance->handle());
              } else if (field_data.field_type() == FieldType::Integer) {
                fields_json[field_name] =
                    field_data.Get<int32_t>(instance->handle());
              } else if (field_data.field_type() == FieldType::Boolean) {
                fields_json[field_name] =
                    field_data.Get<bool>(instance->handle());
              } else if (field_data.field_type() == FieldType::String) {
                MonoObject* val =
                    field_data.Get<MonoObject*>(instance->handle());
                if (val) {
                  MonoObjectWrapper wrapper{val};
                  fields_json[field_name] = wrapper.AsString();
                }
              } else if (field_data.field_type() == FieldType::Prefab) {
                MonoObject* prefab_obj =
                    field_data.Get<MonoObject*>(instance->handle());
                if (prefab_obj) {
                  MonoClassField* path_field = mono_class_get_field_from_name(
                      Engine::script_manager().prefab_class(), "path");
                  if (path_field) {
                    MonoString* path_str = nullptr;
                    mono_field_get_value(prefab_obj, path_field, &path_str);
                    if (path_str) {
                      const char* cstr = mono_string_to_utf8(path_str);
                      if (cstr && cstr[0]) {
                        json pj;
                        pj["type"] = "Prefab";
                        pj["path"] = cstr;
                        fields_json[field_name] = pj;
                      }
                      mono_free((void*)cstr);
                    }
                  }
                }
              } else if (field_data.field_type() == FieldType::Entity) {
                MonoObject* entity_obj =
                    field_data.Get<MonoObject*>(instance->handle());
                if (entity_obj) {
                  MonoClassField* id_field = mono_class_get_field_from_name(
                      Engine::script_manager().entity_class(), "entityId");
                  if (id_field) {
                    uint64_t id_val = 0;
                    mono_field_get_value(entity_obj, id_field, &id_val);
                    entt::entity ent = static_cast<entt::entity>(id_val);
                    if (scene && scene->HasEntity(ent) &&
                        scene->HasComponent<IdComponent>(ent)) {
                      json ej;
                      ej["type"] = "Entity";
                      ej["uuid"] =
                          scene->GetComponent<IdComponent>(ent).Id.ToString();
                      fields_json[field_name] = ej;
                    }
                  }
                }
              }
            }
            if (!fields_json.empty()) {
              script_json["fields"] = fields_json;
            }
          }

          scripts.push_back(script_json);
        }
        return scripts;
      },
      // Deserialize
      [](Entity& entity, const json& behaviors_json, Scene* scene) {
        if (!behaviors_json.is_array()) {
          return;
        }
        if (!entity.HasComponent<BehaviorsComponent>()) {
          entity.AddComponent<BehaviorsComponent>();
        }
        auto& bc = entity.GetComponent<BehaviorsComponent>();
        for (const auto& script_entry : behaviors_json) {
          std::string script_name;
          json fields_json;

          if (script_entry.is_string()) {
            // Old format: just a script name string
            script_name = script_entry.get<std::string>();
          } else if (script_entry.is_object()) {
            // New format: {name, fields}
            script_name = script_entry.value("name", "");
            if (script_entry.contains("fields")) {
              fields_json = script_entry["fields"];
            }
          }

          if (script_name.empty()) {
            continue;
          }

          // Check native behavior registry first, then fall back to C#
          // MonoBehavior
          auto& native_registry = Engine::behavior_registry();
          if (native_registry.Has(script_name)) {
            NativeBehavior* native =
                native_registry.Create(script_name, entity);
            if (native) {
              bc.behaviors_.insert(std::pair(script_name, native));
            }
            continue;  // native behaviors don't have C# field serialization
          }

          auto& mono_beh = bc.AddBehavior<MonoBehavior>(entity, script_name);

          // Restore field values
          if (!fields_json.empty() && !mono_beh.script_instance()) {
            LOG_WARN(
                "Script '{}' has no instance - field values cannot be "
                "restored",
                script_name);
          }
          if (!fields_json.empty() && mono_beh.script_instance()) {
            auto* instance = mono_beh.script_instance();
            for (auto& [field_name, field_data] :
                 instance->script_data().fields()) {
              if (!fields_json.contains(field_name)) {
                continue;
              }
              const auto& val = fields_json[field_name];

              if (field_data.field_type() == FieldType::Float &&
                  val.is_number()) {
                float v = val.get<float>();
                field_data.Set(instance->handle(), &v);
              } else if (field_data.field_type() == FieldType::Integer &&
                         val.is_number_integer()) {
                int32_t v = val.get<int32_t>();
                field_data.Set(instance->handle(), &v);
              } else if (field_data.field_type() == FieldType::Boolean &&
                         val.is_boolean()) {
                bool v = val.get<bool>();
                field_data.Set(instance->handle(), &v);
              } else if (field_data.field_type() == FieldType::String &&
                         val.is_string()) {
                MonoString* str =
                    mono_string_new(Engine::script_manager().app_domain(),
                                    val.get<std::string>().c_str());
                field_data.Set(instance->handle(), str);
              } else if (field_data.field_type() == FieldType::Prefab &&
                         val.is_object()) {
                std::string path = val.value("path", "");
                if (!path.empty()) {
                  MonoObject* prefab =
                      mono_object_new(Engine::script_manager().app_domain(),
                                      Engine::script_manager().prefab_class());
                  mono_runtime_object_init(prefab);
                  MonoClassField* path_field = mono_class_get_field_from_name(
                      Engine::script_manager().prefab_class(), "path");
                  if (path_field) {
                    MonoString* path_val = mono_string_new(
                        Engine::script_manager().app_domain(), path.c_str());
                    mono_field_set_value(prefab, path_field, path_val);
                  }
                  field_data.Set(instance->handle(), prefab);
                }
              } else if (field_data.field_type() == FieldType::Entity &&
                         val.is_object()) {
                std::string uuid_str = val.value("uuid", "");
                if (!uuid_str.empty() && scene) {
                  UUID uuid = UUID::FromString(uuid_str);
                  entt::entity ent = scene->FindEntityByUUID(uuid);
                  if (ent != entt::null) {
                    MonoObject* entity_obj = mono_object_new(
                        Engine::script_manager().app_domain(),
                        Engine::script_manager().entity_class());
                    MonoMethod* ctor = mono_class_get_method_from_name(
                        Engine::script_manager().entity_class(), ".ctor", 2);
                    uint64_t scene_ptr = reinterpret_cast<uint64_t>(scene);
                    uint64_t entity_id = static_cast<uint64_t>(ent);
                    void* args[2] = {&scene_ptr, &entity_id};
                    mono_runtime_invoke(ctor, entity_obj, args, nullptr);
                    field_data.Set(instance->handle(), entity_obj);
                  }
                }
              }
            }
          }
        }
      },
  });

  // -------------------------------------------------------------------
  // 11. RectangleTransform
  // -------------------------------------------------------------------
  ComponentSerializerRegistry::Register({
      "RectangleTransform",
      // Has
      [](Entity& entity) -> bool {
        return entity.HasComponent<RectangleTransformComponent>();
      },
      // Serialize
      [](Entity& entity) -> json {
        auto& rt = entity.GetComponent<RectangleTransformComponent>();
        json rtj;
        rtj["position"] = SerializeUtil::Vec2(rt.position);
        rtj["rotation"] = rt.rotation;
        rtj["size"] = SerializeUtil::Vec2(rt.size);
        rtj["scale"] = SerializeUtil::Vec2(rt.scale);
        rtj["anchor"] = static_cast<int>(rt.anchor);
        rtj["pivot"] = static_cast<int>(rt.pivot);
        rtj["size_mode_x"] = static_cast<int>(rt.size_mode_x);
        rtj["size_mode_y"] = static_cast<int>(rt.size_mode_y);
        rtj["padding"] = {rt.padding.x, rt.padding.y, rt.padding.z,
                          rt.padding.w};
        if (rt.margin.x != 0 || rt.margin.y != 0 || rt.margin.z != 0 ||
            rt.margin.w != 0) {
          rtj["margin"] = {rt.margin.x, rt.margin.y, rt.margin.z, rt.margin.w};
        }
        return rtj;
      },
      // Deserialize
      [](Entity& entity, const json& rtj, Scene* /*scene*/) {
        auto& rt = entity.HasComponent<RectangleTransformComponent>()
                       ? entity.GetComponent<RectangleTransformComponent>()
                       : entity.AddComponent<RectangleTransformComponent>();
        rt.position = SerializeUtil::Vec2(rtj.value("position", json::array()));
        rt.rotation = rtj.value("rotation", 0.0f);
        rt.size =
            SerializeUtil::Vec2(rtj.value("size", json::array()), {100, 100});
        rt.scale =
            SerializeUtil::Vec2(rtj.value("scale", json::array()), {1, 1});
        rt.anchor = static_cast<AnchorPreset>(rtj.value("anchor", 0));
        rt.pivot = static_cast<AnchorPreset>(rtj.value("pivot", 0));
        rt.size_mode_x = static_cast<SizeMode>(rtj.value("size_mode_x", 0));
        rt.size_mode_y = static_cast<SizeMode>(rtj.value("size_mode_y", 0));
        if (rtj.contains("padding") && rtj["padding"].is_array() &&
            rtj["padding"].size() >= 4) {
          rt.padding = {rtj["padding"][0], rtj["padding"][1], rtj["padding"][2],
                        rtj["padding"][3]};
        }
        if (rtj.contains("margin") && rtj["margin"].is_array() &&
            rtj["margin"].size() >= 4) {
          rt.margin = {rtj["margin"][0], rtj["margin"][1], rtj["margin"][2],
                       rtj["margin"][3]};
        }
      },
  });

  // -------------------------------------------------------------------
  // 12. Canvas
  // -------------------------------------------------------------------
  ComponentSerializerRegistry::Register({
      "Canvas",
      // Has
      [](Entity& entity) -> bool {
        return entity.HasComponent<CanvasComponent>();
      },
      // Serialize
      [](Entity& entity) -> json {
        auto& c = entity.GetComponent<CanvasComponent>();
        json cj;
        cj["render_mode"] = static_cast<int>(c.render_mode);
        cj["direction"] = static_cast<int>(c.direction);
        cj["alignment"] = static_cast<int>(c.alignment);
        cj["spacing"] = c.spacing;
        cj["start_spacing"] = c.start_spacing;
        cj["end_spacing"] = c.end_spacing;
        cj["sort_order"] = c.sort_order;
        if (c.render_mode == CanvasRenderMode::ScreenSpaceCamera) {
          cj["plane_distance"] = c.plane_distance;
          // Serialize camera reference as UUID
          if (c.camera_entity != entt::null) {
            Scene* s = entity.GetScene();
            if (s && s->HasComponent<IdComponent>(c.camera_entity)) {
              cj["camera_uuid"] =
                  s->GetComponent<IdComponent>(c.camera_entity).Id.ToString();
            }
          }
        }
        if (c.player_index != 0) {
          cj["player_index"] = c.player_index;
        }
        return cj;
      },
      // Deserialize
      [](Entity& entity, const json& cj, Scene* scene) {
        auto& c = entity.AddComponent<CanvasComponent>();
        c.render_mode = static_cast<CanvasRenderMode>(
            cj.value("render_mode",
                     cj.value("type", 0)));  // fallback to old "type" field
        c.direction = static_cast<LayoutDirection>(cj.value("direction", 0));
        c.alignment = static_cast<ChildAlignment>(cj.value("alignment", 0));
        c.spacing = cj.value("spacing", 0.0f);
        c.start_spacing = cj.value("start_spacing", 0.0f);
        c.end_spacing = cj.value("end_spacing", 0.0f);
        c.sort_order = cj.value("sort_order", 0);
        c.plane_distance = cj.value("plane_distance", 10.0f);
        if (cj.contains("camera_uuid") && cj["camera_uuid"].is_string() &&
            scene) {
          UUID cam_uuid =
              UUID::FromString(cj["camera_uuid"].get<std::string>());
          c.camera_entity = scene->FindEntityByUUID(cam_uuid);
        }
        c.player_index = cj.value("player_index", 0);
      },
  });

  // -------------------------------------------------------------------
  // 13. CanvasScaler
  // -------------------------------------------------------------------
  ComponentSerializerRegistry::Register({
      "CanvasScaler",
      // Has
      [](Entity& entity) -> bool {
        return entity.HasComponent<CanvasScalerComponent>();
      },
      // Serialize
      [](Entity& entity) -> json {
        auto& cs = entity.GetComponent<CanvasScalerComponent>();
        json csj;
        csj["scale_mode"] = static_cast<int>(cs.scale_mode);
        csj["reference_resolution"] = {cs.reference_resolution.x,
                                       cs.reference_resolution.y};
        csj["match_width_or_height"] = cs.match_width_or_height;
        csj["reference_pixels_per_unit"] = cs.reference_pixels_per_unit;
        return csj;
      },
      // Deserialize
      [](Entity& entity, const json& csj, Scene* /*scene*/) {
        auto& cs = entity.AddComponent<CanvasScalerComponent>();
        cs.scale_mode = static_cast<ScaleMode>(csj.value("scale_mode", 0));
        if (csj.contains("reference_resolution") &&
            csj["reference_resolution"].is_array() &&
            csj["reference_resolution"].size() >= 2) {
          cs.reference_resolution = {csj["reference_resolution"][0],
                                     csj["reference_resolution"][1]};
        }
        cs.match_width_or_height = csj.value("match_width_or_height", 0.5f);
        cs.reference_pixels_per_unit =
            csj.value("reference_pixels_per_unit", 100.0f);
      },
  });

  // -------------------------------------------------------------------
  // 14. CanvasRect
  // -------------------------------------------------------------------
  ComponentSerializerRegistry::Register({
      "CanvasRect",
      // Has
      [](Entity& entity) -> bool {
        return entity.HasComponent<CanvasRectComponent>();
      },
      // Serialize
      [](Entity& entity) -> json {
        auto& cr = entity.GetComponent<CanvasRectComponent>();
        json crj;
        crj["color"] = {cr.color.x, cr.color.y, cr.color.z, cr.color.w};
        return crj;
      },
      // Deserialize
      [](Entity& entity, const json& crj, Scene* /*scene*/) {
        auto& cr = entity.AddComponent<CanvasRectComponent>();
        if (crj.contains("color") && crj["color"].is_array() &&
            crj["color"].size() >= 4) {
          cr.color = {crj["color"][0], crj["color"][1], crj["color"][2],
                      crj["color"][3]};
        }
      },
  });

  // -------------------------------------------------------------------
  // 15. CanvasImage
  // -------------------------------------------------------------------
  ComponentSerializerRegistry::Register({
      "CanvasImage",
      // Has
      [](Entity& entity) -> bool {
        return entity.HasComponent<CanvasImageComponent>();
      },
      // Serialize
      [](Entity& entity) -> json {
        auto& ci = entity.GetComponent<CanvasImageComponent>();
        json cij;
        if (ci.texture_handle.IsValid()) {
          cij["texture"] = ci.texture_handle.ToString();
        }
        cij["tint"] = {ci.tint.x, ci.tint.y, ci.tint.z, ci.tint.w};
        cij["uv_rect"] = {ci.uv_rect.x, ci.uv_rect.y, ci.uv_rect.z,
                          ci.uv_rect.w};
        return cij;
      },
      // Deserialize
      [](Entity& entity, const json& cij, Scene* /*scene*/) {
        auto& ci = entity.AddComponent<CanvasImageComponent>();
        if (cij.contains("texture") && cij["texture"].is_string()) {
          ci.texture_handle =
              AssetHandle::FromString(cij["texture"].get<std::string>());
        }
        if (cij.contains("tint") && cij["tint"].is_array() &&
            cij["tint"].size() >= 4) {
          ci.tint = {cij["tint"][0], cij["tint"][1], cij["tint"][2],
                     cij["tint"][3]};
        }
        if (cij.contains("uv_rect") && cij["uv_rect"].is_array() &&
            cij["uv_rect"].size() >= 4) {
          ci.uv_rect = {cij["uv_rect"][0], cij["uv_rect"][1], cij["uv_rect"][2],
                        cij["uv_rect"][3]};
        }
      },
  });

  // -------------------------------------------------------------------
  // 16. Text
  // -------------------------------------------------------------------
  ComponentSerializerRegistry::Register({
      "Text",
      // Has
      [](Entity& entity) -> bool {
        return entity.HasComponent<TextComponent>();
      },
      // Serialize
      [](Entity& entity) -> json {
        auto& t = entity.GetComponent<TextComponent>();
        json tj;
        tj["text"] = t.text;
        if (t.font_handle.IsValid()) {
          tj["font_handle"] = t.font_handle.ToString();
        }
        tj["font_size"] = t.font_size;
        tj["color"] = {t.color.x, t.color.y, t.color.z, t.color.w};
        if (t.shadow) {
          tj["shadow"] = true;
          tj["shadow_offset"] = SerializeUtil::Vec2(t.shadow_offset);
          tj["shadow_color"] = {t.shadow_color.x, t.shadow_color.y,
                                t.shadow_color.z, t.shadow_color.w};
        }
        return tj;
      },
      // Deserialize
      [](Entity& entity, const json& tj, Scene* scene) {
        auto& t = entity.AddComponent<TextComponent>();
        t.text = tj.value("text", "");
        if (tj.contains("font_handle") && tj["font_handle"].is_string()) {
          t.font_handle =
              AssetHandle::FromString(tj["font_handle"].get<std::string>());
          if (scene) {
            scene->RequestAsset(t.font_handle);
          }
        }
        t.font_size = tj.value("font_size", 16.0f);
        if (tj.contains("color") && tj["color"].is_array() &&
            tj["color"].size() >= 4) {
          t.color = {tj["color"][0], tj["color"][1], tj["color"][2],
                     tj["color"][3]};
        }
        t.shadow = tj.value("shadow", false);
        if (tj.contains("shadow_offset")) {
          t.shadow_offset = SerializeUtil::Vec2(tj["shadow_offset"], {1, 1});
        }
        if (tj.contains("shadow_color") && tj["shadow_color"].is_array() &&
            tj["shadow_color"].size() >= 4) {
          t.shadow_color = {tj["shadow_color"][0], tj["shadow_color"][1],
                            tj["shadow_color"][2], tj["shadow_color"][3]};
        }
      },
  });

  // -------------------------------------------------------------------
  // 17. TextInput
  // -------------------------------------------------------------------
  ComponentSerializerRegistry::Register({
      "TextInput",
      // Has
      [](Entity& entity) -> bool {
        return entity.HasComponent<TextInputComponent>();
      },
      // Serialize
      [](Entity& entity) -> json {
        auto& ti = entity.GetComponent<TextInputComponent>();
        json tij;
        tij["text"] = ti.text;
        tij["placeholder"] = ti.placeholder;
        tij["max_length"] = ti.max_length;
        tij["cursor_color"] = {ti.cursor_color.x, ti.cursor_color.y,
                               ti.cursor_color.z, ti.cursor_color.w};
        tij["placeholder_color"] = {
            ti.placeholder_color.x, ti.placeholder_color.y,
            ti.placeholder_color.z, ti.placeholder_color.w};
        return tij;
      },
      // Deserialize
      [](Entity& entity, const json& tij, Scene* /*scene*/) {
        auto& ti = entity.AddComponent<TextInputComponent>();
        ti.text = tij.value("text", "");
        ti.placeholder = tij.value("placeholder", "Enter text...");
        ti.max_length = tij.value("max_length", 0);
        if (tij.contains("cursor_color") && tij["cursor_color"].is_array() &&
            tij["cursor_color"].size() >= 4) {
          ti.cursor_color = {tij["cursor_color"][0], tij["cursor_color"][1],
                             tij["cursor_color"][2], tij["cursor_color"][3]};
        }
        if (tij.contains("placeholder_color") &&
            tij["placeholder_color"].is_array() &&
            tij["placeholder_color"].size() >= 4) {
          ti.placeholder_color = {
              tij["placeholder_color"][0], tij["placeholder_color"][1],
              tij["placeholder_color"][2], tij["placeholder_color"][3]};
        }
      },
  });

  // -------------------------------------------------------------------
  // 18. AudioSource
  // -------------------------------------------------------------------
  ComponentSerializerRegistry::Register({
      "AudioSource",
      // Has
      [](Entity& entity) -> bool {
        return entity.HasComponent<AudioSourceComponent>();
      },
      // Serialize
      [](Entity& entity) -> json {
        auto& a = entity.GetComponent<AudioSourceComponent>();
        json aj;
        if (a.clip.IsValid()) {
          aj["clip"] = a.clip.ToString();
        }
        aj["bus"] = static_cast<int>(a.bus);
        aj["volume"] = a.volume;
        aj["pitch"] = a.pitch;
        aj["loop"] = a.loop;
        aj["play_on_start"] = a.play_on_start;
        aj["mute"] = a.mute;
        aj["spatial_blend"] = a.spatial_blend;
        aj["min_distance"] = a.min_distance;
        aj["max_distance"] = a.max_distance;
        return aj;
      },
      // Deserialize
      [](Entity& entity, const json& aj, Scene* scene) {
        auto& a = entity.AddComponent<AudioSourceComponent>();
        std::string clip_str = aj.value("clip", "");
        if (!clip_str.empty()) {
          a.clip = AssetHandle::FromString(clip_str);
          if (scene) {
            scene->RequestAsset(a.clip);
          }
        }
        a.bus = static_cast<AudioBus>(aj.value("bus", 1));
        a.volume = aj.value("volume", 1.0f);
        a.pitch = aj.value("pitch", 1.0f);
        a.loop = aj.value("loop", false);
        a.play_on_start = aj.value("play_on_start", false);
        a.mute = aj.value("mute", false);
        a.spatial_blend = aj.value("spatial_blend", 0.0f);
        a.min_distance = aj.value("min_distance", 1.0f);
        a.max_distance = aj.value("max_distance", 100.0f);
      },
  });

  // -------------------------------------------------------------------
  // 19. ReverbZone
  // -------------------------------------------------------------------
  ComponentSerializerRegistry::Register({
      "ReverbZone",
      // Has
      [](Entity& entity) -> bool {
        return entity.HasComponent<ReverbZoneComponent>();
      },
      // Serialize
      [](Entity& entity) -> json {
        auto& r = entity.GetComponent<ReverbZoneComponent>();
        json rj;
        rj["radius"] = r.radius;
        rj["delay_ms"] = r.delay_ms;
        rj["decay"] = r.decay;
        rj["wet"] = r.wet;
        return rj;
      },
      // Deserialize
      [](Entity& entity, const json& rj, Scene* /*scene*/) {
        auto& r = entity.AddComponent<ReverbZoneComponent>();
        r.radius = rj.value("radius", 10.0f);
        r.delay_ms = rj.value("delay_ms", 150.0f);
        r.decay = rj.value("decay", 0.4f);
        r.wet = rj.value("wet", 0.5f);
      },
  });

  // -------------------------------------------------------------------
  // 20. Button
  // -------------------------------------------------------------------
  ComponentSerializerRegistry::Register({
      "Button",
      // Has
      [](Entity& entity) -> bool {
        return entity.HasComponent<ButtonComponent>();
      },
      // Serialize
      [](Entity& entity) -> json {
        auto& btn = entity.GetComponent<ButtonComponent>();
        json bj;
        bj["normal_color"] = {btn.normal_color.r, btn.normal_color.g,
                              btn.normal_color.b, btn.normal_color.a};
        bj["hovered_color"] = {btn.hovered_color.r, btn.hovered_color.g,
                               btn.hovered_color.b, btn.hovered_color.a};
        bj["pressed_color"] = {btn.pressed_color.r, btn.pressed_color.g,
                               btn.pressed_color.b, btn.pressed_color.a};
        bj["selected_color"] = {btn.selected_color.r, btn.selected_color.g,
                                btn.selected_color.b, btn.selected_color.a};
        bj["disabled_color"] = {btn.disabled_color.r, btn.disabled_color.g,
                                btn.disabled_color.b, btn.disabled_color.a};
        if (btn.normal_texture.IsValid()) {
          bj["normal_texture"] = btn.normal_texture.ToString();
        }
        if (btn.hovered_texture.IsValid()) {
          bj["hovered_texture"] = btn.hovered_texture.ToString();
        }
        if (btn.pressed_texture.IsValid()) {
          bj["pressed_texture"] = btn.pressed_texture.ToString();
        }
        if (btn.selected_texture.IsValid()) {
          bj["selected_texture"] = btn.selected_texture.ToString();
        }
        if (btn.disabled_texture.IsValid()) {
          bj["disabled_texture"] = btn.disabled_texture.ToString();
        }
        if (btn.hovered_offset.x != 0 || btn.hovered_offset.y != 0) {
          bj["hovered_offset"] = SerializeUtil::Vec2(btn.hovered_offset);
        }
        if (btn.pressed_offset.x != 0 || btn.pressed_offset.y != 0) {
          bj["pressed_offset"] = SerializeUtil::Vec2(btn.pressed_offset);
        }
        if (btn.selected_offset.x != 0 || btn.selected_offset.y != 0) {
          bj["selected_offset"] = SerializeUtil::Vec2(btn.selected_offset);
        }
        return bj;
      },
      // Deserialize
      [](Entity& entity, const json& bj, Scene* /*scene*/) {
        auto& btn = entity.AddComponent<ButtonComponent>();
        auto load_color = [](const json& j, const std::string& key,
                             glm::vec4 def) -> glm::vec4 {
          if (j.contains(key) && j[key].is_array() && j[key].size() >= 4) {
            return {j[key][0], j[key][1], j[key][2], j[key][3]};
          }
          return def;
        };
        btn.normal_color = load_color(bj, "normal_color", {1, 1, 1, 1});
        btn.hovered_color =
            load_color(bj, "hovered_color", {0.9f, 0.9f, 0.9f, 1});
        btn.pressed_color =
            load_color(bj, "pressed_color", {0.7f, 0.7f, 0.7f, 1});
        btn.selected_color = load_color(bj, "selected_color", {1, 1, 1, 1});
        btn.disabled_color =
            load_color(bj, "disabled_color", {0.5f, 0.5f, 0.5f, 0.5f});

        auto load_handle = [](const json& j,
                              const std::string& key) -> AssetHandle {
          if (j.contains(key) && j[key].is_string()) {
            return AssetHandle::FromString(j[key].get<std::string>());
          }
          return {};
        };
        btn.normal_texture = load_handle(bj, "normal_texture");
        btn.hovered_texture = load_handle(bj, "hovered_texture");
        btn.pressed_texture = load_handle(bj, "pressed_texture");
        btn.selected_texture = load_handle(bj, "selected_texture");
        btn.disabled_texture = load_handle(bj, "disabled_texture");
        if (bj.contains("hovered_offset")) {
          btn.hovered_offset = SerializeUtil::Vec2(bj["hovered_offset"]);
        }
        if (bj.contains("pressed_offset")) {
          btn.pressed_offset = SerializeUtil::Vec2(bj["pressed_offset"]);
        }
        if (bj.contains("selected_offset")) {
          btn.selected_offset = SerializeUtil::Vec2(bj["selected_offset"]);
        }
      },
  });

  // -------------------------------------------------------------------
  // 21. Interactable
  // -------------------------------------------------------------------
  ComponentSerializerRegistry::Register({
      "Interactable",
      // Has
      [](Entity& entity) -> bool {
        return entity.HasComponent<InteractableComponent>();
      },
      // Serialize
      [](Entity& entity) -> json {
        auto& ic = entity.GetComponent<InteractableComponent>();
        json ij;
        ij["enabled"] = ic.enabled;
        ij["blocks_raycast"] = ic.blocks_raycast;
        return ij;
      },
      // Deserialize
      [](Entity& entity, const json& ij, Scene* /*scene*/) {
        auto& ic = entity.AddComponent<InteractableComponent>();
        ic.enabled = ij.value("enabled", true);
        ic.blocks_raycast = ij.value("blocks_raycast", true);
      },
  });

  // -------------------------------------------------------------------
  // 22. Navigable
  // -------------------------------------------------------------------
  ComponentSerializerRegistry::Register({
      "Navigable",
      // Has
      [](Entity& entity) -> bool {
        return entity.HasComponent<NavigableComponent>();
      },
      // Serialize
      [](Entity& entity) -> json {
        auto& nav = entity.GetComponent<NavigableComponent>();
        json nj;
        Scene* s = entity.GetScene();
        auto serialize_nav = [&](const std::string& key, entt::entity e) {
          if (e != entt::null && s && s->GetRegistry().valid(e) &&
              s->HasComponent<IdComponent>(e)) {
            nj[key] = s->GetComponent<IdComponent>(e).Id.ToString();
          }
        };
        serialize_nav("nav_up", nav.nav_up);
        serialize_nav("nav_down", nav.nav_down);
        serialize_nav("nav_left", nav.nav_left);
        serialize_nav("nav_right", nav.nav_right);
        return nj;
      },
      // Deserialize
      [](Entity& entity, const json& nj, Scene* scene) {
        auto& nav = entity.AddComponent<NavigableComponent>();
        auto load_nav = [&](const std::string& key) -> entt::entity {
          if (nj.contains(key) && nj[key].is_string() && scene) {
            UUID uuid = UUID::FromString(nj[key].get<std::string>());
            entt::entity ent = scene->FindEntityByUUID(uuid);
            if (ent != entt::null) {
              return ent;
            }
          }
          return entt::null;
        };
        nav.nav_up = load_nav("nav_up");
        nav.nav_down = load_nav("nav_down");
        nav.nav_left = load_nav("nav_left");
        nav.nav_right = load_nav("nav_right");
      },
  });

  // -------------------------------------------------------------------
  // 23. SpriteRenderer
  // -------------------------------------------------------------------
  ComponentSerializerRegistry::Register({
      "SpriteRenderer",
      // Has
      [](Entity& entity) -> bool {
        return entity.HasComponent<SpriteRendererComponent>();
      },
      // Serialize
      [](Entity& entity) -> json {
        auto& s = entity.GetComponent<SpriteRendererComponent>();
        json sj;
        if (s.sprite_handle_.IsValid()) {
          sj["sprite_handle"] = s.sprite_handle_.ToString();
        }
        sj["flip_x"] = s.flip_x_;
        sj["flip_y"] = s.flip_y_;
        sj["tint"] = {s.tint_.r, s.tint_.g, s.tint_.b, s.tint_.a};
        sj["sort_layer"] = s.sort_layer_;
        sj["pivot"] = {s.pivot_.x, s.pivot_.y};
        return sj;
      },
      // Deserialize
      [](Entity& entity, const json& sj, Scene* scene) {
        auto& s = entity.AddComponent<SpriteRendererComponent>();

        std::string handle_str = sj.value("sprite_handle", "");
        if (!handle_str.empty()) {
          s.sprite_handle_ = AssetHandle::FromString(handle_str);
          if (scene) {
            scene->RequestAsset(s.sprite_handle_);
          }
          // Ensure the .wsprite (and its backing texture) is loaded
          if (s.sprite_handle_.IsValid()) {
            auto gpu =
                Engine::asset_manager().Get<SpriteGpuData>(s.sprite_handle_);
            if (!gpu) {
              Engine::asset_manager().LoadSync(s.sprite_handle_);
            }
          }
        }

        s.flip_x_ = sj.value("flip_x", false);
        s.flip_y_ = sj.value("flip_y", false);
        if (sj.contains("tint") && sj["tint"].is_array() &&
            sj["tint"].size() >= 4) {
          s.tint_ = {sj["tint"][0], sj["tint"][1], sj["tint"][2],
                     sj["tint"][3]};
        }
        s.sort_layer_ = sj.value("sort_layer", 0);
        if (sj.contains("pivot") && sj["pivot"].is_array() &&
            sj["pivot"].size() >= 2) {
          s.pivot_ = {sj["pivot"][0], sj["pivot"][1]};
        }
      },
  });

  // -------------------------------------------------------------------
  // 25. SpriteAnimator
  // -------------------------------------------------------------------
  ComponentSerializerRegistry::Register({
      "SpriteAnimator",
      // Has
      [](Entity& entity) -> bool {
        return entity.HasComponent<SpriteAnimatorComponent>();
      },
      // Serialize
      [](Entity& entity) -> json {
        auto& s = entity.GetComponent<SpriteAnimatorComponent>();
        json sj;
        if (s.controller_handle_.IsValid()) {
          sj["controller_handle"] = s.controller_handle_.ToString();
        }
        sj["playing"] = s.playing_;
        return sj;
      },
      // Deserialize
      [](Entity& entity, const json& sj, Scene* scene) {
        auto& s = entity.AddComponent<SpriteAnimatorComponent>();

        std::string handle_str = sj.value("controller_handle", "");
        if (!handle_str.empty()) {
          s.controller_handle_ = AssetHandle::FromString(handle_str);
          if (scene) {
            scene->RequestAsset(s.controller_handle_);
          }
          // Ensure the controller is loaded
          if (s.controller_handle_.IsValid()) {
            auto ctrl = Engine::asset_manager().Get<SpriteControllerAssetData>(
                s.controller_handle_);
            if (!ctrl) {
              Engine::asset_manager().LoadSync(s.controller_handle_);
            }
          }
        }

        s.playing_ = sj.value("playing", true);
      },
  });
}

}  // namespace Wiesel
