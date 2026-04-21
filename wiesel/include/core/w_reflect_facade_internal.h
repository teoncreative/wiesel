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

#include <entt/meta/meta.hpp>

#include "core/w_reflect_facade.h"

// Engine-internal bridge between the public facade and the entt-meta backing
// library. ONLY include this from converter implementations or from the
// facade .cc file — never from consumer code.

namespace wiesel::reflect::internal {

// Read a field off an owner instance as a type-erased entt::meta_any.
entt::meta_any ReadFieldErased(FieldHandle field, const void* owner);

// Write a type-erased value into a field on an owner instance.
bool WriteFieldErased(FieldHandle field, void* owner, entt::meta_any value);

}  // namespace wiesel::reflect::internal
