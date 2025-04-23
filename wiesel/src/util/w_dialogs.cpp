
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "util/w_dialogs.hpp"

#include <nfd.h>

#include <thread>

#include "util/w_logger.hpp"

namespace Wiesel::Dialogs {

// todo workaround for this main-thread requirement on Apple platform
// maybe move render things to another thread?
void Init() {
  NFD_Init();
}

void OpenFileDialog(std::vector<FilterEntry> filters,
                    std::function<void(const std::string&)> fn) {
  nfdchar_t* outPath;
  std::vector<nfdnfilteritem_t> filterList;
  nfdfiltersize_t filterCount = filters.size();
  for (int i = 0; i < filterCount; i++) {
    filterList.push_back({filters[i].name, filters[i].spec});
  }
  nfdresult_t result = NFD_OpenDialog(&outPath,  reinterpret_cast<const nfdfilteritem_t*>(filterList.data()), filterList.size(), NULL);
  if (result == NFD_OKAY) {
    auto relative = std::filesystem::relative(outPath);
    fn(relative.string());
    NFD_FreePath(outPath);
  } else {
    fn("");
  }
}

void Destroy() {
  NFD_Quit();
}

}  // namespace Wiesel::Dialogs