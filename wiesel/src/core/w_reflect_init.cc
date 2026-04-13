//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "core/w_reflect_init.h"

#include <entt/meta/factory.hpp>
#include <entt/meta/resolve.hpp>

#include "core/w_reflect.h"
#include "util/w_logger.h"
#include "w_reflect_all.generated.h"

namespace wiesel {

void InitializeReflection() {
  generated::ReflectAll();
  LOG_INFO("Reflection initialized");
}

}  // namespace wiesel
