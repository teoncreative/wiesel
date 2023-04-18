
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

#include "w_pch.h"
#include "scene/w_components.h"
#include "scene/w_entity.h"

namespace Wiesel {
	// Editor
	template<class T>
	void RenderComponentImGui(T& component, Entity entity) {
	}

	template<class T>
	void CallRenderComponentImGui(Entity entity) {
		if (entity.HasComponent<T>()) {
			RenderComponentImGui<T>(entity.GetComponent<T>(), entity);
		}
	}

	template<typename... ComponentTypes>
	void CallRenderComponentImGuiAll(Entity entity) {
		(CallRenderComponentImGui<ComponentTypes>(entity), ...);
	}

	// Adder
	template<class T>
	void RenderAddComponentImGui(Entity entity) {


	}

	template<class T>
	void CallRenderAddComponentImGui(Entity entity) {
		if (!entity.HasComponent<T>()) {
			RenderAddComponentImGui<T>(entity);
		}
	}

	template<typename... ComponentTypes>
	void CallRenderAddComponentImGuiAll(Entity entity) {
		(CallRenderAddComponentImGui<ComponentTypes>(entity), ...);
	}

#define GENERATE_COMPONENT_EDITORS(entity) CallRenderComponentImGuiAll<ALL_COMPONENT_TYPES>(entity);
#define GENERATE_COMPONENT_ADDERS(entity) CallRenderAddComponentImGuiAll<ALL_COMPONENT_TYPES>(entity);
}
