
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "w_notifications.h"

#include <imgui.h>

namespace wiesel {

ImVec4 NotificationManager::GetTypeColor(NotificationType type) {
  switch (type) {
    case NotificationType::Info:
      return {0.5f, 0.7f, 1.0f, 1.0f};
    case NotificationType::Warning:
      return {1.0f, 0.8f, 0.2f, 1.0f};
    case NotificationType::Error:
      return {1.0f, 0.35f, 0.35f, 1.0f};
  }
  return {0.7f, 0.7f, 0.7f, 1.0f};
}

const char* NotificationManager::GetTypeIcon(NotificationType type) {
  switch (type) {
    case NotificationType::Info:
      return "i";
    case NotificationType::Warning:
      return "!";
    case NotificationType::Error:
      return "X";
  }
  return "?";
}

void NotificationManager::Push(const std::string& message,
                               NotificationType type) {
  EditorNotification notif;
  notif.message = message;
  notif.type = type;
  notif.created = std::chrono::steady_clock::now();
  history_.push_front(notif);

  // Cap history
  if (history_.size() > 100) {
    history_.pop_back();
  }
}

void NotificationManager::RenderToasts() {
  auto now = std::chrono::steady_clock::now();
  ImGuiViewport* viewport = ImGui::GetMainViewport();

  float padding = 16.0f;
  float toast_width = 360.0f;
  float toast_height = 0.0f;
  float y_offset = padding;

  // Render from bottom-right, stacking upward
  int visible_count = 0;
  for (auto& notif : history_) {
    if (notif.dismissed) {
      continue;
    }
    float elapsed = std::chrono::duration<float>(now - notif.created).count();
    if (elapsed > toast_duration) {
      notif.dismissed = true;
      continue;
    }

    // Fade out in the last second
    float alpha = 1.0f;
    if (elapsed > toast_duration - 1.0f) {
      alpha = toast_duration - elapsed;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg,
                          ImVec4(0.12f, 0.12f, 0.14f, 0.95f));

    std::string win_id = "##toast_" + std::to_string(visible_count);
    ImVec2 toast_pos(viewport->Pos.x + viewport->Size.x - toast_width - padding,
                     viewport->Pos.y + viewport->Size.y - y_offset - 60.0f);
    ImGui::SetNextWindowPos(toast_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(toast_width, 0));

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoDocking;

    if (ImGui::Begin(win_id.c_str(), nullptr, flags)) {
      ImVec4 color = GetTypeColor(notif.type);

      // Type indicator + message
      ImGui::PushStyleColor(ImGuiCol_Text, color);
      ImGui::Text("[%s]", GetTypeIcon(notif.type));
      ImGui::PopStyleColor();
      ImGui::SameLine();
      ImGui::TextWrapped("%s", notif.message.c_str());

      // Close button
      ImGui::SameLine(toast_width - 32.0f);
      std::string close_id = "x##close_" + std::to_string(visible_count);
      if (ImGui::SmallButton(close_id.c_str())) {
        notif.dismissed = true;
      }

      toast_height = ImGui::GetWindowHeight();
    }
    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    y_offset += toast_height + 4.0f;
    visible_count++;

    if (visible_count >= 5) {
      break;
    }
  }
}

void NotificationManager::RenderHistoryButton() {
  size_t unread = UnreadCount();
  if (unread > 0) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
    std::string label =
        std::to_string(unread) + " notification" + (unread > 1 ? "s" : "");
    if (ImGui::SmallButton(label.c_str())) {
      show_history_ = !show_history_;
      read_count_ = history_.size();
    }
    ImGui::PopStyleColor();
  } else {
    ImGui::TextDisabled("No notifications");
  }
}

void NotificationManager::RenderHistoryPanel() {
  if (!show_history_) {
    return;
  }

  ImGui::SetNextWindowSizeConstraints(ImVec2(400, 200), ImVec2(600, 500));
  if (ImGui::Begin("Notifications", &show_history_)) {
    if (ImGui::Button("Clear All")) {
      ClearHistory();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%zu entries", history_.size());
    ImGui::Separator();

    ImGui::BeginChild("notif_scroll", ImVec2(0, 0), ImGuiChildFlags_None,
                      ImGuiWindowFlags_None);
    for (size_t i = 0; i < history_.size(); i++) {
      auto& notif = history_[i];
      ImVec4 color = GetTypeColor(notif.type);

      ImGui::PushStyleColor(ImGuiCol_Text, color);
      ImGui::Text("[%s]", GetTypeIcon(notif.type));
      ImGui::PopStyleColor();
      ImGui::SameLine();
      ImGui::TextWrapped("%s", notif.message.c_str());

      // Timestamp
      auto elapsed = std::chrono::steady_clock::now() - notif.created;
      auto secs =
          std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
      if (secs < 60) {
        ImGui::SameLine();
        ImGui::TextDisabled("%llds ago", secs);
      } else if (secs < 3600) {
        ImGui::SameLine();
        ImGui::TextDisabled("%lldm ago", secs / 60);
      }

      if (i < history_.size() - 1) {
        ImGui::Separator();
      }
    }
    ImGui::EndChild();
  }
  ImGui::End();

  read_count_ = history_.size();
}

void NotificationManager::ClearHistory() {
  history_.clear();
  read_count_ = 0;
}

void NotificationManager::ToggleHistoryPanel() {
  show_history_ = !show_history_;
  if (show_history_) {
    read_count_ = history_.size();
  }
}

size_t NotificationManager::UnreadCount() const {
  return history_.size() > read_count_ ? history_.size() - read_count_ : 0;
}

}  // namespace wiesel