//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "scene/w_entity.hpp"

#include "behavior/w_behavior.hpp"
#include "scene/w_components.hpp"

namespace Wiesel {

Entity::Entity(entt::entity handle, Scene* scene)
    : entity_handle_(handle), scene_(scene) {
  child_handles_ = nullptr;
  parent_handle_ = entt::null;
  if (HasComponent<TreeComponent>()) {
    TreeComponent& component = GetComponent<TreeComponent>();
    parent_handle_ = component.parent;
    child_handles_ = &component.childs;
  }
}

Entity::~Entity() {}

}  // namespace Wiesel