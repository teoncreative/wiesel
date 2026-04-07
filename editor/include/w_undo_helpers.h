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

#include <imgui.h>

#include <functional>
#include <string>

#include "w_undo.h"

namespace Wiesel::Editor {

// Helper for tracking property edits through ImGui widgets.
// Captures the old value when a widget is first activated,
// and pushes a PropertyCommand when editing ends.
//
// Usage:
//   static UndoTracker<float> tracker;
//   if (ImGui::DragFloat("Speed", &speed)) { /* live preview */ }
//   tracker.Track(command_stack, "Change Speed", speed,
//                 [&](const float& v) { speed = v; });
//
// Call Track() after the ImGui widget. It uses IsItemActivated/
// IsItemDeactivatedAfterEdit internally.
template <typename T>
class UndoTracker {
 public:
  void Track(CommandStack& stack, const std::string& description,
             const T& current_value, std::function<void(const T&)> setter) {
    if (ImGui::IsItemActivated()) {
      old_value_ = current_value;
      active_ = true;
    }
    if (active_ && ImGui::IsItemDeactivatedAfterEdit()) {
      if (old_value_ != current_value) {
        stack.Execute(std::make_unique<PropertyCommand<T>>(
            description, std::move(setter), old_value_, current_value));
      }
      active_ = false;
    }
  }

 private:
  T old_value_{};
  bool active_ = false;
};

}  // namespace Wiesel::Editor
