
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

#ifdef WIESEL_DISCORD_RPC

namespace Wiesel {

class DiscordRPC {
 public:
  DiscordRPC();
  ~DiscordRPC();

  void Initialize(const std::string& application_id);
  void Shutdown();

  bool IsInitialized() const { return initialized_; }

  // Set full presence - caller controls all fields
  void SetPresence(const std::string& details, const std::string& state,
                   const std::string& large_image_key = "",
                   const std::string& large_image_text = "",
                   bool show_elapsed = true);
  void ClearPresence();
  void RunCallbacks();

 private:
  bool initialized_ = false;
  int64_t session_start_time_ = 0;
};

}  // namespace Wiesel

#endif  // WIESEL_DISCORD_RPC