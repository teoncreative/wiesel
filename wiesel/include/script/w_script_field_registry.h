
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

#include "w_pch.h"

typedef struct _MonoObject MonoObject;
typedef struct _MonoClassField MonoClassField;
typedef struct _MonoProperty MonoProperty;

namespace wiesel {

// Callback that reads a value from a Mono field, renders an inspector widget,
// and writes back if changed. Returns true if the value was modified.
//
// object:         the MonoObject that owns the field (behavior or NetworkVariable)
// field:          field to read the current value from
// write_property: if non-null, write changed values via this property (triggers
//                 NetworkVariable.Value setter). If null, write directly to field.
// label:          ImGui label for the widget
using ScriptFieldRenderFn = std::function<bool(
    MonoObject* object, MonoClassField* field, MonoProperty* write_property,
    const std::string& label)>;

class ScriptFieldTypeRegistry {
 public:
  static void Register(const std::string& mono_type_name,
                       ScriptFieldRenderFn render_fn);

  static ScriptFieldRenderFn* Find(const std::string& mono_type_name);

  static std::unordered_map<std::string, ScriptFieldRenderFn>& Registry();
};

}  // namespace wiesel
