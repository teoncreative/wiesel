
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

#include "events/w_events.h"
#include "util/w_mousecodes.h"
#include "w_pch.h"
#include "window/w_window.h"

namespace wiesel {

class PipelineRecreatedEvent : public Event {
 public:
  PipelineRecreatedEvent() {}

  EVENT_CLASS_TYPE(PipelineRecreated)
  EVENT_CLASS_CATEGORY(EventCategory::kEventCategoryApp)
 private:
};

}  // namespace wiesel
