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
#include <glm/glm.hpp>
#include <optional>
#include <string>
#include <vector>
#include "scene/w_entity.h"
#include "scene/w_scene.h"

namespace wiesel::editor {

class CommandStack;

// Describes one "create this kind of entity" command shared by the hierarchy
// context menu, the viewport quick-add popup, and the command palette. The
// factory itself just builds the entity and returns it - parenting,
// positioning, and undo recording happen in the shared dispatch path.
struct EntityFactoryDesc {
  std::string label;   // e.g. "Cube"
  std::string group;   // e.g. "3D Shape"; empty = top level
  std::string icon;    // ICON_LC_* glyph; may be empty
  std::function<Entity(Scene& scene)> create;
};

class EntityFactoryRegistry {
 public:
  static void Register(EntityFactoryDesc desc);

  // All registrations in registration order.
  static const std::vector<EntityFactoryDesc>& All();
};

// Register the built-in entity types (Empty, 3D Shapes, Lights, Camera).
// Called once during editor OnAttach.
void InstallBuiltinEntityFactories();

// Render the contents of an "Add" menu by iterating the registry. Top-level
// factories appear directly; grouped ones are nested in BeginMenu submenus.
// On creation: parents the new entity to `parent` if set, positions it at
// `spawn_pos` if set, executes an EntityCreateCommand, and returns the new
// entity. Returns kInvalidEntity if nothing was clicked.
Entity RenderEntityFactoryMenu(Scene& scene, CommandStack& commands,
                               Entity parent = kInvalidEntity,
                               const glm::vec3* spawn_pos = nullptr);

}  // namespace wiesel::editor
