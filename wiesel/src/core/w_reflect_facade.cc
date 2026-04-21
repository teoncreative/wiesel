//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "core/w_reflect_facade.h"

#include <entt/entity/registry.hpp>
#include <entt/meta/factory.hpp>
#include <entt/meta/meta.hpp>
#include <entt/meta/resolve.hpp>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <vector>

#include "core/w_reflect_facade_internal.h"
#include "core/w_reflect_util.h"
#include "util/w_logger.h"

namespace wiesel::reflect {

namespace detail {

struct FieldEntry {
  std::string name;
  std::string type_name;
  AttrSet attrs;
  uint32_t type_id = 0;        // hash of the field's type
  uint32_t owner_type_id = 0;  // hash of the owning component's type
  entt::meta_data meta_data;
};

struct TypeEntry {
  std::string name;
  uint32_t type_id = 0;
  std::vector<FieldEntry> fields;
  // Field lookup by name.
  std::unordered_map<std::string, const FieldEntry*> field_index;
};

// Global index. TypeEntry pointers are stable because the vector is only
// appended to; we use unique_ptr to guarantee stability even if the vector
// reallocates.
static std::vector<std::unique_ptr<TypeEntry>>& AllTypes() {
  static std::vector<std::unique_ptr<TypeEntry>> v;
  return v;
}

static std::unordered_map<std::string, TypeEntry*>& TypesByName() {
  static std::unordered_map<std::string, TypeEntry*> m;
  return m;
}

static std::unordered_map<uint32_t, TypeEntry*>& TypesById() {
  static std::unordered_map<uint32_t, TypeEntry*> m;
  return m;
}

static const FieldEntry* AsField(const void* raw) {
  return static_cast<const FieldEntry*>(raw);
}

static const TypeEntry* AsType(const void* raw) {
  return static_cast<const TypeEntry*>(raw);
}

// Resolve a field type's hash by looking at the meta_data's type node.
static uint32_t ResolveFieldTypeId(entt::meta_data data) {
  entt::meta_type t = data.type();
  return t ? t.info().hash() : 0;
}

void RegisterTypeImpl(uint32_t type_id, const char* name) {
  entt::meta_type meta = entt::resolve(type_id);
  if (!meta) {
    LOG_WARN("Reflect::RegisterType: no entt meta for '{}' (id {})", name,
             type_id);
    return;
  }

  auto entry = std::make_unique<TypeEntry>();
  entry->name = name;
  entry->type_id = type_id;

  for (auto [id, data] : meta.data()) {
    FieldEntry f;
    const WPropertyMeta* pm = GetPropertyMeta(data);
    if (pm) {
      f.name = pm->display_name;
      f.type_name = pm->type_name;
      f.attrs.serializable = pm->serializable;
      f.attrs.animatable = pm->animatable;
      f.attrs.read_only = pm->read_only;
    }
    f.type_id = ResolveFieldTypeId(data);
    f.owner_type_id = type_id;
    f.meta_data = data;
    entry->fields.push_back(std::move(f));
  }

  TypeEntry* raw = entry.get();
  for (const auto& field : raw->fields) {
    raw->field_index[field.name] = &field;
  }

  TypesByName()[raw->name] = raw;
  TypesById()[raw->type_id] = raw;
  AllTypes().push_back(std::move(entry));
}

// Helper: produce a non-owning meta_any wrapping `owner` as the reflected
// component type, then call data.set(owner_any, value_any). Returns false
// if the owner type isn't known or the field type doesn't match.
template <typename T>
static bool SetTypedField(FieldHandle field, void* owner, const T& value) {
  if (!field) {
    return false;
  }
  const FieldEntry* fe = AsField(field.raw());
  entt::meta_type owner_meta = entt::resolve(fe->owner_type_id);
  if (!owner_meta) {
    return false;
  }
  entt::meta_any owner_any = owner_meta.from_void(owner);
  return fe->meta_data.set(owner_any,
                           entt::meta_any{std::in_place_type<T>, value});
}

}  // namespace detail

// --- TypeHandle / FieldHandle accessors ---

std::string_view TypeHandle::Name() const {
  return entry_ ? std::string_view{detail::AsType(entry_)->name} : "";
}

uint32_t TypeHandle::Id() const {
  return entry_ ? detail::AsType(entry_)->type_id : 0;
}

std::string_view FieldHandle::Name() const {
  return entry_ ? std::string_view{detail::AsField(entry_)->name} : "";
}

std::string_view FieldHandle::TypeName() const {
  return entry_ ? std::string_view{detail::AsField(entry_)->type_name} : "";
}

uint32_t FieldHandle::TypeId() const {
  return entry_ ? detail::AsField(entry_)->type_id : 0;
}

const AttrSet& FieldHandle::Attrs() const {
  static const AttrSet kEmpty{};
  return entry_ ? detail::AsField(entry_)->attrs : kEmpty;
}

// --- Lookup ---

TypeHandle FindType(std::string_view name) {
  auto& map = detail::TypesByName();
  auto it = map.find(std::string{name});
  if (it == map.end()) {
    return {};
  }
  return TypeHandle::FromRaw(it->second);
}

FieldHandle FindField(TypeHandle type, std::string_view field) {
  if (!type) {
    return {};
  }
  const detail::TypeEntry* te = detail::AsType(type.raw());
  auto it = te->field_index.find(std::string{field});
  if (it == te->field_index.end()) {
    return {};
  }
  return FieldHandle::FromRaw(it->second);
}

void ForEachType(const std::function<void(TypeHandle)>& visitor) {
  for (const auto& entry : detail::AllTypes()) {
    visitor(TypeHandle::FromRaw(entry.get()));
  }
}

void ForEachField(TypeHandle type, AttrFlag required,
                  const std::function<void(FieldHandle)>& visitor) {
  if (!type) {
    return;
  }
  const detail::TypeEntry* te = detail::AsType(type.raw());
  for (const auto& f : te->fields) {
    if (f.attrs.Matches(required)) {
      visitor(FieldHandle::FromRaw(&f));
    }
  }
}

void* GetComponentRaw(entt::registry& registry, entt::entity entity,
                      TypeHandle type) {
  if (!type) {
    return nullptr;
  }
  auto* pool = registry.storage(type.Id());
  if (!pool || !pool->contains(entity)) {
    return nullptr;
  }
  return pool->value(entity);
}

// --- Typed setters ---

bool SetFloat(FieldHandle field, void* owner, float value) {
  return detail::SetTypedField(field, owner, value);
}

bool SetInt(FieldHandle field, void* owner, int value) {
  return detail::SetTypedField(field, owner, value);
}

bool SetBool(FieldHandle field, void* owner, bool value) {
  return detail::SetTypedField(field, owner, value);
}

bool SetVec2(FieldHandle field, void* owner, const glm::vec2& value) {
  return detail::SetTypedField(field, owner, value);
}

bool SetVec3(FieldHandle field, void* owner, const glm::vec3& value) {
  return detail::SetTypedField(field, owner, value);
}

bool SetVec4(FieldHandle field, void* owner, const glm::vec4& value) {
  return detail::SetTypedField(field, owner, value);
}

bool SetQuat(FieldHandle field, void* owner, const glm::quat& value) {
  return detail::SetTypedField(field, owner, value);
}

bool SetAssetHandle(FieldHandle field, void* owner, const AssetHandle& value) {
  return detail::SetTypedField(field, owner, value);
}

// --- Internal helpers exposed to converters ---

namespace internal {

entt::meta_any ReadFieldErased(FieldHandle field, const void* owner) {
  if (!field) {
    return {};
  }
  const detail::FieldEntry* fe = detail::AsField(field.raw());
  entt::meta_type owner_meta = entt::resolve(fe->owner_type_id);
  if (!owner_meta) {
    return {};
  }
  entt::meta_any owner_any = owner_meta.from_void(const_cast<void*>(owner));
  return fe->meta_data.get(owner_any);
}

bool WriteFieldErased(FieldHandle field, void* owner, entt::meta_any value) {
  if (!field) {
    return false;
  }
  const detail::FieldEntry* fe = detail::AsField(field.raw());
  entt::meta_type owner_meta = entt::resolve(fe->owner_type_id);
  if (!owner_meta) {
    return false;
  }
  entt::meta_any owner_any = owner_meta.from_void(owner);
  return fe->meta_data.set(owner_any, std::move(value));
}

}  // namespace internal

}  // namespace wiesel::reflect
