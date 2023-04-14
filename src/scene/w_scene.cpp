//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "scene/w_scene.h"
#include "scene/w_entity.h"
#include "rendering/w_renderer.h"
#include "w_engine.h"

namespace Wiesel {

	Scene::Scene() {
		{
			auto entity = CreateEntity("Directional Light");
			auto& transform = entity.GetComponent<TransformComponent>();
			transform.Position = glm::vec3(1.0f, 1.0f, 1.0f);
			entity.AddComponent<LightDirectComponent>();
		}
		{
			auto entity = CreateEntity("Point Light");
			auto& transform = entity.GetComponent<TransformComponent>();
			transform.Position = glm::vec3{0.0f, 1.0f, 0.0f};
			entity.AddComponent<LightPointComponent>();
		}
	}

	Scene::~Scene() {

	}

	Entity Scene::CreateEntity(const std::string& name) {
		return CreateEntityWithUUID(UUID(), name);
	}

	Entity Scene::CreateEntityWithUUID(UUID uuid, const std::string& name) {
		Entity entity = {m_Registry.create(), this};
		entity.AddComponent<IdComponent>(uuid);
		entity.AddComponent<TransformComponent>();
		auto& tag = entity.AddComponent<TagComponent>();
		tag.Tag = name.empty() ? "Entity" : name;

		m_Entities[uuid] = entity;
		return entity;
	}

	void Scene::DestroyEntity(Entity entity) {
		m_Entities.erase(entity.GetUUID());
		m_Registry.destroy(entity);
	}

	void Scene::OnUpdate(float_t deltaTime) {
		auto& lights = Engine::GetRenderer()->GetLightsBufferObject();
		lights.DirectLightCount = 0;
		lights.PointLightCount = 0;
		for (const auto& entity : m_Registry.view<LightDirectComponent>()) {
			auto& light = m_Registry.get<LightDirectComponent>(entity);
			UpdateLight(lights, light.LightData, {entity, this});
		}
		for (const auto& entity : m_Registry.view<LightPointComponent>()) {
			auto& light = m_Registry.get<LightPointComponent>(entity);
			UpdateLight(lights, light.LightData, {entity, this});
		}
	}

	void Scene::Render() {
		// Render models
		for (const auto& entity : GetAllEntitiesWith<ModelComponent, TransformComponent>()) {
			auto& model = m_Registry.get<ModelComponent>(entity);
			auto& transform = m_Registry.get<TransformComponent>(entity);
			Engine::GetRenderer()->DrawModel(model, transform);
		}
	}
}