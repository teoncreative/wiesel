
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

#include <filesystem>
#include <nlohmann/json.hpp>

namespace wiesel {

// Simple JSON-backed config file for user preferences.
// One file per config name, stored under the user data directory.
//
// Usage:
//   Engine::editor_config().Set("editor.theme", 1);
//   int theme = Engine::editor_config().Get<int>("editor.theme", 0);
//   Engine::editor_config().Save();
class UserConfig {
 public:
  explicit UserConfig(const std::filesystem::path& dir,
                      const std::string& filename = "user_config.json");

  // Load from disk. Returns false if file doesn't exist or parse fails.
  bool Load();

  // Save current state to disk. Creates parent directories.
  bool Save();

  // Get a value by key, returning default_value if not present.
  template <typename T>
  T Get(const std::string& key, const T& default_value) const {
    if (data_.contains(key) && !data_[key].is_null()) {
      try {
        return data_[key].get<T>();
      } catch (...) {
        return default_value;
      }
    }
    return default_value;
  }

  // Set a value by key. Does NOT auto-save.
  template <typename T>
  void Set(const std::string& key, const T& value) {
    data_[key] = value;
  }

  bool Has(const std::string& key) const;
  void Erase(const std::string& key);

  const nlohmann::json& data() const { return data_; }

  nlohmann::json& data() { return data_; }

 private:
  std::filesystem::path file_path_;
  nlohmann::json data_;
};

}  // namespace wiesel
