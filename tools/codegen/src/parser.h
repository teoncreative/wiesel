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
#include <vector>

namespace wiesel::code_gen {

enum class PropertyAttribute : uint32_t {
  None = 0,
  Serializable = 1 << 0,
  Animatable = 1 << 1,
  ReadOnly = 1 << 2,
};

inline PropertyAttribute operator|(PropertyAttribute a, PropertyAttribute b) {
  return static_cast<PropertyAttribute>(static_cast<uint32_t>(a) |
                                        static_cast<uint32_t>(b));
}

inline bool HasAttribute(PropertyAttribute flags, PropertyAttribute flag) {
  return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(flag)) != 0;
}

struct ReflectedField {
  std::string type_name;   // e.g. "glm::vec3"
  std::string field_name;  // e.g. "position_"
  PropertyAttribute attributes = PropertyAttribute::None;
};

struct ReflectedClass {
  std::string qualified_name;  // e.g. "Wiesel::TransformComponent"
  std::string short_name;      // e.g. "TransformComponent"
  std::string namespace_name;  // e.g. "Wiesel"
  std::vector<ReflectedField> fields;
};

struct ParseResult {
  std::vector<ReflectedClass> classes;
  std::string source_file;
};

// Parse a header file to find WCLASS/WPROPERTY text markers.
ParseResult ParseHeader(const std::string& header_path);

}  // namespace wiesel::code_gen
