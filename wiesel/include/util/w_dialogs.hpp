
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
#ifdef _WIN32
#include <locale>
#include <codecvt>
#include <string>
#endif

namespace Wiesel::Dialogs {
struct FilterEntry {
  const nfdnchar_t* name;
  const nfdnchar_t* spec;

  FilterEntry(const std::string& name, const std::string& spec) {
#ifdef _WIN32
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    std::wstring wname = converter.from_bytes(name);
    std::wstring wspec = converter.from_bytes(spec);
    this->name = wname.c_str();
    this->spec = wspec.c_str();
#else
    this->name = name.c_str();
    this->spec = spec.c_str();
#endif
  }
};

void Init();

void OpenFileDialog(std::vector<FilterEntry> filters,
                    std::function<void(const std::string&)> fn);

void Destroy();

}  // namespace Wiesel::Dialogs