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
// JSON keys are stable API - do not rename without a version migration:
//   Transform, Camera, LightDirect, LightPoint, RigidBody, BoxCollider,
//   SphereCollider, CapsuleCollider, MeshCollider, UIDocument, Behaviors,
//   RectangleTransform, Canvas, CanvasScaler,
//   AudioSource, ReverbZone, Interactable,
//   Navigable, SpriteRenderer, MeshRenderer, SkinnedMeshRenderer, Animator
//

#include "scene/w_component_serializer.h"

#include "asset/w_asset_manager.h"
#include "audio/w_audio.h"
#include "behavior/w_behavior.h"
#include "behavior/w_native_behavior.h"
#include "mono_wrappers.h"
#include "networking/w_replication_types.h"
#include "physics/w_collider.h"
#include "physics/w_rigidbody.h"
#include "rendering/w_mesh.h"
#include "rendering/w_mesh_renderer.h"
#include "rendering/w_sprite.h"
#include "rendering/w_sprite_asset.h"
#include "scene/w_lights.h"
#include "script/mono/w_monobehavior.h"
#include "ui/w_canvas.h"
#include "ui/w_interactable.h"
#include "ui/w_navigable.h"
#include "ui/w_ui_document.h"
#include "util/w_logger.h"
#include "w_engine.h"

