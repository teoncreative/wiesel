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
#include <memory>
#include <nlohmann/json.hpp>

#include "core/w_reflect_facade.h"
#include "core/w_reflect_facade_internal.h"

// TypeConverter registry. One converter per C++ type describes how to
// round-trip that type through JSON (and, later, ImGui / binary / Mono).
//
// Converters are registered engine-side; consumer code only sees the
// type-erased FieldToJson / FieldFromJson helpers on the facade.

namespace wiesel::reflect {

// Per-type converter contract. Extend with RenderImGui / ToBinary / etc as
// the facade picks up more consumers - the shape must stay uniform so every
// converter can be slotted in.
template <typename T>
struct TypeConverter {
  virtual ~TypeConverter() = default;
  virtual bool ToJson(const T& v, nlohmann::json& out) const = 0;
  virtual bool FromJson(const nlohmann::json& in, T& out) const = 0;
  // TODO (facade follow-up): RenderImGui(const char*, T&, const AttrSet&).
};

// Type-erased base used by the registry. Callers never see this directly.
struct IErasedConverter {
  virtual ~IErasedConverter() = default;
  virtual bool ToJsonErased(FieldHandle field, const void* owner,
                            nlohmann::json& out) const = 0;
  virtual bool FromJsonErased(FieldHandle field, void* owner,
                              const nlohmann::json& in) const = 0;
};

namespace detail {

template <typename T>
class ErasedConverterImpl : public IErasedConverter {
 public:
  explicit ErasedConverterImpl(std::unique_ptr<TypeConverter<T>> impl)
      : impl_(std::move(impl)) {}

  bool ToJsonErased(FieldHandle field, const void* owner,
                    nlohmann::json& out) const override {
    entt::meta_any any = internal::ReadFieldErased(field, owner);
    if (!any) {
      return false;
    }
    const T* ptr = any.try_cast<T>();
    if (!ptr) {
      return false;
    }
    return impl_->ToJson(*ptr, out);
  }

  bool FromJsonErased(FieldHandle field, void* owner,
                      const nlohmann::json& in) const override {
    T tmp{};
    if (!impl_->FromJson(in, tmp)) {
      return false;
    }
    return internal::WriteFieldErased(
        field, owner, entt::meta_any{std::in_place_type<T>, std::move(tmp)});
  }

 private:
  std::unique_ptr<TypeConverter<T>> impl_;
};

void RegisterConverterImpl(uint32_t type_id,
                           std::unique_ptr<IErasedConverter> converter);

}  // namespace detail

class ConverterRegistry {
 public:
  // Install a converter for T. Replaces any previous entry for T.
  template <typename T>
  static void Register(std::unique_ptr<TypeConverter<T>> converter) {
    detail::RegisterConverterImpl(
        entt::type_id<T>().hash(),
        std::make_unique<detail::ErasedConverterImpl<T>>(std::move(converter)));
  }

  // Look up a converter by type id (the hash of the field's C++ type).
  static const IErasedConverter* Find(uint32_t type_id);
};

}  // namespace wiesel::reflect
