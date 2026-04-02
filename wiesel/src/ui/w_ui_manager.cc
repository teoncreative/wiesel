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

#include "asset/w_asset_manager.h"
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

  // Load fonts already registered, and listen for future font registrations
  for (AssetHandle handle :
       Engine::asset_manager().GetAllOfType(AssetType::Font)) {
    const auto* meta = Engine::asset_manager().GetMetadata(handle);
    if (meta && !meta->virtual_source_path.empty()) {
      if (Rml::LoadFontFace(meta->virtual_source_path)) {
        loaded_fonts_.insert(handle);
      } else {
        LOG_WARN("Failed to load RmlUi font: {}", meta->virtual_source_path);
      }
    }
  }

  Engine::asset_manager().OnAssetRegistered([this](AssetHandle handle,
                                                   const AssetMetadata& meta) {
    if (meta.type != AssetType::Font || !initialized_) {
      return;
    }
    if (!meta.virtual_source_path.empty() && !loaded_fonts_.contains(handle)) {
      if (Rml::LoadFontFace(meta.virtual_source_path)) {
        loaded_fonts_.insert(handle);
        LOG_INFO("Loaded RmlUi font: {}", meta.name);
      }
    }
  });

  // Create render pass with color + depth/stencil for clip masking
  auto render_pass =
      std::make_shared<RenderPass>(PassType::PostProcess, "RmlUi Offscreen");
  render_pass->AttachOutput(
      {.type = AttachmentTextureType::Offscreen,
       .format = Engine::renderer()->GetSwapChainImageFormat(),
       .msaa_mode = SamplingMode::DISABLED});
  render_pass->AttachOutput(
      {.type = AttachmentTextureType::DepthStencil,
       .format = Engine::renderer()->FindDepthStencilFormat(),
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
  loaded_fonts_.clear();
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
