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

// Umbrella header for the editor's UI utility library. Pulls in every
// sub-namespace under wiesel::editor::ui (style, draw, layout, popup,
// section, row, button, field). Callers should typically:
//
//   #include "ui/w_ui.h"
//   namespace ui = wiesel::editor::ui;
//   ...
//   ui::popup::Begin(...);
//   ui::layout::BeginSidebarBody(160.0f);
//     ui::row::CategoryRow(...);
//
// If you only need one sub-namespace, include its header directly (e.g.
// `ui/w_ui_style.h`) to keep compile-time dependencies tight.

#include "ui/w_ui_button.h"
#include "ui/w_ui_draw.h"
#include "ui/w_ui_field.h"
#include "ui/w_ui_layout.h"
#include "ui/w_ui_popup.h"
#include "ui/w_ui_row.h"
#include "ui/w_ui_section.h"
#include "ui/w_ui_style.h"
