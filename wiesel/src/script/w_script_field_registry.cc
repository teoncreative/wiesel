
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "script/w_script_field_registry.h"

namespace wiesel {

std::unordered_map<std::string, ScriptFieldRenderFn>&
ScriptFieldTypeRegistry::Registry() {
  static std::unordered_map<std::string, ScriptFieldRenderFn> registry;
  return registry;
}

void ScriptFieldTypeRegistry::Register(const std::string& mono_type_name,
                                       ScriptFieldRenderFn render_fn) {
  Registry()[mono_type_name] = std::move(render_fn);
}

ScriptFieldRenderFn* ScriptFieldTypeRegistry::Find(
    const std::string& mono_type_name) {
  auto& reg = Registry();
  auto it = reg.find(mono_type_name);
  if (it != reg.end()) {
    return &it->second;
  }
  return nullptr;
}

}  // namespace wiesel
