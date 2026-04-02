//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "ui/w_ui_manager.h"

#include <RmlUi/Core.h>
#include <RmlUi/Debugger.h>

#include "rendering/w_renderer.h"
#include "rendering/w_renderpass.h"
#include "w_engine.h"

namespace Wiesel {

UIManager::UIManager() : render_interface_(Engine::renderer()) {}

UIManager::~UIManager() {
  Shutdown();
}

void UIManager::Init() {
  if (initialized_) {
    return;
  }

  Rml::SetSystemInterface(&system_interface_);
  Rml::SetFileInterface(&file_interface_);

  if (!Rml::Initialise()) {
    LOG_ERROR("Failed to initialize RmlUi");
    return;
  }

  // Load default fonts (no render interface needed for this)
  if (!Rml::LoadFontFace("engine://fonts/SourceSans3.ttf")) {
    LOG_WARN("Failed to load default RmlUi font");
  }
  if (!Rml::LoadFontFace("engine://fonts/SourceSans3-Italic.ttf")) {
    LOG_WARN("Failed to load italic RmlUi font");
  }

  // Create render pass and initialize the render interface
  auto render_pass =
      std::make_shared<RenderPass>(PassType::PostProcess, "RmlUi Offscreen");
  render_pass->AttachOutput(
      {.type = AttachmentTextureType::Offscreen,
       .format = Engine::renderer()->GetSwapChainImageFormat(),
       .msaa_mode = SamplingMode::DISABLED});
  render_pass->Bake();

  render_interface_.Init(render_pass);
  Rml::SetRenderInterface(&render_interface_);

  initialized_ = true;
  LOG_INFO("UIManager initialized");
}

void UIManager::Shutdown() {
  if (!initialized_) {
    return;
  }
  if (debugger_initialized_) {
    Rml::Debugger::Shutdown();
    debugger_initialized_ = false;
  }
  Rml::Shutdown();
  initialized_ = false;
}

void UIManager::ToggleDebugger(Rml::Context* context) {
  if (!context) {
    return;
  }
  if (!debugger_initialized_) {
    Rml::Debugger::Initialise(context);
    debugger_initialized_ = true;
    Rml::Debugger::SetVisible(true);
  } else {
    Rml::Debugger::SetContext(context);
    Rml::Debugger::SetVisible(!Rml::Debugger::IsVisible());
  }
}

bool UIManager::IsDebuggerVisible() const {
  return debugger_initialized_ && Rml::Debugger::IsVisible();
}

}  // namespace Wiesel
