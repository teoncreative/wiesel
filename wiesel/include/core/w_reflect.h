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
#include <string>

// Property reflection macros.
//
// During normal compilation these expand to nothing. The code gen tool
// (tools/codegen) scans for them as text markers and generates
// entt::meta registration code.
//
// Usage:
//
//   WCLASS()
//   struct MyComponent : public IComponent {
//       WPROPERTY(Serializable, Animatable)
//       glm::vec3 position = {0, 0, 0};
//
//       WPROPERTY(Serializable)
//       float speed = 1.0f;
//
//       // Not reflected - no macro
//       bool internal_flag = false;
//   };
//
// Supported attributes:
//   Serializable  - included in component serialization
//   Animatable    - targetable by animation property curves
//   ReadOnly      - shown in inspector but not editable

#define WCLASS(...)
#define WPROPERTY(...)

namespace Wiesel {

// Metadata attached to each reflected field via entt::meta .custom<>().
struct WPropertyMeta {
  const char* display_name = "";
  const char* field_name = "";
  const char* type_name = "";
  bool serializable = false;
  bool animatable = false;
  bool read_only = false;
};

}  // namespace Wiesel
