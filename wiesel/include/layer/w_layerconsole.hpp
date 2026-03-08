
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

#include "layer/w_layer.hpp"

namespace Wiesel {

// Drop-down developer console overlay, like Source engine.
// Push this layer after ImGuiLayer. Toggle visibility via
// Engine::console().Toggle() from your app's key handler.
class ConsoleLayer : public Layer {
 public:
  ConsoleLayer();
  ~ConsoleLayer() override;

  void OnEvent(Event& event) override;
  void OnBeginPresent() override;

  // Request input focus on the console input field (call after Toggle)
  void FocusInput() { focus_input_ = true; }

  // Set the key that closes the console when it's open (default: grave accent / backtick)
  void SetToggleKey(KeyCode key) { toggle_key_ = key; }
  KeyCode GetToggleKey() const { return toggle_key_; }

 private:
  KeyCode toggle_key_ = KeyGraveAccent;
  char input_buf_[512] = {};
  std::vector<std::string> history_;
  int history_pos_ = -1;
  bool focus_input_ = false;
  bool initialized_ = false;
};

}  // namespace Wiesel