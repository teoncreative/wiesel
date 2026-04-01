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

#include "asset/w_asset_handle.h"
#include "rendering/w_descriptor.h"
#include "rendering/w_texture.h"
#include "scene/w_components.h"
#include "ui/w_ui_data_model.h"

namespace Rml {
class Context;
class ElementDocument;
}  // namespace Rml

namespace Wiesel {

class Framebuffer;

// Asset data for .rml UI documents
struct UIDocumentAsset {
  std::string vfs_path;
};

// Asset data for .rcss stylesheets
struct UIStylesheetAsset {
  std::string vfs_path;
};

struct UIDocumentComponent : public IComponent {
  UIDocumentComponent() = default;

  // Copy only serialized fields - runtime state resets
  UIDocumentComponent(const UIDocumentComponent& other)
      : document_handle(other.document_handle), visible(other.visible) {}

  AssetHandle document_handle;
  bool visible = true;

  // Runtime - per-document RmlUi context (each document gets its own)
  Rml::Context* rml_context_ = nullptr;
  Rml::ElementDocument* rml_document_ = nullptr;
  AssetHandle loaded_handle_;
  std::string context_name_;
  std::string loaded_vfs_path_;

  // Runtime - data binding model
  UIDataModel data_model;

  // Runtime - offscreen rendering (managed by CanvasFeature)
  std::shared_ptr<AttachmentTexture> offscreen_texture_;
  std::shared_ptr<DescriptorSet> offscreen_descriptor_;
  std::shared_ptr<UniformBuffer> offscreen_ubo_;
  std::shared_ptr<Framebuffer> offscreen_framebuffer_;
  glm::vec2 offscreen_size_{0, 0};
};

}  // namespace Wiesel
