//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "parser.h"

#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>

namespace wiesel::code_gen {

// Trim leading/trailing whitespace.
static std::string Trim(const std::string& s) {
  size_t start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) {
    return "";
  }
  size_t end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
}

static PropertyAttribute ParseAttributes(const std::string& attr_str) {
  PropertyAttribute attrs = PropertyAttribute::None;
  if (attr_str.find("Serializable") != std::string::npos) {
    attrs = attrs | PropertyAttribute::Serializable;
  }
  if (attr_str.find("Animatable") != std::string::npos) {
    attrs = attrs | PropertyAttribute::Animatable;
  }
  if (attr_str.find("ReadOnly") != std::string::npos) {
    attrs = attrs | PropertyAttribute::ReadOnly;
  }
  return attrs;
}

// Extract type and field name from a C++ field declaration line.
// Handles lines like:
//   glm::vec3 position_ = {0, 0, 0};
//   float speed = 1.0f;
//   bool flip_x_ = false;
//   AssetHandle sprite_handle_;
//   std::shared_ptr<Foo> bar_;
//   std::vector<std::string> tags;
static bool ParseFieldDecl(const std::string& line, std::string& type_name,
                           std::string& field_name) {
  std::string trimmed = Trim(line);

  // Remove trailing ; and everything after = (default value)
  size_t eq_pos = trimmed.find('=');
  size_t semi_pos = trimmed.find(';');
  size_t end_pos = std::min(eq_pos, semi_pos);
  if (end_pos != std::string::npos) {
    trimmed = Trim(trimmed.substr(0, end_pos));
  }

  if (trimmed.empty()) {
    return false;
  }

  // The field name is the last token. Everything before it is the type.
  // Find the last space that isn't inside angle brackets.
  int angle_depth = 0;
  size_t last_space = std::string::npos;
  for (size_t i = 0; i < trimmed.size(); ++i) {
    if (trimmed[i] == '<') {
      ++angle_depth;
    } else if (trimmed[i] == '>') {
      --angle_depth;
    } else if (trimmed[i] == ' ' && angle_depth == 0) {
      last_space = i;
    }
  }

  if (last_space == std::string::npos) {
    return false;
  }

  type_name = Trim(trimmed.substr(0, last_space));
  field_name = Trim(trimmed.substr(last_space + 1));

  // Strip & or * from field name if type has trailing ref/ptr
  while (!field_name.empty() &&
         (field_name[0] == '&' || field_name[0] == '*')) {
    type_name += field_name[0];
    field_name = field_name.substr(1);
  }

  return !type_name.empty() && !field_name.empty();
}

// Extract the namespace from lines preceding a struct declaration.
// Looks for "namespace Foo {" or "namespace Foo::Bar {" patterns.
static std::string DetectNamespace(const std::vector<std::string>& lines,
                                   size_t struct_line) {
  // Walk backwards to find namespace declarations.
  // Track brace depth to handle nested namespaces.
  std::vector<std::string> ns_parts;
  int brace_depth = 0;

  for (int i = static_cast<int>(struct_line) - 1; i >= 0; --i) {
    std::string trimmed = Trim(lines[i]);

    // Count braces on this line
    for (char c : trimmed) {
      if (c == '}') {
        ++brace_depth;
      } else if (c == '{') {
        --brace_depth;
      }
    }

    // Check for namespace declaration
    if (brace_depth < 0) {
      // We crossed an opening brace; check if its line has "namespace"
      std::regex ns_regex(R"(namespace\s+([\w:]+))");
      std::smatch match;
      if (std::regex_search(trimmed, match, ns_regex)) {
        ns_parts.insert(ns_parts.begin(), match[1].str());
      }
      brace_depth = 0;  // reset for next scope
    }
  }

  std::string result;
  for (const auto& part : ns_parts) {
    if (!result.empty()) {
      result += "::";
    }
    result += part;
  }
  return result;
}

ParseResult ParseHeader(const std::string& header_path) {
  ParseResult result;
  result.source_file = header_path;

  std::ifstream file(header_path);
  if (!file.is_open()) {
    std::cerr << "error: cannot open " << header_path << "\n";
    return result;
  }

  // Read all lines
  std::vector<std::string> lines;
  std::string line;
  while (std::getline(file, line)) {
    lines.push_back(line);
  }
  file.close();

  // Regex patterns
  std::regex wclass_regex(R"(^\s*WCLASS\s*\()");
  std::regex struct_regex(R"(^\s*(struct|class)\s+(\w+))");
  std::regex wproperty_regex(R"(^\s*WPROPERTY\s*\(([^)]*)\))");

  ReflectedClass* current_class = nullptr;
  bool expect_struct = false;  // true after seeing WCLASS()
  bool expect_field = false;   // true after seeing WPROPERTY()
  PropertyAttribute pending_attrs = PropertyAttribute::None;

  for (size_t i = 0; i < lines.size(); ++i) {
    const std::string& ln = lines[i];
    std::string trimmed = Trim(ln);

    // Skip empty lines and comments
    if (trimmed.empty() || trimmed.rfind("//", 0) == 0) {
      continue;
    }

    // Check for WCLASS()
    if (std::regex_search(ln, wclass_regex)) {
      expect_struct = true;
      continue;
    }

    // After WCLASS(), expect a struct/class declaration
    if (expect_struct) {
      std::smatch match;
      if (std::regex_search(ln, match, struct_regex)) {
        std::string class_name = match[2].str();
        std::string ns = DetectNamespace(lines, i);

        result.classes.emplace_back();
        current_class = &result.classes.back();
        current_class->short_name = class_name;
        current_class->namespace_name = ns;
        if (ns.empty()) {
          current_class->qualified_name = class_name;
        } else {
          current_class->qualified_name = ns + "::" + class_name;
        }
      }
      expect_struct = false;
      continue;
    }

    // Inside a reflected class, look for WPROPERTY() and fields
    if (current_class) {
      // Check for end of class (closing brace at top level)
      if (trimmed == "};") {
        current_class = nullptr;
        continue;
      }

      // Check for WPROPERTY()
      std::smatch prop_match;
      if (std::regex_search(ln, prop_match, wproperty_regex)) {
        pending_attrs = ParseAttributes(prop_match[1].str());
        expect_field = true;
        continue;
      }

      // After WPROPERTY(), the next non-empty non-comment line is the field
      if (expect_field) {
        std::string type_name, field_name;
        if (ParseFieldDecl(trimmed, type_name, field_name)) {
          ReflectedField field;
          field.type_name = type_name;
          field.field_name = field_name;
          field.attributes = pending_attrs;
          current_class->fields.push_back(std::move(field));
        } else {
          std::cerr << "warning: " << header_path << ":" << (i + 1)
                    << ": could not parse field after WPROPERTY: " << trimmed
                    << "\n";
        }
        expect_field = false;
      }
    }
  }

  // Remove classes with no reflected fields
  std::erase_if(result.classes,
                [](const ReflectedClass& c) { return c.fields.empty(); });

  return result;
}

}  // namespace wiesel::code_gen
