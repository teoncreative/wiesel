//
// Created by Metehan Gezer on 12.02.2026.
//

#ifndef WIESEL_PARENT_W_PLATFORM_HPP
#define WIESEL_PARENT_W_PLATFORM_HPP

namespace Wiesel {

std::filesystem::path GetExecutableDirectory();
std::filesystem::path GetUserDataDirectory(const std::string& app_name = "Wiesel");
// Get cache directory (for temporary data, thumbnails, etc.)
std::filesystem::path GetUserCacheDirectory(const std::string& app_name = "Wiesel");

void OpenFileInDefaultEditor(const std::filesystem::path& path);

}

#endif  //WIESEL_PARENT_W_PLATFORM_HPP
