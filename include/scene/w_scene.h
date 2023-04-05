
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
#include <entt/entt.hpp>


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

		WIESEL_GETTER_FN bool IsRunning() const { return m_IsRunning; }
		WIESEL_GETTER_FN bool IsPaused() const { return m_IsPaused; }

		void SetPaused(bool paused) { m_IsPaused = paused; }

		template<typename... Components>
		auto GetAllEntitiesWith() {
			return m_Registry.view<Components...>();
		}
	private:
		friend class Entity;
		friend class Application;

		void Render();

		std::unordered_map<UUID, entt::entity> m_Entities;
		entt::registry m_Registry;
		bool m_IsRunning = false;
		bool m_IsPaused = false;

	};
}