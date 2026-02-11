
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
#ifdef _WIN32
#include <locale>
#include <codecvt>
#include <string>
#endif

namespace Wiesel::Dialogs {
struct FilterEntry {
  std::string name;
  std::string spec;
};

void Init();

void OpenFileDialog(std::vector<FilterEntry> filters,
                    std::function<void(const std::string&)> fn);

void Destroy();

}  // namespace Wiesel::Dialogs