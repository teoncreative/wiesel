//
// Created by Metehan Gezer on 05.03.2026.
//

#include "scene/w_prefab.hpp"

#include "scene/w_scene_serializer.hpp"
#include "asset/w_asset_manager.hpp"
#include "physics/w_collider.hpp"
#include "physics/w_rigidbody.hpp"
#include "rendering/w_mesh.hpp"
#include "scene/w_lights.hpp"
#include "ui/w_canvas.hpp"
#include "util/w_logger.hpp"
#include "w_engine.hpp"

namespace Wiesel {

// Re-use the same serialization helpers from SceneSerializer
static nlohmann::json SerializeVec3(const glm::vec3& v) {
  return {v.x, v.y, v.z};
}

static nlohmann::json SerializeVec2(const glm::vec2& v) {
  return {v.x, v.y};
}

static glm::vec3 DeserializeVec3(const nlohmann::json& v,
                                 glm::vec3 fallback = {0, 0, 0}) {
  if (!v.is_array() || v.size() < 3) return fallback;
  return {v[0].get<float>(), v[1].get<float>(), v[2].get<float>()};
}

static glm::vec2 DeserializeVec2(const nlohmann::json& v,
                                 glm::vec2 fallback = {0, 0}) {
  if (!v.is_array() || v.size() < 2) return fallback;
  return {v[0].get<float>(), v[1].get<float>()};
}

static nlohmann::json SerializeSingleEntity(Entity entity) {
  nlohmann::json j;

  j["name"] = entity.GetName();

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

  // Children
  if (entity.child_handles() && !entity.child_handles()->empty()) {
    nlohmann::json children = nlohmann::json::array();
    for (auto child_id : *entity.child_handles()) {
      Entity child{child_id, entity.GetScene()};
      children.push_back(SerializeSingleEntity(child));
    }
    j["children"] = children;
  }

  return j;
}

static Entity DeserializeSingleEntity(Ref<Scene> scene,
                                      const nlohmann::json& j,
                                      entt::entity parent = entt::null) {
  std::string name = j.value("name", "Entity");
  Entity entity = scene->CreateEntity(name);

  if (parent != entt::null) {
    scene->LinkEntities(parent, entity);
  }

  // Transform
  if (j.contains("Transform")) {
    auto& t = entity.GetComponent<TransformComponent>();
    const auto& tj = j["Transform"];
    t.position =
        DeserializeVec3(tj.value("position", nlohmann::json::array()));
    t.rotation =
        DeserializeVec3(tj.value("rotation", nlohmann::json::array()));
    t.scale = DeserializeVec3(tj.value("scale", nlohmann::json::array()),
                              {1, 1, 1});
    t.pivot = DeserializeVec3(tj.value("pivot", nlohmann::json::array()));
    t.is_changed = true;
  }

  // Camera
  if (j.contains("Camera")) {
    auto& c = entity.AddComponent<CameraComponent>();
    const auto& cj = j["Camera"];
    c.field_of_view = cj.value("field_of_view", 60.0f);
    c.near_plane = cj.value("near_plane", 0.01f);
    c.far_plane = cj.value("far_plane", 1000.0f);
    c.viewport_size = DeserializeVec2(
        cj.value("viewport_size", nlohmann::json::array()), {1920, 1080});
    c.enabled = cj.value("enabled", true);
    Engine::renderer()->SetupCameraComponent(c);
  }

  // Model
  if (j.contains("Model")) {
    auto& m = entity.AddComponent<ModelComponent>();
    const auto& mj = j["Model"];
    std::string handle_str = mj.value("asset_handle", "");
    std::string asset_name = mj.value("asset_name", "");
    std::string asset_path = mj.value("asset_path", "");
    if (!handle_str.empty()) {
      AssetHandle handle = AssetHandle::FromString(handle_str);
      if (!Engine::asset_manager().HasAsset(handle) && !asset_path.empty()) {
        Engine::asset_manager().Register(handle, asset_name, AssetType::Model,
                                     asset_path);
      }
      m.model_handle = handle;
    }
    m.receive_shadows = mj.value("receive_shadows", true);
    m.enable_rendering = mj.value("enable_rendering", true);
  }

  // Directional Light
  if (j.contains("LightDirect")) {
    auto& l = entity.AddComponent<LightDirectComponent>();
    const auto& lj = j["LightDirect"];
    l.light_data.base.color =
        DeserializeVec3(lj.value("color", nlohmann::json::array()), {1, 1, 1});
    l.light_data.base.ambient = lj.value("ambient", 0.2f);
    l.light_data.base.diffuse = lj.value("diffuse", 1.0f);
    l.light_data.base.specular = lj.value("specular", 0.85f);
    l.light_data.base.density = lj.value("density", 1.0f);
  }

  // Point Light
  if (j.contains("LightPoint")) {
    auto& l = entity.AddComponent<LightPointComponent>();
    const auto& lj = j["LightPoint"];
    l.light_data.base.color =
        DeserializeVec3(lj.value("color", nlohmann::json::array()), {1, 1, 1});
    l.light_data.base.ambient = lj.value("ambient", 0.2f);
    l.light_data.base.diffuse = lj.value("diffuse", 1.0f);
    l.light_data.base.specular = lj.value("specular", 0.85f);
    l.light_data.base.density = lj.value("density", 1.0f);
    l.light_data.constant = lj.value("constant", 1.0f);
    l.light_data.linear = lj.value("linear", 0.09f);
    l.light_data.exp = lj.value("exp", 0.032f);
  }

  // RigidBody
  if (j.contains("RigidBody")) {
    auto& rb = entity.AddComponent<RigidBodyComponent>();
    const auto& rbj = j["RigidBody"];
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
  if (j.contains("BoxCollider")) {
    auto& bc = entity.AddComponent<BoxColliderComponent>();
    const auto& bcj = j["BoxCollider"];
    bc.offset =
        DeserializeVec3(bcj.value("offset", nlohmann::json::array()));
    bc.half_extents = DeserializeVec3(
        bcj.value("half_extents", nlohmann::json::array()),
        {0.5f, 0.5f, 0.5f});
    bc.is_trigger = bcj.value("is_trigger", false);
    bc.collision_group = bcj.value("collision_group", 1);
  }

  // Sphere Collider
  if (j.contains("SphereCollider")) {
    auto& sc = entity.AddComponent<SphereColliderComponent>();
    const auto& scj = j["SphereCollider"];
    sc.offset =
        DeserializeVec3(scj.value("offset", nlohmann::json::array()));
    sc.radius = scj.value("radius", 0.5f);
    sc.is_trigger = scj.value("is_trigger", false);
    sc.collision_group = scj.value("collision_group", 1);
  }

  // RectangleTransform
  if (j.contains("RectangleTransform")) {
    auto& rt = entity.HasComponent<RectangleTransformComponent>()
        ? entity.GetComponent<RectangleTransformComponent>()
        : entity.AddComponent<RectangleTransformComponent>();
    const auto& rtj = j["RectangleTransform"];
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
  if (j.contains("Canvas")) {
    auto& c = entity.AddComponent<CanvasComponent>();
    const auto& cj = j["Canvas"];
    c.type = static_cast<CanvasType>(cj.value("type", 0));
    c.direction = static_cast<LayoutDirection>(cj.value("direction", 0));
    c.alignment = static_cast<ChildAlignment>(cj.value("alignment", 0));
    c.spacing = cj.value("spacing", 0.0f);
    c.sort_order = cj.value("sort_order", 0);
  }

  // CanvasRect
  if (j.contains("CanvasRect")) {
    auto& cr = entity.AddComponent<CanvasRectComponent>();
    const auto& crj = j["CanvasRect"];
    if (crj.contains("color") && crj["color"].is_array() && crj["color"].size() >= 4) {
      cr.color = {crj["color"][0], crj["color"][1], crj["color"][2], crj["color"][3]};
    }
  }

  // CanvasImage
  if (j.contains("CanvasImage")) {
    auto& ci = entity.AddComponent<CanvasImageComponent>();
    const auto& cij = j["CanvasImage"];
    if (cij.contains("tint") && cij["tint"].is_array() && cij["tint"].size() >= 4) {
      ci.tint = {cij["tint"][0], cij["tint"][1], cij["tint"][2], cij["tint"][3]};
    }
    if (cij.contains("uv_rect") && cij["uv_rect"].is_array() && cij["uv_rect"].size() >= 4) {
      ci.uv_rect = {cij["uv_rect"][0], cij["uv_rect"][1], cij["uv_rect"][2], cij["uv_rect"][3]};
    }
  }

  // Text
  if (j.contains("Text")) {
    auto& t = entity.AddComponent<TextComponent>();
    const auto& tj = j["Text"];
    t.text = tj.value("text", "");
    t.font_path = tj.value("font_path", "/engine/fonts/default.ttf");
    t.font_size = tj.value("font_size", 16.0f);
    if (tj.contains("color") && tj["color"].is_array() && tj["color"].size() >= 4) {
      t.color = {tj["color"][0], tj["color"][1], tj["color"][2], tj["color"][3]};
    }
  }

  // Recurse into children
  if (j.contains("children") && j["children"].is_array()) {
    for (const auto& child_json : j["children"]) {
      DeserializeSingleEntity(scene, child_json, entity.handle());
    }
  }

  return entity;
}

// --- Public API ---

nlohmann::json Prefab::SerializeEntityTree(Entity entity) {
  nlohmann::json root;
  root["prefab_version"] = 1;
  root["root"] = SerializeSingleEntity(entity);
  return root;
}

bool Prefab::SaveToFile(Entity entity, const std::filesystem::path& path) {
  nlohmann::json j = SerializeEntityTree(entity);

  std::filesystem::create_directories(path.parent_path());

  std::ofstream file(path);
  if (!file.is_open()) {
    LOG_ERROR("Failed to save prefab to: {}", path.string());
    return false;
  }

  file << j.dump(2);
  LOG_INFO("Prefab saved to: {}", path.string());
  return true;
}

Entity Prefab::DeserializeEntityTree(Ref<Scene> scene,
                                     const nlohmann::json& json) {
  if (!json.contains("root")) {
    LOG_ERROR("Prefab JSON missing 'root'");
    return Entity{entt::null, scene.get()};
  }

  return DeserializeSingleEntity(scene, json["root"]);
}

Entity Prefab::InstantiateFromFile(Ref<Scene> scene,
                                   const std::filesystem::path& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    LOG_ERROR("Failed to open prefab file: {}", path.string());
    return Entity{entt::null, scene.get()};
  }

  nlohmann::json j;
  try {
    file >> j;
  } catch (const nlohmann::json::parse_error& e) {
    LOG_ERROR("Failed to parse prefab: {}", e.what());
    return Entity{entt::null, scene.get()};
  }

  return DeserializeEntityTree(scene, j);
}

}  // namespace Wiesel