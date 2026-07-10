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

#include <entt/meta/factory.hpp>
#include <entt/meta/resolve.hpp>

#include "core/w_reflect.h"

#include <string>
#include <vector>

namespace wiesel {

// Try to get the WPropertyMeta from a reflected data member.
// Returns nullptr if the field has no WPropertyMeta custom data.
inline const WPropertyMeta* GetPropertyMeta(entt::meta_data data) {
  auto custom = data.custom();
  // entt 3.16: meta_custom has implicit operator Type*() conversion
  const WPropertyMeta* ptr = custom;
  return ptr;
}

// Check if a reflected field is serializable.
inline bool IsSerializable(entt::meta_data data) {
  const WPropertyMeta* meta = GetPropertyMeta(data);
  return meta && meta->serializable;
}

// Check if a reflected field is animatable.
inline bool IsAnimatable(entt::meta_data data) {
  const WPropertyMeta* meta = GetPropertyMeta(data);
  return meta && meta->animatable;
}

// Check if a reflected field is read-only.
inline bool IsReadOnly(entt::meta_data data) {
  const WPropertyMeta* meta = GetPropertyMeta(data);
  return meta && meta->read_only;
}

// Get the display name of a reflected field.
inline std::string GetDisplayName(entt::meta_data data) {
  const WPropertyMeta* meta = GetPropertyMeta(data);
  return meta ? meta->display_name : "";
}

// Get the original C++ field name.
inline std::string GetFieldName(entt::meta_data data) {
  const WPropertyMeta* meta = GetPropertyMeta(data);
  return meta ? meta->field_name : "";
}

// Get the C++ type name string.
inline std::string GetTypeName(entt::meta_data data) {
  const WPropertyMeta* meta = GetPropertyMeta(data);
  return meta ? meta->type_name : "";
}

// Get all serializable fields of a reflected type.
inline std::vector<entt::meta_data> GetSerializableFields(
    entt::meta_type type) {
  std::vector<entt::meta_data> result;
  for (auto [id, data] : type.data()) {
    if (IsSerializable(data)) {
      result.push_back(data);
    }
  }
  return result;
}

// Get all animatable fields of a reflected type.
inline std::vector<entt::meta_data> GetAnimatableFields(entt::meta_type type) {
  std::vector<entt::meta_data> result;
  for (auto [id, data] : type.data()) {
    if (IsAnimatable(data)) {
      result.push_back(data);
    }
  }
  return result;
}

// --- Type lookup ---

// Resolve a reflected type by its short name (e.g. "TagComponent").
inline entt::meta_type ResolveType(const std::string& name) {
  using namespace entt::literals;
  return entt::resolve(entt::hashed_string{name.c_str()});
}

// --- Get/set field values ---

// Get a field value from a component instance by field display name.
// Returns an empty meta_any on failure.
template <typename Component>
entt::meta_any GetFieldValue(Component& component, const std::string& field) {
  using namespace entt::literals;
  entt::meta_type type = entt::resolve<Component>();
  if (!type) {
    return {};
  }
  entt::meta_data data = type.data(entt::hashed_string{field.c_str()});
  if (!data) {
    return {};
  }
  return data.get(entt::forward_as_meta(component));
}

// Set a field value on a component instance by field display name.
// Returns true on success.
template <typename Component, typename Value>
bool SetFieldValue(Component& component, const std::string& field,
                   Value&& value) {
  using namespace entt::literals;
  entt::meta_type type = entt::resolve<Component>();
  if (!type) {
    return false;
  }
  entt::meta_data data = type.data(entt::hashed_string{field.c_str()});
  if (!data) {
    return false;
  }
  return data.set(entt::forward_as_meta(component), std::forward<Value>(value));
}

// Get a field value using a type-erased component reference.
inline entt::meta_any GetFieldValue(entt::meta_any& component,
                                    entt::meta_data data) {
  return data.get(component);
}

// Set a field value using a type-erased component reference.
inline bool SetFieldValue(entt::meta_any& component, entt::meta_data data,
                          entt::meta_any value) {
  return data.set(component, std::move(value));
}

// --- Iterate all reflected fields ---

// Call a visitor for each reflected field of a type.
// Visitor signature: void(entt::meta_data, const WPropertyMeta&)
template <typename Visitor>
void ForEachField(entt::meta_type type, Visitor&& visitor) {
  for (auto [id, data] : type.data()) {
    const WPropertyMeta* meta = GetPropertyMeta(data);
    if (meta) {
      visitor(data, *meta);
    }
  }
}

}  // namespace wiesel
