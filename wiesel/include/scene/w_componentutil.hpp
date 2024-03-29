
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "scene/w_components.hpp"
#include "scene/w_entity.hpp"
#include "w_pch.hpp"

namespace Wiesel {

void InitializeComponents();

// Editor
template <class T>
__attribute__((noinline)) void RenderComponentImGui(T& component,
                                                    Entity entity) {}

template <class T, class B>
__attribute__((noinline)) bool RenderBehaviorComponentImGui(
    T& component, B& behavior, Entity entity) {
  return false;
}

template <class T>
void CallRenderComponentImGui(Entity entity) {
  if (entity.HasComponent<T>()) {
    RenderComponentImGui<T>(entity.GetComponent<T>(), entity);
  }
}

template <typename... ComponentTypes>
void CallRenderComponentImGuiAll(Entity entity) {
  (CallRenderComponentImGui<ComponentTypes>(entity), ...);
}

// Adder
template <class T>
__attribute__((noinline)) void RenderAddComponentImGui(Entity entity) {}

template <class T>
__attribute__((noinline)) void CallRenderAddComponentImGui(Entity entity) {
  if (!entity.HasComponent<T>()) {
    RenderAddComponentImGui<T>(entity);
  }
}

template <typename... ComponentTypes>
void CallRenderAddComponentImGuiAll(Entity entity) {
  (CallRenderAddComponentImGui<ComponentTypes>(entity), ...);
}

#define GENERATE_COMPONENT_EDITORS(entity) \
  CallRenderComponentImGuiAll<ALL_COMPONENT_TYPES>(entity);
#define GENERATE_COMPONENT_ADDERS(entity) \
  CallRenderAddComponentImGuiAll<ALL_COMPONENT_TYPES>(entity);
}  // namespace Wiesel
