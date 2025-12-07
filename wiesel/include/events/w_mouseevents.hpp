
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

#include "window/w_window.hpp"
#include "events/w_events.hpp"
#include "util/w_mousecodes.hpp"
#include "w_pch.hpp"

namespace Wiesel {
class MouseMovedEvent : public Event {
 public:
  MouseMovedEvent(const float_t x, const float_t y, CursorMode cursorMode)
      : m_MouseX(x), m_MouseY(y), m_CursorMode(cursorMode) {}

  WIESEL_GETTER_FN float_t GetX() const { return m_MouseX; }

  WIESEL_GETTER_FN float_t GetY() const { return m_MouseY; }

  WIESEL_GETTER_FN CursorMode GetCursorMode() const { return m_CursorMode; }

  EVENT_CLASS_TYPE(MouseMoved)
  EVENT_CLASS_CATEGORY(kEventCategoryMouse | kEventCategoryInput)
 private:
  float m_MouseX, m_MouseY;
  CursorMode m_CursorMode;
};

class MouseScrolledEvent : public Event {
 public:
  MouseScrolledEvent(const float_t xOffset, const float_t yOffset)
      : m_XOffset(xOffset), m_YOffset(yOffset) {}

  WIESEL_GETTER_FN float_t GetXOffset() const { return m_XOffset; }

  WIESEL_GETTER_FN float_t GetYOffset() const { return m_YOffset; }

  EVENT_CLASS_TYPE(MouseScrolled)
  EVENT_CLASS_CATEGORY(kEventCategoryMouse | kEventCategoryInput)
 private:
  float m_XOffset, m_YOffset;
};

class MouseButtonEvent : public Event {
 public:
  WIESEL_GETTER_FN MouseCode GetMouseButton() const { return m_Button; }

  EVENT_CLASS_CATEGORY(kEventCategoryMouse | kEventCategoryInput |
                       kEventCategoryMouseButton)
 protected:
  MouseButtonEvent(const MouseCode button) : m_Button(button) {}

  MouseCode m_Button;
};

class MouseButtonPressedEvent : public MouseButtonEvent {
 public:
  MouseButtonPressedEvent(const MouseCode button) : MouseButtonEvent(button) {}

  EVENT_CLASS_TYPE(MouseButtonPressed)
};

class MouseButtonReleasedEvent : public MouseButtonEvent {
 public:
  MouseButtonReleasedEvent(const MouseCode button) : MouseButtonEvent(button) {}

  EVENT_CLASS_TYPE(MouseButtonReleased)
};

}  // namespace Wiesel
