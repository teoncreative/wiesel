
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

#include "w_pch.hpp"

namespace Wiesel {

enum MouseCode : int32_t {
  kMouseButton0 = 0,
  kMouseButton1 = 1,
  kMouseButton2 = 2,
  kMouseButton3 = 3,
  kMouseButton4 = 4,
  kMouseButton5 = 5,
  kMouseButton6 = 6,
  kMouseButton7 = 7,

  kMouseButtonLeft = kMouseButton0,
  kMouseButtonRight = kMouseButton1,
  kMouseButtonMiddle = kMouseButton2
};
}  // namespace Wiesel
