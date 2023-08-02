
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

#include "events/w_events.hpp"
#include "util/w_utils.hpp"
#include "w_pch.hpp"

namespace Wiesel {
struct TextComponent {
  std::string Text;
};

enum CanvasType { CanvasTypeScreenSpace };

struct CanvasComponent {
  CanvasType Type;
};
}  // namespace Wiesel