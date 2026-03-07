
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
  MouseMovedEvent(double x, double y, double delta_x, double delta_y,
                  CursorMode cursor_mode)
      : x_(x), y_(y), delta_x_(delta_x), delta_y_(delta_y),
        cursor_mode_(cursor_mode) {}

  // Absolute screen position (pixels, DPI-scaled)
  WIESEL_GETTER_FN double GetX() const { return x_; }
  WIESEL_GETTER_FN double GetY() const { return y_; }

  // Normalized delta (pixel delta / window width)
  WIESEL_GETTER_FN double GetDeltaX() const { return delta_x_; }
  WIESEL_GETTER_FN double GetDeltaY() const { return delta_y_; }

  WIESEL_GETTER_FN CursorMode GetCursorMode() const { return cursor_mode_; }

  EVENT_CLASS_TYPE(MouseMoved)
  EVENT_CLASS_CATEGORY(kEventCategoryMouse | kEventCategoryInput)
 private:
  double x_, y_;
  double delta_x_, delta_y_;
  CursorMode cursor_mode_;
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
