
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "util/w_discord_rpc.h"

#ifdef WIESEL_DISCORD_RPC

#include <discord_rpc.h>
#include <chrono>
#include "util/w_logger.h"

namespace wiesel {

DiscordRPC::DiscordRPC() = default;

DiscordRPC::~DiscordRPC() {
  Shutdown();
}

void DiscordRPC::Initialize(const std::string& application_id) {
  DiscordEventHandlers handlers{};
  handlers.ready = [](const DiscordUser* user) {
    LOG_INFO("Discord RPC connected: {}", user->username);
  };
  handlers.disconnected = [](int code, const char* msg) {
    LOG_WARN("Discord RPC disconnected: {} ({})", msg, code);
  };
  handlers.errored = [](int code, const char* msg) {
    LOG_ERROR("Discord RPC error: {} ({})", msg, code);
  };

  Discord_Initialize(application_id.c_str(), &handlers, 1, nullptr);
  session_start_time_ = std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
  initialized_ = true;
}

void DiscordRPC::Shutdown() {
  if (initialized_) {
    Discord_ClearPresence();
    Discord_Shutdown();
    initialized_ = false;
  }
}

void DiscordRPC::SetPresence(const std::string& details,
                             const std::string& state,
                             const std::string& large_image_key,
                             const std::string& large_image_text,
                             bool show_elapsed) {
  if (!initialized_) {
    return;
  }

  DiscordRichPresence presence{};
  presence.details = details.c_str();
  presence.state = state.c_str();

  if (!large_image_key.empty()) {
    presence.largeImageKey = large_image_key.c_str();
  }
  if (!large_image_text.empty()) {
    presence.largeImageText = large_image_text.c_str();
  }
  if (show_elapsed) {
    presence.startTimestamp = session_start_time_;
  }

  Discord_UpdatePresence(&presence);
}

void DiscordRPC::ClearPresence() {
  if (!initialized_) {
    return;
  }
  Discord_ClearPresence();
}

void DiscordRPC::RunCallbacks() {
  if (!initialized_) {
    return;
  }
#ifdef DISCORD_DISABLE_IO_THREAD
  Discord_UpdateConnection();
#endif
  Discord_RunCallbacks();
}

}  // namespace wiesel

#endif  // WIESEL_DISCORD_RPC