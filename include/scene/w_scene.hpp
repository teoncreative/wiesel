
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

#include "w_pch.hpp"
#include <entt/entt.hpp>
#include "scene/w_components.hpp"
#include "events/w_events.hpp"
#include "events/w_appevents.hpp"
#include "rendering/w_camera.hpp"

namespace Wiesel {
	class Entity;

	class Scene {
	public:
		Scene();
		~Scene();

		Entity CreateEntity(const std::string& name = std::string());
		Entity CreateEntityWithUUID(UUID uuid, const std::string& name = std::string());
		void DestroyEntity(Entity entity);

		void OnUpdate(float_t deltaTime);
		void OnEvent(Event& event);

		template<typename T>
		void OnRemoveComponent(entt::entity entity) { }
		template<typename T>
		void OnAddComponent(entt::entity entity, T& component) { }

		WIESEL_GETTER_FN Reference<CameraData> GetPrimaryCamera();
		WIESEL_GETTER_FN Entity GetPrimaryCameraEntity();
		WIESEL_GETTER_FN bool IsRunning() const { return m_IsRunning; }
		WIESEL_GETTER_FN bool IsPaused() const { return m_IsPaused; }

		void SetPaused(bool paused) { m_IsPaused = paused; }

		template<typename... Components>
		auto GetAllEntitiesWith() {
			return m_Registry.view<Components...>();
		}

		entt::registry& GetRegistry() {
			return m_Registry;
		}

	private:
		friend class Entity;
		friend class Application;

		bool OnWindowResizeEvent(WindowResizeEvent& event);
		void Render();

		std::unordered_map<UUID, entt::entity> m_Entities;
		entt::registry m_Registry;
		Reference<CameraData> m_Camera;
		entt::entity m_CameraEntity;
		bool m_HasCamera = false;
		bool m_IsRunning = false;
		bool m_IsPaused = false;
	};
}