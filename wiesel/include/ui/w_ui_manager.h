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

#include <unordered_set>

#include "asset/w_asset_handle.h"
#include "ui/w_rmlui_file.h"
#include "ui/w_rmlui_renderer.h"
#include "ui/w_rmlui_system.h"
#include "w_pch.h"

namespace Rml {
class Context;
}

namespace wiesel {

// Manages global RmlUi initialization, font loading, and the shared
// render interface. Does NOT own any contexts or documents - those
// are per-UIDocumentComponent.
class UIManager {
 public:
  UIManager();
  ~UIManager();

  void Init();
  void Shutdown();

  RmlRenderInterface* GetRenderInterface() { return &render_interface_; }

  // RmlUi debugger - call with a context to inspect
  void ToggleDebugger(Rml::Context* context);
  bool IsDebuggerVisible() const;

 private:
  RmlSystemInterface system_interface_;
  RmlFileInterface file_interface_;
  RmlRenderInterface render_interface_;
  bool initialized_ = false;
  bool debugger_initialized_ = false;
  std::unordered_set<AssetHandle> loaded_fonts_;
};

}  // namespace wiesel
