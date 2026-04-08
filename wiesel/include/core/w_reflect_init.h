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

namespace Wiesel {

// Register all WPROPERTY-reflected types with entt::meta.
// This calls the auto-generated ReflectAll() if codegen has run,
// otherwise it is a no-op.
void InitializeReflection();

}  // namespace Wiesel
