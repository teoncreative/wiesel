
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "layer/w_layerconsole.h"

#include <imgui.h>

#include "events/w_keyevents.h"
#include "w_engine.h"

namespace Wiesel {

ConsoleLayer::ConsoleLayer() : Layer("ConsoleLayer") {}

ConsoleLayer::~ConsoleLayer() = default;

void ConsoleLayer::OnEvent(Event& event) {
  auto& console = Engine::console();
  if (!console.IsVisible()) {
    return;
  }

  EventDispatcher dispatcher{event};
  dispatcher.Dispatch<KeyPressedEvent>([this, &console](KeyPressedEvent& e) {
    // Allow the toggle key to close the console
    if (e.GetKeyCode() == toggle_key_ && !e.IsRepeat()) {
      console.SetVisible(false);
      return true;
    }
    return false;
  });

  // Consume all keyboard events so the game doesn't react to typing
  if (event.IsInCategory(EventCategory::kEventCategoryKeyboard)) {
    event.handled_ = true;
  }
}

void ConsoleLayer::OnBeginPresent() {
  auto& console = Engine::console();
  if (!console.IsVisible()) {
    return;
  }

  ImGuiIO& io = ImGui::GetIO();

  // Set initial size/position only once
  ImGui::SetNextWindowSizeConstraints(ImVec2(400, 200),
                                      ImVec2(FLT_MAX, FLT_MAX));
  if (!initialized_) {
    ImGui::SetNextWindowSize(
        ImVec2(io.DisplaySize.x * 0.6f, io.DisplaySize.y * 0.4f));
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.2f, 20.0f));
    initialized_ = true;
  }

  ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;

  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.1f, 0.12f, 0.92f));

  bool visible = console.IsVisible();
  if (ImGui::Begin("Developer Console", &visible, flags)) {
    const auto& log = console.GetLog();

    // Toolbar
    if (ImGui::Button("Clear")) {
      console.Clear();
    }

    ImGui::Separator();

    // Log output
    float footer_height =
        ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    if (ImGui::BeginChild("##ConsoleLog", ImVec2(0, -footer_height),
                          ImGuiChildFlags_None,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
      for (const auto& line : log) {
        ImVec4 color;
        switch (line.level) {
          case ConsoleLogLevel::Warning:
            color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
            break;
          case ConsoleLogLevel::Error:
            color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
            break;
          default:
            color = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
            break;
        }
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(line.text.c_str());
        ImGui::PopStyleColor();
      }
      if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
      }
    }
    ImGui::EndChild();

    // Separator
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.4f, 0.4f, 0.5f, 1.0f));
    ImGui::Separator();
    ImGui::PopStyleColor();

    // History callback
    struct HistoryData {
      std::vector<std::string>* history;
      int* pos;
    };

    HistoryData hist_data{&history_, &history_pos_};

    auto HistoryCallbackFn = [](ImGuiInputTextCallbackData* data) -> int {
      auto* hd = static_cast<HistoryData*>(data->UserData);
      if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
        if (hd->history->empty()) {
          return 0;
        }
        if (data->EventKey == ImGuiKey_UpArrow) {
          if (*hd->pos == -1) {
            *hd->pos = static_cast<int>(hd->history->size()) - 1;
          } else if (*hd->pos > 0) {
            (*hd->pos)--;
          }
        } else if (data->EventKey == ImGuiKey_DownArrow) {
          if (*hd->pos != -1) {
            (*hd->pos)++;
            if (*hd->pos >= static_cast<int>(hd->history->size())) {
              *hd->pos = -1;
            }
          }
        }
        const char* text =
            (*hd->pos >= 0) ? (*hd->history)[*hd->pos].c_str() : "";
        data->DeleteChars(0, data->BufTextLen);
        data->InsertChars(0, text);
      }
      return 0;
    };

    // Input line
    ImGuiInputTextFlags input_flags = ImGuiInputTextFlags_EnterReturnsTrue |
                                      ImGuiInputTextFlags_CallbackHistory;
    ImGui::PushItemWidth(-1);
    if (focus_input_) {
      ImGui::SetKeyboardFocusHere();
      focus_input_ = false;
    }
    if (ImGui::InputText("##ConsoleInput", input_buf_, sizeof(input_buf_),
                         input_flags, HistoryCallbackFn, &hist_data)) {
      if (input_buf_[0] != '\0') {
        if (history_.empty() || history_.back() != input_buf_) {
          history_.push_back(input_buf_);
        }
        console.Execute(input_buf_);
        input_buf_[0] = '\0';
      }
      history_pos_ = -1;
      ImGui::SetKeyboardFocusHere(-1);
    }
    ImGui::PopItemWidth();
  }
  ImGui::End();

  ImGui::PopStyleColor();

  // Handle close via X button
  if (!visible) {
    console.SetVisible(false);
  }
}

}  // namespace Wiesel