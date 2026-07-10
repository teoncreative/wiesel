//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

//
// Created by Metehan Gezer on 05.03.2026.
//

#pragma once

#include <nlohmann/json.hpp>

#include "scene/w_scene.h"
#include "w_pch.h"

namespace wiesel::scene_serializer {

std::string SerializeToString(Scene& scene);
bool DeserializeFromString(Scene& scene, const std::string& json_str);

}  // namespace wiesel
