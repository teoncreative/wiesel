
//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "ui/w_ui_document.h"

#include <RmlUi/Core.h>

namespace wiesel {

UIDocumentRuntime::~UIDocumentRuntime() {
  if (rml_context) {
    data_model.Shutdown();
    Rml::RemoveContext(context_name);
    rml_context = nullptr;
    rml_document = nullptr;
  }
}

UIDocumentRuntime::UIDocumentRuntime(UIDocumentRuntime&& other) noexcept
    : rml_context(other.rml_context),
      rml_document(other.rml_document),
      loaded_handle(std::move(other.loaded_handle)),
      context_name(std::move(other.context_name)),
      loaded_vfs_path(std::move(other.loaded_vfs_path)),
      data_model(std::move(other.data_model)),
      offscreen_texture(std::move(other.offscreen_texture)),
      offscreen_stencil(std::move(other.offscreen_stencil)),
      offscreen_descriptor(std::move(other.offscreen_descriptor)),
      offscreen_ubo(std::move(other.offscreen_ubo)),
      offscreen_framebuffer(std::move(other.offscreen_framebuffer)),
      offscreen_size(other.offscreen_size) {
  other.rml_context = nullptr;
  other.rml_document = nullptr;
}

UIDocumentRuntime& UIDocumentRuntime::operator=(
    UIDocumentRuntime&& other) noexcept {
  if (this != &other) {
    if (rml_context) {
      data_model.Shutdown();
      Rml::RemoveContext(context_name);
    }

    rml_context = other.rml_context;
    rml_document = other.rml_document;
    loaded_handle = std::move(other.loaded_handle);
    context_name = std::move(other.context_name);
    loaded_vfs_path = std::move(other.loaded_vfs_path);
    data_model = std::move(other.data_model);
    offscreen_texture = std::move(other.offscreen_texture);
    offscreen_stencil = std::move(other.offscreen_stencil);
    offscreen_descriptor = std::move(other.offscreen_descriptor);
    offscreen_ubo = std::move(other.offscreen_ubo);
    offscreen_framebuffer = std::move(other.offscreen_framebuffer);
    offscreen_size = other.offscreen_size;

    other.rml_context = nullptr;
    other.rml_document = nullptr;
  }
  return *this;
}

}  // namespace wiesel
