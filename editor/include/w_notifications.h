
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
#include <chrono>
#include <deque>
#include <string>

namespace Wiesel {

enum class NotificationType { Info, Warning, Error };

struct EditorNotification {
  std::string message;
  NotificationType type = NotificationType::Info;
  std::chrono::steady_clock::time_point created;
  bool dismissed = false;
};

class NotificationManager {
 public:
  // Push a new notification (appears as toast + added to history)
  void Push(const std::string& message,
            NotificationType type = NotificationType::Info);

  void PushInfo(const std::string& message) {
    Push(message, NotificationType::Info);
  }

  void PushWarning(const std::string& message) {
    Push(message, NotificationType::Warning);
  }

  void PushError(const std::string& message) {
    Push(message, NotificationType::Error);
  }

  // Render toast stack (bottom-right overlay). Call each frame.
  void RenderToasts();

  // Render notification history panel button (for the top-right bar).
  // Returns true if the history panel is open.
  void RenderHistoryButton();

  // Render the history panel content.
  void RenderHistoryPanel();

  // Clear all history
  void ClearHistory();

  // Number of unread notifications
  size_t UnreadCount() const;

  // Auto-dismiss duration for toasts (seconds)
  float toast_duration = 5.0f;

 private:
  std::deque<EditorNotification> history_;
  bool show_history_ = false;
  size_t read_count_ = 0;

  static ImVec4 GetTypeColor(NotificationType type);
  static const char* GetTypeIcon(NotificationType type);
};

}  // namespace Wiesel