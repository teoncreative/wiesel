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
// Created by Metehan Gezer on 12.02.2026.
//

#ifndef WIESEL_PARENT_W_PLATFORM_HPP
#define WIESEL_PARENT_W_PLATFORM_HPP

namespace Wiesel {

std::filesystem::path GetExecutableDirectory();
std::filesystem::path GetUserDataDirectory(
    const std::string& app_name = "Wiesel");
// Get cache directory (for temporary data, thumbnails, etc.)
std::filesystem::path GetUserCacheDirectory(
    const std::string& app_name = "Wiesel");

void OpenFileInDefaultEditor(const std::filesystem::path& path);

// Console utilities (Windows)
void EnableAnsiColors();
void AllocateConsole();

}  // namespace Wiesel

#endif  //WIESEL_PARENT_W_PLATFORM_HPP
