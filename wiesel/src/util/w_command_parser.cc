//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "util/w_command_parser.h"

namespace wiesel {

const std::string CommandContext::kEmptyString;

namespace {

bool ParseFloat(const std::string& s, float* out) {
  try {
    size_t pos = 0;
    *out = std::stof(s, &pos);
    return pos == s.size();
  } catch (...) {
    return false;
  }
}

bool ParseInt(const std::string& s, int* out) {
  try {
    size_t pos = 0;
    *out = std::stoi(s, &pos);
    return pos == s.size();
  } catch (...) {
    return false;
  }
}

bool ParseBool(const std::string& s, bool* out) {
  std::string l = s;
  for (auto& c : l) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  if (l == "true" || l == "1" || l == "yes" || l == "on") {
    *out = true;
    return true;
  }
  if (l == "false" || l == "0" || l == "no" || l == "off") {
    *out = false;
    return true;
  }
  return false;
}

std::string ParseInto(CommandContext& ctx, const std::string& name,
                      ParamType type, const std::vector<std::string>& tokens,
                      size_t start, size_t count) {
  if (start + count > tokens.size()) {
    return "missing value for '" + name + "'";
  }
  switch (type) {
    case ParamType::Int: {
      int v = 0;
      if (!ParseInt(tokens[start], &v)) {
        return "invalid int for '" + name + "': '" + tokens[start] + "'";
      }
      ctx.Set(name, v);
      return "";
    }
    case ParamType::Float: {
      float v = 0.0f;
      if (!ParseFloat(tokens[start], &v)) {
        return "invalid float for '" + name + "': '" + tokens[start] + "'";
      }
      ctx.Set(name, v);
      return "";
    }
    case ParamType::Bool: {
      bool v = false;
      if (!ParseBool(tokens[start], &v)) {
        return "invalid bool for '" + name + "': '" + tokens[start] + "'";
      }
      ctx.Set(name, v);
      return "";
    }
    case ParamType::String: {
      ctx.Set(name, tokens[start]);
      return "";
    }
    case ParamType::Vec2:
    case ParamType::Vec3:
    case ParamType::Vec4: {
      float xs[4] = {0, 0, 0, 0};
      for (size_t i = 0; i < count; i++) {
        if (!ParseFloat(tokens[start + i], &xs[i])) {
          return "invalid float in '" + name + "' at index " +
                 std::to_string(i);
        }
      }
      if (type == ParamType::Vec2) {
        ctx.Set(name, glm::vec2(xs[0], xs[1]));
      } else if (type == ParamType::Vec3) {
        ctx.Set(name, glm::vec3(xs[0], xs[1], xs[2]));
      } else {
        ctx.Set(name, glm::vec4(xs[0], xs[1], xs[2], xs[3]));
      }
      return "";
    }
  }
  return "unsupported param type";
}

std::vector<std::string> SplitDefault(const std::string& s) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : s) {
    if (c == ' ') {
      if (!cur.empty()) {
        out.push_back(cur);
        cur.clear();
      }
    } else {
      cur += c;
    }
  }
  if (!cur.empty()) {
    out.push_back(cur);
  }
  return out;
}

}  // namespace

// --- CommandContext getters -----------------------------------------------

int CommandContext::Int(const std::string& name, int fallback) const {
  auto it = values_.find(name);
  if (it == values_.end()) {
    return fallback;
  }
  if (const int* v = std::get_if<int>(&it->second)) {
    return *v;
  }
  return fallback;
}

float CommandContext::Float(const std::string& name, float fallback) const {
  auto it = values_.find(name);
  if (it == values_.end()) {
    return fallback;
  }
  if (const float* v = std::get_if<float>(&it->second)) {
    return *v;
  }
  return fallback;
}

bool CommandContext::Bool(const std::string& name, bool fallback) const {
  auto it = values_.find(name);
  if (it == values_.end()) {
    return fallback;
  }
  if (const bool* v = std::get_if<bool>(&it->second)) {
    return *v;
  }
  return fallback;
}

const std::string& CommandContext::String(const std::string& name,
                                          const std::string& fallback) const {
  auto it = values_.find(name);
  if (it == values_.end()) {
    return fallback;
  }
  if (const std::string* v = std::get_if<std::string>(&it->second)) {
    return *v;
  }
  return fallback;
}

glm::vec2 CommandContext::Vec2(const std::string& name,
                               glm::vec2 fallback) const {
  auto it = values_.find(name);
  if (it == values_.end()) {
    return fallback;
  }
  if (const glm::vec2* v = std::get_if<glm::vec2>(&it->second)) {
    return *v;
  }
  return fallback;
}

glm::vec3 CommandContext::Vec3(const std::string& name,
                               glm::vec3 fallback) const {
  auto it = values_.find(name);
  if (it == values_.end()) {
    return fallback;
  }
  if (const glm::vec3* v = std::get_if<glm::vec3>(&it->second)) {
    return *v;
  }
  return fallback;
}

glm::vec4 CommandContext::Vec4(const std::string& name,
                               glm::vec4 fallback) const {
  auto it = values_.find(name);
  if (it == values_.end()) {
    return fallback;
  }
  if (const glm::vec4* v = std::get_if<glm::vec4>(&it->second)) {
    return *v;
  }
  return fallback;
}

