
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

namespace Wiesel {
template <typename V>
struct Color {
  V Red, Green, Blue, Alpha;
};

using Colorf = Color<float>;
using Colori = Color<uint8_t>;

}  // namespace Wiesel