//
// Created by Metehan Gezer on 05.03.2026.
//

#include "scene/w_scene_serializer.hpp"

#include "asset/w_asset_manager.hpp"
#include "physics/w_collider.hpp"
#include "physics/w_rigidbody.hpp"
#include "rendering/w_mesh.hpp"
#include "scene/w_lights.hpp"
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
      // Re-register the asset if it doesn't exist yet
      if (!Engine::asset_manager().HasAsset(handle)) {
        if (!asset_path.empty()) {
          Engine::asset_manager().Register(handle, asset_name, AssetType::Model,
                                       asset_path);
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