// --- Params factories -----------------------------------------------------

namespace Params {

static Param MakeParam(std::string name, ParamType type) {
  Param p;
  p.name = std::move(name);
  p.type = type;
  p.optional = false;
  return p;
}

static Param MakeParamDefault(std::string name, ParamType type,
                              std::string default_tokens) {
  Param p = MakeParam(std::move(name), type);
  p.optional = true;
  p.default_tokens = std::move(default_tokens);
  return p;
}

Param Int(std::string name) {
  return MakeParam(std::move(name), ParamType::Int);
}
Param Int(std::string name, int default_value) {
  return MakeParamDefault(std::move(name), ParamType::Int,
                          std::to_string(default_value));
}
Param Float(std::string name) {
  return MakeParam(std::move(name), ParamType::Float);
}
Param Float(std::string name, float default_value) {
  return MakeParamDefault(std::move(name), ParamType::Float,
                          std::to_string(default_value));
}
Param Bool(std::string name) {
  return MakeParam(std::move(name), ParamType::Bool);
}
Param Bool(std::string name, bool default_value) {
  return MakeParamDefault(std::move(name), ParamType::Bool,
                          default_value ? "true" : "false");
}
Param String(std::string name) {
  return MakeParam(std::move(name), ParamType::String);
}
Param String(std::string name, std::string default_value) {
  return MakeParamDefault(std::move(name), ParamType::String,
                          std::move(default_value));
}
Param Vec2(std::string name) {
  return MakeParam(std::move(name), ParamType::Vec2);
}
Param Vec2(std::string name, glm::vec2 default_value) {
  return MakeParamDefault(
      std::move(name), ParamType::Vec2,
      std::to_string(default_value.x) + " " + std::to_string(default_value.y));
}
Param Vec3(std::string name) {
  return MakeParam(std::move(name), ParamType::Vec3);
}
Param Vec3(std::string name, glm::vec3 default_value) {
  return MakeParamDefault(
      std::move(name), ParamType::Vec3,
      std::to_string(default_value.x) + " " +
          std::to_string(default_value.y) + " " +
          std::to_string(default_value.z));
}
Param Vec4(std::string name) {
  return MakeParam(std::move(name), ParamType::Vec4);
}
Param Vec4(std::string name, glm::vec4 default_value) {
  return MakeParamDefault(
      std::move(name), ParamType::Vec4,
      std::to_string(default_value.x) + " " +
          std::to_string(default_value.y) + " " +
          std::to_string(default_value.z) + " " +
          std::to_string(default_value.w));
}

}  // namespace Params

// --- CommandParser --------------------------------------------------------

int CommandParser::TokensPerParam(ParamType t) {
  switch (t) {
    case ParamType::Vec2:
      return 2;
    case ParamType::Vec3:
      return 3;
    case ParamType::Vec4:
      return 4;
    default:
      return 1;
  }
}

std::vector<std::string> CommandParser::Tokenize(
    const std::string& command_line) {
  std::vector<std::string> tokens;
  std::string current;
  bool in_quotes = false;
  for (size_t i = 0; i < command_line.size(); i++) {
    char c = command_line[i];
    if (c == '"') {
      in_quotes = !in_quotes;
    } else if (c == ' ' && !in_quotes) {
      if (!current.empty()) {
        tokens.push_back(current);
        current.clear();
      }
    } else {
      current += c;
    }
  }
  if (!current.empty()) {
    tokens.push_back(current);
  }
  return tokens;
}

CommandParser::ParseResult CommandParser::ParseArgs(
    const std::vector<Param>& schema,
    const std::vector<std::string>& tokens, size_t start) {
  ParseResult result;
  size_t cursor = start;
  for (const Param& p : schema) {
    const int needed = TokensPerParam(p.type);
    if (cursor + static_cast<size_t>(needed) <= tokens.size()) {
      std::string err = ParseInto(result.context, p.name, p.type, tokens,
                                  cursor, static_cast<size_t>(needed));
      if (!err.empty()) {
        result.error = std::move(err);
        return result;
      }
      cursor += needed;
    } else if (p.optional && !p.default_tokens.empty()) {
      auto def_tokens = SplitDefault(p.default_tokens);
      std::string err = ParseInto(result.context, p.name, p.type, def_tokens,
                                  0, def_tokens.size());
      if (!err.empty()) {
        result.error = "bad default for '" + p.name + "'";
        return result;
      }
    } else if (!p.optional) {
      result.error = "missing required parameter '" + p.name + "'";
      return result;
    }
  }
  result.ok = true;
  return result;
}

int CommandParser::SchemaSlotAtTokenIndex(
    const std::vector<Param>& schema, size_t command_name_tokens,
    size_t cursor_token) {
  if (cursor_token < command_name_tokens) {
    return -1;  // still on command name.
  }
  size_t remaining = cursor_token - command_name_tokens;
  for (size_t i = 0; i < schema.size(); i++) {
    const int needed = TokensPerParam(schema[i].type);
    if (remaining < static_cast<size_t>(needed)) {
      return static_cast<int>(i);
    }
    remaining -= needed;
  }
  return -1;
}

}  // namespace wiesel
