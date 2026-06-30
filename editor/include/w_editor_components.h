
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

#include <functional>
#include <string>
#include <typeindex>
#include <vector>
#include "asset/w_asset_handle.h"
#include "scene/w_components.h"
#include "scene/w_entity.h"
#include "w_pch.h"

namespace wiesel::editor {
class CommandStack;
}

namespace wiesel {

// Describes the inspector UI for one component type.
struct ComponentDesc {
  std::string display_name;
  std::string group;
  std::string icon;  // empty by default; UTF-8 glyph (e.g. ICON_LC_CAMERA)
  int icon_priority = 0;  // higher wins when multiple components have icons
  std::function<void(Entity)> RenderSelf;
  std::function<void(Entity)> RenderAdd;
  std::function<void(Entity)> RenderModal;
  std::function<bool(Entity)> HasComponent;
};

class ComponentUiRegistry {
 public:
  // Register (or overwrite) a component's inspector UI. The three function
  // pointers may be null if that stage doesn't need to draw anything.
  template <typename T>
  static void Register(const std::string& display_name,
                       const std::string& group,
                       void (*render_self)(T&, Entity),
                       void (*render_add)(Entity),
                       void (*render_modal)(Entity),
                       const std::string& icon = "",
                       int icon_priority = 0) {
    ComponentDesc desc;
    desc.display_name = display_name;
    desc.group = group;
    desc.icon = icon;
    desc.icon_priority = icon_priority;
    // Leave RenderSelf/Add/Modal empty when the caller passes nullptr so the
    // inspector can skip no-op drawers entirely (wrapping a no-op in a
    // BeginDrawerFrame/EndDrawerFrame pair shifts the cursor and offsets every
    // subsequent drawer).
    if (render_self) {
      desc.RenderSelf = [render_self](Entity e) {
        render_self(e.GetComponent<T>(), e);
      };
    }
    if (render_add) {
      desc.RenderAdd = [render_add](Entity e) {
        render_add(e);
      };
    }
    if (render_modal) {
      desc.RenderModal = [render_modal](Entity e) {
        render_modal(e);
      };
    }
    desc.HasComponent = [](Entity entity) {
      return entity.HasComponent<T>();
    };
    Install(std::type_index(typeid(T)), std::move(desc));
  }

  // Lookup by type_index. Returns nullptr if the type isn't registered.
  static const ComponentDesc* Get(std::type_index type);

  // All registered types in registration order (stable for iteration).
  static const std::vector<std::type_index>& All();

 private:
  static void Install(std::type_index type, ComponentDesc desc);
};

// Shared drag-drop handler for asset fields (accepts AssetHandle + BrowserFile payloads)
AssetHandle AcceptAssetDragDrop(AssetType required_type);

void InitializeEditorComponents();
void InitializeScriptFieldRenderers();

// Set the active command stack for undo/redo tracking in inspector widgets.
// Must be called before RenderExistingComponents each frame.
void SetInspectorCommandStack(editor::CommandStack* stack);

void RenderExistingComponents(Entity entity);
void RenderModals(Entity entity);
void RenderAddPopup(Entity entity);

// Returns the icon (UTF-8 glyph) of the highest-priority component on the
// entity, or an empty string if no component on the entity has an icon.
// Priority ties are broken by registration order (later wins).
std::string_view GetEntityIcon(Entity entity);
}  // namespace wiesel
