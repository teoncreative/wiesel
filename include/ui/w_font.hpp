
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
#include <ft2build.h>
#include FT_FREETYPE_H
#include <string>

namespace Wiesel {
  class Font {
  public:
    Font(const std::string& fileName);
    ~Font();

    bool IsLoaded();

  private:

  };
}
