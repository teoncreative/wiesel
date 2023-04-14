
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
#include "scene/w_lights.h"
#include "util/imgui/w_imguiutil.h"
#include "util/w_dialogs.h"
#include "util/w_logger.h"
#include "rendering/w_mesh.h"
#include "w_engine.h"
#include "w_application.h"

namespace Wiesel {
	template<>
	void RenderComponentImGui(TransformComponent& component, Entity entity) {
		if (ImGui::ClosableTreeNode("Transform", nullptr)) {
			TransformComponent& transform = entity.GetComponent<TransformComponent>();
			ModelComponent* modelComponent = entity.HasComponent<ModelComponent>() ? &entity.GetComponent<ModelComponent>() : nullptr;

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
	void RenderComponentImGui(ModelComponent& component, Entity entity) {
		static bool visible = true;
		if (ImGui::ClosableTreeNode("Model", &visible)) {
			auto& model = entity.GetComponent<ModelComponent>();
			ImGui::InputText("##", &model.Data.ModelPath, ImGuiInputTextFlags_ReadOnly);
			ImGui::SameLine();
			if (ImGui::Button("...")) {
				Dialogs::OpenFileDialog({{"Model file", "obj,gltf"}},[&entity](const std::string& file) {
					LOG_INFO(file);
					auto& model = entity.GetComponent<ModelComponent>();
					aiScene* aiScene = Engine::LoadAssimpModel(model, file);
					if (aiScene == nullptr) {
						return; // todo alert
					}
					Scene* engineScene = entity.GetScene();
					entt::entity entityHandle = entity.GetHandle();
					Application::Get()->SubmitToMainThread([aiScene, entityHandle, engineScene, file]() {
						Entity entity{entityHandle, engineScene};
						if (entity.HasComponent<ModelComponent>()) {
							auto& transform = entity.GetComponent<TransformComponent>();
							auto& model = entity.GetComponent<ModelComponent>();
							Engine::LoadModel(aiScene, transform, model, file);
						}
					});
				});
			}
			ImGui::Checkbox("Receive Shadows", &model.Data.ReceiveShadows);
			ImGui::TreePop();
		}
		if (!visible) {
			entity.RemoveComponent<ModelComponent>();
			visible = true;
		}
	}

	template<>
	void RenderComponentImGui(LightDirectComponent& component, Entity entity) {
		static bool visible = true;
		if (ImGui::ClosableTreeNode("Directional Light", &visible)) {
			ImGui::DragFloat(PrefixLabel("Ambient").c_str(), &component.LightData.Base.Ambient, 0.01);
			ImGui::DragFloat(PrefixLabel("Diffuse").c_str(), &component.LightData.Base.Diffuse, 0.1);
			ImGui::DragFloat(PrefixLabel("Specular").c_str(), &component.LightData.Base.Specular, 0.1);
			ImGui::DragFloat(PrefixLabel("Density").c_str(), &component.LightData.Base.Density, 0.1);
			ImGui::ColorPicker3(PrefixLabel("Color").c_str(), reinterpret_cast<float*>(&component.LightData.Base.Color));
			ImGui::TreePop();
		}
		if (!visible) {
			entity.RemoveComponent<LightDirectComponent>();
			visible = true;
		}
	}

	template<>
	void RenderComponentImGui(LightPointComponent& component, Entity entity) {
		static bool visible = true;
		if (ImGui::ClosableTreeNode("Point Light", &visible)) {
			ImGui::DragFloat(PrefixLabel("Ambient").c_str(), &component.LightData.Base.Ambient, 0.01);
			ImGui::DragFloat(PrefixLabel("Diffuse").c_str(), &component.LightData.Base.Diffuse, 0.1);
			ImGui::DragFloat(PrefixLabel("Specular").c_str(), &component.LightData.Base.Specular, 0.1);
			ImGui::DragFloat(PrefixLabel("Density").c_str(), &component.LightData.Base.Density, 0.1);
			if (ImGui::TreeNode("Attenuation")) {
				ImGui::DragFloat(PrefixLabel("Constant").c_str(), &component.LightData.Constant, 0.1);
				ImGui::DragFloat(PrefixLabel("Linear").c_str(), &component.LightData.Linear, 0.1);
				ImGui::DragFloat(PrefixLabel("Quadratic").c_str(), &component.LightData.Exp, 0.1);
				ImGui::TreePop();
			}
			ImGui::ColorPicker3("Color", reinterpret_cast<float*>(&component.LightData.Base.Color));
			ImGui::TreePop();
		}
		if (!visible) {
			entity.RemoveComponent<LightPointComponent>();
			visible = true;
		}
	}

	template<>
	void RenderAddComponentImGui<ModelComponent>(Entity entity) {
		if (ImGui::MenuItem("Model")) {
			entity.AddComponent<ModelComponent>();
		}
	}

	template<>
	void RenderAddComponentImGui<LightPointComponent>(Entity entity) {
		if (ImGui::MenuItem("Point Light")) {
			entity.AddComponent<LightPointComponent>();
		}
	}

	template<>
	void RenderAddComponentImGui<LightDirectComponent>(Entity entity) {
		if (ImGui::MenuItem("Directional Light")) {
			entity.AddComponent<LightDirectComponent>();
		}
	}
}