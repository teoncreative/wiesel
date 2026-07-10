//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "core/w_reflect_converters.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <string>
#include <unordered_map>

#include "asset/w_asset_handle.h"
#include "util/w_logger.h"

namespace wiesel::reflect {

namespace detail {

static std::unordered_map<uint32_t, std::unique_ptr<IErasedConverter>>&
ConverterMap() {
  static std::unordered_map<uint32_t, std::unique_ptr<IErasedConverter>> m;
  return m;
}

void RegisterConverterImpl(uint32_t type_id,
                           std::unique_ptr<IErasedConverter> converter) {
  ConverterMap()[type_id] = std::move(converter);
}

}  // namespace detail

const IErasedConverter* ConverterRegistry::Find(uint32_t type_id) {
  auto& m = detail::ConverterMap();
  auto it = m.find(type_id);
  return it == m.end() ? nullptr : it->second.get();
}

// --- Facade JSON entry points ---

bool FieldToJson(FieldHandle field, const void* owner, nlohmann::json& out) {
  if (!field) {
    return false;
  }
  const auto* conv = ConverterRegistry::Find(field.TypeId());
  if (!conv) {
    return false;
  }
  return conv->ToJsonErased(field, owner, out);
}

bool FieldFromJson(FieldHandle field, void* owner, const nlohmann::json& in) {
  if (!field) {
    return false;
  }
  const auto* conv = ConverterRegistry::Find(field.TypeId());
  if (!conv) {
    return false;
  }
  return conv->FromJsonErased(field, owner, in);
}

// --- Built-in converters ---

namespace {

template <typename T>
struct ScalarConverter : TypeConverter<T> {
  bool ToJson(const T& v, nlohmann::json& out) const override {
    out = v;
    return true;
  }
  bool FromJson(const nlohmann::json& in, T& out) const override {
    if (!in.is_number() && !in.is_boolean()) {
      return false;
    }
    out = in.get<T>();
    return true;
  }
};

struct BoolConverter : TypeConverter<bool> {
  bool ToJson(const bool& v, nlohmann::json& out) const override {
    out = v;
    return true;
  }
  bool FromJson(const nlohmann::json& in, bool& out) const override {
    if (!in.is_boolean()) {
      return false;
    }
    out = in.get<bool>();
    return true;
  }
};

struct StringConverter : TypeConverter<std::string> {
  bool ToJson(const std::string& v, nlohmann::json& out) const override {
    out = v;
    return true;
  }
  bool FromJson(const nlohmann::json& in, std::string& out) const override {
    if (!in.is_string()) {
      return false;
    }
    out = in.get<std::string>();
    return true;
  }
};

struct Vec2Converter : TypeConverter<glm::vec2> {
  bool ToJson(const glm::vec2& v, nlohmann::json& out) const override {
    out = {v.x, v.y};
    return true;
  }
  bool FromJson(const nlohmann::json& in, glm::vec2& out) const override {
    if (!in.is_array() || in.size() < 2) {
      return false;
    }
    out = {in[0].get<float>(), in[1].get<float>()};
    return true;
  }
};

struct Vec3Converter : TypeConverter<glm::vec3> {
  bool ToJson(const glm::vec3& v, nlohmann::json& out) const override {
    out = {v.x, v.y, v.z};
    return true;
  }
  bool FromJson(const nlohmann::json& in, glm::vec3& out) const override {
    if (!in.is_array() || in.size() < 3) {
      return false;
    }
    out = {in[0].get<float>(), in[1].get<float>(), in[2].get<float>()};
    return true;
  }
};

struct Vec4Converter : TypeConverter<glm::vec4> {
  bool ToJson(const glm::vec4& v, nlohmann::json& out) const override {
    out = {v.x, v.y, v.z, v.w};
    return true;
  }
  bool FromJson(const nlohmann::json& in, glm::vec4& out) const override {
    if (!in.is_array() || in.size() < 4) {
      return false;
    }
    out = {in[0].get<float>(), in[1].get<float>(), in[2].get<float>(),
           in[3].get<float>()};
    return true;
  }
};

struct QuatConverter : TypeConverter<glm::quat> {
  bool ToJson(const glm::quat& v, nlohmann::json& out) const override {
    out = {v.w, v.x, v.y, v.z};
    return true;
  }
  bool FromJson(const nlohmann::json& in, glm::quat& out) const override {
    if (!in.is_array() || in.size() < 4) {
      return false;
    }
    out = {in[0].get<float>(), in[1].get<float>(), in[2].get<float>(),
           in[3].get<float>()};
    return true;
  }
};

struct AssetHandleConverter : TypeConverter<AssetHandle> {
  bool ToJson(const AssetHandle& v, nlohmann::json& out) const override {
    out = v.IsValid() ? v.ToString() : std::string{};
    return true;
  }
  bool FromJson(const nlohmann::json& in, AssetHandle& out) const override {
    if (!in.is_string()) {
      return false;
    }
    std::string s = in.get<std::string>();
    out = s.empty() ? AssetHandle{} : AssetHandle::FromString(s);
    return true;
  }
};

}  // namespace

void InitializeBuiltinConverters() {
  ConverterRegistry::Register<float>(std::make_unique<ScalarConverter<float>>());
  ConverterRegistry::Register<double>(
      std::make_unique<ScalarConverter<double>>());
  ConverterRegistry::Register<int32_t>(
      std::make_unique<ScalarConverter<int32_t>>());
  ConverterRegistry::Register<uint32_t>(
      std::make_unique<ScalarConverter<uint32_t>>());
  ConverterRegistry::Register<int64_t>(
      std::make_unique<ScalarConverter<int64_t>>());
  ConverterRegistry::Register<uint64_t>(
      std::make_unique<ScalarConverter<uint64_t>>());
  ConverterRegistry::Register<int8_t>(
      std::make_unique<ScalarConverter<int8_t>>());
  ConverterRegistry::Register<uint8_t>(
      std::make_unique<ScalarConverter<uint8_t>>());
  ConverterRegistry::Register<bool>(std::make_unique<BoolConverter>());
  ConverterRegistry::Register<std::string>(std::make_unique<StringConverter>());
  ConverterRegistry::Register<glm::vec2>(std::make_unique<Vec2Converter>());
  ConverterRegistry::Register<glm::vec3>(std::make_unique<Vec3Converter>());
  ConverterRegistry::Register<glm::vec4>(std::make_unique<Vec4Converter>());
  ConverterRegistry::Register<glm::quat>(std::make_unique<QuatConverter>());
  ConverterRegistry::Register<AssetHandle>(
      std::make_unique<AssetHandleConverter>());
  LOG_INFO("Reflection: {} built-in converters installed", 15);
}

}  // namespace wiesel::reflect
