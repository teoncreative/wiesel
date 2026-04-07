//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "systems/w_ui_document_system.h"

#include <RmlUi/Core.h>

#include "asset/w_asset_manager.h"
#include "behavior/w_behavior.h"
#include "scene/w_scene.h"
#include "script/mono/w_monobehavior.h"
#include "ui/w_ui_document.h"
#include "w_engine.h"

namespace Wiesel {

void UIDocumentSystem::Update(Scene& scene, float delta_time) {
  PROFILE_ZONE_SCOPED_N("UIDocumentSystem::Update");
  entt::registry& registry = scene.GetRegistry();
  auto& assets = Engine::asset_manager();
  for (auto entity : registry.view<UIDocumentComponent>()) {
    auto& doc = registry.get<UIDocumentComponent>(entity);
    if (doc.document_handle.IsValid() &&
        (!doc.rml_context_ || doc.loaded_handle_ != doc.document_handle)) {
      // Destroy old context
      if (doc.rml_context_) {
        doc.data_model.Shutdown();
        Rml::RemoveContext(doc.context_name_);
        doc.rml_context_ = nullptr;
        doc.rml_document_ = nullptr;
      }
      // Create new per-document context
      auto doc_asset = assets.GetOrLoad<UIDocumentAsset>(doc.document_handle);
      if (doc_asset && !doc_asset->vfs_path.empty()) {
        doc.context_name_ =
            "ui_" + std::to_string(static_cast<uint32_t>(entity));
        // Use viewport size as initial context dimensions.
        // RmlUi does initial layout at creation time - too small causes
        // incorrect text wrapping and block sizing.
        glm::vec2 vp = scene.GetRenderResolution();
        if (vp.x < 1 || vp.y < 1) {
          vp = {1920, 1080};
        }
        doc.rml_context_ = Rml::CreateContext(
            doc.context_name_,
            Rml::Vector2i(static_cast<int>(vp.x), static_cast<int>(vp.y)));
        if (doc.rml_context_) {
          const auto* meta = assets.GetMetadata(doc.document_handle);
          const auto* ui_props =
              meta ? meta->GetProperties<UIDocumentAssetProperties>() : nullptr;
          doc.data_model.Init(doc.rml_context_, ui_props);

          // Wire callbacks to dispatch to C# behaviors on this entity
          if (ui_props) {
            for (const auto& decl : ui_props->variables) {
              if (decl.mode == UIVariableMode::TwoWay) {
                doc.data_model.OnChanged(
                    decl.name, [&registry, entity, name = decl.name]() {
                      if (registry.valid(entity) &&
                          registry.any_of<BehaviorsComponent>(entity)) {
                        auto& bc = registry.get<BehaviorsComponent>(entity);
                        for (auto& [bname, behavior] : bc.behaviors_) {
                          auto* mb = dynamic_cast<MonoBehavior*>(behavior);
                          if (mb && mb->script_instance()) {
                            mb->script_instance()->OnUIDataChanged(name);
                          }
                        }
                      }
                    });
              }
            }
            for (const auto& event_name : ui_props->events) {
              doc.data_model.BindEvent(
                  event_name,
                  [&registry, entity, name = event_name](Rml::Event& /*ev*/) {
                    if (registry.valid(entity) &&
                        registry.any_of<BehaviorsComponent>(entity)) {
                      auto& bc = registry.get<BehaviorsComponent>(entity);
                      for (auto& [bname, behavior] : bc.behaviors_) {
                        auto* mb = dynamic_cast<MonoBehavior*>(behavior);
                        if (mb && mb->script_instance()) {
                          mb->script_instance()->OnUIEvent(name);
                        }
                      }
                    }
                  });
            }
          }

          doc.rml_document_ =
              doc.rml_context_->LoadDocument(doc_asset->vfs_path);
          if (doc.rml_document_) {
            doc.rml_document_->Show();
            LOG_INFO("Created RmlUi context '{}' with document '{}'",
                     doc.context_name_, doc_asset->vfs_path);
          } else {
            LOG_ERROR("Failed to load RmlUi document: {}", doc_asset->vfs_path);
          }
        } else {
          LOG_ERROR("Failed to create RmlUi context");
        }
      }
      doc.loaded_handle_ = doc.document_handle;
    }
    // Sync visibility
    if (doc.rml_document_) {
      if (doc.visible && !doc.rml_document_->IsVisible()) {
        doc.rml_document_->Show();
      } else if (!doc.visible && doc.rml_document_->IsVisible()) {
        doc.rml_document_->Hide();
      }
    }
  }
}

}  // namespace Wiesel
