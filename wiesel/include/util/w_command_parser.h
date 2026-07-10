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

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <variant>

#include "w_pch.h"

namespace wiesel {

// Typed parameter schema shared by the developer console and any other
// registry that accepts user-supplied arguments.
enum class ParamType {
  Int = 0,
  Float = 1,
  Bool = 2,
  String = 3,
  Vec2 = 4,
  Vec3 = 5,
  Vec4 = 6,
};

struct Param {
  std::string name;
  ParamType type = ParamType::String;
  bool optional = false;
  // Whitespace-joined default token list (1 token for scalars, N tokens
  // for vecN). The parser uses this when no user value was supplied.
  std::string default_tokens;
};

namespace Params {

Param Int(std::string name);
Param Int(std::string name, int default_value);
Param Float(std::string name);
Param Float(std::string name, float default_value);
Param Bool(std::string name);
Param Bool(std::string name, bool default_value);
Param String(std::string name);
Param String(std::string name, std::string default_value);
Param Vec2(std::string name);
Param Vec2(std::string name, glm::vec2 default_value);
Param Vec3(std::string name);
Param Vec3(std::string name, glm::vec3 default_value);
Param Vec4(std::string name);
Param Vec4(std::string name, glm::vec4 default_value);

// Variadic helper. Writes as one argument:
//   Params::Make(Params::Vec3("pos"), Params::String("name", "Entity"))
template <typename... Ts>
std::vector<Param> Make(Ts&&... ps) {
  return {std::forward<Ts>(ps)...};
}

}  // namespace Params

// Holds values parsed out of a token stream according to a schema.
class CommandContext {
 public:
  using Value = std::variant<int, float, bool, std::string, glm::vec2,
                             glm::vec3, glm::vec4>;

  CommandContext() = default;

  void Set(const std::string& name, Value value) {
    values_[name] = std::move(value);
  }

  bool Has(const std::string& name) const {
    return values_.find(name) != values_.end();
  }

  int Int(const std::string& name, int fallback = 0) const;
  float Float(const std::string& name, float fallback = 0.0f) const;
  bool Bool(const std::string& name, bool fallback = false) const;
  const std::string& String(const std::string& name,
                            const std::string& fallback = kEmptyString) const;
  glm::vec2 Vec2(const std::string& name,
                 glm::vec2 fallback = glm::vec2(0.0f)) const;
  glm::vec3 Vec3(const std::string& name,
                 glm::vec3 fallback = glm::vec3(0.0f)) const;
  glm::vec4 Vec4(const std::string& name,
                 glm::vec4 fallback = glm::vec4(0.0f)) const;

 private:
  static const std::string kEmptyString;
  std::unordered_map<std::string, Value> values_;
};

// Stateless command-line parser. Tokenizes inputs and walks a schema
// to fill in a CommandContext.
class CommandParser {
 public:
  struct ParseResult {
    bool ok = false;
    std::string error;
    CommandContext context;
  };

  // Number of tokens a single param of this type consumes from the
  // stream (1 for scalars / String, N for VecN).
  static int TokensPerParam(ParamType type);

  // Splits a command line into tokens with "..." quoting support.
  static std::vector<std::string> Tokenize(const std::string& command_line);

  // Walks `schema` and consumes tokens starting at `start`. Fills
  // defaults for optional params when tokens run out; returns the first
  // parse error encountered (with `ok = false`) otherwise.
  static ParseResult ParseArgs(const std::vector<Param>& schema,
                               const std::vector<std::string>& tokens,
                               size_t start = 0);

  // For autocomplete / hint UIs: returns the schema slot index that
  // cursor_token (an index into the full token list, *not* a token past
  // the command name) refers to. Returns -1 when cursor is on the
  // command-name slot or past the schema.
  static int SchemaSlotAtTokenIndex(const std::vector<Param>& schema,
                                    size_t command_name_tokens,
                                    size_t cursor_token);
};

}  // namespace wiesel
