
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

#include "events/w_events.hpp"
#include "util/w_mousecodes.hpp"
#include "w_pch.hpp"
#include "window/w_window.hpp"

namespace Wiesel {

class PipelineRecreatedEvent : public Event {
 public:
  PipelineRecreatedEvent() {}

  EVENT_CLASS_TYPE(PipelineRecreated)
  EVENT_CLASS_CATEGORY(EventCategory::kEventCategoryApp)
 private:
};

}  // namespace Wiesel
