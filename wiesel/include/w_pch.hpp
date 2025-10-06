//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#ifdef __cplusplus
#include <vulkan/vulkan.h>
#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#include <algorithm>  // Necessary for std::clamp
#include <any>
#include <array>
#include <chrono>
#include <cstdint>  // Necessary for uint32_t
#include <filesystem>
#include <fstream>
#include <functional>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <iostream>
#include <limits>  // Necessary for std::numeric_limits
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <list>
#include <span>
#include <format>

#include "util/w_attributes.hpp"
#include "util/w_platformdetection.hpp"
#include "util/w_tracy.hpp"

#endif