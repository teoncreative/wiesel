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

namespace wiesel {

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
  UIDocumentComponent(const UIDocumentComponent&) = default;

  AssetHandle document_handle;
  bool visible = true;
};

// Transient runtime data for UI documents. Emplaced automatically by
// UIDocumentSystem when the entity has a UIDocumentComponent. Destroyed
// by UIDocumentSystem when the component is removed or the entity is
// destroyed, cleaning up the Rml context.
struct UIDocumentRuntime {
  ~UIDocumentRuntime();

  // Prevent copies - each runtime owns a unique Rml context
  UIDocumentRuntime() = default;
  UIDocumentRuntime(const UIDocumentRuntime&) = delete;
  UIDocumentRuntime& operator=(const UIDocumentRuntime&) = delete;
  UIDocumentRuntime(UIDocumentRuntime&& other) noexcept;
  UIDocumentRuntime& operator=(UIDocumentRuntime&& other) noexcept;

  Rml::Context* rml_context = nullptr;
  Rml::ElementDocument* rml_document = nullptr;
  AssetHandle loaded_handle;
  std::string context_name;
  std::string loaded_vfs_path;

  // Data binding model
  UIDataModel data_model;

  // Offscreen rendering (managed by CanvasFeature)
  std::shared_ptr<AttachmentTexture> offscreen_texture;
  std::shared_ptr<AttachmentTexture> offscreen_stencil;
  std::shared_ptr<DescriptorSet> offscreen_descriptor;
  std::shared_ptr<UniformBuffer> offscreen_ubo;
  glm::vec2 offscreen_size{0, 0};
};

}  // namespace wiesel
