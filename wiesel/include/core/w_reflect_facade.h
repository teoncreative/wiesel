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

#include <cstdint>
#include <functional>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <string>
#include <string_view>

#include <entt/entity/fwd.hpp>

#include "asset/w_asset_handle.h"
#include "core/w_reflect.h"

// Public reflection facade. Consumer code must go through this API and must
// never call entt::meta, entt::resolve, or meta_any directly. entt is the
// backing library today but that is an implementation detail.

namespace wiesel::reflect {

enum class AttrFlag : uint32_t {
  None = 0,
  Serializable = 1 << 0,
  Animatable = 1 << 1,
  ReadOnly = 1 << 2,
};

inline AttrFlag operator|(AttrFlag a, AttrFlag b) {
  return static_cast<AttrFlag>(static_cast<uint32_t>(a) |
                               static_cast<uint32_t>(b));
}

inline AttrFlag operator&(AttrFlag a, AttrFlag b) {
  return static_cast<AttrFlag>(static_cast<uint32_t>(a) &
                               static_cast<uint32_t>(b));
}

inline bool HasAttr(AttrFlag set, AttrFlag q) {
  return static_cast<uint32_t>(set & q) != 0;
}

struct AttrSet {
  bool serializable = false;
  bool animatable = false;
  bool read_only = false;

  bool Matches(AttrFlag required) const {
    if (HasAttr(required, AttrFlag::Serializable) && !serializable) {
      return false;
    }
    if (HasAttr(required, AttrFlag::Animatable) && !animatable) {
      return false;
    }
    if (HasAttr(required, AttrFlag::ReadOnly) && !read_only) {
      return false;
    }
    return true;
  }
};

// Opaque handle to a reflected type. Cheap to copy. Invalid handles compare
// false; test with `if (handle)`.
class TypeHandle {
 public:
  TypeHandle() = default;

  explicit operator bool() const { return entry_ != nullptr; }

  std::string_view Name() const;
  uint32_t Id() const;  // entt type_info hash; opaque to callers.

  const void* raw() const { return entry_; }
  static TypeHandle FromRaw(const void* e) {
    TypeHandle t;
    t.entry_ = e;
    return t;
  }

 private:
  const void* entry_ = nullptr;
};

// Opaque handle to a reflected field within a type.
class FieldHandle {
 public:
  FieldHandle() = default;

  explicit operator bool() const { return entry_ != nullptr; }

  std::string_view Name() const;
  std::string_view TypeName() const;
  uint32_t TypeId() const;
  const AttrSet& Attrs() const;

  const void* raw() const { return entry_; }
  static FieldHandle FromRaw(const void* e) {
    FieldHandle f;
    f.entry_ = e;
    return f;
  }

 private:
  const void* entry_ = nullptr;
};

// Look up a type by its registered short name (e.g. "TagComponent").
TypeHandle FindType(std::string_view name);

// Look up a field on a type by name.
FieldHandle FindField(TypeHandle type, std::string_view field);

// Visit every registered type in registration order.
void ForEachType(const std::function<void(TypeHandle)>& visitor);

// Visit each field whose attributes include all of `required`.
void ForEachField(TypeHandle type, AttrFlag required,
                  const std::function<void(FieldHandle)>& visitor);

// Get a raw pointer to an entity's component for a reflected type, or
// nullptr if the component is missing.
void* GetComponentRaw(entt::registry& registry, entt::entity entity,
                      TypeHandle type);

// Typed setters. Used by animation/runtime callers that know the target
// type statically. Return false if the field type does not match.
bool SetFloat(FieldHandle field, void* owner, float value);
bool SetInt(FieldHandle field, void* owner, int value);
bool SetBool(FieldHandle field, void* owner, bool value);
bool SetVec2(FieldHandle field, void* owner, const glm::vec2& value);
bool SetVec3(FieldHandle field, void* owner, const glm::vec3& value);
bool SetVec4(FieldHandle field, void* owner, const glm::vec4& value);
bool SetQuat(FieldHandle field, void* owner, const glm::quat& value);
bool SetAssetHandle(FieldHandle field, void* owner, const AssetHandle& value);

// Type-erased JSON round-trip. Returns false if no converter is registered
// for the field's type. Uses the TypeConverter registry.
bool FieldToJson(FieldHandle field, const void* owner, nlohmann::json& out);
bool FieldFromJson(FieldHandle field, void* owner, const nlohmann::json& in);

// Codegen emits a call to this after the entt::meta_factory registration.
// Do not call from hand-written code.
template <typename T>
void RegisterType(const char* name);

// Called once during engine init (after InitializeReflection) to install
// built-in converters for scalars, glm types, and AssetHandle.
void InitializeBuiltinConverters();

}  // namespace wiesel::reflect

// Template definition — intentionally in the public header because codegen
// generates one call per reflected class. The template body is thin and
// forwards to a non-template helper defined in w_reflect_facade.cc.
#include <entt/core/type_info.hpp>

namespace wiesel::reflect::detail {

void RegisterTypeImpl(uint32_t type_id, const char* name);

}  // namespace wiesel::reflect::detail

namespace wiesel::reflect {

template <typename T>
void RegisterType(const char* name) {
  detail::RegisterTypeImpl(entt::type_id<T>().hash(), name);
}

// Runtime hash of a C++ type. Useful for editor/UI code that needs to
// dispatch on a FieldHandle's TypeId() without touching entt directly.
template <typename T>
inline uint32_t TypeHashOf() {
  return entt::type_id<T>().hash();
}

}  // namespace wiesel::reflect
