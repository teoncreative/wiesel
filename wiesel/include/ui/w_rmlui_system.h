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

#include <RmlUi/Core/SystemInterface.h>

namespace wiesel {

class RmlSystemInterface : public Rml::SystemInterface {
 public:
  double GetElapsedTime() override;
  bool LogMessage(Rml::Log::Type type, const Rml::String& message) override;
  void SetClipboardText(const Rml::String& text) override;
  void GetClipboardText(Rml::String& text) override;
  void SetMouseCursor(const Rml::String& cursor_name) override;
};

}  // namespace wiesel
