
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

#include "w_pch.h"
#ifdef _WIN32
#include <codecvt>
#include <locale>
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

void SaveFileDialog(std::vector<FilterEntry> filters,
                    std::function<void(const std::string&)> fn);

void SelectFolderDialog(std::function<void(const std::string&)> fn);

void Destroy();

}  // namespace Wiesel::Dialogs