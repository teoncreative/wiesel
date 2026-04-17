
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

#include <nlohmann/json.hpp>

#include "w_pch.h"

typedef struct _MonoObject MonoObject;
typedef struct _MonoClassField MonoClassField;
typedef struct _MonoProperty MonoProperty;

namespace wiesel {

class Scene;

// Serialize: read value from Mono field, return as JSON
using ScriptFieldSerializeFn = std::function<nlohmann::json(
    MonoObject* object, MonoClassField* field)>;

// Deserialize: read JSON value, write to Mono field
using ScriptFieldDeserializeFn = std::function<void(
    MonoObject* object, MonoClassField* field, const nlohmann::json& value,
    Scene* scene)>;

// Render: ImGui widget that reads/edits the field value.
// write_property: if non-null, write via property setter (e.g., NetworkVariable.Value)
// Returns true if the value was modified.
using ScriptFieldRenderFn = std::function<bool(
    MonoObject* object, MonoClassField* field, MonoProperty* write_property,
    const std::string& label)>;

// Serialize a standalone value (e.g. an RPC argument or a boxed value).
// For value types: `value` is a boxed MonoObject*.
// For reference types: `value` is the MonoObject* directly.
using ScriptValueSerializeFn = std::function<nlohmann::json(MonoObject* value)>;

// Deserialize a standalone value from JSON.
// Returns a MonoObject* (boxed for value types, reference for reference types).
using ScriptValueDeserializeFn =
    std::function<MonoObject*(const nlohmann::json& value, Scene* scene)>;

struct ScriptFieldTypeDesc {
  ScriptFieldSerializeFn Serialize;
  ScriptFieldDeserializeFn Deserialize;
  ScriptFieldRenderFn Render;  // may be null (engine-only types don't render)
  ScriptValueSerializeFn SerializeValue;      // for RPC args, raw values
  ScriptValueDeserializeFn DeserializeValue;  // for RPC args, raw values
};

class ScriptFieldTypeRegistry {
 public:
  static void Register(const std::string& mono_type_name,
                       ScriptFieldTypeDesc desc);

  static ScriptFieldTypeDesc* Find(const std::string& mono_type_name);

  // Convenience: serialize a field if its type is registered
  static bool SerializeField(const std::string& type_name,
                             MonoObject* object, MonoClassField* field,
                             nlohmann::json& out);

  // Convenience: deserialize a field if its type is registered
  static bool DeserializeField(const std::string& type_name,
                               MonoObject* object, MonoClassField* field,
                               const nlohmann::json& value, Scene* scene);

  // Convenience: serialize a standalone value (e.g. RPC arg)
  static bool SerializeValue(const std::string& type_name, MonoObject* value,
                             nlohmann::json& out);

  // Convenience: deserialize a standalone value (e.g. RPC arg)
  static MonoObject* DeserializeValue(const std::string& type_name,
                                      const nlohmann::json& value,
                                      Scene* scene);

 private:
  static std::unordered_map<std::string, ScriptFieldTypeDesc>& Registry();
};

// Register built-in types (int, float, bool, string).
// Called from engine init. Editor calls its own init for Render functions.
void InitializeScriptFieldTypes();

}  // namespace wiesel
