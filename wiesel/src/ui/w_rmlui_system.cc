//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "ui/w_rmlui_system.h"

#include "w_engine.h"

namespace Wiesel {

double RmlSystemInterface::GetElapsedTime() {
  static auto start = std::chrono::steady_clock::now();
  auto now = std::chrono::steady_clock::now();
  return std::chrono::duration<double>(now - start).count();
}

bool RmlSystemInterface::LogMessage(Rml::Log::Type type,
                                    const Rml::String& message) {
  switch (type) {
    case Rml::Log::LT_ERROR:
    case Rml::Log::LT_ASSERT:
      LOG_ERROR("[RmlUi] {}", message);
      break;
    case Rml::Log::LT_WARNING:
      LOG_WARN("[RmlUi] {}", message);
      break;
    case Rml::Log::LT_INFO:
      LOG_INFO("[RmlUi] {}", message);
      break;
    default:
      LOG_DEBUG("[RmlUi] {}", message);
      break;
  }
  return true;
}

void RmlSystemInterface::SetClipboardText(const Rml::String& text) {
  Engine::window()->SetClipboardText(text);
}

void RmlSystemInterface::GetClipboardText(Rml::String& text) {
  text = Engine::window()->GetClipboardText();
}

}  // namespace Wiesel
