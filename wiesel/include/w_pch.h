//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#ifndef WIESEL_PCH_H
#define WIESEL_PCH_H

#define NOMINMAX

#ifdef __cplusplus

#include <vulkan/vulkan.h>

#include <algorithm>  // Necessary for std::clamp
#include <any>
#include <array>
#include <chrono>
#include <cstdint>  // Necessary for uint32_t
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

#include <initializer_list>
#include <iostream>
#include <limits>  // Necessary for std::numeric_limits
#include <list>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <span>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "util/w_attributes.h"
#include "util/w_platformdetection.h"
#include "util/w_tracy.h"

#endif  // __cplusplus

#endif  // WIESEL_PCH_H