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

#include "util/w_platform.h"

// clang-format off
// Import order important
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>  // For SHGetKnownFolderPath
#elif __APPLE__
#include <mach-o/dyld.h>
#include <unistd.h>
#include <pwd.h>
#else  // Linux
#include <unistd.h>
#include <pwd.h>
#endif
// clang-format on

namespace wiesel {

std::filesystem::path GetExecutableDirectory() {
#ifdef _WIN32
  wchar_t path[MAX_PATH];
  DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);

  if (length == 0) {
    throw std::runtime_error("Failed to get executable path");
  }

  std::filesystem::path exe_path(path);
  return exe_path.parent_path();

#elif __APPLE__
  char path[1024];
  uint32_t size = sizeof(path);

  if (_NSGetExecutablePath(path, &size) != 0) {
    throw std::runtime_error("Failed to get executable path");
  }

  // Resolve symlinks
  char real_path[PATH_MAX];
  if (realpath(path, real_path) == nullptr) {
    throw std::runtime_error("Failed to resolve executable path");
  }

  return std::filesystem::path(real_path).parent_path();

#else  // Linux
  char path[PATH_MAX];
  ssize_t count = readlink("/proc/self/exe", path, PATH_MAX);

  if (count == -1) {
    throw std::runtime_error("Failed to get executable path");
  }

  path[count] = '\0';
  return std::filesystem::path(path).parent_path();
#endif
}

std::filesystem::path GetUserDataDirectory(const std::string& app_name) {
#ifdef _WIN32
  // Get AppData/Local folder
  PWSTR path_ptr = nullptr;
  HRESULT hr =
      SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path_ptr);

  if (FAILED(hr)) {
    CoTaskMemFree(path_ptr);
    throw std::runtime_error("Failed to get AppData path");
  }

  std::filesystem::path user_data(path_ptr);
  CoTaskMemFree(path_ptr);

  user_data /= app_name;

#elif __APPLE__
  // ~/Library/Application Support/AppName
  const char* home = getenv("HOME");
  if (!home) {
    struct passwd* pw = getpwuid(getuid());
    home = pw->pw_dir;
  }

  std::filesystem::path user_data(home);
  user_data /= "Library/Application Support";
  user_data /= app_name;

#else  // Linux
  // Follow XDG Base Directory specification
  // $XDG_DATA_HOME or ~/.local/share
  const char* xdg_data = getenv("XDG_DATA_HOME");

  std::filesystem::path user_data;
  if (xdg_data && xdg_data[0] != '\0') {
    user_data = xdg_data;
  } else {
    const char* home = getenv("HOME");
    if (!home) {
      struct passwd* pw = getpwuid(getuid());
      home = pw->pw_dir;
    }
    user_data = home;
    user_data /= ".local/share";
  }

  user_data /= app_name;
#endif

  // Create directory if it doesn't exist
  if (!std::filesystem::exists(user_data)) {
    std::filesystem::create_directories(user_data);
  }

  return user_data;
}

// Get cache directory (for temporary data, thumbnails, etc.)
std::filesystem::path GetUserCacheDirectory(const std::string& app_name) {
#ifdef _WIN32
  // AppData/Local/AppName/Cache
  return GetUserDataDirectory(app_name) / "Cache";

#elif __APPLE__
  // ~/Library/Caches/AppName
  const char* home = getenv("HOME");
  if (!home) {
    struct passwd* pw = getpwuid(getuid());
    home = pw->pw_dir;
  }

  std::filesystem::path cache(home);
  cache /= "Library/Caches";
  cache /= app_name;

  if (!std::filesystem::exists(cache)) {
    std::filesystem::create_directories(cache);
  }

  return cache;
#else  // Linux
  // $XDG_CACHE_HOME or ~/.cache
  const char* xdg_cache = getenv("XDG_CACHE_HOME");

  std::filesystem::path cache;
  if (xdg_cache && xdg_cache[0] != '\0') {
    cache = xdg_cache;
  } else {
    const char* home = getenv("HOME");
    if (!home) {
      struct passwd* pw = getpwuid(getuid());
      home = pw->pw_dir;
    }
    cache = home;
    cache /= ".cache";
  }

  cache /= app_name;

  if (!std::filesystem::exists(cache)) {
    std::filesystem::create_directories(cache);
  }

  return cache;
#endif
}

void OpenFileInDefaultEditor(const std::filesystem::path& path) {
#ifdef _WIN32
  ShellExecuteW(nullptr, L"open", path.wstring().c_str(), nullptr, nullptr,
                SW_SHOWNORMAL);
#elif __APPLE__
  std::string cmd = "open \"" + path.string() + "\"";
  system(cmd.c_str());
#else  // Linux
  std::string cmd = "xdg-open \"" + path.string() + "\"";
  system(cmd.c_str());
#endif
}

void EnableAnsiColors() {
#ifdef _WIN32
  HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  if (hOut != INVALID_HANDLE_VALUE) {
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
  }
#endif
}

void AllocateConsole() {
#ifdef _WIN32
  ::AllocConsole();
  FILE* f = nullptr;
  freopen_s(&f, "CONOUT$", "w", stdout);
  freopen_s(&f, "CONOUT$", "w", stderr);
  freopen_s(&f, "CONIN$", "r", stdin);
#endif
}

}  // namespace wiesel