
//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "scene/w_componentutil.h"
#include "util/w_imguiutil.h"
#include "scene/w_lights.h"

namespace Wiesel {
	template<class T>
	void RenderComponent(T& component, Entity entity) {

	}

	template<>
	void RenderComponent(TransformComponent& component, Entity entity) {
		if (ImGui::TreeNode("Transform")) {
			bool changed = false;
			changed |= ImGui::DragFloat3(PrefixLabel("Position").c_str(), reinterpret_cast<float*>(&component.Position), 0.1);
			changed |= ImGui::DragFloat3(PrefixLabel("Rotation").c_str(), reinterpret_cast<float*>(&component.Rotation), 0.1);
			changed |= ImGui::DragFloat3(PrefixLabel("Scale").c_str(), reinterpret_cast<float*>(&component.Scale), 0.1);
			if (changed) {
				component.IsChanged = true;
			}
			ImGui::TreePop();
		}
	}

	template<>
	void RenderComponent(LightDirectComponent& component, Entity entity) {
		if (ImGui::TreeNode("Direct Light")) {
			ImGui::DragFloat(PrefixLabel("Ambient").c_str(), &component.LightData.Base.Ambient, 0.01);
			ImGui::DragFloat(PrefixLabel("Diffuse").c_str(), &component.LightData.Base.Diffuse, 0.1);
			ImGui::DragFloat(PrefixLabel("Specular").c_str(), &component.LightData.Base.Specular, 0.1);
			ImGui::DragFloat(PrefixLabel("Density").c_str(), &component.LightData.Base.Density, 0.1);
			ImGui::ColorPicker3(PrefixLabel("Color").c_str(), reinterpret_cast<float*>(&component.LightData.Base.Color));
			ImGui::TreePop();
		}
	}

	template<>
	void RenderComponent(LightPointComponent& component, Entity entity) {
		if (ImGui::TreeNode("Point Light")) {
			ImGui::DragFloat(PrefixLabel("Ambient").c_str(), &component.LightData.Base.Ambient, 0.01);
			ImGui::DragFloat(PrefixLabel("Diffuse").c_str(), &component.LightData.Base.Diffuse, 0.1);
			ImGui::DragFloat(PrefixLabel("Specular").c_str(), &component.LightData.Base.Specular, 0.1);
			ImGui::DragFloat(PrefixLabel("Density").c_str(), &component.LightData.Base.Density, 0.1);
			ImGui::ColorPicker3("Color", reinterpret_cast<float*>(&component.LightData.Base.Color));
			ImGui::TreePop();
		}
	}
}