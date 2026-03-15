//
// Created by Claude on 05.03.2026.
//

#include "scene/w_scene_serializer.hpp"

#include "asset/w_asset_manager.hpp"
#include "behavior/w_behavior.hpp"
#include "mono_util.h"
#include "physics/w_collider.hpp"
#include "physics/w_rigidbody.hpp"
#include "rendering/w_mesh.hpp"
#include "scene/w_lights.hpp"
#include "script/mono/w_monobehavior.hpp"
#include "ui/w_canvas.hpp"
#include "util/w_logger.hpp"

namespace Wiesel {

SceneSerializer::SceneSerializer(Ref<Scene> scene) : scene_(std::move(scene)) {}

// --- Vector helpers ---

nlohmann::json SceneSerializer::SerializeVec2(const glm::vec2& v) {
  return {v.x, v.y};
}

nlohmann::json SceneSerializer::SerializeVec3(const glm::vec3& v) {
  return {v.x, v.y, v.z};
}

nlohmann::json SceneSerializer::SerializeVec4(const glm::vec4& v) {
  return {v.x, v.y, v.z, v.w};
}

glm::vec2 SceneSerializer::DeserializeVec2(const nlohmann::json& v,
                                           glm::vec2 fallback) {
  if (!v.is_array() || v.size() < 2) return fallback;
  return {v[0].get<float>(), v[1].get<float>()};
}

glm::vec3 SceneSerializer::DeserializeVec3(const nlohmann::json& v,
                                           glm::vec3 fallback) {
  if (!v.is_array() || v.size() < 3) return fallback;
  return {v[0].get<float>(), v[1].get<float>(), v[2].get<float>()};
}

glm::vec4 SceneSerializer::DeserializeVec4(const nlohmann::json& v,
                                           glm::vec4 fallback) {
  if (!v.is_array() || v.size() < 4) return fallback;
  return {v[0].get<float>(), v[1].get<float>(), v[2].get<float>(),
          v[3].get<float>()};
}

// --- Entity serialization ---

nlohmann::json SceneSerializer::SerializeEntity(Entity entity) const {
  nlohmann::json j;

  j["uuid"] = entity.GetUUID().ToString();
  j["name"] = entity.GetName();

  // Parent reference
  auto parent = entity.GetParent();
  if (parent) {
    j["parent"] = parent.GetUUID().ToString();
  }

  // Transform
  if (entity.HasComponent<TransformComponent>()) {
    auto& t = entity.GetComponent<TransformComponent>();
    nlohmann::json transform;
    transform["position"] = SerializeVec3(t.position);
    transform["rotation"] = SerializeVec3(t.rotation);
    transform["scale"] = SerializeVec3(t.scale);
    transform["pivot"] = SerializeVec3(t.pivot);
    j["Transform"] = transform;
  }

  // Camera
  if (entity.HasComponent<CameraComponent>()) {
    auto& c = entity.GetComponent<CameraComponent>();
    nlohmann::json cam;
    cam["field_of_view"] = c.field_of_view;
    cam["near_plane"] = c.near_plane;
    cam["far_plane"] = c.far_plane;
    cam["viewport_size"] = SerializeVec2(c.viewport_size);
    cam["enabled"] = c.enabled;
    j["Camera"] = cam;
  }

  // Model
  if (entity.HasComponent<ModelComponent>()) {
    auto& m = entity.GetComponent<ModelComponent>();
    nlohmann::json model;
    if (m.model_handle.IsValid()) {
      model["asset_handle"] = m.model_handle.ToString();
      const auto* meta = Engine::asset_manager().GetMetadata(m.model_handle);
      if (meta) {
        model["asset_name"] = meta->name;
        model["asset_path"] = meta->virtual_source_path;
      }
    }
    model["receive_shadows"] = m.receive_shadows;
    model["enable_rendering"] = m.enable_rendering;

    // Serialize per-slot material handles and overrides
    if (!m.material_slot_handles.empty()) {
      nlohmann::json slots = nlohmann::json::array();
      for (size_t i = 0; i < m.material_slot_handles.size(); i++) {
        nlohmann::json slot;
        if (m.material_slot_handles[i].IsValid()) {
          slot["material_handle"] = m.material_slot_handles[i].ToString();
        }
        // Save material instance overrides
        if (i < m.material_instances.size() && m.material_instances[i] &&
            !m.material_instances[i]->overrides.empty()) {
          nlohmann::json overrides;
          for (const auto& [key, val] : m.material_instances[i]->overrides) {
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

    j["Model"] = model;
  }

  // Directional Light
  if (entity.HasComponent<LightDirectComponent>()) {
    auto& l = entity.GetComponent<LightDirectComponent>();
    nlohmann::json light;
    light["color"] = SerializeVec3(l.light_data.base.color);
    light["ambient"] = l.light_data.base.ambient;
    light["diffuse"] = l.light_data.base.diffuse;
    light["specular"] = l.light_data.base.specular;
    light["density"] = l.light_data.base.density;
    j["LightDirect"] = light;
  }

  // Point Light
  if (entity.HasComponent<LightPointComponent>()) {
    auto& l = entity.GetComponent<LightPointComponent>();
    nlohmann::json light;
    light["color"] = SerializeVec3(l.light_data.base.color);
    light["ambient"] = l.light_data.base.ambient;
    light["diffuse"] = l.light_data.base.diffuse;
    light["specular"] = l.light_data.base.specular;
    light["density"] = l.light_data.base.density;
    light["constant"] = l.light_data.constant;
    light["linear"] = l.light_data.linear;
    light["exp"] = l.light_data.exp;
    j["LightPoint"] = light;
  }

  // RigidBody
  if (entity.HasComponent<RigidBodyComponent>()) {
    auto& rb = entity.GetComponent<RigidBodyComponent>();
    nlohmann::json body;
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
    j["RigidBody"] = body;
  }

  // Box Collider
  if (entity.HasComponent<BoxColliderComponent>()) {
    auto& bc = entity.GetComponent<BoxColliderComponent>();
    nlohmann::json collider;
    collider["offset"] = SerializeVec3(bc.offset);
    collider["half_extents"] = SerializeVec3(bc.half_extents);
    collider["is_trigger"] = bc.is_trigger;
    collider["collision_group"] = bc.collision_group;
    j["BoxCollider"] = collider;
  }

  // Sphere Collider
  if (entity.HasComponent<SphereColliderComponent>()) {
    auto& sc = entity.GetComponent<SphereColliderComponent>();
    nlohmann::json collider;
    collider["offset"] = SerializeVec3(sc.offset);
    collider["radius"] = sc.radius;
    collider["is_trigger"] = sc.is_trigger;
    collider["collision_group"] = sc.collision_group;
    j["SphereCollider"] = collider;
  }

  // Behaviors (C# scripts) with field values
  if (entity.HasComponent<BehaviorsComponent>()) {
    auto& bc = entity.GetComponent<BehaviorsComponent>();
    nlohmann::json scripts = nlohmann::json::array();
    for (auto& [name, behavior] : bc.behaviors_) {
      nlohmann::json script_json;
      script_json["name"] = name;

      // Serialize field values from the ScriptInstance
      auto* mono_behavior = dynamic_cast<MonoBehavior*>(behavior);
      if (mono_behavior && mono_behavior->script_instance()) {
        auto* instance = mono_behavior->script_instance();
        nlohmann::json fields_json;
        for (auto& [field_name, field_data] : instance->script_data().fields()) {
          if (field_data.field_type() == FieldType::Float) {
            fields_json[field_name] = field_data.Get<float>(instance->handle());
          } else if (field_data.field_type() == FieldType::Integer) {
            fields_json[field_name] = field_data.Get<int32_t>(instance->handle());
          } else if (field_data.field_type() == FieldType::Boolean) {
            fields_json[field_name] = field_data.Get<bool>(instance->handle());
          } else if (field_data.field_type() == FieldType::String) {
            MonoObject* val = field_data.Get<MonoObject*>(instance->handle());
            if (val) {
              MonoObjectWrapper wrapper{val};
              fields_json[field_name] = wrapper.AsString();
            }
          } else if (field_data.field_type() == FieldType::Prefab) {
            MonoObject* prefab_obj = field_data.Get<MonoObject*>(instance->handle());
            if (prefab_obj) {
              MonoClassField* path_field = mono_class_get_field_from_name(
                  Engine::script_manager().prefab_class(), "path");
              if (path_field) {
                MonoString* path_str = nullptr;
                mono_field_get_value(prefab_obj, path_field, &path_str);
                if (path_str) {
                  const char* cstr = mono_string_to_utf8(path_str);
                  if (cstr && cstr[0]) {
                    nlohmann::json pj;
                    pj["type"] = "Prefab";
                    pj["path"] = cstr;
                    fields_json[field_name] = pj;
                  }
                  mono_free((void*)cstr);
                }
              }
            }
          } else if (field_data.field_type() == FieldType::Entity) {
            MonoObject* entity_obj = field_data.Get<MonoObject*>(instance->handle());
            if (entity_obj) {
              MonoClassField* id_field = mono_class_get_field_from_name(
                  Engine::script_manager().entity_class(), "entityId");
              if (id_field) {
                uint64_t id_val = 0;
                mono_field_get_value(entity_obj, id_field, &id_val);
                entt::entity ent = static_cast<entt::entity>(id_val);
                if (scene_->HasEntity(ent) && scene_->HasComponent<IdComponent>(ent)) {
                  nlohmann::json ej;
                  ej["type"] = "Entity";
                  ej["uuid"] = scene_->GetComponent<IdComponent>(ent).Id.ToString();
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
    if (!scripts.empty()) {
      j["Behaviors"] = scripts;
    }
  }

  // RectangleTransform
  if (entity.HasComponent<RectangleTransformComponent>()) {
    auto& rt = entity.GetComponent<RectangleTransformComponent>();
    nlohmann::json rtj;
    rtj["position"] = SerializeVec2(rt.position);
    rtj["rotation"] = rt.rotation;
    rtj["size"] = SerializeVec2(rt.size);
    rtj["scale"] = SerializeVec2(rt.scale);
    rtj["anchor"] = static_cast<int>(rt.anchor);
    rtj["pivot"] = static_cast<int>(rt.pivot);
    rtj["size_mode_x"] = static_cast<int>(rt.size_mode_x);
    rtj["size_mode_y"] = static_cast<int>(rt.size_mode_y);
    rtj["padding"] = {rt.padding.x, rt.padding.y, rt.padding.z, rt.padding.w};
    j["RectangleTransform"] = rtj;
  }

  // Canvas
  if (entity.HasComponent<CanvasComponent>()) {
    auto& c = entity.GetComponent<CanvasComponent>();
    nlohmann::json cj;
    cj["type"] = static_cast<int>(c.type);
    cj["direction"] = static_cast<int>(c.direction);
    cj["alignment"] = static_cast<int>(c.alignment);
    cj["spacing"] = c.spacing;
    cj["sort_order"] = c.sort_order;
    j["Canvas"] = cj;
  }

  // CanvasRect
  if (entity.HasComponent<CanvasRectComponent>()) {
    auto& cr = entity.GetComponent<CanvasRectComponent>();
    nlohmann::json crj;
    crj["color"] = {cr.color.x, cr.color.y, cr.color.z, cr.color.w};
    j["CanvasRect"] = crj;
  }

  // CanvasImage
  if (entity.HasComponent<CanvasImageComponent>()) {
    auto& ci = entity.GetComponent<CanvasImageComponent>();
    nlohmann::json cij;
    cij["tint"] = {ci.tint.x, ci.tint.y, ci.tint.z, ci.tint.w};
    cij["uv_rect"] = {ci.uv_rect.x, ci.uv_rect.y, ci.uv_rect.z, ci.uv_rect.w};
    j["CanvasImage"] = cij;
  }

  // Text
  if (entity.HasComponent<TextComponent>()) {
    auto& t = entity.GetComponent<TextComponent>();
    nlohmann::json tj;
    tj["text"] = t.text;
    tj["font_path"] = t.font_path;
    tj["font_size"] = t.font_size;
    tj["color"] = {t.color.x, t.color.y, t.color.z, t.color.w};
    j["Text"] = tj;
  }

  return j;
}

void SceneSerializer::DeserializeEntity(const nlohmann::json& entity_json) {
  std::string uuid_str = entity_json.value("uuid", "");
  std::string name = entity_json.value("name", "Entity");

  UUID uuid = UUID::FromString(uuid_str);
  Entity entity = scene_->CreateEntityWithUUID(uuid, name);

  // Transform
  if (entity_json.contains("Transform")) {
    auto& t = entity.GetComponent<TransformComponent>();
    const auto& tj = entity_json["Transform"];
    t.position = DeserializeVec3(tj.value("position", nlohmann::json::array()));
    t.rotation = DeserializeVec3(tj.value("rotation", nlohmann::json::array()));
    t.scale = DeserializeVec3(tj.value("scale", nlohmann::json::array()),
                              {1, 1, 1});
    t.pivot = DeserializeVec3(tj.value("pivot", nlohmann::json::array()));
    t.is_changed = true;
  }

  // Camera
  if (entity_json.contains("Camera")) {
    auto& c = entity.AddComponent<CameraComponent>();
    const auto& cj = entity_json["Camera"];
    c.field_of_view = cj.value("field_of_view", 60.0f);
    c.near_plane = cj.value("near_plane", 0.01f);
    c.far_plane = cj.value("far_plane", 1000.0f);
    c.viewport_size =
        DeserializeVec2(cj.value("viewport_size", nlohmann::json::array()),
                        {1920, 1080});
    c.enabled = cj.value("enabled", true);
  }

  // Model
  if (entity_json.contains("Model")) {
    auto& m = entity.AddComponent<ModelComponent>();
    const auto& mj = entity_json["Model"];

    std::string handle_str = mj.value("asset_handle", "");
    std::string asset_name = mj.value("asset_name", "");
    std::string asset_path = mj.value("asset_path", "");

    if (!handle_str.empty()) {
      AssetHandle handle = AssetHandle::FromString(handle_str);
      if (!Engine::asset_manager().HasAsset(handle)) {
        // Handle not found, check if registered under a different handle (from asset registry)
        if (!asset_path.empty()) {
          for (auto& h : Engine::asset_manager().GetAll()) {
            const auto* m2 = Engine::asset_manager().GetMetadata(h);
            if (m2 && m2->virtual_source_path == asset_path && m2->type == AssetType::Model) {
              handle = h;
              break;
            }
          }
        }
        // Still not found, register with the scene file's handle as fallback
        if (!Engine::asset_manager().HasAsset(handle) && !asset_path.empty()) {
          Engine::asset_manager().Register(handle, asset_name, AssetType::Model, asset_path);
        }
      }
      m.model_handle = handle;
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
        }
        // Restore overrides
        if (slot.contains("overrides")) {
          if (!m.material_instances[i]) {
            m.material_instances[i] = CreateReference<MaterialInstance>();
          }
          auto& inst = m.material_instances[i];
          for (auto& [key, val] : slot["overrides"].items()) {
            if (val.is_number()) {
              inst->overrides[key] = val.get<float>();
            } else if (val.is_array() && val.size() >= 4) {
              inst->overrides[key] = glm::vec4(val[0], val[1], val[2], val[3]);
            }
          }
        }
      }
    }
  }

  // Directional Light
  if (entity_json.contains("LightDirect")) {
    auto& l = entity.AddComponent<LightDirectComponent>();
    const auto& lj = entity_json["LightDirect"];
    l.light_data.base.color =
        DeserializeVec3(lj.value("color", nlohmann::json::array()),
                        {1, 1, 1});
    l.light_data.base.ambient = lj.value("ambient", 0.2f);
    l.light_data.base.diffuse = lj.value("diffuse", 1.0f);
    l.light_data.base.specular = lj.value("specular", 0.85f);
    l.light_data.base.density = lj.value("density", 1.0f);
  }

  // Point Light
  if (entity_json.contains("LightPoint")) {
    auto& l = entity.AddComponent<LightPointComponent>();
    const auto& lj = entity_json["LightPoint"];
    l.light_data.base.color =
        DeserializeVec3(lj.value("color", nlohmann::json::array()),
                        {1, 1, 1});
    l.light_data.base.ambient = lj.value("ambient", 0.2f);
    l.light_data.base.diffuse = lj.value("diffuse", 1.0f);
    l.light_data.base.specular = lj.value("specular", 0.85f);
    l.light_data.base.density = lj.value("density", 1.0f);
    l.light_data.constant = lj.value("constant", 1.0f);
    l.light_data.linear = lj.value("linear", 0.09f);
    l.light_data.exp = lj.value("exp", 0.032f);
  }

  // RigidBody
  if (entity_json.contains("RigidBody")) {
    auto& rb = entity.AddComponent<RigidBodyComponent>();
    const auto& rbj = entity_json["RigidBody"];
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
  }

  // Box Collider
  if (entity_json.contains("BoxCollider")) {
    auto& bc = entity.AddComponent<BoxColliderComponent>();
    const auto& bcj = entity_json["BoxCollider"];
    bc.offset = DeserializeVec3(bcj.value("offset", nlohmann::json::array()));
    bc.half_extents =
        DeserializeVec3(bcj.value("half_extents", nlohmann::json::array()),
                        {0.5f, 0.5f, 0.5f});
    bc.is_trigger = bcj.value("is_trigger", false);
    bc.collision_group = bcj.value("collision_group", 1);
  }

  // Sphere Collider
  if (entity_json.contains("SphereCollider")) {
    auto& sc = entity.AddComponent<SphereColliderComponent>();
    const auto& scj = entity_json["SphereCollider"];
    sc.offset = DeserializeVec3(scj.value("offset", nlohmann::json::array()));
    sc.radius = scj.value("radius", 0.5f);
    sc.is_trigger = scj.value("is_trigger", false);
    sc.collision_group = scj.value("collision_group", 1);
  }

  // Behaviors (C# scripts) with field values
  if (entity_json.contains("Behaviors") && entity_json["Behaviors"].is_array()) {
    if (!entity.HasComponent<BehaviorsComponent>()) {
      entity.AddComponent<BehaviorsComponent>();
    }
    auto& bc = entity.GetComponent<BehaviorsComponent>();
    for (const auto& script_entry : entity_json["Behaviors"]) {
      std::string script_name;
      nlohmann::json fields_json;

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

      if (script_name.empty()) continue;
      auto& mono_beh = bc.AddBehavior<MonoBehavior>(entity, script_name);

      // Restore field values
      if (!fields_json.empty() && !mono_beh.script_instance()) {
        LOG_WARN("Script '{}' has no instance - field values cannot be restored", script_name);
      }
      if (!fields_json.empty() && mono_beh.script_instance()) {
        auto* instance = mono_beh.script_instance();
        for (auto& [field_name, field_data] : instance->script_data().fields()) {
          if (!fields_json.contains(field_name)) continue;
          const auto& val = fields_json[field_name];

          if (field_data.field_type() == FieldType::Float && val.is_number()) {
            float v = val.get<float>();
            field_data.Set(instance->handle(), &v);
          } else if (field_data.field_type() == FieldType::Integer && val.is_number_integer()) {
            int32_t v = val.get<int32_t>();
            field_data.Set(instance->handle(), &v);
          } else if (field_data.field_type() == FieldType::Boolean && val.is_boolean()) {
            bool v = val.get<bool>();
            field_data.Set(instance->handle(), &v);
          } else if (field_data.field_type() == FieldType::String && val.is_string()) {
            MonoString* str = mono_string_new(
                Engine::script_manager().app_domain(), val.get<std::string>().c_str());
            field_data.Set(instance->handle(), str);
          } else if (field_data.field_type() == FieldType::Prefab && val.is_object()) {
            std::string path = val.value("path", "");
            if (!path.empty()) {
              MonoObject* prefab = mono_object_new(
                  Engine::script_manager().app_domain(),
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
          } else if (field_data.field_type() == FieldType::Entity && val.is_object()) {
            std::string uuid_str = val.value("uuid", "");
            if (!uuid_str.empty()) {
              UUID uuid = UUID::FromString(uuid_str);
              entt::entity ent = scene_->FindEntityByUUID(uuid);
              if (ent != entt::null) {
                MonoObject* entity_obj = mono_object_new(
                    Engine::script_manager().app_domain(),
                    Engine::script_manager().entity_class());
                MonoMethod* ctor = mono_class_get_method_from_name(
                    Engine::script_manager().entity_class(), ".ctor", 2);
                uint64_t scene_ptr = reinterpret_cast<uint64_t>(scene_.get());
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
  }

  // RectangleTransform
  if (entity_json.contains("RectangleTransform")) {
    auto& rt = entity.HasComponent<RectangleTransformComponent>()
        ? entity.GetComponent<RectangleTransformComponent>()
        : entity.AddComponent<RectangleTransformComponent>();
    const auto& rtj = entity_json["RectangleTransform"];
    rt.position = DeserializeVec2(rtj.value("position", nlohmann::json::array()));
    rt.rotation = rtj.value("rotation", 0.0f);
    rt.size = DeserializeVec2(rtj.value("size", nlohmann::json::array()), {100, 100});
    rt.scale = DeserializeVec2(rtj.value("scale", nlohmann::json::array()), {1, 1});
    rt.anchor = static_cast<AnchorPreset>(rtj.value("anchor", 0));
    rt.pivot = static_cast<AnchorPreset>(rtj.value("pivot", 0));
    rt.size_mode_x = static_cast<SizeMode>(rtj.value("size_mode_x", 0));
    rt.size_mode_y = static_cast<SizeMode>(rtj.value("size_mode_y", 0));
    if (rtj.contains("padding") && rtj["padding"].is_array() && rtj["padding"].size() >= 4) {
      rt.padding = {rtj["padding"][0], rtj["padding"][1], rtj["padding"][2], rtj["padding"][3]};
    }
  }

  // Canvas
  if (entity_json.contains("Canvas")) {
    auto& c = entity.AddComponent<CanvasComponent>();
    const auto& cj = entity_json["Canvas"];
    c.type = static_cast<CanvasType>(cj.value("type", 0));
    c.direction = static_cast<LayoutDirection>(cj.value("direction", 0));
    c.alignment = static_cast<ChildAlignment>(cj.value("alignment", 0));
    c.spacing = cj.value("spacing", 0.0f);
    c.sort_order = cj.value("sort_order", 0);
  }

  // CanvasRect
  if (entity_json.contains("CanvasRect")) {
    auto& cr = entity.AddComponent<CanvasRectComponent>();
    const auto& crj = entity_json["CanvasRect"];
    if (crj.contains("color") && crj["color"].is_array() && crj["color"].size() >= 4) {
      cr.color = {crj["color"][0], crj["color"][1], crj["color"][2], crj["color"][3]};
    }
  }

  // CanvasImage
  if (entity_json.contains("CanvasImage")) {
    auto& ci = entity.AddComponent<CanvasImageComponent>();
    const auto& cij = entity_json["CanvasImage"];
    if (cij.contains("tint") && cij["tint"].is_array() && cij["tint"].size() >= 4) {
      ci.tint = {cij["tint"][0], cij["tint"][1], cij["tint"][2], cij["tint"][3]};
    }
    if (cij.contains("uv_rect") && cij["uv_rect"].is_array() && cij["uv_rect"].size() >= 4) {
      ci.uv_rect = {cij["uv_rect"][0], cij["uv_rect"][1], cij["uv_rect"][2], cij["uv_rect"][3]};
    }
  }

  // Text
  if (entity_json.contains("Text")) {
    auto& t = entity.AddComponent<TextComponent>();
    const auto& tj = entity_json["Text"];
    t.text = tj.value("text", "");
    t.font_path = tj.value("font_path", "/engine/fonts/default.ttf");
    t.font_size = tj.value("font_size", 16.0f);
    if (tj.contains("color") && tj["color"].is_array() && tj["color"].size() >= 4) {
      t.color = {tj["color"][0], tj["color"][1], tj["color"][2], tj["color"][3]};
    }
  }
}

// --- Full scene serialization ---

std::string SceneSerializer::SerializeToString() const {
  nlohmann::json root;

  // Serialize entities in hierarchy order
  nlohmann::json entities = nlohmann::json::array();
  for (auto entity_id : scene_->GetSceneHierarchy()) {
    Entity entity{entity_id, scene_.get()};
    entities.push_back(SerializeEntity(entity));

    // Also serialize children recursively
    if (entity.child_handles()) {
      std::function<void(const std::vector<entt::entity>&)> serialize_children;
      serialize_children = [&](const std::vector<entt::entity>& children) {
        for (auto child_id : children) {
          Entity child{child_id, scene_.get()};
          entities.push_back(SerializeEntity(child));
          if (child.child_handles() && !child.child_handles()->empty()) {
            serialize_children(*child.child_handles());
          }
        }
      };
      if (!entity.child_handles()->empty()) {
        serialize_children(*entity.child_handles());
      }
    }
  }

  root["entities"] = entities;

  return root.dump(2);
}

bool SceneSerializer::Serialize(const std::filesystem::path& path) const {
  std::string json_str = SerializeToString();

  std::ofstream file(path);
  if (!file.is_open()) {
    LOG_ERROR("Failed to open file for writing: {}", path.string());
    return false;
  }

  file << json_str;
  LOG_INFO("Scene saved to: {}", path.string());
  return true;
}

bool SceneSerializer::DeserializeFromString(const std::string& json_str) {
  nlohmann::json root;
  try {
    root = nlohmann::json::parse(json_str);
  } catch (const nlohmann::json::parse_error& e) {
    LOG_ERROR("Failed to parse scene JSON: {}", e.what());
    return false;
  }

  if (!root.contains("entities") || !root["entities"].is_array()) {
    LOG_ERROR("Scene file missing 'entities' array");
    return false;
  }

  // First pass: create all entities
  for (const auto& entity_json : root["entities"]) {
    DeserializeEntity(entity_json);
  }

  // Second pass: restore parent-child relationships
  for (const auto& entity_json : root["entities"]) {
    if (entity_json.contains("parent") && entity_json["parent"].is_string()) {
      std::string parent_uuid_str = entity_json["parent"].get<std::string>();
      std::string child_uuid_str = entity_json["uuid"].get<std::string>();

      UUID parent_uuid = UUID::FromString(parent_uuid_str);
      UUID child_uuid = UUID::FromString(child_uuid_str);

      // Find entities by UUID in registry
      entt::entity parent_entity = entt::null;
      entt::entity child_entity = entt::null;

      auto view = scene_->GetAllEntitiesWith<IdComponent>();
      for (auto e : view) {
        auto& id = scene_->GetComponent<IdComponent>(e);
        if (id.Id == parent_uuid) parent_entity = e;
        if (id.Id == child_uuid) child_entity = e;
      }

      if (parent_entity != entt::null && child_entity != entt::null) {
        scene_->LinkEntities(parent_entity, child_entity);
      }
    }
  }

  LOG_INFO("Scene deserialized: {} entities", root["entities"].size());
  return true;
}

bool SceneSerializer::Deserialize(const std::filesystem::path& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    LOG_ERROR("Failed to open scene file: {}", path.string());
    return false;
  }

  std::string json_str((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

  LOG_INFO("Loading scene from: {}", path.string());
  return DeserializeFromString(json_str);
}

}  // namespace Wiesel