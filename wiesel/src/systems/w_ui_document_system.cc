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

namespace wiesel {

void UIDocumentSystem::Update(Scene& scene, float delta_time) {
  PROFILE_ZONE_SCOPED_N("UIDocumentSystem::Update");
  entt::registry& registry = scene.GetRegistry();
  auto& assets = Engine::asset_manager();

  // Remove runtimes for entities that lost their UIDocumentComponent
  for (auto entity : registry.view<UIDocumentRuntime>(entt::exclude<UIDocumentComponent>)) {
    registry.remove<UIDocumentRuntime>(entity);
  }

  for (auto entity : registry.view<UIDocumentComponent>()) {
    auto& doc = registry.get<UIDocumentComponent>(entity);

    if (!doc.document_handle.IsValid()) {
      if (registry.any_of<UIDocumentRuntime>(entity)) {
        registry.remove<UIDocumentRuntime>(entity);
      }
      continue;
    }

    // Ensure runtime exists
    if (!registry.any_of<UIDocumentRuntime>(entity)) {
      registry.emplace<UIDocumentRuntime>(entity);
    }
    auto& rt = registry.get<UIDocumentRuntime>(entity);

    // Reload if handle changed
    if (!rt.rml_context || rt.loaded_handle != doc.document_handle) {
      // Destroy old context via RAII - just re-emplace
      if (rt.rml_context) {
        registry.remove<UIDocumentRuntime>(entity);
        registry.emplace<UIDocumentRuntime>(entity);
      }
      auto& rt2 = registry.get<UIDocumentRuntime>(entity);

      auto doc_asset = assets.GetOrLoad<UIDocumentAsset>(doc.document_handle);
      if (doc_asset && !doc_asset->vfs_path.empty()) {
        IdComponent id_component = registry.get<IdComponent>(entity);
        rt2.context_name = "ui_" + id_component.Id.ToString();

        glm::vec2 vp = scene.GetRenderResolution();
        if (vp.x < 1 || vp.y < 1) {
          vp = {1920, 1080};
        }
        rt2.rml_context = Rml::CreateContext(
            rt2.context_name,
            Rml::Vector2i(static_cast<int>(vp.x), static_cast<int>(vp.y)));
        if (rt2.rml_context) {
          const auto* meta = assets.GetMetadata(doc.document_handle);
          const auto* ui_props =
              meta ? meta->GetProperties<UIDocumentAssetProperties>() : nullptr;
          rt2.data_model.Init(rt2.rml_context, ui_props);

          // Wire callbacks to dispatch to C# behaviors on this entity
          if (ui_props) {
            for (const auto& decl : ui_props->variables) {
              if (decl.mode == UIVariableMode::TwoWay) {
                rt2.data_model.OnChanged(
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
              rt2.data_model.BindEvent(
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

          rt2.rml_document =
              rt2.rml_context->LoadDocument(doc_asset->vfs_path);
          if (rt2.rml_document) {
            rt2.rml_document->Show();
            LOG_INFO("Created RmlUi context '{}' with document '{}'",
                     rt2.context_name, doc_asset->vfs_path);
          } else {
            LOG_ERROR("Failed to load RmlUi document: {}",
                      doc_asset->vfs_path);
          }
        } else {
          LOG_ERROR("Failed to create RmlUi context");
        }
      }
      rt2.loaded_handle = doc.document_handle;
    }

    // Sync visibility
    auto* runtime = registry.try_get<UIDocumentRuntime>(entity);
    if (runtime && runtime->rml_document) {
      if (doc.visible && !runtime->rml_document->IsVisible()) {
        runtime->rml_document->Show();
      } else if (!doc.visible && runtime->rml_document->IsVisible()) {
        runtime->rml_document->Hide();
      }
    }
  }
}

}  // namespace wiesel
