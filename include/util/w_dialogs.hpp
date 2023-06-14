
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

#include "nfd.hpp"
#include "w_pch.hpp"

namespace Wiesel::Dialogs {
  typedef struct {
    const nfdnchar_t* name;
    const nfdnchar_t* spec;
  } FilterEntry;

  void Init();

  void OpenFileDialog(std::vector<FilterEntry> filters, std::function<void(const std::string&)> fn);

  void Destroy();

}