namespace wiesel {

using json = nlohmann::json;

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

void InitializeComponentSerializers() {
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
        collider["is_one_way"] = bc.is_one_way;
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
        bc.is_one_way = bcj.value("is_one_way", false);
        bc.collision_group = bcj.value("collision_group", 1);
      },
  });

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
        collider["is_one_way"] = sc.is_one_way;
        collider["collision_group"] = sc.collision_group;
        return collider;
      },
      // Deserialize
      [](Entity& entity, const json& scj, Scene* /*scene*/) {
        auto& sc = entity.AddComponent<SphereColliderComponent>();
        sc.offset = SerializeUtil::Vec3(scj.value("offset", json::array()));
        sc.radius = scj.value("radius", 0.5f);
        sc.is_trigger = scj.value("is_trigger", false);
        sc.is_one_way = scj.value("is_one_way", false);
        sc.collision_group = scj.value("collision_group", 1);
      },
  });

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
        collider["is_one_way"] = cc.is_one_way;
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
        cc.is_one_way = ccj.value("is_one_way", false);
        cc.collision_group = ccj.value("collision_group", 1);
      },
  });

  ComponentSerializerRegistry::Register({
      "MeshCollider",
      // Has
      [](Entity& entity) -> bool {
        return entity.HasComponent<MeshColliderComponent>();
      },
      // Serialize
      [](Entity& entity) -> json {
        auto& mc = entity.GetComponent<MeshColliderComponent>();
        json collider;
        collider["offset"] = SerializeUtil::Vec3(mc.offset);
        collider["is_trigger"] = mc.is_trigger;
        collider["is_one_way"] = mc.is_one_way;
        collider["collision_group"] = mc.collision_group;
        if (mc.collider_handle.IsValid()) {
          collider["collider_handle"] = mc.collider_handle.ToString();
        }
        return collider;
      },
      // Deserialize
      [](Entity& entity, const json& mcj, Scene* /*scene*/) {
        auto& mc = entity.AddComponent<MeshColliderComponent>();
        mc.offset = SerializeUtil::Vec3(mcj.value("offset", json::array()));
        mc.is_trigger = mcj.value("is_trigger", false);
        mc.is_one_way = mcj.value("is_one_way", false);
        mc.collision_group = mcj.value("collision_group", 1);
        if (mcj.contains("collider_handle")) {
          mc.collider_handle = AssetHandle::FromString(
              mcj["collider_handle"].get<std::string>());
        }
      },
  });

  ComponentSerializerRegistry::Register({
      "UIDocument",
      // Has
      [](Entity& entity) -> bool {
        return entity.HasComponent<UIDocumentComponent>();
      },
      // Serialize
      [](Entity& entity) -> json {
        auto& doc = entity.GetComponent<UIDocumentComponent>();
        json j;
        j["document"] = doc.document_handle.ToString();
        j["visible"] = doc.visible;
        return j;
      },
      // Deserialize
      [](Entity& entity, const json& dj, Scene* /*scene*/) {
        auto& doc = entity.AddComponent<UIDocumentComponent>();
        if (dj.contains("document") && dj["document"].is_string()) {
          doc.document_handle =
              AssetHandle::FromString(dj["document"].get<std::string>());
        }
        doc.visible = dj.value("visible", true);
      },
  });

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
                  MonoClassField* handle_field = mono_class_get_field_from_name(
                      Engine::script_manager().prefab_class(), "handle");
                  if (handle_field) {
                    MonoString* handle_str = nullptr;
                    mono_field_get_value(prefab_obj, handle_field, &handle_str);
                    if (handle_str) {
                      const char* cstr = mono_string_to_utf8(handle_str);
                      if (cstr && cstr[0]) {
                        json pj;
                        pj["type"] = "Prefab";
                        pj["handle"] = cstr;
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

          if (script_entry.is_object()) {
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
                // Support both "handle" (new) and "path" (legacy) keys
                std::string handle_str = val.value("handle", val.value("path", ""));
                if (!handle_str.empty()) {
                  MonoObject* prefab =
                      mono_object_new(Engine::script_manager().app_domain(),
                                      Engine::script_manager().prefab_class());
                  mono_runtime_object_init(prefab);
                  MonoClassField* handle_field = mono_class_get_field_from_name(
                      Engine::script_manager().prefab_class(), "handle");
                  if (handle_field) {
                    MonoString* mono_val = mono_string_new(
                        Engine::script_manager().app_domain(), handle_str.c_str());
                    mono_field_set_value(prefab, handle_field, mono_val);
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
        c.render_mode =
            static_cast<CanvasRenderMode>(cj.value("render_mode", 0));
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

  // MeshRendererComponent
  ComponentSerializerRegistry::Register({
      "MeshRenderer",
      [](Entity& entity) -> bool {
        return entity.HasComponent<MeshRendererComponent>();
      },
      [](Entity& entity) -> json {
        auto& mr = entity.GetComponent<MeshRendererComponent>();
        json j;
        if (mr.model_handle.IsValid()) {
          j["model_handle"] = mr.model_handle.ToString();
        }
        j["mesh_index"] = mr.mesh_index;
        j["enable_rendering"] = mr.enable_rendering;
        j["receive_shadows"] = mr.receive_shadows;
        if (mr.material_handle.IsValid()) {
          j["material_handle"] = mr.material_handle.ToString();
        }
        return j;
      },
      [](Entity& entity, const json& j, Scene* scene) {
        auto& mr = entity.AddComponent<MeshRendererComponent>();
        std::string model_str = j.value("model_handle", "");
        if (!model_str.empty()) {
          mr.model_handle = AssetHandle::FromString(model_str);
          if (scene) {
            scene->RequestAsset(mr.model_handle);
          }
        }
        mr.mesh_index = j.value("mesh_index", -1);
        mr.enable_rendering = j.value("enable_rendering", true);
        mr.receive_shadows = j.value("receive_shadows", true);
        std::string mat_str = j.value("material_handle", "");
        if (!mat_str.empty()) {
          mr.material_handle = AssetHandle::FromString(mat_str);
        }
      },
  });

  // SkinnedMeshRendererComponent
  ComponentSerializerRegistry::Register({
      "SkinnedMeshRenderer",
      [](Entity& entity) -> bool {
        return entity.HasComponent<SkinnedMeshRendererComponent>();
      },
      [](Entity& entity) -> json {
        auto& mr = entity.GetComponent<SkinnedMeshRendererComponent>();
        json j;
        if (mr.model_handle.IsValid()) {
          j["model_handle"] = mr.model_handle.ToString();
        }
        j["mesh_index"] = mr.mesh_index;
        j["enable_rendering"] = mr.enable_rendering;
        j["receive_shadows"] = mr.receive_shadows;
        if (mr.material_handle.IsValid()) {
          j["material_handle"] = mr.material_handle.ToString();
        }
        // Save skeleton root's UUID for resolution on load
        if (mr.skeleton_root != entt::null) {
          Scene* scene = entity.GetScene();
          if (scene && scene->HasEntity(mr.skeleton_root) &&
              scene->HasComponent<IdComponent>(mr.skeleton_root)) {
            j["skeleton_root_uuid"] =
                scene->GetComponent<IdComponent>(mr.skeleton_root)
                    .Id.ToString();
          }
        }
        return j;
      },
      [](Entity& entity, const json& j, Scene* scene) {
        auto& mr = entity.AddComponent<SkinnedMeshRendererComponent>();
        std::string model_str = j.value("model_handle", "");
        if (!model_str.empty()) {
          mr.model_handle = AssetHandle::FromString(model_str);
          if (scene) {
            scene->RequestAsset(mr.model_handle);
          }
        }
        mr.mesh_index = j.value("mesh_index", -1);
        mr.enable_rendering = j.value("enable_rendering", true);
        mr.receive_shadows = j.value("receive_shadows", true);
        std::string mat_str = j.value("material_handle", "");
        if (!mat_str.empty()) {
          mr.material_handle = AssetHandle::FromString(mat_str);
        }
        // Resolve skeleton root from saved UUID
        std::string root_uuid_str = j.value("skeleton_root_uuid", "");
        if (!root_uuid_str.empty() && scene) {
          UUID root_uuid = UUID::FromString(root_uuid_str);
          entt::entity root_entity = scene->FindEntityByUUID(root_uuid);
          if (root_entity != entt::null) {
            mr.skeleton_root = root_entity;
          } else {
            LOG_WARN("SkinnedMeshRenderer: could not resolve skeleton_root {}",
                     root_uuid_str);
          }
        }
      },
  });

  // Unified Animator serializer
  ComponentSerializerRegistry::Register({
      "Animator",
      // Has
      [](Entity& entity) -> bool {
        return entity.HasComponent<AnimatorComponent>();
      },
      // Serialize
      [](Entity& entity) -> json {
        auto& a = entity.GetComponent<AnimatorComponent>();
        json aj;
        if (a.controller_handle.IsValid()) {
          aj["controller_handle"] = a.controller_handle.ToString();
        }
        aj["playing"] = a.playing;
        aj["playback_speed"] = a.playback_speed;
        return aj;
      },
      // Deserialize
      [](Entity& entity, const json& aj, Scene* scene) {
        auto& a = entity.AddComponent<AnimatorComponent>();
        std::string handle_str = aj.value("controller_handle", "");
        if (!handle_str.empty()) {
          a.controller_handle = AssetHandle::FromString(handle_str);
          if (scene) {
            scene->RequestAsset(a.controller_handle);
          }
          if (a.controller_handle.IsValid()) {
            Engine::asset_manager().LoadSync(a.controller_handle);
          }
        }
        a.playing = aj.value("playing", true);
        a.playback_speed = aj.value("playback_speed", 1.0f);
      },
  });

  ComponentSerializerRegistry::Register({
      "NetworkIdentity",
      // Has
      [](Entity& entity) -> bool {
        return entity.HasComponent<NetworkIdentityComponent>();
      },
      // Serialize
      [](Entity& entity) -> json {
        auto& n = entity.GetComponent<NetworkIdentityComponent>();
        json nj;
        nj["authority"] = static_cast<int>(n.authority);
        return nj;
      },
      // Deserialize
      [](Entity& entity, const json& nj, Scene* /*scene*/) {
        auto& n = entity.AddComponent<NetworkIdentityComponent>();
        n.authority = static_cast<NetworkAuthority>(
            nj.value("authority", 1));
      },
  });
}

}  // namespace wiesel
