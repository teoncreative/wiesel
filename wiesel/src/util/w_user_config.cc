
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "util/w_user_config.h"

#include <fstream>

#include "util/w_logger.h"

namespace wiesel {

UserConfig::UserConfig(const std::filesystem::path& dir,
                       const std::string& filename)
    : file_path_(dir / filename), data_(nlohmann::json::object()) {}

bool UserConfig::Load() {
  if (!std::filesystem::exists(file_path_)) {
    return false;
  }
  std::ifstream file(file_path_);
  if (!file.is_open()) {
    LOG_ERROR("Failed to open config file: {}", file_path_.string());
    return false;
  }
  try {
    file >> data_;
  } catch (const nlohmann::json::parse_error& e) {
    LOG_ERROR("Failed to parse config file {}: {}", file_path_.string(),
              e.what());
    data_ = nlohmann::json::object();
    return false;
  }
  return true;
}

bool UserConfig::Save() {
  std::filesystem::create_directories(file_path_.parent_path());
  std::ofstream file(file_path_);
  if (!file.is_open()) {
    LOG_ERROR("Failed to save config file: {}", file_path_.string());
    return false;
  }
  file << data_.dump(2);
  return true;
}

bool UserConfig::Has(const std::string& key) const {
  return data_.contains(key);
}

void UserConfig::Erase(const std::string& key) {
  data_.erase(key);
}

}  // namespace wiesel
