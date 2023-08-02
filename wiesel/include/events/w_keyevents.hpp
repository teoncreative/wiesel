
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

#include "w_events.hpp"
#include "w_pch.hpp"

namespace Wiesel {
class KeyEvent : public Event {
 public:
  WIESEL_GETTER_FN KeyCode GetKeyCode() const { return m_KeyCode; }

  EVENT_CLASS_CATEGORY(EventCategory::Keyboard | EventCategory::Input)
 protected:
  KeyEvent(const KeyCode keycode) : m_KeyCode(keycode) {}

  KeyCode m_KeyCode;
};

class KeyPressedEvent : public KeyEvent {
 public:
  KeyPressedEvent(const KeyCode keycode, bool isRepeat = false)
      : KeyEvent(keycode), m_IsRepeat(isRepeat) {}

  WIESEL_GETTER_FN bool IsRepeat() const { return m_IsRepeat; }

  EVENT_CLASS_TYPE(KeyPressed)
 private:
  bool m_IsRepeat;
};

class KeyReleasedEvent : public KeyEvent {
 public:
  KeyReleasedEvent(const KeyCode keycode) : KeyEvent(keycode) {}

  EVENT_CLASS_TYPE(KeyReleased)
};

class KeyTypedEvent : public KeyEvent {
 public:
  KeyTypedEvent(const KeyCode keycode) : KeyEvent(keycode) {}

  EVENT_CLASS_TYPE(KeyTyped)
};
}  // namespace Wiesel
