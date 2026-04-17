
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "scene/w_scene.h"
#include "scene/w_scene_handle.h"
#include "w_pch.h"

namespace wiesel {

// Shared hash computation for Entity and EntityRef.
// Both types produce the same hash for the same (entity, scene) pair.
inline size_t HashEntityScenePair(entt::entity entity, SceneHandle scene) {
  size_t h1 = std::hash<uint32_t>{}(scene.id);
  size_t h2 = std::hash<uint32_t>{}(static_cast<uint32_t>(entity));
  return h1 ^ (h2 << 1);
}

struct EntityRef;

// Short-lived wrapper for an entity + its scene.
// Use for per-frame component access. Do NOT store across frames.
// For storage, use EntityRef instead.
class Entity {
 public:
  constexpr Entity() : entity_handle_(entt::null) { }
  Entity(entt::entity handle, Scene* scene);
  ~Entity() = default;

  template <typename T, typename... Args>
  T& AddComponent(Args&&... args) {
    return scene_->AddComponent<T>(entity_handle_, std::forward<Args>(args)...);
  }

  template <typename T>
  T& GetComponent() {
    return scene_->GetComponent<T>(entity_handle_);
  }

  template <typename T>
  bool HasComponent() const {
    return scene_->HasComponent<T>(entity_handle_);
  }

  template <typename T>
  void RemoveComponent() {
    scene_->RemoveComponent<T>(entity_handle_);
  }

  operator bool() const { return entity_handle_ != entt::null; }

  operator entt::entity() const { return entity_handle_; }

  operator uint32_t() const { return (uint32_t)entity_handle_; }

  UUID GetUUID() { return GetComponent<IdComponent>().Id; }

  const std::string& GetName() { return GetComponent<TagComponent>().name; }

  Entity GetParent() const { return {parent_handle_, scene_}; }

  entt::entity parent_handle() const { return parent_handle_; }

  const std::vector<entt::entity>* child_handles() const {
    return child_handles_;
  }

  bool operator==(const Entity& other) const {
    return entity_handle_ == other.entity_handle_ &&
           scene_ == other.scene_;
  }

  bool operator==(const EntityRef& other) const;

  bool operator!=(const Entity& other) const { return !(*this == other); }

  entt::entity handle() const { return entity_handle_; }

  Scene* GetScene() const { return scene_; }

  SceneHandle GetSceneHandle() const { return scene_handle_; }

  // Convert to an EntityRef for safe storage across frames
  EntityRef ToRef() const;

  void RemoveFromScene();

 private:
  entt::entity entity_handle_{};
  entt::entity parent_handle_{};
  SceneHandle scene_handle_{};
  Scene* scene_{};
  std::vector<entt::entity>* child_handles_{nullptr};
};

// Lightweight, storable reference to an entity in a specific scene.
// Safe to store in maps, undo commands, network packets, etc.
struct EntityRef {
  entt::entity entity = entt::null;
  SceneHandle scene_handle;

  EntityRef() = default;

  EntityRef(entt::entity entity, SceneHandle scene)
      : entity(entity), scene_handle(scene) {}

  explicit EntityRef(const Entity& e)
      : entity(e.handle()), scene_handle(e.GetSceneHandle()) {}

  operator bool() const { return entity != entt::null && scene_handle; }
  operator entt::entity() const { return entity; }

  bool operator==(const EntityRef& other) const {
    return entity == other.entity && scene_handle == other.scene_handle;
  }

  bool operator==(const Entity& other) const {
    return entity == other.handle() && scene_handle == other.GetSceneHandle();
  }

  bool operator!=(const EntityRef& other) const { return !(*this == other); }

  // Resolve to Entity. Returns null Entity if scene was destroyed.
  WIESEL_GETTER_FN Entity Resolve() const {
    Scene* scene = ResolveScene();
    if (!scene) {
      return {entt::null, nullptr};
    }
    return {entity, scene};
  }

  WIESEL_GETTER_FN Scene* ResolveScene() const {
    return scene_handle.Resolve();
  }

  WIESEL_GETTER_FN bool Same(entt::entity other_entity, const Scene& other_scene) const {
    return scene_handle == other_scene.GetHandle() && entity == other_entity;
  }

};

constexpr EntityRef kInvalidEntityRef{};
constexpr Entity kInvalidEntity{};

// Entity member implementations that depend on EntityRef
inline bool Entity::operator==(const EntityRef& other) const {
  return entity_handle_ == other.entity && scene_handle_ == other.scene_handle;
}

inline EntityRef Entity::ToRef() const {
  return {entity_handle_, scene_handle_};
}

inline void Entity::RemoveFromScene() {
  scene_ = nullptr;
}

}  // namespace wiesel

namespace std {
template <>
struct hash<wiesel::Entity> {
  size_t operator()(const wiesel::Entity& e) const noexcept {
    return wiesel::HashEntityScenePair(e.handle(), e.GetSceneHandle());
  }
};

template <>
struct hash<wiesel::EntityRef> {
  size_t operator()(const wiesel::EntityRef& r) const noexcept {
    return wiesel::HashEntityScenePair(r.entity, r.scene_handle);
  }
};

template <>
struct formatter<wiesel::EntityRef> {
  constexpr auto parse(format_parse_context& ctx) {
    return ctx.begin();
  }

  auto format(const wiesel::EntityRef& ref, format_context& ctx) const {
    if (!ref) {
      return format_to(ctx.out(), "EntityRef(null)");
    }
    return format_to(ctx.out(), "EntityRef(entity={}, scene={})",
        static_cast<uint32_t>(ref.entity),
        ref.scene_handle.id);
  }
};

template <>
struct formatter<wiesel::Entity> {
  constexpr auto parse(format_parse_context& ctx) {
    return ctx.begin();
  }

  auto format(const wiesel::Entity& entity, format_context& ctx) const {
    if (!entity) {
      return format_to(ctx.out(), "Entity(null)");
    }
    return format_to(ctx.out(), "Entity(entity={}, scene={} {})",
        static_cast<uint32_t>(entity.handle()),
        entity.GetScene()->GetName(),
        entity.GetSceneHandle().id);
  }
};

}  // namespace std
