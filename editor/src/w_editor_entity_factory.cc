//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "w_editor_entity_factory.h"

#include <imgui.h>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "scene/w_components.h"
#include "scene/w_lights.h"
#include "w_editor_icons.h"
#include "w_engine.h"
#include "w_undo.h"

namespace wiesel::editor {

namespace {
std::vector<EntityFactoryDesc>& Registry() {
  static std::vector<EntityFactoryDesc> v;
  return v;
}
}  // namespace

void EntityFactoryRegistry::Register(EntityFactoryDesc desc) {
  Registry().push_back(std::move(desc));
}

const std::vector<EntityFactoryDesc>& EntityFactoryRegistry::All() {
  return Registry();
}

void InstallBuiltinEntityFactories() {
  EntityFactoryRegistry::Register({
      .label = "Empty Entity",
      .group = "",
      .icon = ICON_LC_GHOST,
      .create = [](Scene& s) { return s.CreateEntity(); },
  });

  struct Shape {
    const char* name;
    const char* icon;
  };
  const Shape shapes[] = {
      {"Cube", ICON_LC_BOX},
      {"Sphere", ICON_LC_CIRCLE},
      {"Plane", ICON_LC_RECTANGLE_HORIZONTAL},
      {"Cylinder", ICON_LC_CYLINDER},
      {"Capsule", ICON_LC_PILL},
  };
  for (const Shape& shape : shapes) {
    EntityFactoryRegistry::Register({
        .label = shape.name,
        .group = "3D Shape",
        .icon = shape.icon,
        .create =
            [name = std::string(shape.name)](Scene& s) {
              Entity e = s.CreateEntity(name);
              auto& mc = e.AddComponent<MeshRendererComponent>();
              mc.model_handle = Engine::GetPrimitive(name);
              mc.mesh_index = 0;
              return e;
            },
    });
  }

  EntityFactoryRegistry::Register({
      .label = "Directional Light",
      .group = "Light",
      .icon = ICON_LC_SUN,
      .create =
          [](Scene& s) {
            Entity e = s.CreateEntity("Directional Light");
            e.AddComponent<LightDirectComponent>();
            return e;
          },
  });
  EntityFactoryRegistry::Register({
      .label = "Point Light",
      .group = "Light",
      .icon = ICON_LC_LIGHTBULB,
      .create =
          [](Scene& s) {
            Entity e = s.CreateEntity("Point Light");
            e.AddComponent<LightPointComponent>();
            return e;
          },
  });

  EntityFactoryRegistry::Register({
      .label = "Camera",
      .group = "",
      .icon = ICON_LC_CAMERA,
      .create =
          [](Scene& s) {
            Entity e = s.CreateEntity("Camera");
            e.AddComponent<CameraComponent>();
            return e;
          },
  });
}

namespace {
// Group icon overrides keyed by group name. When unset we fall back to the
// first child's icon, which keeps things sensible for ad-hoc groups.
const std::map<std::string, const char*>& GroupIcons() {
  static const std::map<std::string, const char*> m = {
      {"3D Shape", ICON_LC_BOX},
      {"Light", ICON_LC_LIGHTBULB},
  };
  return m;
}

const char* IconForGroup(const std::string& group) {
  if (group.empty()) {
    return nullptr;
  }
  const auto& m = GroupIcons();
  auto it = m.find(group);
  if (it != m.end()) {
    return it->second;
  }
  for (const auto& f : EntityFactoryRegistry::All()) {
    if (f.group == group && !f.icon.empty()) {
      return f.icon.c_str();
    }
  }
  return nullptr;
}

bool DrawFactoryItem(const EntityFactoryDesc& f) {
  if (!f.icon.empty()) {
    ImGui::SetNextMenuItemIcon(f.icon.c_str());
  }
  return ImGui::MenuItem(f.label.c_str());
}
}  // namespace

Entity RenderEntityFactoryMenu(Scene& scene, CommandStack& commands,
                               Entity parent, const glm::vec3* spawn_pos) {
  const auto& factories = EntityFactoryRegistry::All();
  const EntityFactoryDesc* picked = nullptr;

  // Top-level (no group) factories first, in registration order.
  for (const auto& f : factories) {
    if (!f.group.empty()) {
      continue;
    }
    if (DrawFactoryItem(f)) {
      picked = &f;
    }
  }

  // Group factories by `group` field, preserving registration order both for
  // groups and items within them.
  std::vector<std::string> group_order;
  std::map<std::string, std::vector<const EntityFactoryDesc*>> groups;
  for (const auto& f : factories) {
    if (f.group.empty()) {
      continue;
    }
    auto it = groups.find(f.group);
    if (it == groups.end()) {
      group_order.push_back(f.group);
      groups[f.group] = {&f};
    } else {
      it->second.push_back(&f);
    }
  }

  for (const std::string& group : group_order) {
    if (const char* icon = IconForGroup(group)) {
      ImGui::SetNextMenuItemIcon(icon);
    }
    if (ImGui::BeginMenu(group.c_str())) {
      for (const EntityFactoryDesc* f : groups[group]) {
        if (DrawFactoryItem(*f)) {
          picked = f;
        }
      }
      ImGui::EndMenu();
    }
  }

  if (!picked) {
    return kInvalidEntity;
  }

  Entity created = picked->create(scene);
  if (!created) {
    return kInvalidEntity;
  }
  if (parent) {
    scene.LinkEntities(parent, created);
  }
  if (spawn_pos) {
    auto& tc = created.GetComponent<TransformComponent>();
    tc.SetPosition(*spawn_pos);
  }
  commands.Execute(std::make_unique<EntityCreateCommand>(created.ToRef()));
  return created;
}

}  // namespace wiesel::editor
